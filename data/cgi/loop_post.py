#!/usr/bin/env python3

import os

def loop():
	while True:
		pass

def get_main_page() -> str:
	return "<form method='POST' action='/cgi/loop_post.py'>" \
		   "<button type='submit'>Create a loop</button>" \
		   "</form>"

def unknown_method(method: str):
	body = '<html><body>\n<h1>Python CGI "Test timeout"</h1>' \
		   f'<h2>405 Method {method} is not allowed</h2></body></html>'
	response = "Status: 405 Method Not Allowed\r\nContent-Type: text/html\r\n" \
			   f"Content-Length: {len(body)}\r\n\r\n{body}"
	print(response)

def handle_request():
	method = os.environ.get("REQUEST_METHOD", "")
	if method == "GET":
		body = get_main_page()
	elif method == "POST":
		loop()
	else:
		return unknown_method(method)
	body = '<html><body>\n<h1>Python CGI "Test timeout"</h1>' \
		   f'{body}</body></html>'
	response = "Status: 200 OK\r\nContent-Type: text/html\r\n" \
			   f"Content-Length: {len(body)}\r\n\r\n{body}"
	print(response)


if __name__ == "__main__":
	handle_request()
