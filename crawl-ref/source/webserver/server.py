#!/usr/bin/env python

import os
import errno
import sys
import re
import random

import tornado.httpserver
import tornado.ioloop
import tornado.web
import tornado.template

import logging
import logging.handlers

from conf import config
from util import *
from ws_handler import *
from game_data_handler import GameDataHandler
import process_handler
import userdb


title_imgs = []
title_regex = re.compile(r"^title_.*\.png$")
def scan_titles():
    title_imgs = []
    for f in os.listdir(config.static_path):
        if title_regex.match(f):
            title_imgs.append(f)
    return title_imgs


def maybe_minified(module):
    if not config.get("use_minified", True):
        return "/static/" + module
    path = os.path.join(config.get("static_path"), module + ".min.js")
    if os.path.exists(path):
        return "/static/" + module + ".min"
    else:
        return "/static/" + module


class MainHandler(tornado.web.RequestHandler):
    def __init__(self, application, request, action):
        super(MainHandler, self).__init__(application, request)
        self.action = action

    def get(self, arg=None):
        self.render("client.html", title_img=random.choice(title_imgs),
                    username=None, config=config, action=self.action,
                    maybe_minified=maybe_minified,
                    use_cdn=config.get("use_cdn", True))


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
    pidfile = config.get("pidfile")
    uid = config.get("uid", 0)
    gid = config.get("gid", 0)
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
    os.chown(pidfile, uid, gid)


def remove_pidfile():
    pidfile = config.get("pidfile")
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
    if config.get("gid") is not None:
        os.setgid(config.get("gid"))
    if config.get("uid") is not None:
        os.setuid(config.get("uid"))


def signal_handler(signum, frame):
    logging.info("Received signal %i, shutting down.", signum)
    shutdown()
    if len(sockets) == 0:
        ioloop.stop()


def usr1_handler(signum, frame):
    logging.info("Received USR1, reloading config.")
    try:
        config.load()
        config.init_games()
    except ValueError:
        logging.error("Error in config file", exc_info=True)
    global title_imgs
    title_imgs = scan_titles()


def usr2_handler(signum, frame):
    logging.info("Received USR2, reloading player title data.")
    config.load_player_titles()


def purge_login_tokens_timeout():
    userdb.purge_login_tokens()
    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.add_timeout(time.time() + 60 * 60 * 24,
                       purge_login_tokens_timeout)


def bind_server():
    settings = {
        "static_path": config.static_path,
        "template_loader": DynamicTemplateLoader.get(config.template_path)
        }

    if config.get("no_cache"):
        settings["static_handler_class"] = NoCacheHandler

    application = tornado.web.Application([
        (r"/", MainHandler, {"action": "lobby"}),
        (r"/play/(.*)", MainHandler, {"action": "play"}),
        (r"/watch/(.*)", MainHandler, {"action": "watch"}),
        (r"/socket", CrawlWebSocket),
        (r"/gamedata/(.*)/(.*)", GameDataHandler)
        ], gzip=True, **settings)

    kwargs = {}
    if config.get("http_connection_timeout") is not None:
        kwargs["connection_timeout"] = config.get("http_connection_timeout")

    servers = []

    if config.get("binds"):
        server = tornado.httpserver.HTTPServer(application, **kwargs)
        for bind in config.binds:
            server.listen(bind["port"], bind["address"])
            logging.debug("Listening on %s:%s" % (bind["address"], bind["port"]))
        servers.append(server)
    if config.get("ssl_options"):
        # TODO: allow different ssl_options per bind pair
        server = tornado.httpserver.HTTPServer(application,
                                               ssl_options=config.ssl_options, **kwargs)
        for bind in config.get("ssl_binds", []):
            server.listen(bind["port"], bind["address"])
            logging.debug("Listening on %s:%s (SSL)" % (bind["address"], bind["port"]))
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

    if not logging_config.get("enable_access_log"):
        logging.getLogger('tornado.access').setLevel(logging.FATAL)


def check_config():
    success = True
    for game_data in config.get("games"):
        if not os.path.exists(game_data["crawl_binary"]):
            logging.error("Crawl executable %s doesn't exist!", game_data["crawl_binary"])
            success = False

        if ("client_path" in game_data and
                not os.path.exists(game_data["client_path"])):
            logging.error("Client data path %s doesn't exist!", game_data["client_path"])
            success = False

        if type(game_data.get("options", [])) is not list:
            logging.error("The options field should be a list!")
            success = False
        if type(game_data.get("pre_options", [])) is not list:
            logging.error("The pre_options field should be a list!")
            success = False

    if not os.path.isdir(config.static_path):
        logging.error("static_path doesn't exist on the filesystem (%s)." % config.static_path)
        success = False
    if os.path.isdir(config.static_path) and not scan_titles():
        logging.error("No title images (title_*.png) found in static_path (%s)." % config.static_path)
        success = False

    if config.get('dgl_status_file') and not os.path.isfile(config.dgl_status_file):
        try:
            f = open(config.dgl_status_file, 'w')
            f.close()
        except (OSError, IOError) as e:
            logging.error("dgl_status_file (%s) doesn't exist and couldn't create (%s)" %
                    (config.dgl_status_file, e))
            success = False
        else:
            logging.warning("Created dgl_status_file (%s)" % config.dgl_status_file)

    if not os.path.isfile(config.get('password_db')):
        if os.path.isdir(os.path.dirname(config.password_db)):
            logging.warning("password_db doesn't exist (%s), will create it" % config.password_db)
        else:
            logging.error("Can't create password_db, parent directory doesn't exist (%s)" % config.password_db)
            success = False

    init_prog = config.get('init_player_program')
    if init_prog and not os.access(config.init_player_program, os.X_OK):
        logging.error("init_player_program (%s) is not executable" % init_prog)
        success = False

    return success


if __name__ == "__main__":
    if config.get("chroot"):
        os.chroot(config.chroot)

    init_logging(config.logging_config)

    if not check_config():
        err_exit("Errors in config. Exiting.")

    if config.get("daemon"):
        daemonize()

    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)
    signal.signal(signal.SIGUSR1, usr1_handler)
    signal.signal(signal.SIGUSR2, usr2_handler)

    if config.get("umask") is not None:
        os.umask(config.umask)

    write_pidfile()

    servers = bind_server()

    shed_privileges()

    title_imgs = scan_titles()

    userdb.ensure_user_db_exists()

    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.set_blocking_log_threshold(0.5)

    status_file_timeout()
    purge_login_tokens_timeout()
    start_reading_milestones()

    if config.get("watch_socket_dirs"):
        process_handler.watch_socket_dirs()

    logging.info("Webtiles server started! (PID: %s)" % os.getpid())

    try:
        ioloop.start()
    except KeyboardInterrupt:
        logging.info("Received keyboard interrupt, shutting down.")
        for server in servers:
            server.stop()
        shutdown()
        ioloop.add_timeout(time.time() + 2, ioloop.stop)
        try:
            ioloop.start()  # We'll wait until all crawl processes have ended.
        except RuntimeError as e:
            if str(e) == 'IOLoop is already running':
                pass
            else:
                raise

    remove_pidfile()
