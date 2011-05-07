import os, os.path
import subprocess

import tornado.httpserver
import tornado.websocket
import tornado.ioloop
import tornado.web
import tornado.escape

import crypt
import sqlite3

import logging
import sys
import signal
import time, datetime
import collections

from config import *

logging.basicConfig(**logging_config)

class TornadoFilter(logging.Filter):
    def filter(self, record):
        if record.module == "web": return False
        return True
logging.getLogger().addFilter(TornadoFilter())

def user_passwd_match(username, passwd): # Returns the correctly cased username.
    crypted_pw = crypt.crypt(passwd, passwd)

    conn = sqlite3.connect(password_db)
    c = conn.cursor()
    c.execute("select username,password from dglusers where username=? collate nocase",
              (username,))
    result = c.fetchone()
    c.close()
    conn.close()

    if result is None or crypted_pw != result[1]:
        return None
    else:
        return result[0]

sockets = set()
current_id = 0
shutting_down = False

Game = collections.namedtuple("Game", ["id", "name"])

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        host = self.request.host
        if self.request.protocol == "https":
            protocol = "wss://"
        else:
            protocol = "ws://"
        self.render("client.html", socket_server = protocol + host + "/socket")

class CrawlWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, app, req, **kwargs):
        tornado.websocket.WebSocketHandler.__init__(self, app, req, **kwargs)
        self.username = None
        self.p = None
        self.timeout = None
        self.last_action_time = time.time()
        self.watched_game = None
        self.watchers = set()
        self.ttyrec_filename = None
        self.where = None
        self.wheretime = time.time()

        self.ioloop = tornado.ioloop.IOLoop.instance()

        global current_id
        self.id = current_id
        current_id += 1

    def __hash__(self):
        return self.id

    def __eq__(self, other):
        return self.id == other.id

    def open(self):
        logging.info("Socket opened from ip %s.", self.request.remote_ip)
        global sockets
        sockets.add(self)

        self.reset_timeout()

        if max_connections < len(sockets):
            self.write_message("$('#crt').html('The maximum number of connections has been"
                               + " reached, sorry :('); $('#login').hide();");
            self.close()
        elif shutting_down:
            self.close()
        else:
            self.update_lobby()

    def idle_time(self):
        return time.time() - self.last_action_time

    def is_running(self):
        return self.p is not None

    def is_in_lobby(self):
        return not self.is_running() and self.watched_game is None

    def update_lobby(self):
        running_games = [game for game in sockets if game.is_running()]
        lobby_html = self.render_string("lobby.html", running_games = running_games)
        self.write_message("$('#lobby_body').html(" +
                           tornado.escape.json_encode(lobby_html) + ");")

    def send_game_links(self):
        game_choices = [
            Game("webtiles-0.8", "DCSS 0.8")
        ]
        play_html = self.render_string("game_links.html", game_choices = game_choices)
        self.write_message("$('#play_now').html(" +
                           tornado.escape.json_encode(play_html) + ");")

    def reset_timeout(self):
        if self.timeout:
            self.ioloop.remove_timeout(self.timeout)

        self.received_pong = False
        self.write_message("ping();")
        self.timeout = self.ioloop.add_timeout(time.time() + connection_timeout,
                                               self.check_connection)

    def check_connection(self):
        self.timeout = None

        if not self.received_pong:
            logging.info("Connection to remote ip %s timed out.", self.request.remote_ip)
            self.close()

        if not self.client_terminated:
            self.reset_timeout()

    def start_crawl(self, game_id):
        if self.username == None: return

        self.game_id = game_id

        self.p = subprocess.Popen([crawl_binary,
                                   "-name", self.username,
                                   "-rc", os.path.join(rcfile_path, self.username + ".rc"),
                                   "-macro", os.path.join(macro_path, self.username + ".macro"),
                                   "-morgue", os.path.join(morgue_path, self.username)],
                                  stdin = subprocess.PIPE,
                                  stdout = subprocess.PIPE,
                                  stderr = subprocess.PIPE)

        self.ioloop.add_handler(self.p.stdout.fileno(), self.on_stdout,
                                self.ioloop.READ | self.ioloop.ERROR)
        self.ioloop.add_handler(self.p.stderr.fileno(), self.on_stderr,
                                self.ioloop.READ | self.ioloop.ERROR)

        self.create_mock_ttyrec()

        update_global_status()

    def create_mock_ttyrec(self):
        now = datetime.datetime.utcnow()
        self.ttyrec_filename = os.path.join(running_game_path,
                                            self.username + now.strftime("%Y-%m-%d.%H-%M-%S")
                                            + ".ttyrec")
        f = open(self.ttyrec_filename, "w")
        f.close()

    def delete_mock_ttyrec(self):
        if self.ttyrec_filename:
            os.remove(self.ttyrec_filename)
            self.ttyrec_filename = None

    def check_where(self):
        wherefile = os.path.join(morgue_path, self.username, self.username + ".dglwhere")
        try:
            stat = os.stat(wherefile)
            if stat.st_mtime > self.wheretime:
                self.wheretime = time.time()
                f = open(wherefile, "r")
                _, _, newwhere = f.readline().partition("|")
                f.close()

                if self.where != newwhere:
                    self.where = newwhere
                    update_global_status()
        except (OSError, IOError):
            pass

    def add_watcher(self, watcher):
        self.watchers.add(watcher)
        self.p.stdin.write("^r") # Redraw

    def remove_watcher(self, watcher):
        self.watchers.remove(watcher)

    def stop_watching(self):
        if self.watched_game:
            self.watched_game.remove_watcher(self)
            self.watched_game = None
            self.write_message("set_watching(false);")
            self.write_message("set_layer('lobby');")
            self.update_lobby();

    def shutdown(self, msg):
        if not self.client_terminated:
            self.write_message("connection_closed(" +
                               tornado.escape.json_encode(msg) + ");");
            self.close()
        if self.p is not None:
            self.p.send_signal(subprocess.signal.SIGHUP)

    def poll_crawl(self):
        if self.p is not None and self.p.poll() is not None:
            self.ioloop.remove_handler(self.p.stdout.fileno())
            self.ioloop.remove_handler(self.p.stderr.fileno())
            self.p.stdout.close()
            self.p.stderr.close()
            self.p = None

            self.delete_mock_ttyrec()
            self.where = None

            if self.client_terminated:
                global sockets
                sockets.remove(self)
            else:
                if shutting_down:
                    self.close()
                else:
                    # Go back to lobby
                    self.write_message("crawl_ended();")
                    self.update_lobby()

            update_global_status()

            for watcher in list(self.watchers):
                watcher.stop_watching()

            if shutting_down and len(sockets) == 0:
                # The last crawl process has ended, now we can go
                self.ioloop.stop()

    def on_message(self, message):
        self.last_action_time = time.time()

        login_start = "Login: "
        if message.startswith(login_start):
            message = message[len(login_start):]
            username, _, password = message.partition(' ')
            real_username = user_passwd_match(username, password)
            if real_username:
                logging.info("User %s logged in from ip %s.",
                             username, self.request.remote_ip)
                self.username = real_username
                self.write_message("logged_in(" +
                                   tornado.escape.json_encode(real_username) + ");")
                self.send_game_links()
            else:
                logging.warn("Failed login for user %s from ip %s.",
                             username, self.request.remote_ip)
                self.write_message("login_failed();")

        elif message.startswith("Play: "):
            if self.p or self.watched_game: return
            game_id = message[len("Play: "):]
            self.start_crawl(game_id)

        elif message == "Pong":
            self.received_pong = True

        elif message == "UpdateLobby":
            self.update_lobby()

        elif message.startswith("Watch: "):
            if self.p or self.watched_game: return
            watch_username = message[len("Watch: "):]
            sockets = [socket for socket in find_user_sockets(watch_username)
                       if socket.is_running()]
            if len(sockets) >= 1:
                socket = sockets[0]
                logging.info("User %s (ip: %s) started watching %s.",
                             self.username, self.request.remote_ip, socket.username)
                self.watched_game = socket
                socket.add_watcher(self)
                self.write_message("set_watching(true);")
            else:
                self.write_message("set_layer('lobby');")

        elif message == "GoLobby":
            if self.is_running():
                self.p.send_signal(subprocess.signal.SIGHUP)
            elif self.watched_game:
                self.stop_watching()

        elif self.p is not None:
            logging.debug("Message: %s (user: %s)", message, self.username)
            self.poll_crawl()
            if self.p is not None:
                self.p.stdin.write(message.encode("utf8"))

    def on_close(self):
        global sockets
        if self.p is None and self in sockets:
            sockets.remove(self)
            if shutting_down and len(sockets) == 0:
                # The last socket has been closed, now we can go
                self.ioloop.stop()
        elif self.p:
            self.p.send_signal(subprocess.signal.SIGHUP)

        if self.watched_game:
            self.watched_game.remove_watcher(self)

        if self.timeout:
            self.ioloop.remove_timeout(self.timeout)

        logging.info("Socket for ip %s closed.", self.request.remote_ip)

    def on_stderr(self, fd, events):
        if events & self.ioloop.ERROR:
            self.poll_crawl()
        elif events & self.ioloop.READ:
            s = self.p.stderr.readline()

            if self.client_terminated:
                return

            if not (s.isspace() or s == ""):
                logging.info("ERR: %s from %s: %s",
                             self.username, self.request.remote_ip, s.strip())

            self.poll_crawl()

    def on_stdout(self, fd, events):
        if events & self.ioloop.ERROR:
            self.poll_crawl()
        elif events & self.ioloop.READ:
            msg = self.p.stdout.readline()

            if self.client_terminated:
                return

            self.write_message(msg)
            for watcher in self.watchers:
                watcher.write_message(msg)

            self.poll_crawl()
            self.check_where()

def update_global_status():
    update_all_lobbys()
    write_dgl_status_file()

def update_all_lobbys():
    for socket in list(sockets):
        if socket.is_in_lobby():
            socket.update_lobby()

def write_dgl_status_file():
    try:
        f = open(dgl_status_file, "w")
        for socket in list(sockets):
            if socket.username and socket.is_running():
                f.write(socket.username + "#" +
                        socket.game_id + "#" +
                        (socket.where or "") + "#" +
                        "Tiles" + "#" +
                        str(int(socket.idle_time())) + "#" +
                        str(len(socket.watchers)) + "#")
        f.close()
    except (OSError, IOError) as e:
        logging.warn("Could not write dgl status file: %s", e)

def find_user_sockets(username):
    for socket in list(sockets):
        if socket.username and socket.username.lower() == username.lower():
            yield socket

def shutdown(msg = "The server is shutting down. Your game has been saved."):
    global shutting_down
    shutting_down = True
    for socket in list(sockets):
        socket.shutdown(msg)

def handler(signum, frame):
    shutdown()
    if len(sockets) == 0:
        ioloop.stop()

signal.signal(signal.SIGTERM, handler)
signal.signal(signal.SIGHUP, handler)

settings = {
    "static_path": static_path,
    "template_path": template_path
}

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/socket", CrawlWebSocket),
], **settings)

application.listen(bind_port, bind_address)
if ssl_options:
    application.listen(ssl_port, ssl_address, ssl_options = ssl_options)

ioloop = tornado.ioloop.IOLoop.instance()

try:
    ioloop.start()
except KeyboardInterrupt:
    shutdown()
    if len(sockets) > 0:
        ioloop.start() # We'll wait until all crawl processes have ended.
