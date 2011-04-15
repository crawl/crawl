import os, os.path
import subprocess
import tornado.httpserver
import tornado.websocket
import tornado.ioloop
import tornado.web

bind_address = ""
bind_port = 8080

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        host = self.request.host
        self.render("client.html", socket_server = "ws://" + host + "/socket")

class CrawlWebSocket(tornado.websocket.WebSocketHandler):
    def open(self):
        print "SOCKET OPENED"
        self.ioloop = tornado.ioloop.IOLoop.instance()
        self.p = subprocess.Popen("./crawl",
                                  stdin = subprocess.PIPE,
                                  stdout = subprocess.PIPE,
                                  stderr = subprocess.PIPE)

        self.ioloop.add_handler(self.p.stdout.fileno(), self.on_stdout, self.ioloop.READ)
        self.ioloop.add_handler(self.p.stderr.fileno(), self.on_stderr, self.ioloop.READ)

        self.message_buffer = ""

    def close_pipes(self):
        self.ioloop.remove_handler(self.p.stdout.fileno())
        self.ioloop.remove_handler(self.p.stderr.fileno())

    def poll_crawl(self):
        if self.p.poll() is not None:
            self.close()

    def on_message(self, message):
        print "MESSAGE:", message
        self.p.stdin.write(message)

    def on_close(self):
        self.close_pipes()
        if self.p.poll() is None:
            self.p.terminate()
        print "SOCKET CLOSED"

    def on_stderr(self, fd, events):
        s = self.p.stderr.readline()
        if not (s.isspace() or s == ""):
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
    "static_path": "./webserver/static/"
}

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/socket", CrawlWebSocket),
], **settings)

application.listen(bind_port, bind_address)

ioloop = tornado.ioloop.IOLoop.instance()
ioloop.start()
