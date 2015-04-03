#!/usr/bin/env python
import os, os.path, errno, sys

import tornado.httpserver
import tornado.ioloop
import tornado.web
import tornado.template

import logging, logging.handlers

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

class NoCacheHandler(tornado.web.StaticFileHandler):
    def set_extra_headers(self, path):
        self.set_header("Cache-Control", "no-cache, no-store, must-revalidate")
        self.set_header("Pragma", "no-cache")
        self.set_header("Expires", "0")

def err_exit(errmsg):
    logging.error(errmsg)
    sys.exit(errmsg)

def daemonize():
    try:
        pid = os.fork()
        if pid > 0:
            os._exit(0)
    except OSError, e:
        err_exit("Fork #1 failed! (%s)" % (e.strerror))

    os.setsid()

    try:
        pid = os.fork()
        if pid > 0:
            os._exit(0)
    except OSError, e:
        err_exit("Fork #2 failed! (%s)" % e.strerror)

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
            err_exit("PIDfile %s contains non-numeric value" % pidfile)
        try:
            os.kill(pid, 0)
        except OSError, why:
            if why[0] == errno.ESRCH:
                # The pid doesn't exist.
                logging.warn("Removing stale pidfile %s" % pidfile)
                os.remove(pidfile)
            else:
                err_exit("Can't check status of PID %s from pidfile %s: %s" %
                         (pid, pidfile, why[1]))
        else:
            err_exit("Another Webtiles server is running, PID %s\n" % pid)

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

    if hasattr(config, "no_cache") and config.no_cache:
        settings["static_handler_class"] = NoCacheHandler

    application = tornado.web.Application([
            (r"/", MainHandler),
            (r"/socket", CrawlWebSocket),
            (r"/gamedata/(.*)/(.*)", GameDataHandler)
            ], gzip=True, **settings)

    kwargs = {}
    if http_connection_timeout is not None:
        kwargs["connection_timeout"] = http_connection_timeout

    servers = []

    if bind_nonsecure:
        server = tornado.httpserver.HTTPServer(application, **kwargs)
        try:
            listens = bind_pairs
        except NameError:
            listens = ( (bind_address, bind_port), )
        for (addr, port) in listens:
            logging.info("Listening on %s:%d" % (addr, port))
            server.listen(port, addr)
        servers.append(server)
    if ssl_options:
        # TODO: allow different ssl_options per bind pair
        server = tornado.httpserver.HTTPServer(application,
                                               ssl_options = ssl_options, **kwargs)
        try:
            listens = ssl_bind_pairs
        except NameError:
            listens = ( (ssl_address, ssl_port), )
        for (addr, port) in listens:
            logging.info("Listening on %s:%d" % (addr, port))
            server.listen(port, addr)
        servers.append(server)

    return servers

def init_logging(logging_config):
    filename = logging_config.get("filename")
    if filename:
        max_bytes = logging_config.get("max_bytes", 10*1000*1000)
        backup_count = logging_config.get("backup_count", 5)
        hdlr = logging.handlers.RotatingFileHandler(
            filename, maxBytes=max_bytes, backupCount=backup_count)
    else:
        hdlr = logging.StreamHandler(None)
    fs = logging_config.get("format", "%(levelname)s:%(name)s:%(message)s")
    dfs = logging_config.get("datefmt", None)
    fmt = logging.Formatter(fs, dfs)
    hdlr.setFormatter(fmt)
    logging.getLogger().addHandler(hdlr)
    level = logging_config.get("level")
    if level is not None:
        logging.getLogger().setLevel(level)
    logging.getLogger().addFilter(TornadoFilter())
    logging.addLevelName(logging.DEBUG, "DEBG")
    logging.addLevelName(logging.WARNING, "WARN")


def check_config():
    success = True
    for (game_id, game_data) in games.iteritems():
        if not os.path.exists(game_data["crawl_binary"]):
            logging.warning("Crawl executable %s doesn't exist!", game_data["crawl_binary"])
            success = False

        if ("client_path" in game_data and
            not os.path.exists(game_data["client_path"])):
            logging.warning("Client data path %s doesn't exist!", game_data["client_path"])
            success = False
    return success


if __name__ == "__main__":
    if chroot:
        os.chroot(chroot)

    init_logging(logging_config)

    if not check_config():
        err_exit("Errors in config. Exiting.")

    if daemon:
        daemonize()

    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)

    if umask is not None:
        os.umask(umask)

    write_pidfile()

    servers = bind_server()

    shed_privileges()

    if dgl_mode:
        ensure_user_db_exists()

    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.set_blocking_log_threshold(0.5)

    if dgl_mode:
        status_file_timeout()
        purge_login_tokens_timeout()
        start_reading_milestones()

    if watch_socket_dirs:
        process_handler.watch_socket_dirs()

    logging.info("Webtiles server started! (PID: %s)" % os.getpid())

    try:
        ioloop.start()
    except KeyboardInterrupt:
        logging.info("Received keyboard interrupt, shutting down.")
        for server in servers: server.stop()
        shutdown()
        ioloop.add_timeout(time.time() + 2, ioloop.stop)
        if len(sockets) > 0:
            ioloop.start() # We'll wait until all crawl processes have ended.

    remove_pidfile()
