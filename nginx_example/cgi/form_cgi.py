#!/usr/bin/env python3

import os
import cgi


def get_main_page() -> str:
	return "<form method='POST' action='/cgi/form_cgi.py'>" \
		   "<label for='name'>Name:</label>" \
		   "<input type='text' id='name' name='name'><br><br>" \
		   "<label for='city'>City:</label>" \
		   "<input type='city' id='city' name='city'><br><br>" \
		   "<input type='submit' value='Submit'>" \
		   "</form>"


def greet_user() -> str:
	form = cgi.FieldStorage()
	name = form.getvalue("name", "Unknown person")
	city = form.getvalue("city", "Nowhere")
	return f"<h2>Hello, {name} from {city}!</h2>"


def unknown_method(method: str):
	body = '<html><body>\n<h1>Python CGI "Test form"</h1>' \
		   f'<h2>405 Method {method} is not allowed</h2></body></html>'
	response = "Status: 405 Method Not Allowed\r\nContent-Type: text/html\r\n" \
			   f"Content-Length: {len(body)}\r\n\r\n{body}"
	print(response)


def handle_request():
	method = os.environ.get("REQUEST_METHOD", "")
	if method == "GET":
		body = get_main_page()
	elif method == "POST":
		body = greet_user()
	else:
		return unknown_method(method)
	body = '<html><body>\n<h1>Python CGI "Test form"</h1>' \
		   f'{body}</body></html>'
	response = "Status: 200 OK\r\nContent-Type: text/html\r\n" \
			   f"Content-Length: {len(body)}\r\n\r\n{body}"
	print(response)


if __name__ == "__main__":
	handle_request()
