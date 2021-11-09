#!/usr/bin/env python
from __future__ import absolute_import

import argparse
import errno
import logging
import logging.handlers
import os
import os.path
import sys
import signal
import time

import tornado.httpserver
import tornado.ioloop
import tornado.template
import tornado.web
from tornado.ioloop import IOLoop

# TODO: refactor more of this file into webtiles?
import webtiles
from webtiles import auth, load_games, process_handler, userdb, config
from webtiles import game_data_handler, util, ws_handler

# load config values from the traditional module location
import config as server_config
config.init_config_from_module(server_config)

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        host = self.request.host
        if self.request.protocol == "https" or self.request.headers.get("x-forwarded-proto") == "https":
            protocol = "wss://"
        else:
            protocol = "ws://"

        recovery_token = None
        recovery_token_error = None

        recovery_token = self.get_argument("ResetToken",None)
        if recovery_token:
            recovery_token_error = userdb.find_recovery_token(recovery_token)[2]
            if recovery_token_error:
                logging.warning("Recovery token error from %s", self.request.remote_ip)

        self.render("client.html", socket_server = protocol + host + "/socket",
                    username = None,
                    config = config,
                    reset_token = recovery_token, reset_token_error = recovery_token_error)

class NoCacheHandler(tornado.web.StaticFileHandler):
    def set_extra_headers(self, path):
        self.set_header("Cache-Control", "no-cache, no-store, must-revalidate")
        self.set_header("Pragma", "no-cache")
        self.set_header("Expires", "0")

def err_exit(errmsg, exc_info=False):
    logging.error(errmsg, exc_info=exc_info)
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
    pidfile = config.get('pidfile')
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
        except OSError as why:
            if why.errno == errno.ESRCH:
                # The pid doesn't exist.
                logging.warn("Removing stale pidfile %s" % pidfile)
                os.remove(pidfile)
            else:
                err_exit("Can't check status of PID %s from pidfile %s: %s" %
                         (pid, pidfile, why.strerror))
        else:
            err_exit("Another Webtiles server is running, PID %s\n" % pid)

    with open(pidfile, "w") as f:
        f.write(str(os.getpid()))

def remove_pidfile():
    pidfile = config.get('pidfile')
    if not pidfile:
        return

    try:
        os.remove(pidfile)
    except OSError as e:
        if e.errno == errno.EACCES or e.errno == errno.EPERM:
            logging.warn("No permission to delete pidfile!")
        else:
            logging.error("Failed to delete pidfile!")
    except:
        logging.error("Failed to delete pidfile!")

def shed_privileges():
    if config.get('gid') is not None:
        os.setgid(config.get('gid'))
    if config.get('uid') is not None:
        os.setuid(config.get('uid'))

def stop_everything():
    for server in servers:
        server.stop()
    ws_handler.shutdown()
    # TODO: shouldn't this actually wait for everything to close??
    if len(ws_handler.sockets) == 0:
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
        "static_path": config.get('static_path'),
        "template_loader": util.DynamicTemplateLoader.get(config.get('template_path')),
        "debug": bool(config.get('development_mode', False)),
        }

    if config.get('no_cache', False):
        settings["static_handler_class"] = NoCacheHandler

    application = tornado.web.Application([
            (r"/", MainHandler),
            (r"/socket", ws_handler.CrawlWebSocket),
            (r"/gamedata/([0-9a-f]*\/.*)", game_data_handler.GameDataHandler)
            ], gzip=config.get('use_gzip',True), **settings)

    kwargs = {}
    if config.get('http_connection_timeout') is not None:
        kwargs["idle_connection_timeout"] = config.get('http_connection_timeout')

    # TODO: the logic looks odd here, as it is set to None in the default
    # config.py. But I'm not really sure how this is used so I don't want to
    # mess with it...
    if config.get('http_xheaders', False):
        kwargs["xheaders"] = config.get('http_xheaders')

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

    if config.get('bind_nonsecure'):
        server = server_wrap(**kwargs)

        if config.get('bind_pairs'):
            listens = config.get('bind_pairs')
        else:
            listens = ( (config.get('bind_address'), config.get('bind_port')), )
        for (addr, port) in listens:
            logging.info("Listening on %s:%d" % (addr, port))
            server.listen(port, addr)
        servers.append(server)

    if config.get('ssl_options'):
        # TODO: allow different ssl_options per bind pair
        server = server_wrap(ssl_options=config.get('ssl_options'), **kwargs)

        if config.get('ssl_bind_pairs'):
            listens = config.get('ssl_bind_pairs')
        else:
            listens = ( (config.get('ssl_address'), config.get('ssl_port')), )
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
        logging.getLogger().addFilter(util.TornadoFilter())
    logging.addLevelName(logging.DEBUG, "DEBG")
    logging.addLevelName(logging.WARNING, "WARN")


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


def usr1_handler(signum, frame):
    assert signum == signal.SIGUSR1
    logging.info("Received USR1, reloading config.")
    try:
        IOLoop.current().add_callback_from_signal(config.load_game_data)
    except AttributeError:
        # This is for compatibility with ancient versions < Tornado 3.
        try:
            config.load_game_data()
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
    parser.add_argument('--reset-password', type=str, help='A username to generate a password reset token for. Does not start the server.')
    parser.add_argument('--clear-reset-password', type=str, help='A username to clear password reset tokens for. Does not start the server.')
    result = parser.parse_args()
    if result.live_debug or result.reset_password or result.clear_reset_password:
        if not result.logfile:
            result.logfile = '-'
        if result.daemon is None:
            result.daemon = False
        result.pidfile = False
        config.set('live_debug', True)
    return result


# override config with any arguments supplied on the command line
def export_args_to_config(args):
    if args.port:
        config.set('bind_nonsecure', True)
        config.set('bind_address', "")  # TODO: ??
        config.set('bind_port', args.port)
        if config.get('bind_pairs') is not None:
            config.clear('bind_pairs')
        logging.info("Using command-line supplied port: %d", args.port)
        if config.get('ssl_options'):
            logging.info("    (Overrides config-specified SSL settings.)")
            config.set('ssl_options', None)
    if args.daemon is not None:
        logging.info("Command line override for daemonize: %r", args.daemon)
        config.set('daemon', args.daemon)
    if args.pidfile is not None:
        if not args.pidfile:
            if config.get('pidfile'):
                logging.info("Command line overrides config-specified PID file!")
            config.set('pidfile', None)

def reset_token_commands(args):
    if args.clear_reset_password:
        username = args.clear_reset_password
    else:
        username = args.reset_password

    try:
        config.validate()
    except:
        err_exit("Errors in config. Exiting.", exc_info=True)

    # duplicate some minimal setup needed for this to work
    config.get('logging_config').pop('filename', None)
    args.logfile = "<stdout>"  # make the log message easier to read

    init_logging(config.get('logging_config'))

    if config.get('dgl_mode'):
        userdb.ensure_user_db_exists()
        userdb.upgrade_user_db()
    userdb.ensure_settings_db_exists()
    user_info = userdb.get_user_info(username)
    if not user_info:
        err_exit("Reset/clear password failed; invalid user: %s" % username)

    # don't crash on the default config
    if config.get('lobby_url') is None:
        config.set('lobby_url', "[insert lobby url here]")

    if args.clear_reset_password:
        ok, msg = userdb.clear_password_token(username)
        if not ok:
            err_exit("Error clearing password reset token for %s: %s" % (username, msg))
        else:
            print("Password reset token cleared for account '%s'." % username)
    else:
        ok, msg = userdb.generate_forgot_password(username)
        if not ok:
            err_exit("Error generating password reset token for %s: %s" % (username, msg))
        else:
            if not user_info[1]:
                logging.warning("No email set for account '%s', use caution!" % username)
            print("Setting a password reset token on account '%s'." % username)
            print("Email: %s\nMessage body to send to user:\n%s\n" % (user_info[1], msg))


def server_main():
    args = parse_args()
    if config.get('chroot'):
        os.chroot(config.get('chroot'))

    ## TODO: is there a better way to handle this? Needed for games.d
    config.server_path = os.path.dirname(os.path.abspath(__file__))

    if args.reset_password or args.clear_reset_password:
        reset_token_commands(args)
        return

    if config.get('live_debug'):
        logging.info("Starting in live-debug mode.")
        config.set('watch_socket_dirs', False)

    # do this here so it can happen before logging init
    if args.logfile:
        if args.logfile == "-":
            config.get('logging_config').pop('filename', None)
            args.logfile = "<stdout>"  # make the log message easier to read
        else:
            config.get('logging_config')['filename'] = args.logfile

    init_logging(config.get('logging_config'))

    if args.logfile:
        logging.info("Using command-line supplied logfile: '%s'", args.logfile)

    export_args_to_config(args)

    try:
        config.load_game_data()
        config.validate()
    except:
        err_exit("Errors in config. Exiting.", exc_info=True)

    if config.get('daemon', False):
        daemonize()

    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)

    if config.get('umask') is not None:
        os.umask(config.get('umask'))

    write_pidfile()

    global servers
    servers = bind_server()
    ensure_tornado_current()

    shed_privileges()

    # is this ever set to False by anyone in practice?
    dgl_mode = config.get('dgl_mode', True)

    if dgl_mode:
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

    if dgl_mode:
        ws_handler.status_file_timeout()
        auth.purge_login_tokens_timeout()
        ws_handler.start_reading_milestones()

        if config.get('watch_socket_dirs', False):
            process_handler.watch_socket_dirs()

    # start the lobby update timeout loop
    ws_handler.do_periodic_lobby_updates()

    logging.info("DCSS Webtiles server started with Tornado %s! (PID: %s)" %
                                                (tornado.version, os.getpid()))

    IOLoop.current().start()

    logging.info("Bye!")
    remove_pidfile()


if __name__ == "__main__":
    server_main()
