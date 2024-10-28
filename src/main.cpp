#include "Webserv.hpp"

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
		return 1;
	}
	std::cout << "Start program" << std::endl;
	Webserv& server = Webserv::getInstance();
	return server.startServer(argv[1]);
}
