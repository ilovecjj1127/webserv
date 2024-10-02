#include "Webserv.hpp"

void Webserv::_handleEvent( epoll_event& event ) {
	if (_server_sockets_map.find(event.data.fd) != _server_sockets_map.end()) {
		_handleConnection(event.data.fd);
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

void Webserv::_handleConnection( const int server_fd ) {
	sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
	if (client_fd == -1) {
		perror("Failed to accept connection");
		return;
	} else {
		_clients_map[client_fd].last_activity = time(nullptr);
        _clients_map[client_fd].server_fd = server_fd;
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
        _get_target_server(client_fd, request.headers["Host"]);
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
	ssize_t bytes = write(fd_out, chunk.data(), chunk_size);
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
	ssize_t bytes = read(fd_in, buffer, sizeof(buffer));
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
