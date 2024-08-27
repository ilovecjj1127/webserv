#include "Webserv.hpp"

Webserv Webserv::_instance;

Webserv::Webserv( void ) {
	std::cout << "Webserv instance created" << std::endl;
	_keep_running = true;
	_server_fd = -1;
	_epoll_fd = -1;
	_event_array_size = 16;
	_listen_port = 8081;
	_root_path = "./nginx_example/html";
	_index_page = "/index.html";
	_error_page_404 = "/404.html";
}

Webserv::~Webserv( void ) {
	std::cout << "Webserv instance destroyed" << std::endl;
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
	std::cout << "Server is running on port " << _listen_port << std::endl;
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
	epoll_event event;
	while (true) {
		int n = epoll_wait(_epoll_fd, events, _event_array_size, -1);
		if ( n == -1) {
			perror("epoll_wait");
			break;
		}
		for (int i = 0; i < n; ++i) {
			if (events[i].data.fd == _server_fd) {
				// Handle new incoming connection
				sockaddr_in client_addr;
				socklen_t client_len = sizeof(client_addr);
				int client_fd = accept(_server_fd, (sockaddr*)&client_addr, &client_len);
				if (client_fd == -1) {
					perror("Failed to accept connection");
					continue;
				}
				if (_setNonBlocking(client_fd) == -1) {
					perror("Failed to set non-blocking mode: client_fd");
					close(client_fd);
					continue;
				}
				event.events = EPOLLIN | EPOLLET;
				event.data.fd = client_fd;
				if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
					perror("epoll_ctl: client_fd");
					close(client_fd);
					continue;
				}
				std::cout << "Accepted connection on client_fd " << client_fd << std::endl;
			} else {
				// Handle data from client
				int client_fd = events[i].data.fd;
				Request request;
				if (_getClientRequest(client_fd, request) == 0) {
					// Now it works only with correct requests. Should be changed
					printRequest(request);
					_sendHtml(client_fd, request.path);
				}
				close(client_fd);
				std::cout << "Connection was closed. Client_fd: " << client_fd << std::endl;
			}
		}
		if (!_keep_running) {
			std::cout << "\nInterrupted by signal" << std::endl;
			break;
		}
	}
	close(_epoll_fd);
}

void Webserv::_sendHtml(int client_fd, const std::string& file_path, size_t status_code) {
	if (file_path == "/" && file_path != _index_page) {
		return _sendHtml(client_fd, _index_page, 200);
	}
	std::string full_path = _root_path + file_path;
	std::ifstream file(full_path);
	if (!file.is_open() && file_path != _error_page_404) {
		std::cerr << "Failed to open file: " << full_path << std::endl;
		return _sendHtml(client_fd, _error_page_404, 404);
	} else if (!file.is_open()) {
		std::cerr << "Failed to open file: " << full_path << std::endl;
		std::string page404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
		send(client_fd, page404.c_str(), page404.size(), 0);
		return;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string html_content = buffer.str();
	std::string header = _getHtmlHeader(html_content.size(), status_code);
	send(client_fd, header.c_str(), header.size(), 0);
	send(client_fd, html_content.c_str(), html_content.size(), 0);
	std::cout << "Sent HTML file: " << full_path << std::endl;
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
