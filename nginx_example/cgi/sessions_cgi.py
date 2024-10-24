#!/usr/bin/env python3

import cgi
import os
import random


def unknown_method(method: str):
	body = '<html><body>\n<h1>Python CGI "Test sessions"</h1>' \
		   f'<h2>405 Method {method} is not allowed</h2></body></html>'
	response = "Status: 405 Method Not Allowed\r\nContent-Type: text/html\r\n" \
			   f"Content-Length: {len(body)}\r\n\r\n{body}"
	print(response)

def get_cookies() -> dict:
	cookies = {}
	line = os.environ.get("COOKIE", "")
	if not line:
		return {}
	pairs = line.split("; ")
	for pair in pairs:
		if pair.count("=") != 1:
			continue
		key, value = pair.split("=")
		cookies[key] = value
	return cookies

def handle_request():
	method = os.environ.get("REQUEST_METHOD", "")
	if method != "GET":
		return unknown_method(method)
	cookies = get_cookies()
	response = "Status: 200 OK\r\nContent-Type: text/html\r\n"
	session_id = cookies.get("session_id", 0)
	if session_id != 0:
		msg = f'Your session_id: {session_id}'
	else:
		session_id = random.randint(1, 999999)
		msg = f'New session was created: {session_id}'
		response += f"Set-Cookie: session_id={session_id}\r\n"
	body = '<html><body>\n<h1>Python CGI "Test sessions"</h1>' \
		   f'<p>{msg}</p></body></html>'
	response += f"Content-Length: {len(body)}\r\n\r\n{body}"
	print(response)


if __name__ == "__main__":
	handle_request()
