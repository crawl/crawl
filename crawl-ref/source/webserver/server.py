import os, os.path
import subprocess

import tornado.httpserver
import tornado.websocket
import tornado.ioloop
import tornado.web

import crypt
import sqlite3

import logging

bind_address = ""
bind_port = 8080

logging.basicConfig(filename = "webtiles.log", level = logging.INFO)

password_db = "./webserver/passwd.db3"

static_path = "./webserver/static"
template_path = "./webserver/"

crawl_binary = "./crawl"

rcfile_path = "./rcs/"
macro_path = "./rcs/"
morgue_path = "./rcs/"

max_connections = 100


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

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        host = self.request.host
        self.render("client.html", socket_server = "ws://" + host + "/socket")

class CrawlWebSocket(tornado.websocket.WebSocketHandler):
    def open(self):
        logging.info("Socket opened from ip %s.", self.request.remote_ip)
        self.username = None
        self.p = None
        self.ioloop = tornado.ioloop.IOLoop.instance()
        self.message_buffer = ""

        global current_connections
        current_connections += 1

        if max_connections < current_connections:
            self.write_message("$('#crt').html('The maximum number of connections has been"
                               + " reached, sorry :('); $('#login').hide();");
            self.close()

    def start_crawl(self):
        self.p = subprocess.Popen([crawl_binary,
                                   "-name", self.username,
                                   "-rc", os.path.join(rcfile_path, self.username + ".rc"),
                                   "-macro", os.path.join(macro_path, self.username + ".macro"),
                                   "-morgue", os.path.join(morgue_path, self.username)],
                                  stdin = subprocess.PIPE,
                                  stdout = subprocess.PIPE,
                                  stderr = subprocess.PIPE)

        self.ioloop.add_handler(self.p.stdout.fileno(), self.on_stdout, self.ioloop.READ)
        self.ioloop.add_handler(self.p.stderr.fileno(), self.on_stderr, self.ioloop.READ)

    def close_pipes(self):
        if self.p is not None:
            self.ioloop.remove_handler(self.p.stdout.fileno())
            self.ioloop.remove_handler(self.p.stderr.fileno())

    def poll_crawl(self):
        if self.p.poll() is not None:
            self.close()

    def on_message(self, message):
        if message.startswith("Login: "):
            parts = message.split()
            if len(parts) != 3:
                self.write_message("login_failed();")
            else:
                _, username, password = message.split()
                if user_passwd_match(username, password):
                    logging.info("User %s logged in from ip %s.",
                                 username, self.request.remote_ip)
                    self.username = username
                    self.start_crawl()
                else:
                    logging.warn("Failed login for user %s from ip %s.",
                                 username, self.request.remote_ip)
                    self.write_message("login_failed();")
        elif self.p is not None:
            logging.debug("Message: %s (user: %s)", message, self.username)
            self.p.stdin.write(message.encode("utf8"))

    def on_close(self):
        global current_connections
        current_connections -= 1
        self.close_pipes()
        if self.p is not None and self.p.poll() is None:
            self.p.terminate()
        logging.info("Socket for ip %s closed.", self.request.remote_ip)

    def on_stderr(self, fd, events):
        s = self.p.stderr.readline()
        if not (s.isspace() or s == ""):
            logging.debug("%s: %s", self.username, s)
        self.poll_crawl()

    def on_stdout(self, fd, events):
        self.write_message(self.p.stdout.readline())
        self.poll_crawl()

settings = {
    "static_path": static_path,
    "template_path": template_path
}

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/socket", CrawlWebSocket),
], **settings)

application.listen(bind_port, bind_address)

ioloop = tornado.ioloop.IOLoop.instance()
ioloop.start()
