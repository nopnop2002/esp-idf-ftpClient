#!/usr/bin/env python
# -*- coding: utf-8 -*-
# python3 -m pip install pyftpdlib
import os
import argparse
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer
from pyftpdlib.authorizers import DummyAuthorizer

class Handler(FTPHandler):

	def on_connect(self):
		# do something when client connects
		pass

	def on_disconnect(self):
		# do something when client disconnects
		pass

	def on_login(self, username):
		# do something when user login
		pass

	def on_logout(self, username):
		# do something when user logs out
		pass

	def on_file_sent(self, file):
		# do something when a file has been sent
		print("on_file_sent={}".format(file))

	def on_file_received(self, file):
		# do something when a file has been received
		print("on_file_received={}".format(file))

def main():
	parser = argparse.ArgumentParser()
	parser.add_argument('--user', default="ftpuser", help='ftp user name')
	parser.add_argument('--password', default="ftppass", help='ftp user password')
	parser.add_argument('--port', default=2121, help='ftp port')
	args = parser.parse_args()
	print("args.user={}".format(args.user))
	print("args.password={}".format(args.password))
	print("args.port={}".format(args.port))

	# Instantiate a dummy authorizer for managing 'virtual' users
	authorizer = DummyAuthorizer()

	# Define a new user having full r/w permissions and a read-only
	authorizer.add_user(args.user, args.password, os.getcwd(), perm='elradfmw')

	# anonymous user
	#authorizer.add_anonymous(os.getcwd())

	# Instantiate FTP handler class
	handler = Handler
	handler.authorizer = authorizer

	# Disable remote connection
	#server = pyftpdlib.servers.FTPServer(("127.0.0.1", 2121), handler)

	# Enable remote connection
	server = FTPServer(("0.0.0.0", 2121), handler)

	# start ftp server
	server.serve_forever()

if __name__ == '__main__':
	main()
