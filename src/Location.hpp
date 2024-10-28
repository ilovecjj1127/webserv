#pragma once

#include <set>
#include <string>
#include <unordered_map>

#include "Request.hpp"


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
