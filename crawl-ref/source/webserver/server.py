#!/usr/bin/env python2.7
import os, os.path, errno, sys

import tornado.httpserver
import tornado.ioloop
import tornado.web
import tornado.template

import logging

from config import *
from util import *
from ws_handler import *
from game_data_handler import GameDataHandler
import process_handler

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        host = self.request.host
        if self.request.protocol == "https":
            protocol = "wss://"
        else:
            protocol = "ws://"
        self.render("client.html", socket_server = protocol + host + "/socket",
                    username = None, config = config)

def daemonize():
    try:
        pid = os.fork()
        if pid > 0:
            os._exit(0)
    except OSError, e:
        logging.error("Fork #1 failed! (%s)", e.strerror)
        sys.exit(1)

    os.setsid()

    try:
        pid = os.fork()
        if pid > 0:
            os._exit(0)
    except OSError, e:
        logging.error("Fork #2 failed! (%s)", e.strerror)
        sys.exit(1)

    with open("/dev/null", "rw") as f:
        os.dup2(f.fileno(), sys.stdin.fileno())
        os.dup2(f.fileno(), sys.stdout.fileno())
        os.dup2(f.fileno(), sys.stderr.fileno())

def write_pidfile():
    if not pidfile:
        return
    if os.path.exists(pidfile):
        try:
            with open(pidfile) as f:
                pid = int(f.read())
        except ValueError:
            sys.exit("PIDfile %s contains non-numeric value" % pidfile)
        try:
            os.kill(pid, 0)
        except OSError, why:
            if why[0] == errno.ESRCH:
                # The pid doesn't exist.
                logging.warn("Removing stale pidfile %s" % pidfile)
                os.remove(pidfile)
            else:
                sys.exit("Can't check status of PID %s from pidfile %s: %s" %
                         (pid, pidfile, why[1]))
        else:
            sys.exit("Another Webtiles server is running, PID %s\n" % pid)

    with open(pidfile, "w") as f:
        f.write(str(os.getpid()))

def remove_pidfile():
    if not pidfile:
        return
    try:
        os.remove(pidfile)
    except OSError, e:
        if e.errno == errno.EACCES or e.errno == errno.EPERM:
            logging.warn("No permission to delete pidfile!")
        else:
            logging.error("Failed to delete pidfile!")
    except:
        logging.error("Failed to delete pidfile!")

def shed_privileges():
    if gid is not None:
        os.setgid(gid)
    if uid is not None:
        os.setuid(uid)


def signal_handler(signum, frame):
    logging.info("Received signal %i, shutting down.", signum)
    shutdown()
    if len(sockets) == 0:
        ioloop.stop()

def bind_server():
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

    servers = []

    if bind_nonsecure:
        server = tornado.httpserver.HTTPServer(application, **kwargs)
        server.listen(bind_port, bind_address)
        servers.append(server)
    if ssl_options:
        server = tornado.httpserver.HTTPServer(application,
                                               ssl_options = ssl_options, **kwargs)
        server.listen(ssl_port, ssl_address)
        servers.append(server)

    return servers


if __name__ == "__main__":
    if chroot:
        os.chroot(chroot)

    logging.basicConfig(**logging_config)
    logging.getLogger().addFilter(TornadoFilter())
    logging.addLevelName(logging.DEBUG, "DEBG")
    logging.addLevelName(logging.WARNING, "WARN")

    if daemon:
        daemonize()

    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)

    if umask is not None:
        os.umask(umask)

    write_pidfile()

    servers = bind_server()

    shed_privileges()

    ensure_user_db_exists()

    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.set_blocking_log_threshold(0.5)

    if dgl_mode:
        status_file_timeout()
        purge_login_tokens_timeout()
        start_reading_milestones()

    logging.info("Webtiles server started! (PID: %s)" % os.getpid())

    try:
        ioloop.start()
    except KeyboardInterrupt:
        logging.info("Received keyboard interrupt, shutting down.")
        for server in servers: server.stop()
        shutdown()
        if len(sockets) > 0:
            ioloop.start() # We'll wait until all crawl processes have ended.

    remove_pidfile()
