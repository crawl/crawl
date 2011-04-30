import os, os.path
import subprocess

import tornado.httpserver
import tornado.websocket
import tornado.ioloop
import tornado.web

import crypt
import sqlite3

bind_address = ""
bind_port = 8080

password_db = "./webserver/passwd.db3"

debug_log = False

static_path = "./webserver/static"

crawl_binary = "./crawl"

rcfile_path = "./rcs/"
macro_path = "./rcs/"
morgue_path = "./rcs/"

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

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        host = self.request.host
        self.render("client.html", socket_server = "ws://" + host + "/socket")

class CrawlWebSocket(tornado.websocket.WebSocketHandler):
    def open(self):
        print "SOCKET OPENED"
        self.username = None
        self.p = None
        self.ioloop = tornado.ioloop.IOLoop.instance()
        self.message_buffer = ""

    def start_crawl(self):
        print "USERNAME:", self.username
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
            _, username, password = message.split()
            if user_passwd_match(username, password):
                self.username = username
                self.start_crawl()
            else:
                self.write_message("login_failed();")
        elif self.p is not None:
            if debug_log: print "MESSAGE:", message
            self.p.stdin.write(message)

    def on_close(self):
        self.close_pipes()
        if self.p is not None and self.p.poll() is None:
            self.p.terminate()
        print "SOCKET CLOSED"

    def on_stderr(self, fd, events):
        s = self.p.stderr.readline()
        if debug_log and not (s.isspace() or s == ""):
            print "ERR: ", s,
        self.poll_crawl()

    def on_stdout(self, fd, events):
        char = self.p.stdout.read(1)
        if char == '\n':
            self.write_message(self.message_buffer)
            self.message_buffer = ""
        else:
            self.message_buffer += char

        self.poll_crawl()

settings = {
    "static_path": static_path
}

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/socket", CrawlWebSocket),
], **settings)

application.listen(bind_port, bind_address)

ioloop = tornado.ioloop.IOLoop.instance()
ioloop.start()
