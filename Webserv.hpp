#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <cstring>
#include <csignal>
#include <fcntl.h>
#include <unordered_map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

#include "Logger.hpp"
#include "Request.hpp"

struct ClientData {
	Request		request;
	std::string	response;
	int			response_code;
	size_t		bytes_sent_total;
	size_t		bytes_write_total;
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
	std::unordered_map<int, ClientData> _clients_map;
	std::unordered_map<int, int> _pipe_map;
	size_t _chunk_size;

	void _stopServer( void );
	int _initServer( void );
	int _initError( const char* err_msg );
	int _setNonBlocking( int fd );
	void _mainLoop( void );
	void _closeClientFd( int client_fd, const char* err_msg );
	std::string _getHtmlHeader( size_t content_length, size_t status_code );
	void _handleConnection( void );
	void _modifyEpollSocketOut( int client_fd );
	void _sendResponse( int client_fd );
	int _prepareResponse( int client_fd, const std::string& file_path, size_t status_code = 200 );
	int _getClientRequest( int client_fd );

	void _executeCgi( int client_fd, std::string& path );
	char** _createEnvp( const Request& req, std::string& path );
	void _handlePipes( epoll_event& event );

public:
	Webserv( const Webserv& ) = delete;
	Webserv& operator = ( const Webserv& ) = delete;

	Logger logger;

	static Webserv& getInstance( void );
	static void handleSigInt(int signum);

	int startServer( void );
};
