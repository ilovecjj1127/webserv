#include "Webserv.hpp"

Webserv Webserv::_instance;

Webserv::Webserv( void ) {
	logger.setLevel(DEBUG);
	logger.debug("Webserv instance created");
	_keep_running = true;
	_server_fd = -1;
	_epoll_fd = -1;
	_event_array_size = 16;
	_listen_port = 8081;
	_root_path = "./nginx_example/html";
	_index_page = "/index.html";
	_error_page_404 = "/404.html";
	_chunk_size = 4096;
	_timeout_period = 5;

}

Webserv::~Webserv( void ) {
	logger.debug("Webserv instance destroyed");
}

Webserv& Webserv::getInstance( void ) {
	return _instance;
}

int Webserv::startServer( void ) {
	if (_initServer() == 1) {
		return 1;
	}
	_mainLoop();
	return 0;
}

int Webserv::_initServer( void ) {
	_server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_server_fd == -1) {
		return _initError("Failed to create socket");
	}
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(_listen_port);
	int opt = 1;
	if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		return _initError("Failed to set socket opt");
	}
	if (bind(_server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		return _initError("Failed to bind socket");
	}
	if (listen(_server_fd, SOMAXCONN) == -1) {
		return _initError("Failed to listen on socket");
	}
	if (_setNonBlocking(_server_fd) == -1) {
		return _initError("Failed to set non-blocking mode: server_fd");
	}
	_epoll_fd = epoll_create(1);
	if (_epoll_fd == -1) {
		return _initError("Failed to create epoll");
	}
	epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = _server_fd;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server_fd, &event) == -1) {
		return _initError("Failed to add server_fd to epoll");
	}
	logger.info("Server is listening on port " + std::to_string(_listen_port));
	signal(SIGINT, handleSigInt);
	return 0;
}

int Webserv::_initError( const char* err_msg ) {
	perror(err_msg);
	if (_server_fd != -1) {
		close(_server_fd);
	}
	if (_epoll_fd != -1) {
		close(_epoll_fd);
	}
	return 1;
}

int Webserv::_setNonBlocking( int fd ) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		return -1;
	}
	flags |= O_NONBLOCK;
	return fcntl(fd, F_SETFL, flags);
}

void Webserv::_mainLoop( void ) {
	epoll_event events[_event_array_size];
	time_t last_timeout_check = time(nullptr);
	while (_keep_running) {
		int n = epoll_wait(_epoll_fd, events, _event_array_size, _timeout_period * 1000);
		if ( n == -1) {
			if (_keep_running) perror("epoll_wait");
			break;
		} else if (difftime(time(nullptr), last_timeout_check) > _timeout_period) {
			_checkTimeouts();
			last_timeout_check = time(nullptr);
		}
		for (int i = 0; i < n; ++i) {
			_handleEvent(events[i]);
		}
	}
	for (const auto &client_pair : _clients_map) {
		close(client_pair.first);
	}
	close(_epoll_fd);
	if (!_keep_running) logger.info("Interrupted by signal");
}

void Webserv::_handleEvent( epoll_event& event ) {
	if (event.data.fd == _server_fd) {
		_handleConnection();
	} else if (event.events & EPOLLIN) {
		int client_fd = event.data.fd;
		ClientData& client_data = _clients_map[client_fd];
		if (_getClientRequest(client_fd) == 0) {
			client_data.response = _prepareResponse(client_data.request.path);
			_modifyEpollSocketOut(client_fd);
		}
	} else if (event.events & EPOLLOUT) {
		_sendResponse(event.data.fd);
	}
}

void Webserv::_checkTimeouts( void ) {
	time_t now = time(nullptr);
	for (auto it = _clients_map.begin(); it != _clients_map.end();) {
		int client_fd = it->first;
		time_t last_activity = it->second.last_activity;
		++it;
		if (difftime(now, last_activity) > _timeout_period) {
			logger.debug("Timeout for client_fd " + std::to_string(client_fd));
			_closeClientFd(client_fd, nullptr);
		}
	}
}

void Webserv::_handleConnection( void ) {
	sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(_server_fd, (sockaddr*)&client_addr, &client_len);
	if (client_fd == -1) {
		perror("Failed to accept connection");
		return;
	} else {
		_clients_map[client_fd].last_activity = time(nullptr);
	}
	if (_setNonBlocking(client_fd) == -1) {
		_closeClientFd(client_fd, "Failed to set non-blocking mode: client_fd");
		return;
	}
	epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = client_fd;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
		_closeClientFd(client_fd, "epoll_ctl: add client_fd");
		return;
	}
	logger.debug("Accepted connection on client_fd " + std::to_string(client_fd));
}

void Webserv::_closeClientFd( int client_fd, const char* err_msg ) {
	close(client_fd);
	_clients_map.erase(client_fd);
	if (err_msg != nullptr) {
		perror(err_msg);
	}
	logger.debug("Connection was closed. Client_fd: " + std::to_string(client_fd));
}

int Webserv::_getClientRequest( int client_fd ) {
	char buffer[_chunk_size];
	Request& request = _clients_map[client_fd].request;
	ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
	logger.debug(std::to_string(bytes) + " bytes received from client_fd " + std::to_string(client_fd));
	if (bytes <= 0) {
		_closeClientFd(client_fd, "Recv failed");
		return 1;
	} else if (bytes > 0) {
		request.raw.append(buffer, bytes);
	}
	if (request.status == NEW && request.raw.find("\r\n\r\n") != std::string::npos) {
		request.status = request.parseRequest();
	} else if (request.status == FULL_HEADER) {
		request.status = request.getRequestBody();
	}
	if (logger.getLevel() == DEBUG && request.status == FULL_BODY) {
		request.printRequest();
	}
	_clients_map[client_fd].last_activity = time(nullptr);
	if (request.status == NEW || request.status == FULL_HEADER) {
		return 2;
	}
	return 0;
}

void Webserv::_modifyEpollSocketOut( int client_fd ) {
	epoll_event event;
	event.events = EPOLLOUT;
	event.data.fd = client_fd;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &event) == -1) {
		_closeClientFd(client_fd, "epoll_ctl: mod client_fd");
	}
}

std::string Webserv::_prepareResponse( const std::string& file_path, size_t status_code ) {
	if (file_path == "/" && file_path != _index_page) {
		return _prepareResponse(_index_page, 200);
	}
	std::string full_path = _root_path + file_path;
	std::ifstream file(full_path);
	if (!file.is_open() && file_path != _error_page_404) {
		logger.warning("Failed to open file: " + full_path);
		return _prepareResponse(_error_page_404, 404);
	} else if (!file.is_open()) {
		logger.warning("Failed to open file: " + full_path);
		std::string page404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
		return page404;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string response = buffer.str();
	response = _getHtmlHeader(response.size(), status_code) + response;
	return response;
}

void Webserv::_sendResponse( int client_fd ) {
	std::string& response = _clients_map[client_fd].response;
	size_t bytes_sent_total = _clients_map[client_fd].bytes_sent_total;
	std::size_t chunk_size = _chunk_size;
	if (response.size() == bytes_sent_total) {
		logger.debug("Nothing to send");
		_closeClientFd(client_fd, nullptr);
		return;
	} else if (response.size() - bytes_sent_total < _chunk_size) {
		chunk_size = response.size() - bytes_sent_total;
	}
	std::string_view chunk(response.c_str() + bytes_sent_total, chunk_size);
	ssize_t bytes_sent = send(client_fd, chunk.data(), chunk_size, 0);
	logger.debug(std::to_string(bytes_sent) + " bytes sent to client_fd " + std::to_string(client_fd));
	if (bytes_sent <= 0) {
		_closeClientFd(client_fd, "send: error");
	} else if (bytes_sent + bytes_sent_total == response.size()) {
		_closeClientFd(client_fd, nullptr);
	} else {
		_clients_map[client_fd].bytes_sent_total += bytes_sent;
		_clients_map[client_fd].last_activity = time(nullptr);
	}
}

std::string Webserv::_getHtmlHeader( size_t content_length, size_t status_code ) {
	std::string header = "HTTP/1.1 ";
	if (status_code == 404) {
		header += "404 Not Found\r\n";
	} else {
		header += "200 OK\r\n";
	}
	header += "Content-Type: text/html\r\n";
	header += "Content-Length: " + std::to_string(content_length) + "\r\n\r\n";
	return header;
}

void Webserv::_stopServer( void ) {
	_keep_running = false;
	close(_server_fd);
}

void Webserv::handleSigInt(int signum) {
	(void)signum;
	Webserv::getInstance()._stopServer();
}
