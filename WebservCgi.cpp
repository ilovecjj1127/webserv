#include "Webserv.hpp"

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
	_prepareResponseError(_clients_map[client_fd], 500);
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
			_prepareResponseError(_clients_map[client_fd], 500);
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
		_prepareResponseError(_clients_map[client_fd], 500);
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
	// env_map["SERVER_PORT"] = std::to_string(_listen_port);
	
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
