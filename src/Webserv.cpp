#include "Webserv.hpp"

Webserv Webserv::_instance;

Webserv::Webserv( void ) : logger(Logger::getInstance()) {
	logger.setLevel(DEBUG);
	logger.debug("Webserv instance created");
	_keep_running = true;
	_epoll_fd = -1;
	_event_array_size = 16;
	_chunk_size = 4096;
	_timeout_period = 5;
}

Webserv::~Webserv( void ) {
	logger.debug("Webserv instance destroyed");
}

Webserv& Webserv::getInstance( void ) {
	return _instance;
}

int Webserv::startServer( const std::string& config_file ) {
	if (_parseConfigFile(config_file) != 0) {
		return 1;
	}
	if (_initWebserv() != 0) {
		return 1;
	}
	_mainLoop();
	return 0;
}

void Webserv::_stopServer( void ) {
	_keep_running = false;
	for ( const auto& [server_fd, server_ptr] : _server_sockets_map ) {
		close(server_fd);
	}
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
				client_data.response.prepareResponseError(500);
				_modifyEpollSocketOut(client_fd);
				return _closeCgiPipe(client_data.cgi.fd_in, client_data.cgi, nullptr);
			}
		}
	}
}

void Webserv::_getTargetServer(int client_fd, const std::string& host) {
	ClientData& client_data = _clients_map[client_fd];
	size_t delimiter = host.find(":");
	std::string hostname = host.substr(0, delimiter);
	int server_fd = client_data.server_fd;
	for (ServerData* server : _server_sockets_map[server_fd]) {
		const std::vector<std::string>& server_names = server->server_names;
		auto it = std::find(server_names.begin(), server_names.end(), hostname);
		if (it != server_names.end()) {
			client_data.server = server;
			return;
		}
	}
	client_data.server = _server_sockets_map[server_fd].front();
}
