#include "Webserv.hpp"

int Webserv::_initWebserv( void ) {
	std::unordered_map<std::string, int> listen_map;
	for (ServerData& server : _servers) {
		if (_initServer(server, listen_map) != 0) {
			return -1;
		}
	}
	_epoll_fd = epoll_create(1);
	if (_epoll_fd == -1) {
		return _initError("Failed to create epoll", -1);
	}
	for (const auto& [server_fd, server_ptr] : _server_sockets_map) {
		if (_addServerToEpoll(server_fd) != 0) {
			return -1;
		}
	}
	logger.info("Webserv is running now");
	signal(SIGINT, handleSigInt);
	return 0;
}

int Webserv::_initServer( ServerData& server, std::unordered_map<std::string, int>& listen_map) {
	for (const auto& [ip_address, port] : server.listen_group) {
		std::string ip_port = std::to_string(ip_address) + ":" + std::to_string(port);
		int server_fd;
		if (listen_map.find(ip_port) == listen_map.end()) {
			server_fd = _createServerSocket(ip_address, port);
			if (server_fd == -1) {
				return -1;
			}
			listen_map[ip_port] = server_fd;
		} else {
			server_fd = listen_map[ip_port];
		}
		_server_sockets_map[server_fd].push_back(&server);
	}
	return 0;
}

int Webserv::_createServerSocket( uint32_t ip_address, uint16_t port ) {
	int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_fd == -1) {
		return _initError("Failed to create socket", -1);
	}
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(ip_address);
	server_addr.sin_port = htons(port);
	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		return _initError("Failed to set socket opt", server_fd);
	}
	if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		return _initError("Failed to bind socket", server_fd);
	}
	if (listen(server_fd, SOMAXCONN) == -1) {
		return _initError("Failed to listen on socket", server_fd);
	}
	if (_setNonBlocking(server_fd) == -1) {
		return _initError("Failed to set non-blocking mode: server_fd", server_fd);
	}
	return server_fd;
}

int Webserv::_addServerToEpoll( const int server_fd ) {
	epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = server_fd;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
		return _initError("Failed to add server_fd to epoll", -1);
	}
	return 0;
}

void Webserv::handleSigInt(int signum) {
	(void)signum;
	Webserv::getInstance()._stopServer();
}

int Webserv::_initError( const char* err_msg, int fd ) {
	perror(err_msg);
	if (fd != -1) {
		close(fd);
	}
	for ( const auto& [server_fd, server_ptr] : _server_sockets_map ) {
		close(server_fd);
	}
	if (_epoll_fd != -1) {
		close(_epoll_fd);
	}
	return -1;
}
