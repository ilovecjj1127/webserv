#include "Webserv.hpp"

Webserv Webserv::_instance;

Webserv::Webserv( void ) {
	std::cout << "Webserv instance created" << std::endl;
	_keep_running = true;
	_server_fd = -1;
	_listen_port = 8081;
}

Webserv::~Webserv( void ) {
	std::cout << "Webserv instance destroyed" << std::endl;
}

Webserv& Webserv::getInstance( void ) {
	return _instance;
}

int Webserv::startServer( void ) {
	if (_initServer() == 1) {
		return 1;
	}
	_mainLoop();
	return 0;
}

int Webserv::_initServer( void ) {
	_server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_server_fd == -1) {
		std::cerr << "Failed to create socket" << std::endl;
		return 1;
	}
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(_listen_port);
	if (bind(_server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		std::cerr << "Failed to bind socket" << std::endl;
		close(_server_fd);
		return 1;
	}
	if (listen(_server_fd, 10) == -1) {
		std::cerr << "Failed to listen on socket" << std::endl;
		close(_server_fd);
		return 1;
	}
	std::cout << "Server is listening" << std::endl;
	signal(SIGINT, handleSigInt);
	return 0;
}

void Webserv::_mainLoop( void ) {
	while (true) {
		sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client_fd = accept(_server_fd, (sockaddr*)&client_addr, &client_len);
		if (client_fd == -1) {
			if (!_keep_running) {
				std::cout << "\nInterrupted by signal" << std::endl;
				break;
			}
			std::cerr << "Failed to accept connection" << std::endl;
			continue;
		}
		std::cout << "Accepted a connection" << std::endl;
		Request request;
		if (_getClientRequest(client_fd, request) != 0) {
			continue;
		}
		printRequest(request);
		_sendHtml(client_fd, "./nginx_example/html/index.html");
		close(client_fd);
		std::cout << "Connection was closed " << std::endl;
	}
}

void Webserv::_sendHtml(int client_fd, const std::string& file_path) {
	std::ifstream file(file_path);
	if (!file.is_open()) {
		std::cerr << "Failed to open file: " << file_path << std::endl;
		std::string page404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
		send(client_fd, page404.c_str(), page404.size(), 0);
		return;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string html_content = buffer.str();
	std::string header = _getHtmlHeader(html_content.size());
	send(client_fd, header.c_str(), header.size(), 0);
	send(client_fd, html_content.c_str(), html_content.size(), 0);
	std::cout << "Sent HTML file: " << file_path << std::endl;
}

std::string Webserv::_getHtmlHeader( size_t content_length ) {
	std::string header = "HTTP/1.1 200 OK\r\n";
	header += "Content-Type: text/html\r\n";
	header += "Content-Length: " + std::to_string(content_length) + "\r\n\r\n";
	return header;
}

void Webserv::_stopServer( void ) {
	_keep_running = false;
	close(_server_fd);
}

void Webserv::handleSigInt(int signum) {
	(void)signum;
	Webserv::getInstance()._stopServer();
}
