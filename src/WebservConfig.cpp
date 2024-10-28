#include "Webserv.hpp"

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

int Webserv::_parseListenGroup( ServerData& server, std::istringstream& line_stream, const std::string& line ) {
	uint32_t ip = 0;
	try {
		long port_value = std::stol(line.substr(line.find_last_of(':') + 1));
		if (port_value < 1 || port_value > 65535) {
			throw std::out_of_range("Port number out of range: " + line);
		}
		uint16_t port = static_cast<uint16_t>(port_value);
		if (line.find(":") != line.find_last_of(":")) {
			std::string ip_address;
			std::getline(line_stream, ip_address, ':');
			ip = _ipStringToDecimal(ip_address);
		}
		server.listen_group.push_back(std::make_pair(ip, port));
	} catch (const std::invalid_argument& e) {
		logger.error("Invalid ip:port format: " + std::string(e.what()));
		return 1;
	} catch (const std::out_of_range& e) {
		logger.error("[ip:port] out of range: " + std::string(e.what()));
		return 1;
	}
	return 0;
}

int Webserv::_parseErrorPage( std::istringstream& line_stream, std::unordered_map<int, std::string>& error_pages ) {
	int error_code;
	std::string error_path;
	while (line_stream >> error_code) {
		if (error_code < 400 || error_code > 599) {
			logger.error("Invalid error code: " + std::to_string(error_code));
			return 1;
		}
		error_pages[error_code] = "";
	}
	if (line_stream.fail()) {
		line_stream.clear();
		line_stream >> error_path;
	}
	for (auto& entry : error_pages) {
		entry.second = error_path;
	}
	return 0;
}

int Webserv::_parseServerData( ServerData& server, ConfigData& config_data, const std::string& line ) {
	size_t delimiter = line.find(":");
	if (delimiter == std::string::npos) {
		logger.error("Invalid server line: " + line);
		return 1;
	}
	std::istringstream line_stream(line.substr(delimiter + 1));

	if (line.find("listen:") != std::string::npos) {
		return _parseListenGroup(server, line_stream, line);
	} else if (line.find("server_name:") != std::string::npos) {
		std::string server_name;
		while (line_stream >> server_name) {
			server.server_names.push_back(server_name);
		}
	} else if (line.find("autoindex:") != std::string::npos && line.find("on") != std::string::npos) {
		config_data.autoindex = 1;
	} else if (line.find("index:") != std::string::npos) {
		line_stream >> config_data.index_page;
	} else if (line.find("client_max_body_size:") != std::string::npos) {
		line_stream >> config_data.client_max_body_size;
	} else if (line.find("error_page:") != std::string::npos) {
		if (_parseErrorPage(line_stream, config_data.error_pages) == 1) return 1;
	} else {
		logger.error("Invalid config line: " + line);
		return 1;
	}
	return 0;
}

int Webserv::_parseAllowedMethod( std::istringstream& line_stream, std::set<Method>& allowed_methods ) {
	allowed_methods = {};
	std::string method;
	std::unordered_map<std::string, Method> methods_map = {
		{"GET", GET},
		{"POST", POST},
		{"DELETE", DELETE}
	};
	while (line_stream >> method) {
		auto it = methods_map.find(method);
		if (it != methods_map.end()) {
			allowed_methods.insert(it->second);
		} else {
			logger.warning("Invalid method: " + method);
			return 1;
		}
	}
	return 0;
}

int Webserv::_parseLocation( Location& location, const std::string& line ) {
	size_t delimiter = line.find(":");
	if (delimiter == std::string::npos) {
		logger.error("Invalid location line: " + line);
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
		if (_parseErrorPage(line_stream, location.error_pages) == 1) return 1;
	} else if (line.find("rewrite:") != std::string::npos) {
		line_stream >> location.redirect_code >> location.redirect_path;
	} else if (line.find("limit_except:") != std::string::npos) {
		if (_parseAllowedMethod(line_stream, location.allowed_methods) == 1) return 1;
	} else {
		logger.error("Invalid config line: " + line);
		return 1;
	}
	return 0;
}

int Webserv::_parseLoggingLevel( const std::string& line ) {
	size_t delimiter = line.find(":");
	if (delimiter == std::string::npos) {
		logger.error("Invalid line: " + line);
		return 1;
	}
	std::istringstream line_stream(line.substr(delimiter + 1));
	std::string level;
	line_stream >> level;
	if (level.find_last_not_of(" \t") != std::string::npos) {
		level = level.substr(0, level.find_last_not_of(" \t") + 1);
	}
	std::unordered_map<std::string, Level> levels_map = {
		{"DEBUG", DEBUG},
		{"INFO", INFO},
		{"WARNING", WARNING},
		{"ERROR", ERROR},
		{"SILENCE", SILENCE}
	};
	auto it = levels_map.find(level);
	if (it != levels_map.end()) {
		logger.setLevel(it->second);
	} else {
		logger.error("Invalid Level: " + line);
		return 1;
	}
	return 0;
}

void Webserv::_checkParamsPriority( ServerData& server, ConfigData& config_data ) {
	for (Location& location : server.locations) {
		if (location.autoindex == -1) {
			location.autoindex = config_data.autoindex;
		}
		if (location.client_max_body_size == SIZE_MAX) {
			location.client_max_body_size = config_data.client_max_body_size;
		}
		if (location.index_page.empty()) {
			location.index_page = config_data.index_page;
		}
		for (auto& [code, path] : config_data.error_pages) {
			if (location.error_pages.find(code) == location.error_pages.end()) {
				location.error_pages[code] = path;
			}
		}
	}
}

int Webserv::_parseConfigLine( const std::string& line, ServerData& server, Location& location, ConfigData& config_data ) {
	int curr_indentation = _getIndentation(line);
	if (config_data.status == SERVER) {
		if (config_data.server_indentation == 0) {
			config_data.server_indentation = curr_indentation;
		} else if (curr_indentation != config_data.server_indentation) {
			logger.error("Unclear indentation: " + line);
			return 1;
		}
		if (_parseServerData(server, config_data, line)) return 1;
	} else if (config_data.status == LOCATION) {
		if (config_data.location_indentation == 0) {
			config_data.location_indentation = curr_indentation;
		}
		if (curr_indentation == config_data.location_indentation) {
			if (_parseLocation(location, line)) return 1;
		} else if (curr_indentation == config_data.server_indentation) {
			server.locations.push_back(location);
			location = Location();
			config_data.location_indentation = 0;
			config_data.status = SERVER;
			if (_parseServerData(server, config_data, line)) return 1;
		} else {
			logger.error("Unclear indentation: " + line);
			return 1;
		}
	}
	return 0;
}

int Webserv::_addServer( ServerData& server, ConfigData& config_data, Location& location ) {
	if (config_data.status == LOCATION) {
		server.locations.push_back(location);
		location = Location();
	}
	if (server.listen_group.empty()) {
		logger.error("Listen group not found.");
		return 1;
	}
	_checkParamsPriority(server, config_data);
	_servers.push_back(server);
	server = ServerData();
	config_data = ConfigData();
	return 0;
}

int Webserv::_parseLocationPathLine( const std::string& line, ServerData& server, Location& location, ConfigData& config_data ) {
	if (config_data.status == LOCATION) {
		server.locations.push_back(location);
		location = Location();
		config_data.location_indentation = 0;
	}
	size_t delimiter1 = line.find('/');
	size_t delimiter2 = line.find(':');
	if (delimiter1 == std::string::npos || delimiter2 == std::string::npos) {
		logger.warning("Invalid location line: " + line);
		return 1;
	}
	location.path = line.substr(delimiter1, delimiter2 - delimiter1);
	config_data.status = LOCATION;
	return 0;
}

int Webserv::_parseConfigFile( const std::string& config_path ) {
	std::ifstream file(config_path);
	if (!file.is_open()) {
		logger.warning("Failed to open config file: ");
		return 1;
	}
	std::string line;
	ServerData server;
	Location location;
	ConfigData config_data;

	while (std::getline(file, line)) {
		if (line.find('#') != std::string::npos) {
			line = line.substr(0, line.find('#'));
        }
		if (line.find_first_not_of(" \t") == std::string::npos) {
			continue;
		}
		if (line.find("logging_level:") != std::string::npos) {
			if (_parseLoggingLevel(line) == 1) return 1;
		} else if (line.find("server:") != std::string::npos) {
			if (config_data.status != START && _addServer(server, config_data, location)) return 1;
			config_data.status = SERVER;
		} else if (line.find("location") != std::string::npos) {
			if (_parseLocationPathLine(line, server, location, config_data)) return 1;
		} else {
			if (_parseConfigLine(line, server, location, config_data)) return 1;
		}
	}
	if (_addServer(server, config_data, location)) return 1;
	_sortLocationByPath();
	if (logger.getLevel() == DEBUG) _printConfig();
	return 0;
}