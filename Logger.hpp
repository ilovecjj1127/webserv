#pragma once

#include <iostream>
#include <string>

enum Level {
	DEBUG,
	INFO,
	WARNING,
	ERROR,
	SILENCE
};

class Logger {
private:
	Level _level;

public:
	Logger( Level level = INFO );
	Logger( const Logger& other );
	~Logger( void ) {};

	Logger& operator = ( const Logger& other );

	Level getLevel( void ) const;
	void setLevel( Level level );
	
	void debug( const std::string& msg ) const;
	void info( const std::string& msg ) const;
	void warning( const std::string& msg ) const;
	void error( const std::string& msg ) const;
};