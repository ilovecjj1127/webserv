#include "Webserv.hpp"

void Webserv::printRequest( const Request& request ) const {
	std::cout << "\033[32m" << "_______\nREQUEST" << std::endl;
	std::string method = "";
	for (const auto& it : methods) {
		if (request.method == it.second) {
			method = it.first;
			break;
		}
	}
	std::cout << "Method: " << method << std::endl;
	std::cout << "Path: '" << request.path << "'\n";
	std::cout << "\nParams: " << std::endl;
	for (const auto& it : request.params) {
		std::cout << it.first << " = " << it.second << std::endl;
	}
	std::cout << "\nHeaders: " << std::endl;
	for (const auto& it : request.headers) {
		std::cout << it.first << ": " << it.second << std::endl;
	}
	std::cout << "\nBody:\n" << request.body << std::endl;
	std::cout << "_______" << std::endl << "\033[0m";
}
