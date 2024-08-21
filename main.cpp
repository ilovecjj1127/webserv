#include "Webserv.hpp"

int main() {
	std::cout << "Start program" << std::endl;
	Webserv& server = Webserv::getInstance();
	return server.startServer();
}
