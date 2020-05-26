#!/usr/bin/env python
from __future__ import absolute_import

import argparse
import errno
import logging
import logging.handlers
import os
import os.path
import sys

import tornado.httpserver
import tornado.ioloop
import tornado.template
import tornado.web
from tornado.ioloop import IOLoop

import auth
import load_games
import process_handler
import userdb
import config
from config import *  # TODO: remove
from game_data_handler import GameDataHandler
from util import *
from ws_handler import *


class MainHandler(tornado.web.RequestHandler):
    def get(self):
        host = self.request.host
        if self.request.protocol == "https" or self.request.headers.get("x-forwarded-proto") == "https":
            protocol = "wss://"
        else:
            protocol = "ws://"

        recovery_token = None
        recovery_token_error = None

        if getattr(config, "allow_password_reset", False):
            recovery_token = self.get_argument("ResetToken",None)
            if recovery_token:
                recovery_token_error = userdb.find_recovery_token(recovery_token)[2]

        self.render("client.html", socket_server = protocol + host + "/socket",
                    username = None, config = config,
                    reset_token = recovery_token, reset_token_error = recovery_token_error)

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
    except OSError as e:
        err_exit("Fork #1 failed! (%s)" % (e.strerror))

    os.setsid()

    try:
        pid = os.fork()
        if pid > 0:
            os._exit(0)
    except OSError as e:
        err_exit("Fork #2 failed! (%s)" % e.strerror)

    with open("/dev/null", "r") as f:
        os.dup2(f.fileno(), sys.stdin.fileno())
    with open("/dev/null", "w") as f:
        os.dup2(f.fileno(), sys.stdout.fileno())
        os.dup2(f.fileno(), sys.stderr.fileno())

def write_pidfile():
    if not getattr(config, 'pidfile', None):
        return
    if os.path.exists(config.pidfile):
        try:
            with open(config.pidfile) as f:
                pid = int(f.read())
        except ValueError:
            err_exit("PIDfile %s contains non-numeric value" % config.pidfile)
        try:
            os.kill(pid, 0)
        except OSError as why:
            if why.errno == errno.ESRCH:
                # The pid doesn't exist.
                logging.warn("Removing stale pidfile %s" % config.pidfile)
                os.remove(config.pidfile)
            else:
                err_exit("Can't check status of PID %s from pidfile %s: %s" %
                         (pid, config.pidfile, why.strerror))
        else:
            err_exit("Another Webtiles server is running, PID %s\n" % pid)

    with open(config.pidfile, "w") as f:
        f.write(str(os.getpid()))

def remove_pidfile():
    if not getattr(config, 'pidfile', None):
        return
    try:
        os.remove(config.pidfile)
    except OSError as e:
        if e.errno == errno.EACCES or e.errno == errno.EPERM:
            logging.warn("No permission to delete pidfile!")
        else:
            logging.error("Failed to delete pidfile!")
    except:
        logging.error("Failed to delete pidfile!")

def shed_privileges():
    if getattr(config, 'gid', None) is not None:
        os.setgid(config.gid)
    if getattr(config, 'uid', None) is not None:
        os.setuid(config.uid)

def stop_everything():
    for server in servers:
        server.stop()
    shutdown()
    # TODO: shouldn't this actually wait for everything to close??
    if len(sockets) == 0:
        IOLoop.current().stop()
    else:
        IOLoop.current().add_timeout(time.time() + 2, IOLoop.current().stop)

def signal_handler(signum, frame):
    logging.info("Received signal %i, shutting down.", signum)
    try:
        IOLoop.current().add_callback_from_signal(stop_everything)
    except AttributeError:
        # This is for compatibility with ancient versions < Tornado 3. It
        # probably won't shutdown correctly and is *definitely* incorrect for
        # modern versions of Tornado; but this is how it was done on the
        # original implementation of webtiles + Tornado 2.4 that was in use
        # through about 2020.
        stop_everything()

def bind_server():
    settings = {
        "static_path": config.static_path,
        "template_loader": DynamicTemplateLoader.get(config.template_path),
        "debug": bool(getattr(config, 'development_mode', False)),
        }

    if hasattr(config, "no_cache") and config.no_cache:
        settings["static_handler_class"] = NoCacheHandler

    application = tornado.web.Application([
            (r"/", MainHandler),
            (r"/socket", CrawlWebSocket),
            (r"/gamedata/([0-9a-f]*\/.*)", GameDataHandler)
            ], gzip=getattr(config,"use_gzip",True), **settings)

    kwargs = {}
    if getattr(config, 'http_connection_timeout', None) is not None:
        kwargs["idle_connection_timeout"] = config.http_connection_timeout

    # TODO: the logic looks odd here, as it is set to None in the default
    # config.py. But I'm not really sure how this is used so I don't want to
    # mess with it...
    if getattr(config, "http_xheaders", False):
        kwargs["xheaders"] = config.http_xheaders

    servers = []

    def server_wrap(**kwargs):
        try:
            return tornado.httpserver.HTTPServer(application, **kwargs)
        except TypeError:
            # Ugly backwards-compatibility hack. Removable once Tornado 3
            # is out of the picture (if ever)
            del kwargs["idle_connection_timeout"]
            server = tornado.httpserver.HTTPServer(application, **kwargs)
            logging.error(
                    "Server configuration sets `idle_connection_timeout` "
                    "but this is not available in your version of "
                    "Tornado. Please upgrade to at least Tornado 4 for "
                    "this to work.""")
            return server

    if config.bind_nonsecure:
        server = server_wrap(**kwargs)

        try:
            listens = config.bind_pairs
        except AttributeError:
            listens = ( (config.bind_address, config.bind_port), )
        for (addr, port) in listens:
            logging.info("Listening on %s:%d" % (addr, port))
            server.listen(port, addr)
        servers.append(server)

    if config.ssl_options:
        # TODO: allow different ssl_options per bind pair
        server = server_wrap(ssl_options=config.ssl_options, **kwargs)

        try:
            listens = config.ssl_bind_pairs
        except NameError:
            listens = ( (config.ssl_address, config.ssl_port), )
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
    if tornado.version_info[0] >= 3:
        # hide regular successful access messages, e.g. `200 GET` messages
        # messages. 404s are still shown. TODO: would there be demand for
        # sending this to its own file in a configurable way?
        logging.getLogger("tornado.access").setLevel(logging.WARNING)
    else:
        # the tornado version is ancient enough that it doesn't have its own
        # logging streams; this filter suppresses any logging done from the
        # `web` module at level INFO.
        logging.getLogger().addFilter(TornadoFilter())
    logging.addLevelName(logging.DEBUG, "DEBG")
    logging.addLevelName(logging.WARNING, "WARN")


def check_config():
    success = True
    for (game_id, game_data) in config.games.items():
        if not os.path.exists(game_data["crawl_binary"]):
            logging.warning("Crawl executable for %s (%s) doesn't exist!",
                            game_id, game_data["crawl_binary"])
            success = False

        if ("client_path" in game_data and
            not os.path.exists(game_data["client_path"])):
            logging.warning("Client data path %s doesn't exist!", game_data["client_path"])
            success = False

    load_games.collect_game_modes()

    if getattr(config, "allow_password_reset", False) and not config.lobby_url:
        logging.warning("Lobby URL needs to be defined!")
        success = False
    return success


def monkeypatch_tornado24():
    # extremely ugly compatibility hack, to ease transition for servers running
    # the ancient patched tornado 2.4.
    IOLoop.current = staticmethod(IOLoop.instance)


def ensure_tornado_current():
    try:
        tornado.ioloop.IOLoop.current()
    except AttributeError:
        monkeypatch_tornado24()
        tornado.ioloop.IOLoop.current()
        logging.error(
            "You are running a deprecated version of tornado; please update"
            " to at least version 4.")


def _do_load_games():
    if getattr(config, "use_game_yaml", False):
        config.games = load_games.load_games(config.games)


def usr1_handler(signum, frame):
    assert signum == signal.SIGUSR1
    logging.info("Received USR1, reloading config.")
    try:
        IOLoop.current().add_callback_from_signal(_do_load_games)
    except AttributeError:
        # This is for compatibility with ancient versions < Tornado 3.
        try:
            _do_load_games()
        except Exception:
            logging.exception("Failed to update games after USR1 signal.")
    except Exception:
        logging.exception("Failed to update games after USR1 signal.")


def parse_args():
    parser = argparse.ArgumentParser(
        description='Dungeon Crawl webtiles server',
        epilog='Command line options will override config settings.')
    parser.add_argument('-p', '--port', type=int, help='A port to bind; disables SSL.')
    # TODO: --ssl-port or something?
    parser.add_argument('--logfile',
                        help='A logfile to write to; use "-" for stdout.')
    parser.add_argument('--daemon', action='store_true', default=None,
                        help='Daemonize after start.')
    parser.add_argument('-n', '--no-daemon', action='store_false', default=None,
                        dest='daemon',
                        help='Do not daemonize after start.')
    parser.add_argument('--no-pidfile', dest='pidfile', action='store_false',
                        default=None, help='Do not use a PID-file.')

    # live debug mode is intended to be able to run (more or less) safely with
    # a concurrent real webtiles server. However, still be careful with this...
    parser.add_argument('--live-debug', action='store_true',
        help=('Debug mode for server admins. Will use a separate directory for sockets. '
              'Entails --no-pidfile, --no-daemon, --logfile -, watch_socket_dirs=False. '
              '(Further command line options can override these.)'))
    result = parser.parse_args()
    if result.live_debug:
        if not result.logfile:
            result.logfile = '-'
        if result.daemon is None:
            result.daemon = False
        result.pidfile = False
        config.live_debug = True
    return result


# override config with any arguments supplied on the command line
def export_args_to_config(args):
    if args.port:
        config.bind_nonsecure = True
        config.bind_address = ""  # TODO: ??
        config.bind_port = args.port
        if getattr(config, 'bind_pairs', None) is not None:
            del config.bind_pairs
        logging.info("Using command-line supplied port: %d", args.port)
        if getattr(config, 'ssl_options', None):
            logging.info("    (Overrides config-specified SSL settings.)")
            config.ssl_options = None
    if args.daemon is not None:
        logging.info("Command line override for daemonize: %r", args.daemon)
        config.daemon = args.daemon
    if args.pidfile is not None:
        if not args.pidfile:
            if getattr(config, 'pidfile', None):
                logging.info("Command line overrides config-specified PID file!")
            config.pidfile = None


def server_main():
    args = parse_args()
    if config.chroot:
        os.chroot(config.chroot)

    if getattr(config, 'live_debug', False):
        logging.info("Starting in live-debug mode.")
        config.watch_socket_dirs = False

    # do this here so it can happen before logging init
    if args.logfile:
        if args.logfile == "-":
            config.logging_config.pop('filename', None)
            args.logfile = "<stdout>"  # make the log message easier to read
        else:
            config.logging_config['filename'] = args.logfile

    init_logging(config.logging_config)

    if args.logfile:
        logging.info("Using command-line supplied logfile: '%s'", args.logfile)

    _do_load_games()

    export_args_to_config(args)

    if not check_config():
        err_exit("Errors in config. Exiting.")

    if config.daemon:
        daemonize()

    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)

    if getattr(config, 'umask', None) is not None:
        os.umask(config.umask)

    write_pidfile()

    global servers
    servers = bind_server()
    ensure_tornado_current()

    shed_privileges()

    if config.dgl_mode:
        userdb.ensure_user_db_exists()
        userdb.upgrade_user_db()
    userdb.ensure_settings_db_exists()

    signal.signal(signal.SIGUSR1, usr1_handler)

    try:
        IOLoop.current().set_blocking_log_threshold(0.5) # type: ignore
        logging.info("Blocking call timeout: 500ms.")
    except:
        # this is the new normal; still not sure of a way to deal with this.
        logging.info("Webserver running without a blocking call timeout.")

    if config.dgl_mode:
        status_file_timeout()
        auth.purge_login_tokens_timeout()
        start_reading_milestones()

        if config.watch_socket_dirs:
            process_handler.watch_socket_dirs()

    logging.info("DCSS Webtiles server started with Tornado %s! (PID: %s)" %
                                                (tornado.version, os.getpid()))

    IOLoop.current().start()

    logging.info("Bye!")
    remove_pidfile()


if __name__ == "__main__":
    server_main()
