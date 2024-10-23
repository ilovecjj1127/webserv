#include "Response.hpp"

bool Response::_checkIfDirectory( const std::string& file_path ) {
	if (local_path.back() == '/' && _isDirectory(local_path)) {
		if (location->autoindex == 1) {
			_generateDirectoryList(file_path);
		} else {
			prepareResponseError(403);
		}
		return true;
	} else {
		return false;
	}
}

int Response::_isDirectory( const std::string& full_path ) {
	struct stat path_stat;
	if (stat(full_path.c_str(), &path_stat) == 0) {
		return (S_ISDIR(path_stat.st_mode));
	}
	return 0;
}

void Response::_generateDirectoryList( const std::string& file_path ) {
	std::ostringstream html;
	html << "<html><body><h1>Index of " << file_path << "</h1>\n<hr><pre><table>\n";
	DIR *dir = opendir(local_path.c_str());
	if (dir == nullptr) {
		prepareResponseError(403);
		logger.warning("Failed to open directory" + local_path);
		return;
	}
	struct dirent *entry;
	while ((entry = readdir(dir)) != nullptr) {
		html << _getEntryLine(entry);
	}
	closedir(dir);
	html << "</table>\n</pre><hr></body>\n</html>\n";
	full_response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
	full_response += std::to_string(html.str().size())+ "\r\n\r\n" + html.str();
}

std::string Response::_getEntryLine( struct dirent* entry ) {
	std::string name, full_path, size, mod_time;
	name = entry->d_name;
	full_path = _build_path(local_path, name);
	if (name == ".") {
		return "";
	}
	if (name == "..") {
		name += "/";
	} else {
		_getEntryStats(full_path, size, mod_time);
		if (entry->d_type == DT_DIR) {
			name += "/";
			size = "-";
		}
	}
	return std::string("<tr><td style=\"width:70%\"><a href=\"") + name + "\">" + 
		   name + "</a></td>" + "<td style=\"width:20%\">" + mod_time + "</td>" +
		   "<td align=\"right\">" + size + "</td></tr>\n";

}

void Response::_getEntryStats( const std::string& path, std::string& size, std::string& mod_time ) {
	struct stat entry_stat;
	if (stat(path.c_str(), &entry_stat) == -1) {
		size = "Unknown";
		mod_time = "Unknown";
		return;
	}
	struct tm *tm = localtime(&entry_stat.st_mtime);
	char timebuf[80];
	strftime(timebuf, sizeof(timebuf), "%d-%b-%Y %H:%M", tm);
	mod_time = std::string(timebuf);
	size = std::to_string(entry_stat.st_size);
}
