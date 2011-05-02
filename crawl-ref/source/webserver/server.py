import os
import subprocess
import tornado.httpserver
import tornado.websocket
import tornado.ioloop
import tornado.web

os.chdir("/home/florian/Projekte/crawl/crawl-ref/source/")

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        self.write("""
        <html><head>
        <title>Web Crawl</title>
        <script type=\"text/javascript\" src=\"/static/jquery.js\"></script>
        <script type=\"text/javascript\" src=\"/static/enums.js\"></script>
        <script type=\"text/javascript\" src=\"/static/client.js\"></script>
        <script type=\"text/javascript\" src=\"/static/tileinfo-floor.js\"></script>
        <script type=\"text/javascript\" src=\"/static/tileinfo-wall.js\"></script>
        <script type=\"text/javascript\" src=\"/static/tileinfo-feat.js\"></script>
        <script type=\"text/javascript\" src=\"/static/tileinfo-dngn.js\"></script>
        <script type=\"text/javascript\" src=\"/static/tileinfo-main.js\"></script>
        <script type=\"text/javascript\" src=\"/static/tileinfo-player.js\"></script>
        <script type=\"text/javascript\" src=\"/static/tileinfo-icons.js\"></script>
        <script type=\"text/javascript\" src=\"/static/tileinfo-gui.js\"></script>
        <link rel=\"stylesheet\" type=\"text/css\" href=\"/static/style.css\" />
        </head>
        <body>
        <canvas id=\"crt\" width=\"50\" height=\"50\"></canvas>
        <canvas id=\"dungeon\" width=\"50\" height=\"50\"></canvas>
        </body>
        </html>""")

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

        self.message_buffer = "";

    def close_pipes(self):
        self.ioloop.remove_handler(self.p.stdout.fileno())
        self.ioloop.remove_handler(self.p.stderr.fileno())

    def on_message(self, message):
        print "MESSAGE:", message
        self.p.stdin.write(message)

    def on_close(self):
        self.close_pipes()
        self.p.terminate()
        print "SOCKET CLOSED"

    def on_stderr(self, fd, events):
        print "ERR: ", self.p.stderr.readline(),
        if self.p.poll():
            self.close_pipes()

    def on_stdout(self, fd, events):
        char = self.p.stdout.read(1)
        if char == '\n':
            self.write_message(self.message_buffer)
            self.message_buffer = ""
        else:
            self.message_buffer += char

        if self.p.poll():
            self.close_pipes()

settings = {
    "static_path": "/home/florian/Projekte/web-crawl-server/static/"
}

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/socket", CrawlWebSocket),
], **settings)

application.listen(8080)
ioloop = tornado.ioloop.IOLoop.instance()

ioloop.start()
