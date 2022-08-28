# python3 -m pip install pyftpdlib
import pyftpdlib.authorizers
import pyftpdlib.handlers
import pyftpdlib.servers
import argparse
import logging
import os

from pyftpdlib.handlers import FTPHandler

class Handler(FTPHandler):
    def on_file_sent(self, file):
        logging.info("send file! {}".format(file))
        #super(Handler, self).ftp_RETR(file)
    def on_file_received(self, file):
        logging.info("received file! {}".format(file))
        #super(Handler, self).ftp_RETR(file)


logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.INFO)
parser = argparse.ArgumentParser()
parser.add_argument('--user', default="user", help='ftp user name. default is user')
parser.add_argument('--password', default="password", help='ftp user password. default is password')
parser.add_argument('--port', default=2121, help='ftp port. default is 2121')
args = parser.parse_args()
logging.info("user={} {}".format(type(args.user), args.user))
logging.info("password={} {}".format(type(args.password), args.password))
logging.info("port={} {}".format(type(args.port), args.port))


authorizer = pyftpdlib.authorizers.DummyAuthorizer()
#authorizer.add_user('user', 'password', '/home/nop/ftp-server', perm='elradfmw')
authorizer.add_user(args.user, args.password, os.getcwd(), perm='elradfmw')
handler = Handler
logging.info(handler.passive_ports)
handler.authorizer = authorizer
# Disable remote connection
#server = pyftpdlib.servers.FTPServer(("127.0.0.1", 2121), handler)
# Enable remote connection
server = pyftpdlib.servers.FTPServer(("0.0.0.0", 2121), handler)
server.serve_forever()

