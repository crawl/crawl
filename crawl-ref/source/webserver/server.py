#!/usr/bin/env python2.7
import os, os.path

import tornado.httpserver
import tornado.ioloop
import tornado.web
import tornado.template

import logging

from config import *
from util import *
from ws_handler import *
from game_data_handler import GameDataHandler

logging.basicConfig(**logging_config)
logging.getLogger().addFilter(TornadoFilter())
logging.addLevelName(logging.DEBUG, "DEBG")
logging.addLevelName(logging.WARNING, "WARN")

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        host = self.request.host
        if self.request.protocol == "https":
            protocol = "wss://"
        else:
            protocol = "ws://"
        self.render("client.html", socket_server = protocol + host + "/socket",
                    username = None)

def signal_handler(signum, frame):
    logging.info("Received signal %i, shutting down.", signum)
    shutdown()
    if len(sockets) == 0:
        ioloop.stop()

def purge_login_tokens():
    for token in list(login_tokens):
        if datetime.datetime.now() > login_tokens[token]:
            del login_tokens[token]

def purge_login_tokens_timeout():
    purge_login_tokens()
    ioloop.add_timeout(time.time() + 60 * 60 * 1000,
                       purge_login_tokens_timeout)

def status_file_timeout():
    write_dgl_status_file()
    ioloop.add_timeout(time.time() + status_file_update_rate,
                       status_file_timeout)

signal.signal(signal.SIGTERM, signal_handler)
signal.signal(signal.SIGHUP, signal_handler)

settings = {
    "static_path": static_path,
    "template_loader": DynamicTemplateLoader.get(template_path)
}

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/socket", CrawlWebSocket),
    (r"/gamedata/(.*)/(.*)", GameDataHandler)
], **settings)

kwargs = {}
if http_connection_timeout is not None:
    kwargs["connection_timeout"] = http_connection_timeout

if bind_nonsecure:
    application.listen(bind_port, bind_address, **kwargs)
if ssl_options:
    application.listen(ssl_port, ssl_address, ssl_options = ssl_options,
                       **kwargs)

if gid is not None:
    os.setgid(gid)
if uid is not None:
    os.setuid(uid)

ioloop = tornado.ioloop.IOLoop.instance()
ioloop.set_blocking_log_threshold(0.5)

if dgl_mode:
    status_file_timeout()
    purge_login_tokens_timeout()

logging.info("Webtiles server started!")

try:
    ioloop.start()
except KeyboardInterrupt:
    logging.info("Received keyboard interrupt, shutting down.")
    shutdown()
    if len(sockets) > 0:
        ioloop.start() # We'll wait until all crawl processes have ended.
