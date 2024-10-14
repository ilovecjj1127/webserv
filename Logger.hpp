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
	Logger( Level level = INFO );
	~Logger( void ) {};

	static Logger _instance;

	Level _level;

public:
	Logger( const Logger& ) = delete;
	Logger& operator = ( const Logger& ) = delete;

	static Logger& getInstance( void );

	Level getLevel( void ) const;
	void setLevel( Level level );
	
	void debug( const std::string& msg ) const;
	void info( const std::string& msg ) const;
	void warning( const std::string& msg ) const;
	void error( const std::string& msg ) const;
};