#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <csignal>
#include <fcntl.h>
#include <map>
#include <unordered_set>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>

using str_map = std::map<std::string, std::string>;

enum Method {
	GET,
	POST,
	DELETE
};

struct Request {
	Method		method;
	std::string	path;
	str_map		params;
	str_map		headers;
	std::string	body;
};

class Webserv {
private:
	Webserv( void );
	~Webserv( void );

	static Webserv _instance;

	bool _keep_running;
	int _server_fd;
	int _epoll_fd;
	size_t _event_array_size;
	uint16_t _listen_port;
	std::string _root_path;
	std::string _index_page;
	std::string _error_page_404;
	std::unordered_set<int> _open_clients_fds;

	void _stopServer( void );
	int _initServer( void );
	int _initError( const char* err_msg );
	int _setNonBlocking( int fd );
	void _mainLoop( void );
	void _closeClientFd( int client_fd, const char* err_msg );
	void _sendHtml( int client_fd, const std::string& file_path, size_t status_code = 200 );
	std::string _getHtmlHeader( size_t content_length, size_t status_code );
	int _getClientRequest( int client_fd, Request& request );
	int _parseRequest( const std::string& r, Request& request );
	int _parseRequestLine( const std::string& line, Request& request );
	int _parseRequestTarget( const std::string& line, Request& request );

public:
	Webserv( const Webserv& ) = delete;
	Webserv& operator = ( const Webserv& ) = delete;

	static const std::map<std::string, Method> methods;

	static Webserv& getInstance( void );
	static void handleSigInt(int signum);

	int startServer( void );

	// Debug functions:
	void printRequest( const Request& request ) const; 
};

#endif