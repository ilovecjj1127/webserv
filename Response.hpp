#pragma once

#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "Location.hpp"
#include "Logger.hpp"

using map_int_str = std::unordered_map<int, std::string>;
using map_str_str = std::unordered_map<std::string, std::string>;

class Response {
private:
	static const map_int_str _response_codes;
	static const map_int_str _error_pages;
	static const map_str_str _mime_types;

	bool _checkIfDirectory( const std::string& file_path, const std::string& full_path );
	int _checkCgiAccess( const std::string& full_path );
	void _prepareStaticFile(const std::string& path, const std::string& extension, size_t status_code );
	std::string _getErrorPagePath(const map_int_str& error_pages, size_t status_code);
	int _saveResponsePage( std::string& filepath, int status_code );
	std::string _getFileExtension( const std::string& filepath );
	std::string _getHtmlHeader( size_t content_length, size_t status_code, const std::string& extension );
	int _isDirectory( const std::string& full_path );
	void _generateDirectoryList( const std::string& dir_path, const std::string& file_path );
	std::string _getModTime( const std::string& path );
	int _stringToInt( const std::string& str );


public:
	Response( void );
	Response( const Response& other );
	~Response( void ) {};

	Response& operator = ( const Response& other );

	std::string	full_response;
	Location*	location;
	Logger&		logger;

	int prepareResponse( const std::string& file_path, size_t status_code = 200 );
	void prepareResponseError( size_t status_code );
	void handleCgiResponse( void );
};
