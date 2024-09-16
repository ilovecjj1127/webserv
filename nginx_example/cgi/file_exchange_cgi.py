#!/usr/bin/env python3

import cgi
import cgitb
import logging
import os


logging.basicConfig(
	filename="/usr/share/nginx/media/out.log",
	format='%(asctime)s - %(levelname)s: %(message)s',
	level=logging.DEBUG
)

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

	def get_full_response(self):
		code_response = {
			200: self._successful_response
		}
		return code_response[self.status_code]()
	
	def _successful_response(self) -> str:
		body = '<html><body>\n<h1>Python CGI "File Exchange"</h1>' \
			   f'{self.body}</body></html>'
		response = "HTTP/1.1 200 OK\nContent-Type: text/html\n" \
				   f"Content-Length: {len(body)}\n\n{body}"
		return response


class Cgi:
	def __init__(self):
		self.uploads_dir = "/usr/share/nginx/media"
		self.delete_js_filepath = "/usr/share/nginx/cgi/deleteFileForm.js"
		self.storage = cgi.FieldStorage()
		self.components = CgiComponents()

	def handle_request(self):
		method = os.environ.get("REQUEST_METHOD", "")
		response = Response()
		files = os.listdir(self.uploads_dir) # Should run in try-except
		if method == "GET":
			if not files:
				response.body += "<p>No files here yet</p>"
			else:
				response.body += "<h2>File list:</h2>\n<ul>"
				for file in files:
					response.body += self.components.file_with_delete_button(file)
				response.body += "</ul>"
			response.body += self.components.upload_form
			with open(self.delete_js_filepath, "r") as js_file:
				js_script = js_file.read()
			response.body += f"<script>\n{js_script}</script>"
		elif method == "POST":
			logging.debug("Got post request")
			if not self.storage:
				logging.debug("Empty storage")
			for key in self.storage:
				logging.debug(f"Key: {key},\ndata:\n{self.storage[key]}")
			file_object = self.storage["file"]
			response.body += self._create_file(file_object)
		elif method == "DELETE":
			filename = self.storage.getvalue("filename")
			response.body += self._delete_file(filename)
		else:
			response.body += f"<h2>Unknown method: {method}</h2>"
		print(response.get_full_response())

	def _delete_file(self, filename: str) -> str:
		filepath = f"{self.uploads_dir}/{filename}"
		if not os.path.isfile(filepath):
			return f"<p>File '{filename}' does't exist</p>"
		try:
			os.remove(filepath)
			return f"<p>File '{filename}' was deleted successfully</p>"
		except Exception as e:
			return f"<p>Error deleting file '{filename}': {e}</p>"

	def _create_file(self, file_object: cgi.FieldStorage) -> str:
		filename = os.path.basename(file_object.filename)
		filepath = f"{self.uploads_dir}/{filename}"
		if os.path.isfile(filepath):
			return f"<p>File '{filename}' is already exist</p>"
		try:
			with open(filepath, "wb") as file:
				file.write(file_object.file.read())
			return f"<p>File {filename} was uploaded</p>"
		except Exception as e:
			return f"<p>Error creating file '{filename}': {e}</p>"


def run_cgi():
	cgitb.enable()
	cgi = Cgi()
	logging.debug("Run CGI")
	cgi.handle_request()


if __name__ == "__main__":
	run_cgi()
