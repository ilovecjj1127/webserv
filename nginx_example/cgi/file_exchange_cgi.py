#!/usr/bin/env python3

import cgi
import cgitb
import logging
import os


# logging.basicConfig(
# 	filename="/usr/share/nginx/media/out.log",
# 	format='%(asctime)s - %(levelname)s: %(message)s',
# 	level=logging.DEBUG
# )

class CgiComponents:
	def __init__(self):
		self.upload_form = "<br><h2>Upload file</h2>" \
			"<form method='POST' action='/cgi/file_exchange_cgi.py'" \
				"enctype='multipart/form-data'>" \
			"<label for='file'>Choose a file:</label><br><br>" \
			"<input type='file' id='file' name='file' required><br><br>" \
			"<input type='submit' value='Upload'></form>"

	def file_with_delete_button(self, filename: str) -> str:
		line = f"<li>{filename} <form class='deleteFileForm' data-filename='{filename}'>\n" \
			   f"<input type='submit' value='Delete'>\n" \
				"</form></li>"
		return line


class Response:
	def __init__(self):
		self.status_code = 200
		self.body = ""

	def get_full_response(self) -> str:
		code_response = {
			200: self._successful_response,
			201: self._created_response,
			400: self._bad_request_response,
			405: self._method_not_allowed_response,
			500: self._server_error_response
		}
		return code_response[self.status_code]()
	
	def _successful_response(self) -> str:
		body = '<html><body>\n<h1>Python CGI "File Exchange"</h1>' \
			   f'{self.body}</body></html>'
		response = "Status: 200 OK\r\nContent-Type: text/html\r\n" \
				   f"Content-Length: {len(body)}\r\n\r\n{body}"
		return response
	
	def _created_response(self) -> str:
		body = '<html><body>\n<h1>Python CGI "File Exchange"</h1>' \
			   f'{self.body}</body></html>'
		response = "Status: 201 Created\r\nContent-Type: text/html\r\n" \
				   f"Content-Length: {len(body)}\r\n\r\n{body}"
		return response
	
	def _bad_request_response(self) -> str:
		body = '<html><body>\n<h1>Python CGI "File Exchange"</h1>' \
			   f'<h2>400 Bad Request</h2>{self.body}</body></html>'
		response = "Status: 400 Bad Request\r\nContent-Type: text/html\r\n" \
				   f"Content-Length: {len(body)}\r\n\r\n{body}"
		return response
	
	def _method_not_allowed_response(self) -> str:
		body = '<html><body>\n<h1>Python CGI "File Exchange"</h1>' \
			   '<h2>405 This method is not allowed</h2></body></html>'
		response = "Status: 405 Method Not Allowed\r\nContent-Type: text/html\r\n" \
				   f"Content-Length: {len(body)}\r\n\r\n{body}"
		return response
	
	def _server_error_response(self) -> str:
		body = '<html><body>\n<h1>Python CGI "File Exchange"</h1>' \
			   f'<h2>500 Internal Server Error</h2>{self.body}</body></html>'
		response = "Status: 500 Internal Server Error\r\nContent-Type: text/html\r\n" \
				   f"Content-Length: {len(body)}\r\n\r\n{body}"
		return response


class Cgi:
	def __init__(self):
		self.uploads_dir = "./nginx_example/media"
		self.delete_js_filepath = "./nginx_example/cgi/deleteFileForm.js"
		self.storage = cgi.FieldStorage()
		self.components = CgiComponents()

	def handle_request(self):
		method = os.environ.get("REQUEST_METHOD", "")
		response = Response()
		if method == "GET":
			self._get_main_page(response)
		elif method == "POST":
			self._create_file(response)
		elif method == "DELETE":
			self._delete_file(response)
		else:
			response.status_code = 405
		print(response.get_full_response())

	def _get_main_page(self, response: Response):
		try:
			files = os.listdir(self.uploads_dir)
		except Exception as e:
			response.status_code = 500
			response.body = f"<p>Error receiving a list of files: {e}</p>"
			return
		if not files:
			response.body = "<p>No files here yet</p>"
		else:
			response.body = "<h2>File list:</h2>\n<ul>"
			for file in files:
				response.body += self.components.file_with_delete_button(file)
			response.body += "</ul>"
		response.body += self.components.upload_form
		with open(self.delete_js_filepath, "r") as js_file:
			js_script = js_file.read()
		response.body += f"<script>\n{js_script}</script>"

	def _delete_file(self, response: Response):
		filename = self.storage.getvalue("filename")
		if filename is None:
			response.status_code = 400
			response.body = "<p>Field 'filename' is required<p>"
			return
		filepath = f"{self.uploads_dir}/{filename}"
		if not os.path.isfile(filepath):
			response.status_code = 400
			response.body = f"<p>File '{filename}' does't exist</p>"
			return
		try:
			os.remove(filepath)
			response.body = f"<p>File '{filename}' was deleted successfully</p>"
		except Exception as e:
			response.status_code = 500
			response.body = f"<p>Error deleting a file '{filename}': {e}</p>"

	def _create_file(self, response: Response):
		if "file" not in self.storage:
			response.status_code = 400
			response.body = "<p>Field 'file' is required<p>"
			return
		file_object = self.storage["file"]
		filename = os.path.basename(file_object.filename)
		filepath = f"{self.uploads_dir}/{filename}"
		if os.path.isfile(filepath):
			response.status_code = 400
			response.body = f"<p>File '{filename}' already exists</p>"
			return
		try:
			with open(filepath, "wb") as file:
				file.write(file_object.file.read())
			response.status_code = 201
			response.body = f"<p>File {filename} was uploaded</p>"
		except Exception as e:
			response.status_code = 500
			response.body = f"<p>Error creating file '{filename}': {e}</p>"


def run_cgi():
	cgitb.enable()
	cgi = Cgi()
	cgi.handle_request()


if __name__ == "__main__":
	run_cgi()
