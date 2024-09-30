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
	_client_max_body_size = 10;

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
		logger.debug("Epoll got events: " + std::to_string(n));
		if (n == -1) {
			if (_keep_running) perror("epoll_wait");
			break;
		} else if (difftime(time(nullptr), last_timeout_check) >= _timeout_period) {
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
	} else if (_pipe_map.find(event.data.fd) != _pipe_map.end()) {
		if (event.events & EPOLLOUT) {
			_sendCgiRequest(event.data.fd);
		} else if ((event.events & EPOLLIN) || (event.events & EPOLLHUP)) {
			_getCgiResponse(event.data.fd);
		}
	} else if (_clients_map.find(event.data.fd) == _clients_map.end()) {
		return;
	} else if (event.events & EPOLLIN) {
		int client_fd = event.data.fd;
		ClientData& client_data = _clients_map[client_fd];
		if (_getClientRequest(client_fd) == 0) {
			if (_prepareResponse(client_fd, client_data.request.path)) {
				logger.debug("static page");
				logger.debug(client_data.response);
				_modifyEpollSocketOut(client_fd);
			}
		}
	} else if (event.events & EPOLLOUT) {
		_sendClientResponse(event.data.fd);
	}
}

void Webserv::_checkTimeouts( void ) {
	time_t now = time(nullptr);
	for (auto it = _clients_map.begin(); it != _clients_map.end();) {
		int client_fd = it->first;
		ClientData& client_data = it->second;
		++it;
		if (difftime(now, client_data.last_activity) >= _timeout_period) {
			if (client_data.cgi.pid == 0) {
				logger.debug("Timeout for client_fd " + std::to_string(client_fd));
				_closeClientFd(client_fd, nullptr);
			} else {
				logger.debug("CGI timeout for client_fd " + std::to_string(client_fd));
				client_data.last_activity = time(nullptr);
				client_data.response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 25\r\n\r\n500 Internal Server Error";
				_modifyEpollSocketOut(client_fd);
				return _closeCgiPipe(client_data.cgi.fd_in, client_data.cgi, nullptr);
			}
		}
	}
}

void Webserv::_sendCgiRequest( int fd_out ) {
	int client_fd = _pipe_map[fd_out];
	ClientData& client_data = _clients_map[client_fd];
	Request& request = client_data.request;
	size_t bytes_write_total = client_data.bytes_write_total;
	if (bytes_write_total == request.body.size()) {
		return _closeCgiPipe(fd_out, client_data.cgi, nullptr);
	}
	size_t chunk_size = _chunk_size;
	if (request.body.size() - bytes_write_total < _chunk_size) {
		chunk_size = request.body.size() - bytes_write_total;
	}
	std::string_view chunk(request.body.c_str() + bytes_write_total, chunk_size);
	size_t bytes = write(fd_out, chunk.data(), chunk_size);
	client_data.last_activity = time(nullptr);
	if (bytes < 0) {
		client_data.response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 25\r\n\r\n500 Internal Server Error";
		_modifyEpollSocketOut(client_fd);
		return _closeCgiPipe(client_data.cgi.fd_in, client_data.cgi, "write pipe: ");
	}
	_clients_map[client_fd].bytes_write_total += bytes;
	logger.debug("Body size: " + std::to_string(request.body.size()) + " bytes write: " + std::to_string(bytes));
}

void Webserv::_getCgiResponse( int fd_in ) {
	int client_fd = _pipe_map[fd_in];
	ClientData& client_data = _clients_map[client_fd];
	std::string& response = client_data.response;
	char buffer[_chunk_size];
	size_t bytes = read(fd_in, buffer, sizeof(buffer));
	client_data.last_activity = time(nullptr);
	logger.info("bytes read from pipe: " + std::to_string(bytes));
	if (bytes < 0) {
		response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 25\r\n\r\n500 Internal Server Error";
		_modifyEpollSocketOut(client_fd);
		return _closeCgiPipe(fd_in, client_data.cgi, "read pipe: ");
	} else if (bytes > 0) {
		response.append(buffer, bytes);
	}
	if (bytes == 0) {
		size_t pos = response.find("Status:");
		if (pos != std::string::npos) {
			response.replace(pos, 7, "HTTP/1.1");
		} else {
			response.insert(0, "HTTP/1.1 200 OK\r\n");
		}
		logger.info(response);
		_modifyEpollSocketOut(client_fd);
		_closeCgiPipe(fd_in, client_data.cgi, nullptr);
	}
}

void Webserv::_closeCgiPipe( int pipe_fd, CgiData& cgi, const char* err_msg ) {
	close(pipe_fd);
	_pipe_map.erase(pipe_fd);
	if (err_msg != nullptr) {
		perror(err_msg);
	}
	logger.debug("Pipe was closed. Pipe: " + std::to_string(pipe_fd));
	if (pipe_fd == cgi.fd_out) {
		cgi.fd_out = 0;
	} else {
		cgi.fd_in = 0;
		if (cgi.fd_out != 0) {
			_closeCgiPipe(cgi.fd_out, cgi, nullptr);
		}
		kill(cgi.pid, SIGKILL);
		cgi.pid = 0;
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
	epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
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
		if (request.content_length > _client_max_body_size) {
			_clients_map[client_fd].response = "HTTP/1.1 413 Request Entity Too Large\r\nContent-Length: 28\r\n\r\n413 Request Entity Too Large";
			_modifyEpollSocketOut(client_fd);
			return 3;
		}
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

int Webserv::_prepareResponse( int client_fd, const std::string& file_path, size_t status_code ) {
	std::string& response = _clients_map[client_fd].response;
	if (file_path == "/" && file_path != _index_page) {
		return _prepareResponse(client_fd, _index_page, 200);
	}
	if (file_path.substr(file_path.size() - 3) == ".py") {
		std::string cgi_file = "./nginx_example" + file_path;
		logger.info("CGI file: " + cgi_file);
		if (access(cgi_file.c_str(), F_OK) == 0) {
		 	return _executeCgi(client_fd, cgi_file);
		} else {
			return _prepareResponse(client_fd, _error_page_404, 404);
		}
	}
	std::string full_path = _root_path + file_path;
	std::ifstream file(full_path);
	if (!file.is_open() && file_path != _error_page_404) {
		logger.warning("Failed to open file: " + full_path);
		return _prepareResponse(client_fd, _error_page_404, 404);
	} else if (!file.is_open()) {
		logger.warning("Failed to open file: " + full_path);
		std::string page404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
		response = page404;
		return 1;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	response = buffer.str();
	response = _getHtmlHeader(response.size(), status_code) + response;
	return 1;
}

char**	computeCmd( std::string path ) {
	char** cmds = (char **)calloc(3, sizeof(char *));
	if (!cmds) {
		return NULL;
	}
	cmds[0] = strdup("python3");
	if (!cmds[0]) {
		free(cmds);
		return NULL;
	}
	cmds[1] = strdup(path.c_str());
	return (cmds);
}

void free_array(char** arr) {
	int i = -1;

	if (!arr) {
		return ;
	}
	while (arr[++i]) {
		free(arr[i]);
	}
	free(arr);
}

int Webserv::_endCgi( int fd_res[2], int fd_body[2], int client_fd ) {
	if (fd_res[0]) {
		close(fd_res[0]);
	}
	if (fd_res[1]) {
		close(fd_res[1]);
	}
	if (fd_body[0]) {
		close(fd_body[0]);
	}
	if (fd_body[1]) {
		close(fd_body[1]);
	}
	std::string& response = _clients_map[client_fd].response;
	response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 25\r\n\r\n500 Internal Server Error";
	return 1;
}

int Webserv::_executeCgi( int client_fd, std::string& path ) {
	int fd_res[2], fd_body[2];
	if (pipe(fd_res) == -1 || pipe(fd_body) == -1) {
		logger.warning("Pipe failed.");
		return _endCgi(fd_res, fd_body, client_fd);
	}
	pid_t pid = fork();
	if (pid == -1) {
		logger.warning("Fork failed.");
		return _endCgi(fd_res, fd_body, client_fd);
	} else if (pid == 0) {
		close(fd_res[0]);
		close(fd_body[1]);
		dup2(fd_body[0], STDIN_FILENO);
		dup2(fd_res[1], STDOUT_FILENO);
		close(fd_res[1]);
		close(fd_body[0]);
		char** cmds = computeCmd(path);
		char** envp = _createEnvp(_clients_map[client_fd].request, path);
		execve("/usr/bin/python3", cmds, envp);
		free_array(cmds);
		free_array(envp);
		exit(EXIT_FAILURE);
	}
	_clients_map[client_fd].cgi.pid = pid;
	close(fd_body[0]);
	close(fd_res[1]);
	_connectCgi(client_fd, fd_res[0], fd_body[1]);
	return 0;
}

void Webserv::_connectCgi( int client_fd, int fd_in, int fd_out) {
	ClientData& client_data = _clients_map[client_fd];
	CgiData& cgi = client_data.cgi;
	cgi.client_fd = client_fd;
	_setNonBlocking(fd_out);
	_setNonBlocking(fd_in);
	Method method = client_data.request.method;
	if (method == POST || method == DELETE) {
		epoll_event event;
		event.events = EPOLLOUT;
		event.data.fd = fd_out;
		if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd_out, &event) == -1) {
			_clients_map[client_fd].response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 25\r\n\r\n500 Internal Server Error";
			_modifyEpollSocketOut(client_fd);
			close(fd_in);
			return _closeCgiPipe(fd_out, cgi, "Failed to add cgi.fd_out to epoll: ");
		}
		_pipe_map[fd_out] = client_fd;
		cgi.fd_out = fd_out;
	} else {
		close(fd_out);
	}
	epoll_event event;
	event.events = EPOLLIN | EPOLLHUP;
	event.data.fd = fd_in;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd_in, &event) == -1) {
		_clients_map[client_fd].response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 25\r\n\r\n500 Internal Server Error";
		_modifyEpollSocketOut(client_fd);
		return _closeCgiPipe(fd_in, cgi, "Failed to add cgi.fd_in to epoll: ");
	}
	_pipe_map[fd_in] = client_fd;
	cgi.fd_in = fd_in;
}

// https://datatracker.ietf.org/doc/html/rfc3875#autoid-16
char** Webserv::_createEnvp( const Request& req, std::string& path ) {
	(void)path;
	str_map env_map(req.headers);
	env_map["PATH_INFO"] = req.path;
	env_map["SERVER_PROTOCOL"] = "HTTP/1.1";
	env_map["GATEWAY_INTERFACE"] = "CGI/1.1";
	for (auto it = req.params.begin(); it != req.params.end(); ++it) {
		env_map["QUERY_STRING"] += it->first;
		env_map["QUERY_STRING"] += "=";
		env_map["QUERY_STRING"] += it->second;
		if (it != req.params.end()) {
			env_map["QUERY_STRING"] += "&";
		}
	}
	std::unordered_map<Method, std::string> methods_map = {
		{GET, "GET"},
		{POST, "POST"},
		{DELETE, "DELETE"}
	};
	env_map["REQUEST_METHOD"] = methods_map[req.method];
	env_map["SERVER_PORT"] = std::to_string(_listen_port);
	
	char** envp;
	std::string temp_str;
	envp = (char **)calloc(env_map.size() + 1, sizeof(char *));
	int i = 0;
	for (const auto& it : env_map) {
		temp_str = "";
		for (auto& c: it.first) {
			if (c == '-') {
				temp_str += '_';
			} else {
				temp_str += toupper(c);
			}
		}
		temp_str +=  ("=" + it.second);
		envp[i] = strdup(temp_str.c_str());
		i++;
	}
	envp[i] = NULL;
	return (envp);
}

void Webserv::_sendClientResponse( int client_fd ) {
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
