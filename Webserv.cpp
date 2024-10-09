#include "Webserv.hpp"

Webserv Webserv::_instance;

const std::unordered_map<int, std::string> Webserv::_response_codes = {
	{200, "200 OK"},
	{403, "403 Forbidden"},
	{404, "404 Not Found"},
	{405, "405 Method Not Allowed"},
	{413, "413 Request Entity Too Large"},
	{500, "500 Internal Server Error"}
};

const std::unordered_map<int, std::string> Webserv::_error_pages = {
	{403, "./default_pages/403.html"},
	{404, "./default_pages/404.html"},
	{405, "./default_pages/405.html"},
	{413, "./default_pages/413.html"},
	{500, "./default_pages/500.html"},
	{0, "./default_pages/unknown.html"}
};

Webserv::Webserv( void ) {
	logger.setLevel(DEBUG);
	logger.debug("Webserv instance created");
	_keep_running = true;
	_epoll_fd = -1;
	_event_array_size = 16;
	_chunk_size = 4096;
	_timeout_period = 5;
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
	_fakeConfigParser();
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
					return a.path.size() > b.path.size();  // Sort by path length (longest first)
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
			if (!location.root.empty()) std::cout << "\talias: " << location.root << std::endl;
			if (!location.index_page.empty()) std::cout << "\tindex: " << location.index_page << std::endl;
			if (!location.redirect_path.empty()) std::cout << "\tredirect_path: " << location.redirect_path << std::endl;
			std::cout << "\tautoindex: " << location.autoindex << std::endl;
		}
	}
	std::cout << "_______" << "\033[0m" << std::endl;
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
				_prepareResponseError(client_data, 500);
				_modifyEpollSocketOut(client_fd);
				return _closeCgiPipe(client_data.cgi.fd_in, client_data.cgi, nullptr);
			}
		}
	}
}

//#include <sys/stat.h>
int isDirectory( const std::string& full_path ) {
	struct stat path_stat;
	if (stat(full_path.c_str(), &path_stat) == 0) {
		return (S_ISDIR(path_stat.st_mode));
	}
	return 0;
}

std::string getModTime( const std::string& path ) {
	struct stat file_stat;
	if (stat(path.c_str(), &file_stat) == 0) {
		struct tm *tm = localtime(&file_stat.st_mtime);
		char timebuf[80];
		strftime(timebuf, sizeof(timebuf), "%d-%b-%Y %H:%M", tm);
		return std::string(timebuf);
	}
	return "Unknown";
}

void Webserv::_generateDirectoryList( const std::string &dir_path, int client_fd ) {
	std::ostringstream html;
	std::string name, full_path, size, mod_time;
	std::string& response = _clients_map[client_fd].response;
	std::string& file_path = _clients_map[client_fd].request.path;

	html << "<html><body><h1>Index of " << file_path << "</h1>\n<hr><pre><table>\n";

	DIR *dir = opendir(dir_path.c_str());
	if (dir == nullptr) {
		_prepareResponseError(_clients_map[client_fd], 403);
		logger.warning("Failed to open directory" + dir_path);
		return;
	}
	struct dirent *entry;
	while ((entry = readdir(dir)) != nullptr) {
		name = entry->d_name;
		full_path = dir_path + name;
		if (name == ".") continue;
		if (name == "..") {
			name += "/";
		} else if (entry->d_type == DT_DIR) {
			name += "/";
			size = "-";
			mod_time = getModTime(full_path);
		} else {
			struct stat st;
			stat(full_path.c_str(), &st);
			size = std::to_string(st.st_size);
			mod_time = getModTime(full_path);
		}
		html << "<tr><td style=\"width:70%\"><a href=\"" << name << "\">" << name << "</a></td>"
			 << "<td style=\"width:20%\">" << mod_time << "</td>"
			 << "<td align=\"right\">" << size << "</td></tr>\n";
	}
	closedir(dir);
	html << "</table>\n</pre><hr></body>\n</html>\n";
	response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
	response += std::to_string(html.str().size())+ "\r\n\r\n" + html.str();
}


// ngnix: "index /index.html" works same as "index index.html" 
// ngnix: all error response 404 error page. if not found error page, response 403 forbidden?
int Webserv::_prepareResponse( int client_fd, const std::string& file_path, size_t status_code ) {
	std::string& response = _clients_map[client_fd].response;
	std::string& root_path = _clients_map[client_fd].location->root;
	std::string full_path = root_path + file_path;
	if (file_path == _clients_map[client_fd].location->path && !_clients_map[client_fd].location->index_page.empty()) {
		std::string index_page = "/" + _clients_map[client_fd].location->index_page;
		if (access((root_path + index_page).c_str(), F_OK) == 0) {
			return _prepareResponse(client_fd, index_page, 200);
		}
	}
	if (full_path.back() == '/' && isDirectory(full_path)) {
		if (_clients_map[client_fd].location->autoindex) {
			_generateDirectoryList(full_path, client_fd);
		} else {
			_prepareResponseError(_clients_map[client_fd], 403);
		}
		return 1;
	}
	if (full_path.substr(full_path.size() - 3) == ".py") {
		logger.info("CGI file: " + full_path);
		if (access(full_path.c_str(), F_OK) == 0) {
		 	return _executeCgi(client_fd, full_path);
		} else {
			_prepareResponseError(_clients_map[client_fd], 404);
			return 1;
		}
	}
	std::ifstream file(full_path);
	if (isDirectory(full_path) || !file.is_open()) {
		logger.warning("Failed to open file: " + full_path);
		_prepareResponseError(_clients_map[client_fd], 404);
		return 1;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	response = buffer.str();
	response = _getHtmlHeader(response.size(), status_code) + response;
	return 1;
}

void Webserv::_prepareResponseError( ClientData& client_data, size_t status_code ) {
	std::string filepath = "";
	if (client_data.location != nullptr) {
		filepath = _getErrorPagePath(client_data.location->error_pages, status_code);
	}
	if (!filepath.empty() && _saveResponsePage(client_data, filepath, status_code) == 0) {
		return;
	}
	filepath = _getErrorPagePath(_error_pages, status_code);
	if (filepath.empty()) {
		filepath = _error_pages.at(0);
	}
	_saveResponsePage(client_data, filepath, status_code);
}

std::string Webserv::_getErrorPagePath(const std::unordered_map<int, std::string>& error_pages, size_t status_code) {
	if (error_pages.find(status_code) == error_pages.end()) {
		return "";
	}
	const std::string& filepath = error_pages.at(status_code);
	if (access(filepath.c_str(), R_OK) == 0 && isDirectory(filepath) == 0) {
		return filepath;
	}
	return "";
}

int Webserv::_saveResponsePage( ClientData& client_data, std::string& filepath, int status_code ) {
	std::ifstream file(filepath);
	if (!file.is_open()) {
		return 1;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string& response = client_data.response;
	response = buffer.str();
	response = _getHtmlHeader(response.size(), status_code) + response;
	return 0;
}

std::string Webserv::_getHtmlHeader( size_t content_length, size_t status_code ) {
	std::string header = "HTTP/1.1 ";
	if (_response_codes.find(status_code) != _response_codes.end()) {
		header += _response_codes.at(status_code) + "\r\n";
	} else {
		header += std::to_string(status_code) + "\r\n";
	}
	header += "Content-Type: text/html\r\n";
	header += "Content-Length: " + std::to_string(content_length) + "\r\n\r\n";
	return header;
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
