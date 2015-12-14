import codecs
import datetime
import locale
import logging
import os
import random
import re
import signal
import time
import subprocess
import urlparse
import zlib

from tornado.escape import json_encode, json_decode, utf8, url_unescape
import tornado.websocket
import tornado.ioloop
import tornado.template

from conf import config
import checkoutput, userdb
import util

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
    if not config.get("dgl_status_file") \
       or not config.get("status_file_update_rate"):
        return
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
    if not config.get("dgl_status_file") \
       or not config.get("status_file_update_rate"):
        return
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
        milestone_file_tailers.append(util.FileTailer(f, handle_new_milestone))

def handle_new_milestone(line):
    data = util.parse_where_data(line)
    if "name" not in data: return
    game = find_running_game(data.get("name"), data.get("start"))
    if game: game.log_milestone(data)

def find_player_savegames(username):
    used_morgues = set()
    for game in config.get("games"):
        morgue_path = util.dgl_format_str(game["morgue_path"], username, game)
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
            info = util.parse_where_data(data)
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
        self.nerd = None
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
            "reset_rc": self.reset_rc,
            "change_password": self.change_password,
            "get_game_info": self.get_game_info,
            "get_scores": self.get_scores,
        }

    client_closed = property(lambda self: (not self.ws_connection) \
                             or self.ws_connection.client_terminated)

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
        if isinstance(self.ws_connection,
                      getattr(tornado.websocket, "WebSocketProtocol76", ())):
            # Old Websocket versions don't support binary messages
            self.deflate = False
            compression = "off, old websockets"
        elif self.subprotocol == "no-compression":
            compression = "off, client request"
        if hasattr(self, "get_extensions"):
            if any(s.endswith("deflate-frame") for s in self.get_extensions()):
                self.deflate = False
                compression = "deflate-frame extension"

        if config.get("logging_config", {}).get('enable_access_log'):
            msg = ("Socket opened from ip {0} (fd{1}, compression: {2}). "
                   "UA: {3}.".format(self.request.remote_ip,
                                     self.ws_connection.stream.socket.fileno(),
                                     compression,
                                     self.request.headers.get("User-Agent")))
            self.logger.info(msg)
        sockets.add(self)

        self.reset_timeout()

        if config.max_connections < len(sockets):
            self.send_message("close", reason="The maximum number of "
                              "connections has been reached, sorry :(")
            self.close()
        else:
            # don't allow cookie authentication from cross-site requests
            # (protocol mismatch is fine)
            header = self.request.headers.get("Origin", '')
            origin_host = urlparse.urlsplit(header).netloc
            if origin_host != self.request.host:
                msg = ("Cross-site cookie authentication attempt from {0} "
                       "(origin: {1}, host: {2})".format(self.request.remote_ip,
                                                         origin_host,
                                                         self.request.host))
                logging.warning(msg)
                return

            if self.get_cookie("sid"):
                # short-lived session
                sid = url_unescape(self.get_cookie("sid"))
                session = userdb.session_info(sid)
                if session and session.get("username"):
                    self.logger.info("Session user: " + session["username"])
                    self.do_login(session["username"], sid=sid)
            if self.username is None and self.get_cookie("login"):
                # long-lived saved login
                self.token_login(url_unescape(self.get_cookie("login")))
            elif config.get("autologin"):
                self.do_login(config.autologin)

    def check_origin(self, origin):
        return True

    def check_origin(self, origin):
        return True

    def idle_time(self):
        return self.process.idle_time()

    def is_running(self):
        return self.process is not None

    def is_in_lobby(self):
        return not self.is_running() and self.watched_game is None

    def send_lobby(self):
        from process_handler import processes
        self.send_lobby_html()
        data = [p.lobby_entry() for p in processes.values()]
        self.send_message("lobby", entries=data)

    def send_lobby_html(self):
        banner_html = self.render_string("banner.html", username=self.username,
                                         nerd=self.nerd)
        footer_html = self.render_string("footer.html", username=self.username,
                                         nerd=self.nerd)
        self.queue_message("lobby_html", banner=banner_html, footer=footer_html)

    def get_game_info(self):
        saved_games = []
        if config.get("list_savegames") and self.username is not None:
            saved_games = list(find_player_savegames(self.username))
        self.send_message("game_info", games=config.get("games"),
                          saved_games=saved_games,
                          settings=config.get("game_link_settings"))

    def reset_timeout(self):
        if self.timeout:
            self.ioloop.remove_timeout(self.timeout)

        self.received_pong = False
        self.send_message("ping")
        self.timeout = self.ioloop.add_timeout(time.time() \
                                               + config.http_connection_timeout,
                                               self.check_connection)

    def check_connection(self):
        self.timeout = None

        if not self.received_pong:
            self.logger.info("Connection timed out.")
            self.close()
        else:
            if self.is_running() \
               and self.process.idle_time() > config.max_idle_time:
                self.logger.info("Stopping crawl after idle time limit.")
                self.go_lobby()

        if not self.client_closed:
            self.reset_timeout()

    def start_crawl(self, game_id):
        if game_id not in config.games:
            self.go_lobby()
            return

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

        args = (game_params, self.username, self.logger, self.ioloop)
        self.process = process_handler.CrawlProcessHandler(*args)

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

        update_global_status()

    def init_user(self):
        if not config.get('init_player_program'):
            return True
        try:
            output = subprocess.check_output([config.init_player_program,
                                              self.username],
                                             stderr = subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            logging.error("init_player_program failed for user '{0}', "
                          "output: {1}".format(self.username,
                                               e.output.replace("\n", " ")))
            return False
        else:
            if output.strip():
                logging.debug("init_player_program succeeded for user '{0}', "
                              "output: {1}".format(self.username,
                                                   output.replace('\n', ' ')))
            return True

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
            self.send_message("close", reason = "Could not initialize your "
                              "account! This probably means there is something "
                              "wrong with the server configuration.")
            self.logger.warning("User initialization returned an error for "
                                "user {0}!".format(self.username))
            self.username = None
            self.close()
            return
        self.nerd = config.get_nerd(self.username)
        self.queue_message("login_success", username=username, sid=sid,
                           nerd=self.nerd)
        if self.watched_game:
            self.watched_game.update_watcher_description()
        else:
            self.send_lobby_html()
            self.get_game_info()

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
            self.logger.warning("Failed login for user {0}.".format(username))
            self.send_message("login_fail")

    def token_login(self, cookie):
        username, new_cookie = userdb.token_login(cookie, logger=self.logger)
        if username:
            (new_cookie_contents, expires) = new_cookie
            self.do_login(username)
            self.send_message("login_cookie", cookie=new_cookie_contents,
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
        path = util.dgl_format_str(config.games[game_id]["rcfile_path"],
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
        if not config.dgl_mode: return
        if self.is_running():
            self.stop_playing()
        elif self.watched_game:
            self.stop_watching()
        self.send_message("go", path="/")

    def get_rc(self, game_id):
        if game_id not in config.games: return
        if not os.path.isfile(self.rcfile_path(game_id)): return
        with open(self.rcfile_path(game_id), 'r') as f:
            contents = f.read()
        self.send_message("rcfile_contents", contents = contents)

    def set_rc(self, game_id, contents):
        rcfile_path = self.rcfile_path(game_id)
        with open(rcfile_path, 'w') as f:
            f.write(contents.encode("utf8"))

    def reset_rc(self, game_id):
        rcfile_path = self.rcfile_path(game_id)
        os.remove(rcfile_path)
        if not self.init_user():
            self.logger.warning("User initialization returned an error for "
                                "user {0}!".format(self.username))
        try:
            with open(rcfile_path, 'r') as f:
                contents = f.read()
        except:
            contents = ""
        self.send_message("rcfile_contents", contents=contents)

    def change_password(self, old_password, new_password):
        if self.username is None: return
        if userdb.user_passwd_match(self.username, old_password):
            self.logger.info("Changed password for user "
                             "{0}.".format(self.username))
            success = userdb.set_password(self.username, new_password)
        else:
            self.logger.warning("Failed password change attempt for "
                                "user {0}.".format(self.username))
            success = False
        self.send_message("password_change", success=success)

    def parse_scores(self, num_scores, score_file, game_map = None,
                     morgue_url = None):
        try:
            fh = open(score_file, 'rU')
        except EnvironmentError as e:
            logging.error("Unable to open score file {0} "
                          "({1})".format(score_file, e.strerror))
            return None
        scores = []
        for line in fh:
            score = {}
            for token in line.strip().split(':'):
                if token.count('=') == 1:
                    attr, value = token.split('=')
                    score[attr] = value

            if game_map is not None and score["map"] != game_map:
                continue
            score["rank"] = len(scores) + 1
            # convert to comma-separated digits.
            score["score"] = locale.format("%d", int(score["sc"]),
                                           grouping = True)
            score["turns"] = locale.format("%d", int(score["turn"]),
                                          grouping = True)
            score["place"] = "{0}:{1}".format(score["br"], score["lvl"])
            # if the game ended properly, we can give a morgue file.
            if "end" in score:
                # fix 0-origin month to 1-origin
                timestamp = str(int(score["end"].rstrip("DS")) + 100000000)
                ptn = re.compile(r'(\d{2})(\d{2})(\d{2})(\d{2})(\d{6})')
                score["date"] = ptn.sub(r'\2/\3/\4', timestamp)
                if morgue_url is not None and 'name' in score:
                    fstamp = ptn.sub(r'\1\2\3\4-\5', timestamp)
                    url = "{0}morgue-{1}-{2}.txt".format(morgue_url,
                                                         score["name"], fstamp)
                    score["morgue_url"] = url.replace("%n", score["name"])
            if "name" in score:
                score["nerd"] = config.get_nerd(score["name"])
            scores.append(score)
            if len(scores) == num_scores:
                break
        fh.close()

        if not len(scores):
            scores = None
        return scores

    def get_scores(self, num_scores, game_version, game_mode, game_map):
        game_desc = None
        game = None
        scores = None
        num_scores = int(num_scores)
        if config.get("max_score_list_length"):
            num_scores = min(config.max_score_list_length, num_scores)
            if num_scores > 0:
                game = config.get_game(game_version, game_mode)
        else:
            logging.warning("max_score_list_length must be set to return "
                            "scores")
        if game is not None and "score_path" in game:
            valid_entry = True
            morgue_url = None
            score_file = game["score_path"]
            if "morgue_url" in game:
                morgue_url = game["morgue_url"]
            if game_map is None:
                game_desc = game["name"]
            else:
                map_entry = config.game_map(game["id"], game_map)
                if map_entry:
                    score_file = score_file.replace("%m", game_map)
                    game_desc = "{0}, {1}".format(game["name"],
                                                  map_entry["description"])

        ## The requested game and map is valid and we can get scores
        if game_desc is not None:
            scores = self.parse_scores(num_scores, score_file, game_map,
                                       morgue_url)
        self.send_message("scores", game_desc = game_desc, scores = scores)

    def on_message(self, message):
        if self.sid:
            if not userdb.renew_session(self.sid):
                self.send_message("close", reconnect=True)
                self.close()
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
                super(CrawlWebSocket, self).write_message(compressed,
                                                          binary=True)
            else:
                self.uncompressed_bytes_sent += len(msg)
                super(CrawlWebSocket, self).write_message(msg)
        except:
            self.logger.warning("Exception trying to send message.",
                                exc_info = True)
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

        if config.get("logging_config", {}).get('enable_access_log'):
            self.logger.info("Socket closed. ({0} bytes sent, compression "
                             "ratio {1}%%)".format(self.total_message_bytes,
                                                  comp_ratio))
