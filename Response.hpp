#pragma once

#include <string>
#include <unistd.h>
#include <unordered_map>

#include "Location.hpp"
#include "Logger.hpp"


class Response {
private:
	static const std::unordered_map<int, std::string> _response_codes;
	static const std::unordered_map<int, std::string> _error_pages;
	static const std::unordered_map<std::string, std::string> _mime_types;

	Location*	_location;
	Logger*		_logger;

public:
	Response( void );
	Response( const Response& other );
	~Response( void ) {};

	Response& operator = ( const Response& other );

	std::string	full_response;
	Logger&		logger;

	int prepareResponse( const std::string& file_path, size_t status_code );

};
