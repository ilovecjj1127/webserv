#include "Response.hpp"

Response::Response( void ) : logger(Logger::getInstance()) {
	location = nullptr;
}

Response::Response( const Response& other ) : logger(Logger::getInstance()) {
	*this = other;
}

Response& Response::operator = ( const Response& other ) {
	if (this != &other) {
		full_response = other.full_response;
		local_path = other.local_path;
		location = other.location;
	}
	return (*this);
}

int Response::prepareResponse( const std::string& request_path, size_t status_code ) {
	std::string root_path = location->root;
	std::string file_path = request_path.substr(location->path.size());
	local_path = _build_path(root_path, file_path);
	if (file_path == location->path && !location->index_page.empty()) {
		std::string index_page = "/" + location->index_page;
		if (access((root_path + index_page).c_str(), F_OK) == 0) {
			return prepareResponse(index_page, 200);
		}
	} else if (_checkIfDirectory(file_path)) {
		return 1;
	}
	std::string extension = _getFileExtension(file_path);
	if (extension == "py" || extension == "php") {
		return _checkCgiAccess();
	}
	_prepareStaticFile(extension, status_code);
	return 1;
}

int Response::_checkCgiAccess( void ) {
	logger.debug("CGI file: " + local_path);
	if (access(local_path.c_str(), X_OK) == 0) {
		return 0;
	} else {
		prepareResponseError(404);
		return 1;
	}
}

void Response::_prepareStaticFile(const std::string& extension, size_t status_code ) {
	std::ifstream file(local_path);
	if (_isDirectory(local_path) || !file.is_open()) {
		logger.warning("Failed to open file: " + local_path);
		prepareResponseError(404);
		return;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string body = buffer.str();
	std::string header = _getHtmlHeader(body.size(), status_code, extension);
	full_response = header + body;
}

void Response::prepareResponseError( size_t status_code ) {
	std::string filepath = "";
	if (location != nullptr) {
		filepath = _getErrorPagePath(location->error_pages, status_code);
	}
	if (!filepath.empty() && _saveResponsePage(filepath, status_code) == 0) {
		return;
	}
	filepath = _getErrorPagePath(_error_pages, status_code);
	if (filepath.empty()) {
		filepath = _error_pages.at(0);
	}
	_saveResponsePage(filepath, status_code);
}

std::string Response::_getErrorPagePath(const map_int_str& error_pages, size_t status_code) {
	if (error_pages.find(status_code) == error_pages.end()) {
		return "";
	}
	const std::string& filepath = error_pages.at(status_code);
	if (access(filepath.c_str(), R_OK) == 0 && _isDirectory(filepath) == 0) {
		return filepath;
	}
	return "";
}

int Response::_saveResponsePage( std::string& filepath, int status_code ) {
	std::ifstream file(filepath);
	if (!file.is_open()) {
		return 1;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	full_response = buffer.str();
	std::string extension = _getFileExtension(filepath);
	full_response = _getHtmlHeader(full_response.size(), status_code, extension) + full_response;
	return 0;
}

std::string Response::_getHtmlHeader( size_t content_length, size_t status_code,
									  const std::string& extension ) {
	std::string header = "HTTP/1.1 ";
	if (_response_codes.find(status_code) != _response_codes.end()) {
		header += _response_codes.at(status_code) + "\r\n";
	} else {
		header += std::to_string(status_code) + "\r\n";
	}
	std::string content_type;
	if (_mime_types.find(extension) == _mime_types.end()) {
		content_type = _mime_types.at("");
	} else {
		content_type = _mime_types.at(extension);
	}
	header += "Content-Type: " + content_type + "\r\n";
	header += "Content-Length: " + std::to_string(content_length) + "\r\n\r\n";
	return header;
}

void Response::handleCgiResponse( void ) {
	if (full_response.size() < 8 || full_response.compare(0, 8, "Status: ") != 0) {
		full_response.insert(0, "HTTP/1.1 200 OK\r\n");
		return;
	}
	std::string status_str = full_response.substr(8, 4);
	int status_code = _stringToInt(status_str);
	if (status_code < 100 || status_code > 599 || status_str.back() != ' ') {
		prepareResponseError(500);
	} else if (location != nullptr
		&& location->error_pages.find(status_code) != location->error_pages.end()) {
		prepareResponseError(status_code);
	} else {
		full_response.replace(0, 7, "HTTP/1.1");
	}
}
