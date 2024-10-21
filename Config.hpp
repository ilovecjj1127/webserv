#pragma once

#include <unordered_map>
#include <string>

enum ParseStatus {
	START,
	SERVER,
	LOCATION,
};

struct ConfigData {
	std::string 								index_page;
	int											autoindex = 0;
	size_t										client_max_body_size = 1048576;
	std::unordered_map<int, std::string>		error_pages;
	int											server_indentation = 0;
	int											location_indentation = 0;
	ParseStatus									status = START;
};