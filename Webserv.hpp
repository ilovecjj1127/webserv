#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <csignal>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

class Webserv {
private:
	Webserv( void );
	~Webserv( void );

	static Webserv _instance;

	bool _keep_running;
	int _server_fd;
	uint16_t _listen_port;

	void _stopServer( void );
	int _initServer( void );
	void _mainLoop( void );
	void _sendHtml( int client_fd, const std::string& file_path );
	std::string _getHtmlHeader( size_t content_length );

public:
	Webserv( const Webserv& ) = delete;
	Webserv& operator = ( const Webserv& ) = delete;

	static Webserv& getInstance( void );
	static void handleSigInt(int signum);

	int startServer( void );
};

#endif