#include "Webserv.hpp"

Webserv Webserv::_instance;

const std::map<std::string, Method> Webserv::methods = {
	{"GET", GET},
	{"POST", POST},
	{"DELETE", DELETE}
};

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

int Webserv::_getClientRequest( int client_fd, Request& request ) {
	char buffer[4096];
	std::string r;
	while (true) {
		ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
		if (bytes < 0) {
			perror("Recv failed"); // Maybe not allowed
			close(client_fd);
			return 1;
		} else if (bytes > 0) {
			r.append(buffer, bytes);
		}
		if (bytes < (ssize_t)sizeof(buffer)) {
			break;
		}
	}
	return _parseRequest(r, request);
}

int Webserv::_parseRequest( const std::string& r, Request& request ) {
	std::istringstream request_stream(r);
	std::string line;
	std::getline(request_stream, line);
	if (_parseRequestLine(line, request) != 0) {
		return 1;
	} else if (request_stream.eof()) {
		return 0;
	}
	while(std::getline(request_stream, line) && line != "\r") {
		size_t delimiter = line.find(": ");
		if (delimiter == std::string::npos) {
			return 1;
		} else if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		std::string key = line.substr(0, delimiter);
		std::string value = line.substr(delimiter + 2);
		request.headers[key] = value;		
	}
	if (request_stream) {
		std::getline(request_stream, request.body, '\0');
	}
	if (request_stream.eof()) {
		return 0;
	}
	return 1;
}

int Webserv::_parseRequestLine( const std::string& line,
								Request& request ) {
	size_t space1 = line.find(" ");
	size_t space2 = line.rfind(" ");
	if (space1 == space2 || line.substr(space2) != " HTTP/1.1\r") {
		return 1;
	}
	auto it = methods.find(line.substr(0, space1));
	if (it != methods.end()) {
		request.method = it->second;
	} else {
		return 1;
	}
	request.path = line.substr(space1 + 1, space2); // Need to validate it
	return 0;
}

void Webserv::_stopServer( void ) {
	_keep_running = false;
	close(_server_fd);
}

void Webserv::handleSigInt(int signum) {
	(void)signum;
	Webserv::getInstance()._stopServer();
}
