#!/usr/bin/env python3

import cgi
import hashlib
import json
import os
import random


class Cgi:
	def __init__(self):
		self.session_id = 0
		self.username = ""
		self.db_filename = "./data/cgi/fake_db.json"
		self.storage = cgi.FieldStorage()

	def handle_request(self):
		method = os.environ.get("REQUEST_METHOD", "")
		if method not in ("GET", "POST"):
			return self.unknown_method(method)
		if method == "POST":
			self.authentication()
		elif method == "GET" and self.is_authenticated():
			self.show_personal_account()
		else:
			self.show_login_form()
	
	def get_cookies(self) -> dict:
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
	
	def authentication(self):
		fake_db = self.get_fake_db()
		db_users = fake_db["users"]
		username = self.storage.getvalue("username", "")
		password = self.storage.getvalue("password", "")
		if username in db_users:
			password_hash = db_users[username]["password_hash"]
			if self.check_password(password, password_hash):
				session_id = random.randint(1, 999999)
				self.update_db(fake_db, username, session_id)
				return self.successful_login()
		return self.login_failed()
	
	def is_authenticated(self) -> bool:
		cookies = self.get_cookies()
		session_id = cookies.get("session_id", "")
		if not session_id:
			return False
		fake_db = self.get_fake_db()
		db_sessions = fake_db["sessions"]
		if session_id in db_sessions:
			self.session_id = int(session_id)
			self.username = db_sessions[session_id]
			return True
		return False
	
	def get_fake_db(self) -> dict:
		with open(self.db_filename, "r") as json_file:
			return json.load(json_file)

	def update_db(self, db: dict, username: str, session_id: int):
		old_session = str(db["users"][username]["session_id"])
		if old_session in db["sessions"]:
			db["sessions"].pop(old_session)
		db["users"][username]["session_id"] = session_id
		db["sessions"][session_id] = username
		with open(self.db_filename, "w") as json_file:
			json.dump(db, json_file, indent=2)
		self.username = username
		self.session_id = session_id

	def hash_password(self, password: str) -> str:
		return hashlib.sha256(password.encode()).hexdigest()

	def check_password(self, password: str, hash: str) -> bool:
		return self.hash_password(password) == hash

	def unknown_method(self, method: str):
		body = '<html><body>\n<h1>Python CGI "Test sessions"</h1>' \
			   f'<h2>405 Method {method} is not allowed</h2></body></html>'
		response = "Status: 405 Method Not Allowed\r\nContent-Type: text/html\r\n" \
				   f"Content-Length: {len(body)}\r\n\r\n{body}"
		print(response)
	
	def show_login_form(self):
		endpoint = os.environ.get("PATH_INFO", "")
		login_form = f"<form method='POST' action='{endpoint}'>" \
					 "<label for='username'>Username: </label>" \
					 "<input type='text' id='username' name='username'><br><br>" \
					 "<label for='password'>Password: </label>" \
					 "<input type='password' id='password' name='password'><br><br>" \
					 "<input type='submit' value='Submit'>" \
					 "</form>"
		body = '<html><body>\n<h1>Python CGI "Test sessions"</h1>' \
			   f'<h2>Login</h2>{login_form}</body></html>'
		response = "Status: 200 OK\r\nContent-Type: text/html\r\n" \
				   f"Content-Length: {len(body)}\r\n\r\n{body}"
		print(response)

	def successful_login(self):
		body = '<html><body>\n<h1>Python CGI "Test sessions"</h1>' \
			   f'<h2>Hi, {self.username}!</h2><p>Successful login</p></body></html>'
		response = "Status: 200 OK\r\nContent-Type: text/html\r\n" \
				   f"Set-Cookie: session_id={self.session_id}\r\n" \
				   f"Content-Length: {len(body)}\r\n\r\n{body}"
		print(response)

	def show_personal_account(self):
		body = '<html><body>\n<h1>Python CGI "Test sessions"</h1>' \
			   f'<h2>Hi, {self.username}!</h2><p>This is your personal account</p> \
			   </body></html>'
		response = "Status: 200 OK\r\nContent-Type: text/html\r\n" \
				   f"Content-Length: {len(body)}\r\n\r\n{body}"
		print(response)

	def login_failed(self):
		body = '<html><body>\n<h1>Python CGI "Test sessions"</h1>' \
			   f'<h2>Login failed</h2></body></html>'
		response = "Status: 401 Unauthorized\r\nContent-Type: text/html\r\n" \
				   f"Content-Length: {len(body)}\r\n\r\n{body}"
		print(response)


if __name__ == "__main__":
	cgi = Cgi()
	cgi.handle_request()
