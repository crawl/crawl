import os, os.path
import subprocess
import tornado.httpserver
import tornado.websocket
import tornado.ioloop
import tornado.web

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        self.redirect("/static/client.html")

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

    def on_message(self, message):
        print "MESSAGE:", message
        self.p.stdin.write(message)

    def on_close(self):
        self.close_pipes()
        if not self.p.poll():
            self.p.terminate()
        print "SOCKET CLOSED"

    def on_stderr(self, fd, events):
        print "ERR: ", self.p.stderr.readline(),
        if self.p.poll():
            self.close()
            self.close_pipes()

    def on_stdout(self, fd, events):
        char = self.p.stdout.read(1)
        if char == '\n':
            self.write_message(self.message_buffer)
            self.message_buffer = ""
        else:
            self.message_buffer += char

        if self.p.poll():
            self.close()
            self.close_pipes()

settings = {
    "static_path": "./webserver/static/"
}

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/socket", CrawlWebSocket),
], **settings)

application.listen(8080)

ioloop = tornado.ioloop.IOLoop.instance()
ioloop.start()
