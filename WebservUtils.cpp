#include "Webserv.hpp"

int Webserv::_setNonBlocking( int fd ) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		return -1;
	}
	flags |= O_NONBLOCK;
	return fcntl(fd, F_SETFL, flags);
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

void Webserv::_modifyEpollSocketOut( int client_fd ) {
	epoll_event event;
	event.events = EPOLLOUT;
	event.data.fd = client_fd;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &event) == -1) {
		_closeClientFd(client_fd, "epoll_ctl: mod client_fd");
	}
}

int Webserv::_stringToInt( const std::string& str ) {
	try {
		int status_code = std::stoi(str);
		return status_code;
	} catch (const std::invalid_argument&) {
		return -1;
	}
}
