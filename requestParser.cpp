#include "Webserv.hpp"

const std::unordered_map<std::string, Method> Webserv::methods = {
	{"GET", GET},
	{"POST", POST},
	{"DELETE", DELETE}
};

int Webserv::_getClientRequest( int client_fd ) {
	char buffer[4096];
	Request& request = _clients_map[client_fd];
	ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
	if (bytes < 0) {
		_closeClientFd(client_fd, "Recv failed");
		return 1;
	} else {
		request.request_str.append(buffer, bytes);
	}
	if (_parseRequest(request) == 0) {
		return 0;
	}
	_closeClientFd(client_fd, nullptr);
	return 1;
}

int Webserv::_parseRequest( Request& request ) {
	std::istringstream request_stream(request.request_str);
	std::string line;
	std::getline(request_stream, line);
	if (_parseRequestLine(line, request) != 0) {
		return 1;
	} else if (request_stream.eof()) {
		return 0;
	}
	while(std::getline(request_stream, line) && line != "\r") {
		size_t delimiter = line.find(": ");
		if (delimiter == std::string::npos) {
			return 1;
		} else if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		std::string key = line.substr(0, delimiter);
		std::string value = line.substr(delimiter + 2);
		request.headers[key] = value;		
	}
	if (request_stream) {
		std::getline(request_stream, request.body, '\0');
	}
	if (request_stream.eof()) {
		return 0;
	}
	return 1;
}

int Webserv::_parseRequestLine( const std::string& line,
								Request& request ) {
	size_t space1 = line.find(" ");
	size_t space2 = line.rfind(" ");
	if (space1 == space2 || line.substr(space2) != " HTTP/1.1\r") {
		return 1;
	}
	auto it = methods.find(line.substr(0, space1));
	if (it != methods.end()) {
		request.method = it->second;
	} else {
		return 1;
	}
	std::string target = line.substr(space1 + 1, space2 - space1 - 1);
	return _parseRequestTarget(target, request);
}

int	Webserv::_parseRequestTarget( const std::string& line, Request& request ) {
	if (line.empty() || line[0] != '/') {
		return 1;
	}
	size_t	delimiter = line.find("?");
	request.path = line.substr(0, delimiter);
	if (delimiter == std::string::npos) {
		return 0;
	}
	std::istringstream	query_stream(line.substr(delimiter + 1));
	std::string	param_str;
	while (std::getline(query_stream, param_str, '&')) {
		std::string::size_type equal_pos = param_str.find('=');
		if (equal_pos != std::string::npos) {
			std::string key = param_str.substr(0, equal_pos);
			std::string value = param_str.substr(equal_pos + 1);
			request.params[key] = value;
		} else {
			request.params[param_str] = "";
		}
	}
	if (query_stream.eof()) {
		return 0;
	}
	return 1;
}
