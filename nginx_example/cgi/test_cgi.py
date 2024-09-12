#!/usr/bin/env python3

import os
import cgi


# response = '''Content-Type: text/html

# <html><body>
# <h1>Python Basic CGI</h1>
# '''

# env_vars = dict(os.environ)
# if env_vars:
# 	response += "<h2>Your envitonment:</h2>"
# for key, value in env_vars.items():
# 	response += f"<p>{key} = {value}</p>"

# params_storage = cgi.FieldStorage()
# params = {key: params_storage.getvalue(key) for key in params_storage.keys()}
# if params:
# 	response += "<h2>Your parameters:</h2>"
# for key, value in params.items():
# 	response += f"<p>{key} = {value}</p>"
# response += "</body></html>"

body = '''<html><body>
<h1>Python Basic CGI</h1>
</body></html>
'''

response = f"Content-Type: text/html\nContent-Length: {len(body)}\n\n{body}"

print(response)
