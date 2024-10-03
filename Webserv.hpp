#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <cstring>
#include <csignal>
#include <ctime>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <list>
#include <set>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "Logger.hpp"
#include "Request.hpp"


struct CgiData {
	pid_t	pid = 0;
	int		client_fd = 0;
	int		fd_in = 0;
	int		fd_out = 0;
};

struct ServerData {
	std::vector<std::pair<uint32_t, uint16_t>>	listen_group; // <ip_address, port> pairs
	std::vector<std::string>					server_names;
	std::string 								root_path;
	std::string 								index_page;
	std::string 								error_page_404;
};

struct ClientData {
	Request		request;
	std::string	response;
	int			response_code;
	size_t		bytes_sent_total = 0;
	size_t		bytes_write_total = 0;
	time_t		last_activity;
	CgiData		cgi;
	int			server_fd = 0;
	ServerData*	server = nullptr;
};

class Webserv {
private:
	Webserv( void );
	~Webserv( void );

	static Webserv _instance;

	bool _keep_running;
	int _epoll_fd;
	size_t _event_array_size;
	std::string _root_path;
	std::string _index_page;
	std::string _error_page_404;
	std::vector<ServerData> _servers;
	std::unordered_map<int, ClientData> _clients_map;
	std::unordered_map<int, int> _pipe_map;
	std::unordered_map<int, std::list<ServerData*>> _server_sockets_map;
	size_t _chunk_size;
	int _timeout_period;

	void _fakeConfigParser( void );
	void _get_target_server(int client_fd, const std::string& host);

	void _stopServer( void );
	int _initWebserv( void );
	int _initServer( ServerData& server, std::unordered_map<std::string, int>& listen_map);
	int _createServerSocket( uint32_t ip_address, uint16_t port );
	int _addServerToEpoll( const int server_fd );
	int _initError( const char* err_msg, int fd );
	int _setNonBlocking( int fd );
	void _mainLoop( void );
	void _checkTimeouts( void );
	void _handleEvent( epoll_event& event );
	void _closeClientFd( int client_fd, const char* err_msg );
	std::string _getHtmlHeader( size_t content_length, size_t status_code );
	void _handleConnection( const int server_fd );
	void _modifyEpollSocketOut( int client_fd );
	void _sendClientResponse( int client_fd );
	int _prepareResponse( int client_fd, const std::string& file_path, size_t status_code = 200 );
	int _getClientRequest( int client_fd );

	int _executeCgi( int client_fd, std::string& path );
	void _connectCgi( int client_fd, int fd_in, int fd_out);
	int _endCgi( int fd_res[2], int fd_body[2], int client_fd );
	char** _createEnvp( const Request& req, std::string& path );
	void _sendCgiRequest( int fd_out );
	void _getCgiResponse( int fd_in );
	void _closeCgiPipe( int pipe_fd, CgiData& cgi, const char* err_msg );

public:
	Webserv( const Webserv& ) = delete;
	Webserv& operator = ( const Webserv& ) = delete;

	Logger logger;

	static Webserv& getInstance( void );
	static void handleSigInt(int signum);

	int startServer( void );
};
