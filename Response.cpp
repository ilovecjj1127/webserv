#include "Response.hpp"


const map_int_str Response::_response_codes = {
	{200, "200 OK"},
	{403, "403 Forbidden"},
	{404, "404 Not Found"},
	{405, "405 Method Not Allowed"},
	{413, "413 Request Entity Too Large"},
	{500, "500 Internal Server Error"}
};

const map_int_str Response::_error_pages = {
	{403, "./default_pages/403.html"},
	{404, "./default_pages/404.html"},
	{405, "./default_pages/405.html"},
	{413, "./default_pages/413.html"},
	{500, "./default_pages/500.html"},
	{0, "./default_pages/unknown.html"}
};

const map_str_str Response::_mime_types = {
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

Response::Response( void ) : logger(Logger::getInstance()) {
	location = nullptr;
}

Response::Response( const Response& other ) : logger(Logger::getInstance()) {
	*this = other;
}

Response& Response::operator = ( const Response& other ) {
	if (this != &other) {
		full_response = other.full_response;
		location = other.location;
	}
	return (*this);
}

int Response::prepareResponse( const std::string& file_path, size_t status_code ) {
	std::string& root_path = location->root;
	std::string full_path = root_path + file_path;
	if (file_path == location->path && !location->index_page.empty()) {
		std::string index_page = "/" + location->index_page;
		if (access((root_path + index_page).c_str(), F_OK) == 0) {
			return prepareResponse(index_page, 200);
		}
	} else if (_checkIfDirectory(file_path, full_path)) {
		return 1;
	}
	std::string extension = _getFileExtension(file_path);
	if (extension == "py") {
		return _checkCgiAccess(full_path);
	}
	_prepareStaticFile(full_path, extension, status_code);
	return 1;
}

bool Response::_checkIfDirectory( const std::string& file_path, const std::string& full_path ) {
	if (full_path.back() == '/' && _isDirectory(full_path)) {
		if (location->autoindex == 1) {
			_generateDirectoryList(full_path, file_path);
		} else {
			prepareResponseError(403);
		}
		return true;
	} else {
		return false;
	}
}

int Response::_checkCgiAccess( const std::string& full_path ) {
	logger.info("CGI file: " + full_path);
	if (access(full_path.c_str(), X_OK) == 0) {
		return 0;
	} else {
		prepareResponseError(404);
		return 1;
	}
}

void Response::_prepareStaticFile(const std::string& path, const std::string& extension,
								  size_t status_code ) {
	std::ifstream file(path);
	if (_isDirectory(path) || !file.is_open()) {
		logger.warning("Failed to open file: " + path);
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

std::string Response::_getFileExtension( const std::string& filepath ) {
	size_t pos = filepath.rfind('.');
	if (pos == std::string::npos) {
		return "";
	}
	return filepath.substr(pos + 1);
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

//#include <sys/stat.h>
int Response::_isDirectory( const std::string& full_path ) {
	struct stat path_stat;
	if (stat(full_path.c_str(), &path_stat) == 0) {
		return (S_ISDIR(path_stat.st_mode));
	}
	return 0;
}

void Response::_generateDirectoryList( const std::string& dir_path,
									   const std::string& file_path ) {
	std::ostringstream html;
	html << "<html><body><h1>Index of " << file_path << "</h1>\n<hr><pre><table>\n";
	DIR *dir = opendir(dir_path.c_str());
	if (dir == nullptr) {
		prepareResponseError(403);
		logger.warning("Failed to open directory" + dir_path);
		return;
	}
	struct dirent *entry;
	while ((entry = readdir(dir)) != nullptr) {
		html << _getEntryLine(entry, dir_path);
	}
	closedir(dir);
	html << "</table>\n</pre><hr></body>\n</html>\n";
	full_response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
	full_response += std::to_string(html.str().size())+ "\r\n\r\n" + html.str();
}

// std::string Response::_getModTime( const std::string& path ) {
// 	struct stat file_stat;
// 	if (stat(path.c_str(), &file_stat) == 0) {
// 		struct tm *tm = localtime(&file_stat.st_mtime);
// 		char timebuf[80];
// 		strftime(timebuf, sizeof(timebuf), "%d-%b-%Y %H:%M", tm);
// 		return std::string(timebuf);
// 	}
// 	return "Unknown";
// }

std::string Response::_getEntryLine( struct dirent* entry, const std::string& dir_path ) {
	std::string name, full_path, size, mod_time;
	name = entry->d_name;
	full_path = dir_path + name;
	if (name == ".") {
		return "";
	}
	if (name == "..") {
		name += "/";
	} else {
		_getEntryStats(full_path, size, mod_time);
		if (entry->d_type == DT_DIR) {
			name += "/";
			size = "-";
		}
	}
	return std::string("<tr><td style=\"width:70%\"><a href=\"") + name + "\">" + 
		   name + "</a></td>" + "<td style=\"width:20%\">" + mod_time + "</td>" +
		   "<td align=\"right\">" + size + "</td></tr>\n";

}

void Response::_getEntryStats( const std::string& path, std::string& size, std::string& mod_time ) {
	struct stat entry_stat;
	if (stat(path.c_str(), &entry_stat) == -1) {
		size = "Unknown";
		mod_time = "Unknown";
		return;
	}
	struct tm *tm = localtime(&entry_stat.st_mtime);
	char timebuf[80];
	strftime(timebuf, sizeof(timebuf), "%d-%b-%Y %H:%M", tm);
	mod_time = std::string(timebuf);
	size = std::to_string(entry_stat.st_size);
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

int Response::_stringToInt( const std::string& str ) {
	try {
		int status_code = std::stoi(str);
		return status_code;
	} catch (const std::invalid_argument&) {
		return -1;
	}
}
