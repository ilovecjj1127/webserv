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

	// Response.cpp
	int _checkCgiAccess( void );
	void _prepareStaticFile( const std::string& extension, size_t status_code );
	std::string _getErrorPagePath(const map_int_str& error_pages, size_t status_code);
	int _saveResponsePage( std::string& filepath, int status_code );
	std::string _getHtmlHeader( size_t content_length, size_t status_code,
								const std::string& extension );

	// ResponseDirectory.cpp
	bool _checkIfDirectory( const std::string& file_path );
	int _isDirectory( const std::string& full_path );
	void _generateDirectoryList( const std::string& file_path );
	std::string _getEntryLine( struct dirent* entry );
	void _getEntryStats( const std::string& path, std::string& size, std::string& mod_time );

	// ResponseUtils.cpp
	std::string _build_path( const std::string& first, const std::string& second );
	std::string _getFileExtension( const std::string& filepath );
	int _stringToInt( const std::string& str );


public:
	Response( void );
	Response( const Response& other );
	~Response( void ) {};

	Response& operator = ( const Response& other );

	std::string	full_response;
	std::string local_path;
	Location*	location;
	Logger&		logger;

	// Response.cpp
	int prepareResponse( const std::string& file_path, size_t status_code = 200 );
	void prepareResponseError( size_t status_code );
	void handleCgiResponse( void );
};
