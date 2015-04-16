#!/usr/bin/env python

import os
import errno
import sys
import random

import tornado.httpserver
import tornado.ioloop
import tornado.web
import tornado.template

import logging
from logging.handlers import RotatingFileHandler

from conf import config, ConfigError
import util
from ws_handler import *
from game_data_handler import GameDataHandler
from janitor_handler import JanitorHandler
from save_handler import SaveHandler
import process_handler
import userdb


def maybe_minified(module):
    if not config.get("use_minified", True):
        return "/static/" + module
    path = os.path.join(config.get("static_path"), module + ".min.js")
    if os.path.exists(path):
        return "/static/" + module + ".min"
    else:
        logging.warning("use_minified is True, but couldn't find {0}. Falling "
                        "back to non-minified javascript".format(path))
        config["use_minified"] = False
        return "/static/" + module

class MainHandler(tornado.web.RequestHandler):
    def __init__(self, application, request, action):
        super(MainHandler, self).__init__(application, request)
        self.action = action

    def get(self, arg=None):
        self.render("client.html",
                    title_img=random.choice(config.title_images),
                    username=None, action=self.action,
                    maybe_minified=maybe_minified,
                    use_cdn=config.get("use_cdn", True))

class NoCacheHandler(tornado.web.StaticFileHandler):
    def set_extra_headers(self, path):
        self.set_header("Cache-Control", "no-cache, no-store, must-revalidate")
        self.set_header("Pragma", "no-cache")
        self.set_header("Expires", "0")

def err_exit(errmsg):
    logging.error(errmsg)
    sys.exit()

def daemonize():
    try:
        pid = os.fork()
        if pid > 0:
            os._exit(0)
    except OSError, e:
        err_exit("Fork #1 failed! ({0})".format(e.strerror))

    os.setsid()

    try:
        pid = os.fork()
        if pid > 0:
            os._exit(0)
    except OSError, e:
        err_exit("Fork #2 failed! ({0})".format(e.strerror))

    with open("/dev/null", "rw") as f:
        os.dup2(f.fileno(), sys.stdin.fileno())
        os.dup2(f.fileno(), sys.stdout.fileno())
        os.dup2(f.fileno(), sys.stderr.fileno())

def write_pidfile():
    pidfile = config.get("pidfile")
    uid = config.get("uid", -1)
    gid = config.get("gid", -1)
    if not pidfile:
        return
    if os.path.exists(pidfile):
        try:
            with open(pidfile) as f:
                pid = int(f.read())
        except ValueError:
            err_exit("PIDfile {0} contains non-numeric value".format(pidfile))
        try:
            os.kill(pid, 0)
        except OSError, why:
            if why[0] == errno.ESRCH:
                # The pid doesn't exist.
                logging.warn("Removing stale pidfile {0}".format(pidfile))
                os.remove(pidfile)
            else:
                err_exit("Can't check status of PID {0} from pidfile {1}: "
                         "{2}".format(pid, pidfile, why[1]))
        else:
            err_exit("Another Webtiles server is running, PID {0}".format(pid))

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
    logging.info("Received signal {0}, shutting down.".format(signum))
    shutdown()
    if len(sockets) == 0:
        ioloop.stop()

def usr1_handler(signum, frame):
    logging.info("Received USR1, reloading config.")
    config.reload()

def usr2_handler(signum, frame):
    logging.info("Received USR2, reloading player titles.")
    config.reload_player_titles()

def purge_login_tokens_timeout():
    userdb.purge_login_tokens()
    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.add_timeout(time.time() + 60 * 60 * 24,
                       purge_login_tokens_timeout)

def bind_server():
    class ForbiddenHandler(tornado.web.RequestHandler):
        def get(self):
            raise tornado.web.HTTPError(403)

    settings = {
        "static_path": config["static_path"],
        "template_loader": util.DynamicTemplateLoader.get(config["template_path"])
    }

    if config.get("no_cache"):
        settings["static_handler_class"] = NoCacheHandler

    routes = [
        ("/", MainHandler, {"action": "lobby"}),
        ("/play/(.*)", MainHandler, {"action": "play"}),
        ("/watch/(.*)", MainHandler, {"action": "watch"}),
        ("/scores/(.*)", MainHandler, {"action": "scores"}),
        ("/socket", CrawlWebSocket),
        ("/gamedata/(.*)", GameDataHandler),
        ("/janitor/(.*)", JanitorHandler),
        ("/save/(.*)", SaveHandler)]

    application = tornado.web.Application(routes, gzip=True, **settings)

    kwargs = {}
    kwargs["idle_connection_timeout"] = config["http_connection_timeout"]

    servers = []

    if config.get("binds"):
        server = tornado.httpserver.HTTPServer(application, **kwargs)
        for bind in config["binds"]:
            server.listen(bind["port"], bind["address"])
            logging.debug("Listening on {0}:{1}".format(bind["address"],
                                                        bind["port"]))
        servers.append(server)
    if config.get("ssl_options"):
        # TODO: allow different ssl_options per bind pair
        server = tornado.httpserver.HTTPServer(application,
                                               ssl_options=config["ssl_options"],
                                               **kwargs)
        for bind in config.get("ssl_binds", []):
            server.listen(bind["port"], bind["address"])
            logging.debug("Listening on {0}:{1} (SSL)".format(bind["address"],
                                                              bind["port"]))
        servers.append(server)

    return servers

def make_dgl_status_file():
    dgl_file = config.get('dgl_status_file')
    if not dgl_file or os.path.isfile(dgl_file):
        return

    try:
        f = open(dgl_file, 'w')
        f.close()
    except EnvironmentError as e:
        err_exit("dgl_status_file ({0}) doesn't exist and couldn't create "
                 "({1})".format(dgl_file, e.strerror))
    else:
        logging.info("Created dgl_status_file "
                     "({0})".format(dgl_file))

def init_logging():
    try:
        config.check_logging()
    except ConfigError as e:
        err_exit(e.msg)

    log_conf = config["logging_config"]
    if log_conf.get("filename"):
        log_handler = RotatingFileHandler(log_conf["filename"],
                                          maxBytes=log_conf["max_bytes"],
                                          backupCount=log_conf["backup_count"])
    else:
        log_handler = logging.StreamHandler(None)
    log_handler.setFormatter(logging.Formatter(log_conf["format"],
                                               log_conf.get("datefmt")))
    logging.getLogger().addHandler(log_handler)
    if log_conf.get("level") is not None:
        logging.getLogger().setLevel(log_conf["level"])
    logging.getLogger().addFilter(util.TornadoFilter())
    logging.addLevelName(logging.DEBUG, "DEBG")
    logging.addLevelName(logging.WARNING, "WARN")

    if not log_conf.get("enable_access_log"):
        logging.getLogger("tornado.access").setLevel(logging.FATAL)
    if os.environ.get("WEBTILES_DEBUG"):
        logging.getLogger().setLevel(logging.DEBUG)


if __name__ == "__main__":
    if "chroot" in config:
        os.chroot(config["chroot"])

    init_logging()
    make_dgl_status_file()

    if config.get("daemon"):
        daemonize()

    if config.get("umask") is not None:
        os.umask(config["umask"])

    write_pidfile()
    servers = bind_server()
    shed_privileges()

    try:
        config.load()
    except ConfigError as e:
        err_exit(e.msg)

    if config.get("locale"):
        locale.setlocale(locale.LC_ALL, config["locale"])
    else:
        locale.setlocale(locale.LC_ALL, '')

    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)
    signal.signal(signal.SIGUSR1, usr1_handler)
    signal.signal(signal.SIGUSR2, usr2_handler)

    userdb.ensure_user_db_exists()

    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.set_blocking_log_threshold(0.5)

    status_file_timeout()
    purge_login_tokens_timeout()
    start_reading_milestones()

    if config.get("watch_socket_dirs"):
        process_handler.watch_socket_dirs()

    logging.info("Webtiles server started! (PID: {0})".format(os.getpid()))

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
