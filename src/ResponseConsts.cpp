#include "Response.hpp"

const map_int_str Response::_response_codes = {
	{200, "200 OK"},
	{400, "400 Bad Request"},
	{403, "403 Forbidden"},
	{404, "404 Not Found"},
	{405, "405 Method Not Allowed"},
	{413, "413 Request Entity Too Large"},
	{500, "500 Internal Server Error"}
};

const map_int_str Response::_error_pages = {
	{400, "./default_pages/400.html"},
	{403, "./default_pages/403.html"},
	{404, "./default_pages/404.html"},
	{405, "./default_pages/405.html"},
	{413, "./default_pages/413.html"},
	{500, "./default_pages/500.html"},
	{0, "./default_pages/unknown.html"}
};

const map_str_str Response::_mime_types = {
	{"html", "text/html"},
	{"css", "text/css"},
	{"js", "text/javascript"},
	{"jpg", "image/jpeg"},
	{"jpeg", "image/jpeg"},
	{"png", "image/png"},
	{"ico", "image/x-icon"},
	{"json", "application/json"},
	{"pdf", "application/pdf"},
	{"zip", "application/zip"},
	{"", "text/plain"}
};
