#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <unordered_map>

#include "Location.hpp"
#include "Logger.hpp"

using map_int_str = std::unordered_map<int, std::string>;
using map_str_str = std::unordered_map<std::string, std::string>;

class Response {
private:
	static const map_int_str _response_codes;
	static const map_int_str _error_pages;
	static const map_str_str _mime_types;

	Location*	_location;
	Logger*		_logger;

	std::string _getErrorPagePath(const map_int_str& error_pages, size_t status_code);
	int _saveResponsePage( std::string& filepath, int status_code );
	std::string _getFileExtension( const std::string& filepath );
	std::string _getHtmlHeader( size_t content_length, size_t status_code, const std::string& extension );

public:
	Response( void );
	Response( const Response& other );
	~Response( void ) {};

	Response& operator = ( const Response& other );

	std::string	full_response;
	Logger&		logger;

	int prepareResponse( const std::string& file_path, size_t status_code );
	void prepareResponseError( size_t status_code );
};
