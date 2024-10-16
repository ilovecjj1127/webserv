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

int Webserv::startServer( void ) {
	if (_parseConfigFile("default.conf") != 0) {
		return 1;
	}
	// _fakeConfigParser();
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

void Webserv::_sortLocationByPath( void ) {
	for (ServerData& server : _servers) {
		std::vector<Location>& locations = server.locations;
		std::sort(locations.begin(), locations.end(),
				[](const Location& a, const Location& b) {
					return a.path.size() > b.path.size();
				});
	}
}

void Webserv::_printConfig( void ) const {
	std::cout << "\033[36m" << "_______\nCONFIG" << std::endl;
	for (const ServerData& server : _servers) {
		for (const auto& [ip_address, port] : server.listen_group) {
			std::cout << "\nListen at: " << ip_address << ":" << port << std::endl;
		}
		for (const std::string& server_name : server.server_names) {
			std::cout << "Server Name: " <<  server_name << std::endl;
		}
		for (const Location& location : server.locations) {
			std::cout << "Location:\n\tpath: " << location.path << std::endl;
			std::cout << "\troot: " << location.root << std::endl;
			std::cout << "\tindex: " << location.index_page << std::endl;
			std::cout << "\tredirect_path: " << location.redirect_code << " " << location.redirect_path << std::endl;
			std::cout << "\tautoindex: " << location.autoindex << std::endl;
			std::cout << "\tclient_max_body_size: " << location.client_max_body_size << std::endl;
			for (const auto& [error_code, error_page] : location.error_pages) {
				std::cout << "\terror_page: " << error_code << " " << error_page << std::endl;
			}
			std::unordered_map<Method, std::string> methods_map = {
				{GET, "GET"},
				{POST, "POST"},
				{DELETE, "DELETE"}
			};
			std::cout << "\tallowed_methods:";
			for (const Method& method : location.allowed_methods) std::cout << " " << methods_map[method];
			std::cout << std::endl;
		}
	}
	std::cout << "_______" << "\033[0m" << std::endl;
}

enum ParseStatus {
	START,
	SERVER,
	LOCATION,
};

int _getIndentation( const std::string& line ) {
	int indentation = 0;
	for (char c : line) {
		if (c == ' ') {
			++indentation;
		} else if (c == '\t') {
			indentation += 4;
		} else {
			break;
		}
	}
	return indentation;
}

uint32_t Webserv::_ipStringToDecimal( const std::string& ip_address ) {
	uint32_t result = 0;
	std::istringstream ip_stream(ip_address);
	std::string octet;
	int shift = 24;
	int octet_count = 0;
	while (std::getline(ip_stream, octet, '.')) {
		uint32_t octetValue = static_cast<uint32_t>(std::stoi(octet));
		if (octetValue > 255) {
			throw std::out_of_range("Invalid IP address: " + ip_address);
		}
		result |= (octetValue << shift);
        shift -= 8;
		++octet_count;
	}
	if (octet_count != 4 && result != 0) {
		throw std::out_of_range("Invalid IP address: " + ip_address);
	}
	return result;
}

int Webserv::_parseServerData( ServerData& server, ConfigServerData& temp_var, const std::string& line ) {
	size_t delimiter = line.find(":");
	if (delimiter == std::string::npos) {
		logger.error("Invalid server line: " + line);
		return 1;
	}
	std::istringstream line_stream(line.substr(delimiter + 1));

	if (line.find("listen:") != std::string::npos) {
		try {
			long port_value = std::stol(line.substr(line.find_last_of(':') + 1));
			if (port_value < 1 || port_value > 65535) {
				throw std::out_of_range("Port number out of range: " + line);
			}
			uint16_t port = static_cast<uint16_t>(port_value);
			std::string ip_address;
			std::getline(line_stream, ip_address, ':');
			uint32_t ip = _ipStringToDecimal(ip_address);
			server.listen_group.push_back(std::make_pair(ip, port));
		} catch (const std::invalid_argument& e) {
			logger.error("Invalid ip:port format: " + std::string(e.what()));
			return 1;
		} catch (const std::out_of_range& e) {
			logger.error("[ip:port] out of range: " + std::string(e.what()));
			return 1;
		}
	} else if (line.find("server_name:") != std::string::npos) {
		std::string server_name;
		while (line_stream >> server_name) {
			server.server_names.push_back(server_name);
		}
	} else if (line.find("autoindex:") != std::string::npos && line.find("on") != std::string::npos) {
		temp_var.autoindex = 1;
	} else if (line.find("index:") != std::string::npos) {
		line_stream >> temp_var.index_page;
	} else if (line.find("client_max_body_size:") != std::string::npos) {
		line_stream >> temp_var.client_max_body_size;
	} else if (line.find("error_page:") != std::string::npos) {
		int error_code;
		std::string error_path;
		while (line_stream >> error_code) {
			temp_var.error_pages[error_code] = "";
		}
		if (line_stream.fail()) {
			line_stream.clear();
			line_stream >> error_path;
		}
		for (auto& entry : temp_var.error_pages) {
			entry.second = error_path;
		}
	}
	return 0;
}

// put everything in try catch
int Webserv::_parseLocation( Location& location, const std::string& line ) {
	size_t delimiter = line.find(":");
	if (delimiter == std::string::npos) {
		logger.warning("Invalid location line: " + line);
		return 1;
	}
	std::istringstream line_stream(line.substr(delimiter + 1));

	if (line.find("root:") != std::string::npos) {
		line_stream >> location.root;
	} else if (line.find("autoindex:") != std::string::npos) {
		if (line.find("on") != std::string::npos) {
			location.autoindex = 1;
		} else {
			location.autoindex = 0;
		}
	} else if (line.find("index:") != std::string::npos) {
		line_stream >> location.index_page;
	} else if (line.find("client_max_body_size:") != std::string::npos) {
		line_stream >> location.client_max_body_size;
	} else if (line.find("error_page:") != std::string::npos) {
		int error_code;
		std::string error_path;
		while (line_stream >> error_code) {
			location.error_pages[error_code] = "";
		}
		if (line_stream.fail()) {
			line_stream.clear();
			line_stream >> error_path;
		}
		for (auto& entry : location.error_pages) {
			entry.second = error_path;
		}
	} else if (line.find("rewrite:") != std::string::npos) {
		line_stream >> location.redirect_code >> location.redirect_path;
	} else if (line.find("limit_except:") != std::string::npos) {
		location.allowed_methods = {};
		std::string method;
		std::unordered_map<std::string, Method> methods_map = {
			{"GET", GET},
			{"POST", POST},
			{"DELETE", DELETE}
		};
		while (line_stream >> method) {
			auto it = methods_map.find(method);
			if (it != methods_map.end()) {
				location.allowed_methods.insert(it->second);
			} else {
				logger.warning("Invalid method: " + method + " from line: " + line);
				return 1;
			}
		}
	}
	return 0;
}

void Webserv::_checkParamsPriority( ServerData& server, ConfigServerData& temp_var ) {
	for (Location& location : server.locations) {
		if (location.autoindex == -1) {
			location.autoindex = temp_var.autoindex;
		}
		if (location.client_max_body_size == SIZE_MAX) {
			location.client_max_body_size = temp_var.client_max_body_size;
		}
		if (location.index_page.empty()) {
			location.index_page = temp_var.index_page;
		}
		for (auto& [code, path] : temp_var.error_pages) {
			if (location.error_pages.find(code) == location.error_pages.end()) {
				location.error_pages[code] = path;
			}
		}
	}
}

// if wrong identation check needed?
int Webserv::_parseConfigFile( const std::string& config_path ) {
	std::ifstream file(config_path);
	if (!file.is_open()) {
		logger.warning("Failed to open config file: ");
		return 1;
	}

	std::string line;
	ParseStatus status = START;
	ServerData server;
	Location location;
	ConfigServerData temp_var;
	int pre_indentation = 0;

	while (std::getline(file, line)) {
		if (line.find(":") == std::string::npos) continue;
		int curr_indentation = _getIndentation(line);

		if (line.find("server:") != std::string::npos) {
			if (location.path != "") {
				server.locations.push_back(location);
				location = Location();
			}
			if (status != START) {
				_checkParamsPriority(server, temp_var);
				_servers.push_back(server);
				server = ServerData();
				temp_var = ConfigServerData();
			}
			status = SERVER;
		} else if (line.find("location") != std::string::npos) {
			if (location.path != "") {
				server.locations.push_back(location);
				location = Location();
			}
			size_t delimiter1 = line.find('/');
			size_t delimiter2 = line.find(':');
			if (delimiter1 == std::string::npos || delimiter2 == std::string::npos) {
				logger.warning("Invalid location line1: " + line);
				continue;
			}
			location.path = line.substr(delimiter1, delimiter2 - delimiter1);
			status = LOCATION;
		} else if (status == SERVER) {
			if (_parseServerData(server, temp_var, line)) return 1;
		} else if (status == LOCATION) {
			if (curr_indentation >= pre_indentation) {
				if (_parseLocation(location, line)) return 1;
			} else {
				server.locations.push_back(location);
				location = Location();
				status = SERVER;
				if (_parseServerData(server, temp_var, line)) return 1;
			}
		}
		pre_indentation = curr_indentation;
	}
	if (status == LOCATION) {
		server.locations.push_back(location);
	}
	_checkParamsPriority(server, temp_var);
	_servers.push_back(server);
	_sortLocationByPath();
	_printConfig();
	return 0;
}

void Webserv::_fakeConfigParser( void ) {
	ServerData server1;
	std::pair<uint32_t, uint16_t> listen_pair1(0, 8081);
	server1.listen_group.push_back(listen_pair1);
	
	Location location1;
	location1.path = "/";
	location1.root = "./nginx_example/html";
	location1.index_page = "index.html";
	location1.autoindex = true;
	// location1.error_pages[404] = "./temp/404.html";
	server1.locations.push_back(location1);

	Location location2;
	location2.path = "/askme";
	location2.redirect_path = "https://www.google.com/";
	location2.redirect_code = 301;
	server1.locations.push_back(location2);

	Location location3;
	location3.path = "/cgi";
	location3.root = "./nginx_example/cgi";
	location3.autoindex = true;
	// location3.error_pages[400] = "./temp/404.html";
	server1.locations.push_back(location3);
	
	_servers.push_back(server1);

	ServerData server2;
	std::pair<uint32_t, uint16_t> listen_pair2(0, 8082);
	server2.listen_group.push_back(listen_pair2);
	server2.server_names.push_back("localhost");

	Location location4;
	location4.path = "/cgi";
	location4.root = "./nginx_example/cgi";
	location4.client_max_body_size = 10;
	server2.locations.push_back(location4);

	_servers.push_back(server2);

	_sortLocationByPath();
	_printConfig();
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
