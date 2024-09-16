#!/usr/bin/env python3

import os
import cgi


body = "<html><body>\n<h1>Python Basic CGI</h1>"
env_vars = dict(os.environ)
if env_vars:
	body += "<h2>Your envitonment:</h2>"
for key, value in env_vars.items():
	body += f"<p>{key} = {value}</p>"

params_storage = cgi.FieldStorage()
params = {key: params_storage.getvalue(key) for key in params_storage.keys()}
if params:
	body += "<h2>Your parameters:</h2>"
for key, value in params.items():
	body += f"<p>{key} = {value}</p>"
body += "</body></html>"

response = f"HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: {len(body)}\n\n{body}"

print(response)
