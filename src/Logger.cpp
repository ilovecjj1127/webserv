#include "Logger.hpp"

Logger Logger::_instance;

Logger::Logger( Level level ) : _level(level) {}

Logger& Logger::getInstance( void ) {
	return _instance;
}

Level Logger::getLevel( void ) const {
	return _level;
}
void Logger::setLevel( Level level ) {
	_level = level;
}

void Logger::debug( const std::string& msg ) const {
	if (_level > DEBUG) return;
	std::cout << "DEBUG: " << msg << std::endl;
}

void Logger::info( const std::string& msg ) const {
	if (_level > INFO) return;
	std::cout << "INFO: " << msg << std::endl;
}

void Logger::warning( const std::string& msg ) const {
	if (_level > WARNING) return;
	std::cout << "WARNING: " << msg << std::endl;
}

void Logger::error( const std::string& msg ) const {
	if (_level > ERROR) return;
	std::cout << "ERROR: " << msg << std::endl;
}
