#include <iostream>
#include <string.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
	int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_fd == -1) {
		std::cerr << "Failed to create socket" << std::endl;
		return 1;
	}

	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(8081);
	if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		std::cerr << "Failed to bind socket" << std::endl;
		close(server_fd);
		return 1;
	}

	if (listen(server_fd, 10) == -1) {
		std::cerr << "Failed to listen on socket" << std::endl;
		close(server_fd);
		return 1;
	}
	std::cout << "Server is listening" << std::endl;

	sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
	if (client_fd == -1) {
		std::cerr << "Failed to accept connection" << std::endl;
		close(server_fd);
		return 1;
	}
	std::cout << "Accepted a connection" << std::endl;

	const char* response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello, World!";
	send(client_fd, response, strlen(response), 0);

	close(client_fd);
	close(server_fd);
}