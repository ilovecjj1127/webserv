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
	while (_keep_running) {
		int n = epoll_wait(_epoll_fd, events, _event_array_size, -1);
		if ( n == -1) {
			if (_keep_running) perror("epoll_wait");
			break;
		}
		for (int i = 0; i < n; ++i) {
			if (events[i].data.fd == _server_fd) {
				_handleConnection();
			} else if (_clients_map.find(events[i].data.fd) == _clients_map.end()) {
				_handlePipes(events[i]);
			} else if (events[i].events & EPOLLIN) {
				int client_fd = events[i].data.fd;
				ClientData& client_data = _clients_map[client_fd];
				if (_getClientRequest(client_fd) == 0) {
					if (_prepareResponse(client_fd, client_data.request.path)) {
						logger.info("static page");
						logger.info(client_data.response);
						_modifyEpollSocketOut(client_fd);
					}
				}
			} else if (events[i].events & EPOLLOUT) {
				_sendResponse(events[i].data.fd);
			}
		}
	}
	for (const auto &client_pair : _clients_map) {
		close(client_pair.first);
	}
	close(_epoll_fd);
	if (!_keep_running) logger.info("Interrupted by signal");
}

void Webserv::_handlePipes( epoll_event& event ) {
	int client_fd = _pipe_map[event.data.fd];
	std::string& response = _clients_map[client_fd].response;
	Request& request = _clients_map[client_fd].request;
	size_t bytes_write_total = _clients_map[client_fd].bytes_write_total;
	size_t bytes;

	if (event.events & EPOLLIN) {
		char buffer[_chunk_size];
		bytes = read(event.data.fd, buffer, sizeof(buffer));
		logger.info("bytes read from pipe: " + std::to_string(bytes));
		if (bytes < 0) {
			logger.warning("read from pipe failed.");
		} else if (bytes > 0) {
			response.append(buffer, bytes);
		} 
		if (bytes < _chunk_size) {
			size_t pos = response.find("Status:");
			if (pos != std::string::npos) {
				response.replace(pos, 7, "HTTP/1.1");
			}
			logger.info(response);
			_modifyEpollSocketOut(client_fd);
			close(event.data.fd);
			_pipe_map.erase(event.data.fd);
		}
	}
	if (event.events & EPOLLOUT) {
		if (bytes_write_total == request.body.size()) {
			close(event.data.fd);
			_pipe_map.erase(event.data.fd);
			return;
		}
		std::string_view chunk(request.body.c_str() + bytes_write_total);
		size_t chunk_size = _chunk_size;
		if (request.body.size() - bytes_write_total < _chunk_size) {
			chunk_size = chunk.size();
		}
		bytes = write(event.data.fd, chunk.data(), chunk_size);
		_clients_map[client_fd].bytes_write_total += bytes;
		logger.info("body size: " + std::to_string(request.body.size()) + " bytes write: " + std::to_string(bytes));
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
		_clients_map[client_fd];
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
		 	_executeCgi(client_fd, cgi_file);
		}
		return 0;
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

void Webserv::_executeCgi( int client_fd, std::string& path ) {
	int fd_res[2], fd_body[2];
	pid_t pid;
	Request& req = _clients_map[client_fd].request;

	if (pipe(fd_res) == -1 || pipe(fd_body) == -1) {
		logger.warning("Pipe failed.");
		return;
	}
	if ((pid = fork()) == -1) {
		logger.warning("Fork failed.");
		return;
	} else if (pid == 0) {
		close(fd_res[0]);
		close(fd_body[1]);
		dup2(fd_body[0], STDIN_FILENO);
		dup2(fd_res[1], STDOUT_FILENO);
		close(fd_res[1]);
		close(fd_body[0]);
		char** cmds = computeCmd(path);
		char** envp = _createEnvp(req, path);
		execve("/usr/bin/python3", cmds, envp);
		free_array(cmds);
		free_array(envp);
		exit(EXIT_FAILURE);
	}
	close(fd_body[0]);
	close(fd_res[1]);
	if (req.method == POST || req.method == DELETE) {
		epoll_event event;
		event.events = EPOLLOUT;
		event.data.fd = fd_body[1];
		if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd_body[1], &event) == -1) {
			_initError("Failed to add read pipe to epoll");
			return;
		}
		_pipe_map[fd_body[1]] = client_fd;
	} else {
		close(fd_body[1]);
	}
	epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = fd_res[0];
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd_res[0], &event) == -1) {
		_initError("Failed to add read pipe to epoll");
		return;
	}
	_pipe_map[fd_res[0]] = client_fd;
}

// https://datatracker.ietf.org/doc/html/rfc3875#autoid-16
char** Webserv::_createEnvp( const Request& req, std::string& path ) {
	(void)path;
	str_map env_map(req.headers);
	// path
	env_map["PATH_INFO"] = req.path;
	// env_map["SCRIPT_NAME"] = "/ngnix_example/cgi/upload_cgi.py";
	// env_map["SERVER_PORT"] = std::to_string(_listen_port);
	env_map["SERVER_PROTOCOL"] = "HTTP/1.1";
	env_map["GATEWAY_INTERFACE"] = "CGI/1.1";
	// env_map["CONTENT_TYPE"] = "application/x-www-form-urlencoded";
	// params
	for (auto it = req.params.begin(); it != req.params.end(); ++it) {
		env_map["QUERY_STRING"] += it->first;
		env_map["QUERY_STRING"] += "=";
		env_map["QUERY_STRING"] += it->second;
		if (it != req.params.end()) {
			env_map["QUERY_STRING"] += "&";
		}
	}
	// methods
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
