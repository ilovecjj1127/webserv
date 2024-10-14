#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <cstring>
#include <csignal>
#include <ctime>
#include <cstddef>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <list>
#include <set>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <dirent.h>

#include "Logger.hpp"
#include "Request.hpp"


struct CgiData {
	pid_t	pid = 0;
	int		client_fd = 0;
	int		fd_in = 0;
	int		fd_out = 0;
};

struct Location {
	std::string								path;
	std::string								root;
	std::string								index_page;
	int										autoindex = -1;
	size_t									client_max_body_size = SIZE_MAX;
	std::set<Method>						allowed_methods = {GET, POST, DELETE};
	std::unordered_map<int, std::string>	error_pages;
	std::string								redirect_path = "";
	int										redirect_code = 0;
};

struct ServerData {
	std::vector<std::pair<uint32_t, uint16_t>>	listen_group; // <ip_address, port> pairs
	std::vector<std::string>					server_names;
	std::vector<Location>						locations;
};

struct ConfigServerData {
	std::string 								index_page;
	int											autoindex = 0;
	size_t										client_max_body_size = 1048576;
	std::unordered_map<int, std::string>		error_pages;
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
	Location*	location = nullptr;
};

class Webserv {
private:
	Webserv( void );
	~Webserv( void );

	static Webserv _instance;
	static const std::unordered_map<int, std::string> _response_codes;
	static const std::unordered_map<int, std::string> _error_pages;
	static const std::unordered_map<std::string, std::string> _mime_types;

	bool _keep_running;
	int _epoll_fd;
	size_t _event_array_size;
	std::vector<ServerData> _servers;
	std::unordered_map<int, ClientData> _clients_map;
	std::unordered_map<int, int> _pipe_map;
	std::unordered_map<int, std::list<ServerData*>> _server_sockets_map;
	size_t _chunk_size;
	int _timeout_period;

	// Webserv.cpp
	void _stopServer( void );
	void _mainLoop( void );
	void _checkTimeouts( void );
	std::string _getHtmlHeader( size_t content_length, size_t status_code, const std::string& extension );
	int _prepareResponse( int client_fd, const std::string& file_path, size_t status_code = 200 );
	void _generateDirectoryList( const std::string &dir_path, int client_fd );
	void _getTargetServer(int client_fd, const std::string& host);
	void _prepareResponseError( ClientData& client_data, size_t status_code );
	std::string _getErrorPagePath(const std::unordered_map<int, std::string>& error_pages, size_t status_code);
	int _saveResponsePage( ClientData& client_data, std::string& filepath, int status_code );
	std::string _getFileExtension( const std::string& filepath );

	// WebservConfig.cpp
	void _fakeConfigParser( void );
	void _sortLocationByPath( void );
	void _printConfig( void ) const;
	int _parseConfigFile( const std::string& config_path );
	int _parseServerData( ServerData& server, ConfigServerData& temp_var, const std::string& line );
	int _parseLocation( Location& location, const std::string& line );
	void _checkParamsPriority( ServerData& server, ConfigServerData& temp_var );
	uint32_t _ipStringToDecimal( const std::string& ip_address );

	// WebservCgi.cpp
	int _executeCgi( int client_fd, std::string& path );
	void _connectCgi( int client_fd, int fd_in, int fd_out);
	int _endCgi( int fd_res[2], int fd_body[2], int client_fd );
	void _createEnvs( const Request& req, std::vector<std::string>& env_strings );
	void _closeCgiPipe( int pipe_fd, CgiData& cgi, const char* err_msg );

	// WebservEvents.cpp
	void _handleEvent( epoll_event& event );
	void _handleConnection( const int server_fd );
	int _getClientRequest( int client_fd );
	void _sendClientResponse( int client_fd );
	void _getCgiResponse( int fd_in );
	void _sendCgiRequest( int fd_out );
	int _getTargetLocation( int client_fd );
	int _checkRequestValid( const Request& request, int client_fd );
	void _handleCgiResponse( ClientData& client_data );

	// WebservInit.cpp
	int _initWebserv( void );
	int _initServer( ServerData& server, std::unordered_map<std::string, int>& listen_map);
	int _createServerSocket( uint32_t ip_address, uint16_t port );
	int _addServerToEpoll( const int server_fd );
	int _initError( const char* err_msg, int fd );

	// WebservUtils.cpp
	int _setNonBlocking( int fd );
	void _closeClientFd( int client_fd, const char* err_msg );
	void _modifyEpollSocketOut( int client_fd );
	int _stringToInt( const std::string& str );

public:
	Webserv( const Webserv& ) = delete;
	Webserv& operator = ( const Webserv& ) = delete;

	Logger logger;

	static Webserv& getInstance( void );
	static void handleSigInt(int signum);

	int startServer( void );
};