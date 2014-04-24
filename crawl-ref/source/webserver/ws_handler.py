from tornado.escape import json_encode, json_decode, utf8, url_unescape
import tornado.websocket
import tornado.ioloop
import tornado.template

import os
import subprocess
import logging
import signal
import time, datetime
import codecs
import random
import zlib

from conf import config
import checkoutput, userdb
from util import *

sockets = set()
current_id = 0

def shutdown():
    for socket in list(sockets):
        socket.shutdown()
    import process_handler
    for process in process_handler.processes.values():
        process.stop()

def update_global_status():
    write_dgl_status_file()

def update_all_lobbys(game):
    lobby_entry = game.lobby_entry()
    for socket in list(sockets):
        if socket.is_in_lobby():
            socket.send_message("lobby", entries=[lobby_entry])
def remove_in_lobbys(process):
    for socket in list(sockets):
        if socket.is_in_lobby():
            socket.send_message("lobby", remove=process.id)


def write_dgl_status_file():
    f = None
    try:
        f = open(config.dgl_status_file, "w")
        for socket in list(sockets):
            if socket.username and socket.is_running():
                f.write("%s#%s#%s#0x0#%s#%s#\n" %
                        (socket.username, socket.game_id,
                         (socket.process.human_readable_where()),
                         str(socket.process.idle_time()),
                         str(socket.process.watcher_count())))
    except (OSError, IOError) as e:
        logging.warning("Could not write dgl status file: %s", e)
    finally:
        if f: f.close()

def status_file_timeout():
    write_dgl_status_file()
    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.add_timeout(time.time() + config.status_file_update_rate,
                       status_file_timeout)

def find_user_sockets(username):
    for socket in list(sockets):
        if socket.username and socket.username.lower() == username.lower():
            yield socket

def find_running_game(charname, start):
    from process_handler import processes
    for process in processes.values():
        if (process.where.get("name") == charname and
            process.where.get("start") == start):
            return process
    return None

milestone_file_tailers = []
def start_reading_milestones():
    files = config.milestone_files

    for f in files:
        milestone_file_tailers.append(FileTailer(f, handle_new_milestone))

def handle_new_milestone(line):
    data = parse_where_data(line)
    if "name" not in data: return
    game = find_running_game(data.get("name"), data.get("start"))
    if game: game.log_milestone(data)

def find_player_savegames(username):
    used_morgues = set()
    for game in config.get("games"):
        morgue_path = dgl_format_str(game["morgue_path"],
                                     username, game)
        if morgue_path in used_morgues:
            continue
        else:
            used_morgues.add(morgue_path)
        filename = os.path.join(morgue_path, username + ".where")
        try:
            with open(filename, "r") as f:
                data = f.read()
        except IOError:
            continue
        if data.strip() == "": continue

        try:
            info = parse_where_data(data)
        except:
            logging.warning("Exception while trying to parse where file!",
                            exc_info=True)
        else:
            if info.get("status") in ("active", "saved"):
                info["id"] = game["id"]
                info["game"] = game
                yield info

class CrawlWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, app, req, **kwargs):
        tornado.websocket.WebSocketHandler.__init__(self, app, req, **kwargs)
        self.username = None
        self.sid = None
        self.timeout = None
        self.watched_game = None
        self.process = None
        self.game_id = None
        self.received_pong = None

        self.ioloop = tornado.ioloop.IOLoop.instance()

        global current_id
        self.id = current_id
        current_id += 1

        self.deflate = True
        self._compressobj = zlib.compressobj(zlib.Z_DEFAULT_COMPRESSION,
                                             zlib.DEFLATED,
                                             -zlib.MAX_WBITS)
        self.total_message_bytes = 0
        self.compressed_bytes_sent = 0
        self.uncompressed_bytes_sent = 0
        self.message_queue = []

        self.subprotocol = None

        self.logger = logging.LoggerAdapter(logging.getLogger(), {})
        self.logger.process = self._process_log_msg

        self.message_handlers = {
            "login": self.login,
            "token_login": self.token_login,
            "logout": self.logout,
            "play": self.start_crawl,
            "pong": self.pong,
            "watch": self.watch,
            "chat_msg": self.post_chat_message,
            "register": self.register,
            "lobby": self.send_lobby,
            "get_rc": self.get_rc,
            "set_rc": self.set_rc,
            }

    client_closed = property(lambda self: (not self.ws_connection) or self.ws_connection.client_terminated)

    def _process_log_msg(self, msg, kwargs):
        return "#%-5s %s" % (self.id, msg), kwargs

    def __hash__(self):
        return self.id

    def __eq__(self, other):
        return self.id == other.id

    def allow_draft76(self):
        return True

    def select_subprotocol(self, subprotocols):
        if "no-compression" in subprotocols:
            self.deflate = False
            self.subprotocol = "no-compression"
            return "no-compression"
        return None

    def open(self):
        compression = "on"
        if isinstance(self.ws_connection, getattr(tornado.websocket, "WebSocketProtocol76", ())):
            # Old Websocket versions don't support binary messages
            self.deflate = False
            compression = "off, old websockets"
        elif self.subprotocol == "no-compression":
            compression = "off, client request"
        if hasattr(self, "get_extensions"):
            if any(s.endswith("deflate-frame") for s in self.get_extensions()):
                self.deflate = False
                compression = "deflate-frame extension"

        self.logger.info("Socket opened from ip %s (fd%s, compression: %s). UA: %s.",
                         self.request.remote_ip,
                         self.request.connection.stream.socket.fileno(),
                         compression, self.request.headers.get("User-Agent"))
        sockets.add(self)

        self.reset_timeout()

        if config.max_connections < len(sockets):
            self.send_message("close", reason="The maximum number of"
                              + "connections has been reached, sorry :(")
            self.close()
        else:
            host = self.request.protocol + "://" + self.request.host
            # don't allow cookie authentication from cross-site requests
            if self.request.headers.get("Origin") != host:
                return

            if config.dgl_mode:
                if self.get_cookie("sid"):
                    # short-lived session
                    sid = url_unescape(self.get_cookie("sid"))
                    session = userdb.session_info(sid)
                    if session and session.get("username"):
                        self.logger.info("Session user: %s.",
                                         session["username"])
                        self.do_login(session["username"], sid=sid)
                if self.username is None and self.get_cookie("login"):
                    # long-lived saved login
                    self.token_login(url_unescape(self.get_cookie("login")))
                elif config.get("autologin"):
                    self.do_login(config.autologin)
            else:
                self.start_crawl(None)

    def idle_time(self):
        return self.process.idle_time()

    def is_running(self):
        return self.process is not None

    def is_in_lobby(self):
        return not self.is_running() and self.watched_game is None

    def send_lobby(self):
        from process_handler import processes
        data = [p.lobby_entry() for p in processes.values()]
        banner_html = self.render_string("banner.html", username=self.username)
        footer_html = self.render_string("footer.html", username=self.username)
        self.queue_message("lobby_html", banner=banner_html, footer=footer_html)
        self.send_message("lobby", entries=data)

    def send_game_links(self):
        if config.get("list_savegames"):
            saved_games = list(find_player_savegames(self.username))
        else:
            saved_games = []
        # Rerender Banner and Footer
        banner_html = self.render_string("banner.html", username=self.username)
        footer_html = self.render_string("footer.html", username=self.username)
        self.queue_message("lobby_html", banner=banner_html, footer=footer_html)
        play_html = self.render_string("game_links.html", games=config.get("games"), saved_games=saved_games)
        self.send_message("set_game_links", content=play_html)

    def reset_timeout(self):
        if self.timeout:
            self.ioloop.remove_timeout(self.timeout)

        self.received_pong = False
        self.send_message("ping")
        self.timeout = self.ioloop.add_timeout(time.time() + config.connection_timeout,
                                               self.check_connection)

    def check_connection(self):
        self.timeout = None

        if not self.received_pong:
            self.logger.info("Connection timed out.")
            self.close()
        else:
            if self.is_running() and self.process.idle_time() > config.max_idle_time:
                self.logger.info("Stopping crawl after idle time limit.")
                self.stop_playing()

        if not self.client_closed:
            self.reset_timeout()

    def start_crawl(self, game_id):
        if config.dgl_mode and game_id not in config.games:
            self.go_lobby()
            return

        if config.dgl_mode:
            game_params = dict(config.games[game_id])
            if self.username == None:
                if self.watched_game:
                    self.stop_watching()
                self.send_message("login_required", game = game_params["name"])
                return

        if self.process:
            # ignore multiple requests for the same game, can happen when
            # logging in with cookies
            if self.game_id != game_id:
                self.go_lobby()
            return

        self.game_id = game_id

        import process_handler

        for process in process_handler.processes.values():
            if (process.username == self.username and
                process.game_params["id"] == game_id and
                process.process):
                self.process = process
                self.process.add_watcher(self)
                self.send_message("game_started")
                return

        if config.dgl_mode:
            args = (game_params, self.username, self.logger, self.ioloop)
            if (game_params.get("compat_mode") or
                "client_prefix" in game_params):
                self.process = process_handler.CompatCrawlProcessHandler(*args)
            else:
                self.process = process_handler.CrawlProcessHandler(*args)
        else:
            self.process = process_handler.DGLLessCrawlProcessHandler(self.logger, self.ioloop)

        self.process.add_watcher(self)
        try:
            self.process.start()
        except Exception:
            self.logger.warning("Exception starting process!", exc_info=True)
            self.process = None
            self.go_lobby()
        else:
            if self.process is None: # Can happen if the process creation fails
                self.go_lobby()
                return

            self.send_message("game_started")

            if config.dgl_mode:
                if self.process.where == {}:
                    # If location info was found, the lobbys were already
                    # updated by set_where_data
                    update_all_lobbys(self.process)
                update_global_status()

    def stop_playing(self):
        self.process.remove_watcher(self)
        self.process = None

    def crawl_ended(self):
        if self.is_running():
            reason = self.process.exit_reason
            message = self.process.exit_message
            dump_url = self.process.exit_dump_url
            self.process = None
        else:
            reason = self.watched_game.exit_reason
            message = self.watched_game.exit_message
            dump_url = self.watched_game.exit_dump_url
            self.watched_game = None

        if self.client_closed:
            sockets.remove(self)
        else:
            # Go back to lobby
            self.send_message("game_ended", reason = reason,
                              message = message, dump = dump_url)
            if not config.dgl_mode:
                self.start_crawl(None)

        if config.dgl_mode:
            update_global_status()

    def init_user(self):
        with open("/dev/null", "w") as f:
            result = subprocess.call([config.init_player_program, self.username],
                                     stdout = f, stderr = subprocess.STDOUT)
            return result == 0

    def stop_watching(self):
        if self.watched_game:
            self.logger.info("Stopped watching %s.", self.watched_game.username)
            self.watched_game.remove_watcher(self)
            self.watched_game = None

    def shutdown(self):
        if not self.client_closed:
            msg = self.render_string("shutdown.html", game=self)
            self.send_message("close", reason = msg)
            self.close()

    def do_login(self, username, sid=None):
        if not sid:
            sid, session = userdb.new_session()
            session["username"] = username
        self.sid = sid
        userdb.renew_session(sid)
        self.username = username
        self.logger.extra["username"] = username
        if not self.init_user():
            msg = ("Could not initialize your rc and morgue!<br>" +
                   "This probably means there is something wrong " +
                   "with the server configuration.")
            self.send_message("close", reason = msg)
            self.logger.warning("User initialization returned an error for user %s!",
                                self.username)
            self.username = None
            self.close()
            return
        self.queue_message("login_success", username=username, sid=sid)
        if self.watched_game:
            self.watched_game.update_watcher_description()
        else:
            self.send_game_links()

    def login(self, username, password, rememberme):
        real_username = userdb.user_passwd_match(username, password)
        if real_username:
            self.logger.info("User %s logged in.", real_username)
            self.do_login(real_username)
            if rememberme:
                cookie, expires = userdb.get_login_cookie(real_username)
                self.send_message("login_cookie", cookie=cookie,
                                  expires=expires+time.timezone)
        else:
            self.logger.warning("Failed login for user %s.", username)
            self.send_message("login_fail")

    def token_login(self, cookie):
        username, (new_cookie, expires) = userdb.token_login(cookie, logger=self.logger)
        if username:
            self.do_login(username)
            self.send_message("login_cookie", cookie=new_cookie,
                              expires=expires+time.timezone)
        else:
            self.send_message("login_fail")

    def logout(self, cookie):
        if cookie:
            userdb.forget_login_cookie(cookie)
        if self.sid:
            userdb.delete_session(self.sid)

    def pong(self):
        self.received_pong = True

    def rcfile_path(self, game_id):
        if game_id not in config.games: return None
        if not self.username: return None
        path = dgl_format_str(config.games[game_id]["rcfile_path"],
                                     self.username, config.games[game_id])
        return os.path.join(path, self.username + ".rc")

    def send_json_options(self, game_id, player_name):
        def do_send(data, returncode):
            if returncode != 0:
                # fail silently for returncode 1 for now, probably just an old
                # version missing the command line option
                if returncode != 1:
                    self.logger.warning("Error while getting JSON options!")
                return
            self.write_message('{"msg":"options","watcher":true,"options":'
                               + data + '}')

        if not self.username: return
        if game_id not in config.games: return

        game = config.games[game_id]
        if not "send_json_options" in game or not game["send_json_options"]:
            return

        call = [game["crawl_binary"]]

        if "pre_options" in game:
            call += game["pre_options"]

        call += ["-name", player_name,
                 "-rc", self.rcfile_path(game_id)]
        if "options" in game:
            call += game["options"]
        call.append("-print-webtiles-options")

        checkoutput.check_output(call, do_send, self.ioloop)

    def watch(self, username):
        if self.is_running():
            self.stop_playing()

        from process_handler import processes
        procs = [process for process in processes.values()
                 if process.username.lower() == username.lower()]
        if len(procs) >= 1:
            process = procs[0]
            if self.watched_game:
                if self.watched_game == process:
                    return
                self.stop_watching()
            self.logger.info("Started watching %s (P%s).", process.username,
                             process.id)
            self.watched_game = process
            process.add_watcher(self)
            self.send_message("watching_started", username = process.username)
        else:
            self.go_lobby()

    def post_chat_message(self, text):
        receiver = None
        if self.process:
            receiver = self.process
        elif self.watched_game:
            receiver = self.watched_game

        if receiver:
            if self.username is None:
                self.send_message("chat",
                                  text='You need to log in to send messages!')
                return

            receiver.handle_chat_message(self.username, text)

    def register(self, username, password, email):
        error = userdb.register_user(username, password, email)
        if error is None:
            self.logger.info("Registered user %s.", username)
            self.do_login(username)
        else:
            self.logger.info("Registration attempt failed for username %s: %s",
                             username, error)
            self.send_message("register_fail", reason = error)

    def go_lobby(self):
        if self.is_running():
            self.stop_playing()
        elif self.watched_game:
            self.stop_watching()
        self.send_message("go", path="/")

    def get_rc(self, game_id):
        if game_id not in config.games: return
        with open(self.rcfile_path(game_id), 'r') as f:
            contents = f.read()
        self.send_message("rcfile_contents", contents = contents)

    def set_rc(self, game_id, contents):
        rcfile_path = dgl_format_str(config.games[game_id]["rcfile_path"],
                                     self.username, config.games[game_id])
        rcfile_path = os.path.join(rcfile_path, self.username + ".rc")
        with open(rcfile_path, 'w') as f:
            f.write(contents.encode("utf8"))

    def on_message(self, message):
        if self.sid:
            userdb.renew_session(self.sid)
        if message.startswith("{"):
            try:
                obj = json_decode(message)
                if obj["msg"] in self.message_handlers:
                    handler = self.message_handlers[obj["msg"]]
                    del obj["msg"]
                    handler(**obj)
                elif self.process:
                    self.process.handle_input(message)
                elif not self.watched_game:
                    self.logger.warning("Didn't know how to handle msg: %s",
                                        obj["msg"])
            except Exception:
                self.logger.warning("Error while handling JSON message!",
                                    exc_info=True)

        elif self.process:
            # This is just for compatibility with 0.9, 0.10 only sends
            # JSON
            self.process.handle_input(message)

    def flush_messages(self):
        if self.client_closed or len(self.message_queue) == 0:
            return
        msg = "{\"msgs\":[" + ",".join(self.message_queue) + "]}"
        self.message_queue = []

        try:
            self.total_message_bytes += len(msg)
            if self.deflate:
                # Compress like in deflate-frame extension:
                # Apply deflate, flush, then remove the 00 00 FF FF
                # at the end
                compressed = self._compressobj.compress(msg)
                compressed += self._compressobj.flush(zlib.Z_SYNC_FLUSH)
                compressed = compressed[:-4]
                self.compressed_bytes_sent += len(compressed)
                super(CrawlWebSocket, self).write_message(compressed, binary=True)
            else:
                self.uncompressed_bytes_sent += len(msg)
                super(CrawlWebSocket, self).write_message(msg)
        except:
            self.logger.warning("Exception trying to send message.", exc_info = True)
            if self.ws_connection != None:
                self.ws_connection._abort()

    def write_message(self, msg, send=True):
        if self.client_closed: return
        self.message_queue.append(utf8(msg))
        if send:
            self.flush_messages()

    def send_message(self, msg, **data):
        """Sends a JSON message to the client."""
        data["msg"] = msg
        if not self.client_closed:
            self.write_message(json_encode(data))

    def queue_message(self, msg, **data):
        data["msg"] = msg
        if not self.client_closed:
            self.write_message(json_encode(data), False)

    def on_close(self):
        if self in sockets:
            sockets.remove(self)

        if self.is_running():
            self.stop_playing()
        elif self.watched_game:
            self.watched_game.remove_watcher(self)
            self.watched_game = None

        if self.timeout:
            self.ioloop.remove_timeout(self.timeout)

        if self.total_message_bytes == 0:
            comp_ratio = "N/A"
        else:
            comp_ratio = 100 - 100 * (self.compressed_bytes_sent + self.uncompressed_bytes_sent) / self.total_message_bytes

        self.logger.info("Socket closed. (%s bytes sent, compression ratio %s%%)",
                         self.total_message_bytes, comp_ratio)
