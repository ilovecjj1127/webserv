#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>

using str_map = std::unordered_map<std::string, std::string>;

enum Method {
	GET,
	POST,
	DELETE
};

class Request {
private:
	int _parseRequestLine( const std::string& line );
	int _parseTarget( const std::string& line );

public:
	Request( void ) {};
	Request( const Request& other );
	~Request( void ) {};

	Request& operator = ( const Request& other );

	static const std::unordered_map<std::string, Method> methods;

	std::string raw;
	Method		method;
	std::string	path;
	str_map		params;
	str_map		headers;
	std::string	body;

	int parseRequest( void );

	// Debug
	void printRequest( void ) const;
};
