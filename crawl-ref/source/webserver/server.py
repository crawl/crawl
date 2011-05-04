import os, os.path
import subprocess

import tornado.httpserver
import tornado.websocket
import tornado.ioloop
import tornado.web

import crypt
import sqlite3

import logging
import sys
import signal
import time

from config import *

logging.basicConfig(**logging_config)

class TornadoFilter(logging.Filter):
    def filter(self, record):
        if record.module == "web": return False
        return True
logging.getLogger().addFilter(TornadoFilter())

def user_passwd_match(username, passwd):
    crypted_pw = crypt.crypt(passwd, passwd)

    conn = sqlite3.connect(password_db)
    c = conn.cursor()
    c.execute("select password from dglusers where username=?", (username,))
    result = c.fetchone()
    c.close()
    conn.close()

    if result is None:
        return False
    else:
        return crypted_pw == result[0]

current_connections = 0
sockets = set() # Sockets running crawl processes
current_id = 0
shutting_down = False

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        host = self.request.host
        if self.request.protocol == "https":
            protocol = "wss://"
        else:
            protocol = "ws://"
        self.render("client.html", socket_server = protocol + host + "/socket")

class CrawlWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, app, req, **kwargs):
        tornado.websocket.WebSocketHandler.__init__(self, app, req, **kwargs)
        self.username = None
        self.p = None
        self.timeout = None

        global current_id
        self.id = current_id
        current_id += 1

    def __hash__(self):
        return self.id

    def __eq__(self, other):
        return self.id == other.id

    def open(self):
        logging.info("Socket opened from ip %s.", self.request.remote_ip)
        self.ioloop = tornado.ioloop.IOLoop.instance()
        self.crawl_terminated = False

        global current_connections
        current_connections += 1

        self.reset_timeout()

        if max_connections < current_connections:
            self.write_message("$('#crt').html('The maximum number of connections has been"
                               + " reached, sorry :('); $('#login').hide();");
            self.close()
        elif shutting_down:
            self.close()

    def reset_timeout(self):
        if self.timeout:
            self.ioloop.remove_timeout(self.timeout)

        self.received_pong = False
        self.write_message("ping();")
        self.timeout = self.ioloop.add_timeout(time.time() + connection_timeout,
                                               self.check_connection)

    def check_connection(self):
        self.timeout = None

        if not self.received_pong:
            logging.info("Connection to remote ip %s timed out.", self.request.remote_ip)
            self.close()

        if not self.client_terminated:
            self.reset_timeout()

    def start_crawl(self):
        sockets.add(self)
        self.p = subprocess.Popen([crawl_binary,
                                   "-name", self.username,
                                   "-rc", os.path.join(rcfile_path, self.username + ".rc"),
                                   "-macro", os.path.join(macro_path, self.username + ".macro"),
                                   "-morgue", os.path.join(morgue_path, self.username)],
                                  stdin = subprocess.PIPE,
                                  stdout = subprocess.PIPE,
                                  stderr = subprocess.PIPE)

        self.ioloop.add_handler(self.p.stdout.fileno(), self.on_stdout,
                                self.ioloop.READ | self.ioloop.ERROR)
        self.ioloop.add_handler(self.p.stderr.fileno(), self.on_stderr,
                                self.ioloop.READ | self.ioloop.ERROR)

    def shutdown(self, msg):
        if not self.client_terminated:
            self.write_message("connection_closed('" + msg + "');");
            self.close()
        if not self.crawl_terminated:
            self.p.send_signal(subprocess.signal.SIGHUP)

    def close_pipes(self):
        if self.p is not None:
            self.ioloop.remove_handler(self.p.stdout.fileno())
            self.ioloop.remove_handler(self.p.stderr.fileno())
            self.p.stdout.close()
            self.p.stderr.close()

    def poll_crawl(self):
        if not self.crawl_terminated and self.p.poll() is not None:
            self.crawl_terminated = True
            self.close_pipes()
            sockets.remove(self)
            if not self.client_terminated: self.close()

            if shutting_down and len(sockets) == 0:
                # The last crawl process has ended, now we can go
                self.ioloop.stop()

    def on_message(self, message):
        login_start = "Login: "
        if message.startswith(login_start):
            parts = message.split()
            if len(parts) != 3:
                self.write_message("login_failed();")
            else:
                message = message[len(login_start):]
                username, _, password = message.partition(' ')
                if user_passwd_match(username, password):
                    logging.info("User %s logged in from ip %s.",
                                 username, self.request.remote_ip)
                    self.username = username
                    self.start_crawl()
                else:
                    logging.warn("Failed login for user %s from ip %s.",
                                 username, self.request.remote_ip)
                    self.write_message("login_failed();")
        elif message == "Pong":
            self.received_pong = True
        elif self.p is not None:
            logging.debug("Message: %s (user: %s)", message, self.username)
            self.poll_crawl()
            if self.crawl_terminated: return
            self.p.stdin.write(message.encode("utf8"))

    def on_close(self):
        global current_connections
        current_connections -= 1

        if self.p is not None and self.p.poll() is None:
            self.p.send_signal(subprocess.signal.SIGHUP)

        self.ioloop.remove_timeout(self.timeout)

        logging.info("Socket for ip %s closed.", self.request.remote_ip)

    def on_stderr(self, fd, events):
        if events & self.ioloop.ERROR:
            self.poll_crawl()
        elif events & self.ioloop.READ:
            s = self.p.stderr.readline()

            if self.client_terminated or self.crawl_terminated:
                return

            if not (s.isspace() or s == ""):
                logging.debug("%s: %s", self.username, s)

            self.poll_crawl()

    def on_stdout(self, fd, events):
        if events & self.ioloop.ERROR:
            self.poll_crawl()
        elif events & self.ioloop.READ:
            msg = self.p.stdout.readline()

            if self.client_terminated or self.crawl_terminated:
                return

            self.write_message(msg)
            self.poll_crawl()

def shutdown(msg = "The server is shutting down. Your game has been saved."):
    global shutting_down
    shutting_down = True
    for socket in list(sockets):
        socket.shutdown(msg)

def handler(signum, frame):
    shutdown()
    if len(sockets) == 0:
        ioloop.stop()

signal.signal(signal.SIGTERM, handler)
signal.signal(signal.SIGHUP, handler)

settings = {
    "static_path": static_path,
    "template_path": template_path
}

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/socket", CrawlWebSocket),
], **settings)

application.listen(bind_port, bind_address)
if ssl_options:
    application.listen(ssl_port, ssl_address, ssl_options = ssl_options)

ioloop = tornado.ioloop.IOLoop.instance()

try:
    ioloop.start()
except KeyboardInterrupt:
    shutdown()
    if len(sockets) > 0:
        ioloop.start() # We'll wait until all crawl processes have ended.
