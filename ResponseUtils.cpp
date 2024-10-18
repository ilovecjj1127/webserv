#include "Response.hpp"

std::string Response::_getFileExtension( const std::string& filepath ) {
	size_t pos = filepath.rfind('.');
	if (pos == std::string::npos) {
		return "";
	}
	return filepath.substr(pos + 1);
}

int Response::_stringToInt( const std::string& str ) {
	try {
		int status_code = std::stoi(str);
		return status_code;
	} catch (const std::invalid_argument&) {
		return -1;
	}
}
