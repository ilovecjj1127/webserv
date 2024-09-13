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
			} else if (events[i].events & EPOLLIN) {
				int client_fd = events[i].data.fd;
				ClientData& client_data = _clients_map[client_fd];
				if (_getClientRequest(client_fd) == 0) {
					client_data.response = _prepareResponse(client_data.request, client_data.request.path);
					logger.info(client_data.response);
					_modifyEpollSocketOut(client_fd);
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
	event.events = EPOLLIN | EPOLLET;
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
}

int Webserv::_getClientRequest( int client_fd ) {
	char buffer[4096];
	Request& request = _clients_map[client_fd].request;
	ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
	if (bytes < 0) {
		_closeClientFd(client_fd, "Recv failed");
		return 1;
	} else {
		request.raw.append(buffer, bytes);
	}
	if (request.parseRequest() == 0) {
		if (logger.getLevel() == DEBUG) {
			request.printRequest();
		}
		return 0;
	}
	_closeClientFd(client_fd, nullptr);
	return 1;
}

void Webserv::_modifyEpollSocketOut( int client_fd ) {
	epoll_event event;
	event.events = EPOLLOUT | EPOLLET;
	event.data.fd = client_fd;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &event) == -1) {
		_closeClientFd(client_fd, "epoll_ctl: mod client_fd");
	}
}

std::string Webserv::_prepareResponse( const Request& req, const std::string& file_path, size_t status_code ) {
	if (file_path == "/" && file_path != _index_page) {
		return _prepareResponse(req, _index_page, 200);
	}
	if (file_path.substr(file_path.size() - 3) == ".py") {
		std::string cgi_file = "./nginx_example" + file_path;
		logger.info("CGI file: " + cgi_file);
		if (access(cgi_file.c_str(), F_OK) == 0) {
		 	return (_executeCgi(req, cgi_file));
		}
	}
	std::string full_path = _root_path + file_path;
	std::ifstream file(full_path);
	if (!file.is_open() && file_path != _error_page_404) {
		logger.warning("Failed to open file: " + full_path);
		return _prepareResponse(req, _error_page_404, 404);
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

std::string Webserv::_executeCgi( const Request& req, std::string& path ) {
	int fd_res[2], fd_body[2];
	int status = 0;
	pid_t pid;
	ssize_t bytesread;
	std::string response;
	char buffer[1024] = {};

	if (pipe(fd_res) == -1 || pipe(fd_body) == -1) {
		logger.warning("Pipe failed.");
		return ("");
	}
	if ((pid = fork()) == -1) {
		logger.warning("Fork failed.");
		return ("");
	} else if (pid == 0) {
		close(fd_res[0]);
		close(fd_body[1]);
		dup2(fd_body[0], STDIN_FILENO);
		dup2(fd_res[1], STDOUT_FILENO);
		close(fd_res[1]);
		close(fd_res[1]);
		char** cmds = computeCmd(path);
		char** envp = _createEnvp(req, path);
		execve("/usr/bin/python3", cmds, envp);
		free_array(cmds);
		free_array(envp);
		exit(EXIT_FAILURE);
	}
	else {
		close(fd_body[0]);
		if (req.method == POST) {
			write(fd_body[1], req.body.data(), req.body.size());
		}
		close(fd_body[1]);
		waitpid(pid, &status, 0);
	}
	close(fd_res[1]);
	if (!WIFEXITED(status)) {
		logger.warning("Child process failed.");
		return ("");
	}
	while ((bytesread = read(fd_res[0], buffer, sizeof(buffer)))) {
		response.append(buffer);
	}
	if (bytesread < 0) {
		logger.warning("read from pipe failed.");
	}
	// logger.info("CGI response: " + response);
	// std::cout << response.size() << std::endl;
	return response;
}

// https://datatracker.ietf.org/doc/html/rfc3875#autoid-16
char** Webserv::_createEnvp( const Request& req, std::string& path ) {
	(void)path;
	str_map env_map(req.headers);
	// path
	env_map["PATH_INFO"] = req.path;
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
		temp_str = it.first + "=" + it.second;
		envp[i] = strdup(temp_str.c_str());
		i++;
	}
	envp[i] = NULL;
	return (envp);
}


void Webserv::_sendResponse( int client_fd ) {
	std::string& response = _clients_map[client_fd].response;
	ssize_t bytes_sent = send(client_fd, response.c_str(), response.size(), 0);
	if (bytes_sent == -1) {
		_closeClientFd(client_fd, "send: error");
	} else {
		_closeClientFd(client_fd, nullptr);
	}
	logger.debug("Connection was closed. Client_fd: " + std::to_string(client_fd));
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
