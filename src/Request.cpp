#include "Request.hpp"

const std::unordered_map<std::string, Method> Request::methods = {
	{"GET", GET},
	{"POST", POST},
	{"DELETE", DELETE}
};

Request::Request( void ) {
	method = UNDEFINED;
	status = NEW;
}

Request::Request( const Request& other ) {
	*this = other;
}

Request& Request::operator = ( const Request& other ) {
	if (this != &other) {
		raw = other.raw;
		method = other.method;
		path = other.path;
		params = other.params;
		headers = other.headers;;
		body = other.body;
		status = other.status;
		content_length = other.content_length;
	}
	return (*this);
}

RqStatus Request::parseRequest( void ) {
	std::istringstream request_stream(raw);
	std::string line;
	std::getline(request_stream, line);
	if (_parseRequestLine(line) != 0) {
		return INVALID;
	}
	while(std::getline(request_stream, line) && line != "\r") {
		size_t delimiter = line.find(": ");
		if (delimiter == std::string::npos) {
			return INVALID;
		} else if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		std::string key = line.substr(0, delimiter);
		std::string value = line.substr(delimiter + 2);
		headers[key] = value;	
	}
	if (request_stream) {
		std::getline(request_stream, raw, '\0');
	}
	try {
		content_length = std::stoull(headers["Content-Length"]);
	} catch (...) {
		content_length = 0;
	}
	if (request_stream.eof()) {
		return getRequestBody();
	}
	return INVALID;
}

RqStatus Request::getRequestBody( void ) {
	if (raw.size() == content_length) {
		body = raw;
		return FULL_BODY;
	} else if (raw.size() < content_length) {
		return FULL_HEADER;
	}
	return INVALID;
}

int Request::_parseRequestLine( const std::string& line ) {
	size_t space1 = line.find(" ");
	size_t space2 = line.rfind(" ");
	if (space1 == space2 || line.substr(space2) != " HTTP/1.1\r") {
		return 1;
	}
	auto it = methods.find(line.substr(0, space1));
	if (it != methods.end()) {
		method = it->second;
	} else {
		return 1;
	}
	std::string target = line.substr(space1 + 1, space2 - space1 - 1);
	return _parseTarget(target);
}

int	Request::_parseTarget( const std::string& line ) {
	if (line.empty() || line[0] != '/') {
		return 1;
	}
	size_t	delimiter = line.find("?");
	path = line.substr(0, delimiter);
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
			params[key] = value;
		} else {
			params[param_str] = "";
		}
	}
	if (query_stream.eof()) {
		return 0;
	}
	return 1;
}

void Request::printRequest( void ) const {
	std::cout << "\033[32m" << "_______\nREQUEST" << std::endl;
	std::string method_str = "";
	for (const auto& it : methods) {
		if (method == it.second) {
			method_str = it.first;
			break;
		}
	}
	std::cout << "Method: " << method_str << std::endl;
	std::cout << "Path: '" << path << "'\n";
	std::cout << "\nParams: " << std::endl;
	for (const auto& it : params) {
		std::cout << it.first << " = " << it.second << std::endl;
	}
	std::cout << "\nHeaders: " << std::endl;
	for (const auto& it : headers) {
		std::cout << it.first << ": " << it.second << std::endl;
	}
	std::cout << "\nBody:\n" << body << std::endl;
	std::cout << "_______" << std::endl << "\033[0m";
}
