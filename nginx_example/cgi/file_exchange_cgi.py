#!/usr/bin/env python3

import os
import cgi
import cgitb
import logging
import sys


logging.basicConfig(
	# filename="/usr/share/nginx/media/out.log",
	filename="./nginx_example/media/out.log",
	format='%(asctime)s - %(levelname)s: %(message)s',
	level=logging.DEBUG
)


cgitb.enable()

# dir = "/usr/share/nginx/media"
dir = "./nginx_example/media"
try:
	files = os.listdir(dir)
except Exception as e:
	files = [f"Error: {e}",]

storage = cgi.FieldStorage()

upload_form = '''<br><h2>Upload file</h2>
<form method='POST' action='/cgi/file_exchange_cgi.py' enctype='multipart/form-data'>
<label for='file'>Choose a file:</label><br><br>
<input type='file' id='file' name='file' required><br><br>
<input type='submit' value='Upload'>
</form>
'''

def delete_file(filename: str) -> str:
	filepath = f"{dir}/{filename}"
	if not os.path.isfile(filepath):
		return f"<p>File '{filename}' does't exist</p>"
	try:
		os.remove(filepath)
		return f"<p>File '{filename}' was deleted successfully</p>"
	except Exception as e:
		return f"<p>Error deleting file '{filename}': {e}</p>"

def create_file(file_object: cgi.FieldStorage) -> str:
	filename = os.path.basename(file_object.filename)
	filepath = f"{dir}/{filename}"
	if os.path.isfile(filepath):
		return f"<p>File '{filename}' is already exist</p>"
	try:
		with open(filepath, "wb") as file:
			file.write(file_object.file.read())
		return f"<p>File {filename} was uploaded</p>"
	except Exception as e:
		return f"<p>Error creating file '{filename}': {e}</p>"

def file_with_delete_button(filename: str) -> str:
	line = f"<li>{filename} <form class='deleteFileForm' data-filename='{filename}'>\n" \
		   f"<input type='submit' value='Delete'>\n" \
		   "</form></li>"
	return line

body = '<html><body>\n<h1>Python CGI "File Exchange"</h1>'
method = os.environ.get("REQUEST_METHOD", "")
if method == "GET":
	if not files:
		body += "<p>No files here yet</p>"
	else:
		body += "<h2>File list:</h2>\n<ul>"
		for file in files:
			body += file_with_delete_button(file)
		body += "</ul>"
	body += upload_form
	# with open("/usr/share/nginx/cgi/deleteFileForm.js", "r") as js_file:
	with open("./nginx_example/cgi/deleteFileForm.js", "r") as js_file:
		js_script = js_file.read()
	body += f"<script>\n{js_script}</script>"
elif method == "POST":
	file_object = storage["file"]
	body += create_file(file_object)
elif method == "DELETE":
	filename = storage.getvalue("filename")
	body += delete_file(filename)
else:
	body += f"<h2>Unknown method: {method}</h2>"
body += "</body></html>"

response = f"Status: 200 OK\nContent-Type: text/html\nContent-Length: {len(body)}\n\n{body}"
print(response)
