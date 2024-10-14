#include "Response.hpp"


const std::unordered_map<int, std::string> Response::_response_codes = {
	{200, "200 OK"},
	{403, "403 Forbidden"},
	{404, "404 Not Found"},
	{405, "405 Method Not Allowed"},
	{413, "413 Request Entity Too Large"},
	{500, "500 Internal Server Error"}
};

const std::unordered_map<int, std::string> Response::_error_pages = {
	{403, "./default_pages/403.html"},
	{404, "./default_pages/404.html"},
	{405, "./default_pages/405.html"},
	{413, "./default_pages/413.html"},
	{500, "./default_pages/500.html"},
	{0, "./default_pages/unknown.html"}
};

const std::unordered_map<std::string, std::string> Response::_mime_types = {
	{"html", "text/html"},
	{"css", "text/css"},
	{"js", "text/javascript"},
	{"jpg", "image/jpeg"},
	{"jpeg", "image/jpeg"},
	{"png", "image/png"},
	{"ico", "image/x-icon"},
	{"json", "application/json"},
	{"pdf", "application/pdf"},
	{"zip", "application/zip"},
	{"", "text/plain"}
};

Response::Response( void ) logger(Logger::getInstance()) {
	_location = nullptr;
}

Response::Response( const Response& other ) {
	*this = other;
}

Response& Response::operator = ( const Response& other ) logger(Logger::getInstance()) {
	if (this != &other) {
		full_response = other.full_response;
		_location = other._location;
	}
	return (*this);
}

int Response::prepareResponse( const std::string& file_path, size_t status_code ) {
	std::string& root_path = _location->root;
	std::string full_path = root_path + file_path;
	if (file_path == _location->path && !_location->index_page.empty()) {
		std::string index_page = "/" + _location->index_page;
		if (access((root_path + index_page).c_str(), F_OK) == 0) {
			return _prepareResponse(client_fd, index_page, 200);
		}
	}
	if (full_path.back() == '/' && isDirectory(full_path)) {
		if (_location->autoindex == 1) {
			_generateDirectoryList(full_path, client_fd);
		} else {
			_prepareResponseError(_clients_map[client_fd], 403);
		}
		return 1;
	}
	std::string extension = _getFileExtension(file_path);
	if (extension == "py") {
		logger.info("CGI file: " + full_path);
		if (access(full_path.c_str(), X_OK) == 0) {
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
	response = _getHtmlHeader(response.size(), status_code, extension) + response;
	return 1;
}