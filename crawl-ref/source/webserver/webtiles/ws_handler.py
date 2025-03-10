import codecs
import collections
import datetime
import errno
import logging
import os
import random
import signal
import subprocess
import time
import zlib

import tornado.ioloop
import tornado.template
import tornado.websocket
import tornado.gen
from tornado.escape import json_decode
from tornado.escape import json_encode
from tornado.escape import to_unicode
from tornado.escape import utf8
from tornado.ioloop import IOLoop

from webtiles import auth, checkoutput, config, userdb, util, load_games

try:
    from typing import Dict, Set, Tuple, Any, Union, Optional
except:
    pass

sockets = set() # type: Set[CrawlWebSocket]
current_id = 0
shutting_down = False
rand = random.SystemRandom()

def shutdown():
    global shutting_down
    shutting_down = True
    for socket in list(sockets):
        socket.shutdown()

def update_global_status():
    IOLoop.current().add_callback(write_dgl_status_file)

# lobbies that need updating
game_lobby_cache = set() # type: Set[CrawlProcessHandlerBase]

def list_of_names(l):
    result = []
    try:
        c = collections.Counter(l)
        for n in sorted(c.keys()):
            result.append(n)
            if c[n] > 1:
                result[-1] += " (x%d)" % c[n]
    except:
        pass # backwards compat, py2 doesn't have collections.Counter
    return ", ".join(result)

def describe_sockets(names=False):
    slist = list(sockets)
    lobby = [s for s in slist if s.is_in_lobby()]
    lobby_count = len(lobby)
    # anon connections are either in the lobby or spectating
    anon_lobby = len([s for s in lobby if not s.username])
    anon_specs = len([s for s in slist if not s.username]) - anon_lobby
    playing = [s for s in slist if s.is_running()]
    player_count = len(playing)
    idle_count = len([s for s in playing if s.process.is_idle()])
    spec_count = len([s for s in slist if s.watched_game])

    summary = "%d connections: %d playing (%d idle), %d watching (%d anon), %d in lobby (%d anon)" % (
        len(slist), player_count, idle_count, spec_count, anon_specs, lobby_count, anon_lobby)

    if names:
        # this is all a bit brute-force
        if playing:
            summary += "; Playing: %s" % list_of_names([s.username for s in playing])
        watchers = list_of_names([s.username for s in slist if s.watched_game and s.username])
        if watchers:
            summary += "; Watchers: %s" % watchers
        lobby_names = list_of_names([s.username for s in lobby if s.username and not s.account_restricted()])
        if lobby_names:
            summary += "; Lobby: %s" % lobby_names
        restricted = list_of_names([s.username for s in playing if s.username and s.account_restricted()])
        if restricted:
            summary += "; Account restricted: %s" % restricted
        restricted_lobby = list_of_names([s.username for s in lobby if s.username and s.account_restricted()])
        if restricted_lobby:
            summary += "; Account restricted (lobby): %s" % restricted_lobby
    return summary


def do_lobby_updates():
    global game_lobby_cache
    lobby_sockets = [s for s in list(sockets) if s.is_in_lobby()]
    games_to_update = list(game_lobby_cache)
    game_lobby_cache.clear()
    for game in games_to_update:
        if not game.process:
            # handled immediately in `remove_in_lobbys`, ignore here
            continue
        admin_only = game.account_restricted()

        game_entry = game.lobby_entry()
        # Queue up the collected lobby changes in each socket. This loop is
        # synchronous.
        for socket in lobby_sockets:
            if admin_only and not socket.is_admin():
                continue
            socket.queue_message("lobby_entry", **game_entry)

    # ...and finally, flush all the updates. This loop may be asynchronous.
    for socket in lobby_sockets:
        socket.flush_messages()

def do_periodic_lobby_updates():
    # TODO: in the Future, refactor as a simple async loop
    do_lobby_updates()
    rate = config.get('lobby_update_rate')
    IOLoop.current().add_timeout(time.time() + rate, do_periodic_lobby_updates)

load_logging_enabled = False
def do_load_logging(start = False):
    global load_logging_enabled
    if start and load_logging_enabled:
        # don't start multiple timeouts
        return
    rate = config.get('load_logging_rate')
    if rate:
        load_logging_enabled = True
        if not start:
            # don't log on startup call
            logging.info(describe_sockets())
        IOLoop.current().add_timeout(time.time() + rate, do_load_logging)
    else:
        # config disables load logging, no timeout
        load_logging_enabled = False

def update_all_lobbys(game):
    global game_lobby_cache
    # mark the game for display in the lobby
    game_lobby_cache.add(game)

def remove_in_lobbys(game):
    global game_lobby_cache
    if game in game_lobby_cache:
        game_lobby_cache.remove(game)
    for socket in list(sockets):
        if socket.is_in_lobby():
            socket.send_message("lobby_remove", id=game.id,
                                reason=game.exit_reason,
                                message=game.exit_message,
                                dump=game.exit_dump_url)

def global_announce(text):
    for socket in list(sockets):
        socket.send_announcement(text)

_dgl_dir_check = False


@tornado.gen.coroutine
def write_dgl_status_file():
    process_info = ["%s#%s#%s#0x0#%s#%s#" %
                            (socket.username, socket.game_id,
                             (socket.process.human_readable_where()),
                             str(socket.process.idle_time()),
                             str(socket.process.watcher_count()))
                        for socket in list(sockets)
                        if socket.username and socket.show_in_lobby()]
    try:
        status_target = config.get('dgl_status_file')
        global _dgl_dir_check
        if not _dgl_dir_check:
            status_dir = os.path.dirname(status_target)
            # generally created by other things sooner or later, but if we don't do
            # this preemptively here, there's lot of warnings until that happens.
            if not os.path.exists(status_dir):
                os.makedirs(status_dir)
                logging.warning("Creating dgl status file location '%s'", status_dir)
            _dgl_dir_check = True
        yield util.open_and_write(status_target, "\n".join(process_info))
    except (OSError, IOError) as e:
        logging.warning("Could not write dgl status file: %s", e)


@tornado.gen.coroutine
def status_file_timeout():
    yield write_dgl_status_file()
    IOLoop.current().add_timeout(time.time() + config.get('status_file_update_rate'),
                                 status_file_timeout)


def find_user_sockets(username):
    for socket in list(sockets):
        if socket.username and socket.username.lower() == username.lower():
            yield socket


def find_running_game(charname, start):
    from webtiles.process_handler import processes
    for process in list(processes.values()):
        if (process.where.get("name") == charname and
            process.where.get("start") == start):
            return process
    return None


def _milestone_files():
    # First we collect all milestone files explicitly specified in the config.
    files = set()

    top_level_milestones = config.get('milestone_file')
    if top_level_milestones is not None:
        if not isinstance(top_level_milestones, list):
            top_level_milestones = [top_level_milestones]
        files.update(top_level_milestones)

    for game in config.games:
        milestone_file = config.game_param(game, 'milestone_file')
        if not milestone_file and 'dir_path' in config.games[game]:
            # milestone appears in this dir by default
            milestone_file = os.path.join(
                        config.game_param(game, 'dir_path'), 'milestones')
        if milestone_file:
            files.add(milestone_file)

    # Then, make sure for every milestone we have the -seeded and non-seeded
    # variant.
    new_files = set()
    for f in files:
        if f.endswith('-seeded'):
            new_files.add(f[:-7])
        else:
            new_files.add(f + '-seeded')
    files.update(new_files)

    # Finally, drop any files that don't exist
    files = [f for f in files if os.path.isfile(f)]

    return files

milestone_file_tailers = []
def start_reading_milestones():
    milestone_files = _milestone_files()
    for f in milestone_files:
        milestone_file_tailers.append(util.FileTailer(f, handle_new_milestone))

def handle_new_milestone(line):
    data = util.parse_where_data(line)
    if "name" not in data:
        return
    game = find_running_game(data.get("name"), data.get("start"))
    if game and not game.receiving_direct_milestones:
        game.set_where_info(data)

# decorators for admin calls

# for CrawlWebSocket functions that implement some admin-only user-triggered
# action. These will log if someone tries to call them without permissions.
# Usually for functions that are exposed via a message handler.
def admin_required(f):
    def wrapper(self, *args, **kwargs):
        if not self.is_admin():
            logging.error("Non-admin user '%s' attempted admin function '%s'" %
                (self.username and self.username or "[Anon]", f.__name__))
            return
        return f(self, *args, **kwargs)
    return wrapper

# for CrawlWebSocket functions that should do nothing for a non-admin user, but
# aren't user-triggered. (n.b. it is in principle possible to write a fancier
# decorator that subsumes both of these cases, but it is simpler not to.)
def admin_only(f):
    def wrapper(self, *args, **kwargs):
        if not self.is_admin():
            return
        return f(self, *args, **kwargs)
    return wrapper


# XX add defaults when we can be sure the python version supports it
MessageBundle = collections.namedtuple('MessageBundle', ["binmsg", "compressed"])
MessageBundle.__bool__ = lambda self: bool(self.binmsg)

class CrawlWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, app, req, **kwargs):
        tornado.websocket.WebSocketHandler.__init__(self, app, req, **kwargs)
        self.username = None
        self.user_id = None
        self.user_email = None
        self.timeout = None
        self.lobby_timeout = None
        self.watched_game = None
        self.process = None
        self.game_id = None
        self.received_pong = None
        self.save_info = dict()

        tornado.ioloop.IOLoop.current()

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
        self.message_queue = []  # type: List[str]
        self.failed_messages = 0 # messages to webtiles
        self.failed_on_messages = 0 # messages from webtiles

        self.subprotocol = None

        self.chat_hidden = False

        self.logger = logging.LoggerAdapter(logging.getLogger(), {})
        self.logger.process = self._process_log_msg

        self.message_handlers = {
            "login": self.login,
            "token_login": self.token_login,
            "set_login_cookie": self.set_login_cookie,
            "forget_login_cookie": self.forget_login_cookie,
            "play": self.start_crawl,
            "pong": self.pong,
            "watch": self.watch,
            "chat_msg": self.post_chat_message,
            "register": self.register,
            "start_change_email": self.start_change_email,
            "change_email": self.change_email,
            "start_change_password": self.start_change_password,
            "change_password": self.change_password,
            "forgot_password": self.forgot_password,
            "reset_password": self.reset_password,
            "go_lobby": self.go_lobby,
            "go_admin": self.go_admin,
            "get_rc": self.get_rc,
            "set_rc": self.set_rc,
            "admin_announce": self.admin_announce,
            "admin_pw_reset": self.admin_pw_reset,
            "admin_pw_reset_clear": self.admin_pw_reset_clear
            }

    @admin_required
    def admin_announce(self, text):
        self.reset_lobby_timeout()
        global_announce(text)
        self.logger.info("User '%s' sent serverwide announcement: %s", self.username, text)
        self.send_message("admin_log", text="Announcement made ('" + text + "')")

    @admin_only
    def send_socket_stats(self):
        import webtiles.server
        self.reset_lobby_timeout()
        self.send_message("admin_log", text=webtiles.server.version())
        self.send_message("admin_log", text=describe_sockets(True))

    @admin_required
    def admin_pw_reset(self, username):
        self.reset_lobby_timeout()
        user_info = userdb.get_user_info(username)
        if not user_info:
            self.send_message("admin_pw_reset_done", error="Invalid user")
            return
        ok, msg = userdb.generate_forgot_password(user_info.username)
        if not ok:
            self.send_message("admin_pw_reset_done", error=msg)
        else:
            self.logger.info("Admin user '%s' set a password token on account '%s'",
                self.username, user_info.username)
            self.send_message("admin_pw_reset_done", email_body=msg,
                username=user_info.username, email=user_info.email)

    @admin_required
    def admin_pw_reset_clear(self, username):
        self.reset_lobby_timeout()
        ok, err = userdb.clear_password_token(username)
        if ok:
            self.logger.info("Admin user '%s' cleared the reset token on account '%s'", self.username, username)
        else:
            self.logger.info("Error trying to clear reset token for '%s': %s", username, err)

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

        self.logger.info("Socket opened from ip %s (fd%s, compression: %s).",
                         self.request.remote_ip,
                         self.ws_connection.stream.socket.fileno(),
                         compression)
        sockets.add(self)

        self.reset_idle_timeouts()

        if config.get('max_connections') < len(sockets):
            self.append_message("connection_closed('The maximum number of "
                              + "connections has been reached, sorry :(');")
            self.close()
        elif shutting_down:
            self.close()
        else:
            if config.get('dgl_mode'):
                if config.get('autologin'):
                    self.do_login(config.get('autologin'))
                self.send_lobby()
            else:
                self.start_crawl(None)

    def check_origin(self, origin):
        return True

    def idle_time(self):
        return self.process.idle_time()

    def is_running(self, partial=False):
        # during setup, and for a short interval after close, self.process
        # is set, but self.process.process may be unset or indeterminate.
        # after close, `try_cleanup` provides a safe point for this check.
        return self.process is not None and (partial or self.process.process)

    def is_in_lobby(self):
        # caveat: this will return True while a player is logging in directly
        # via a play link...
        return not self.is_running(partial=True) and self.watched_game is None

    def send_lobby_data(self):
        self.queue_message("lobby_clear")
        from webtiles.process_handler import processes
        for process in list(processes.values()):
            if process.account_restricted() and not self.is_admin():
                continue
            self.queue_message("lobby_entry", **process.lobby_entry())
        self.send_message("lobby_complete")

    def send_lobby(self):
        self.send_lobby_data()
        self.send_lobby_html()
        self.send_socket_stats() # admins only

    def account_restricted(self):
        return not self.username or userdb.dgl_account_hold(self.user_flags)

    def show_in_lobby(self):
        return (self.is_running()
            and not self.account_restricted())

    def send_announcement(self, text):
        # TODO: something in lobby?
        if not self.is_in_lobby():
            # show in chat window
            self.send_message("server_announcement", text=text)
            # show in player message window
            if self.is_running():
                self.process.handle_announcement(text)

    def invalidate_saveslot_cache(self, slot):
        # TODO: the following will get false positives. However, to do any
        # better would need some non-trivial refactoring of how crawl handles
        # save slots (which is a bit insane). A heuristic might be to check the
        # binary, but in practice this doesn't help as most servers launch
        # crawl via a wrapper script, not via a direct call.
        if self.save_info.get(slot, None) is not None:
            cache_check = "" if self.save_info[slot] == "" else "[slot full]"
            for g in self.save_info:
                if self.save_info[g] == cache_check:
                    self.save_info[g] = None
        self.save_info[slot] = None

    def update_save_info(self, binary_key, data):
        try:
            data = json_decode(data)
        except Exception:
            # JSON error. It can also happen under some dgamelaunch-config
            # setups if escape codes are incorrectly inserted into
            # the output for some calls. See:
            # https://github.com/crawl/dgamelaunch-config/commit/6ad788ceb5614b3c83d65b61bf26a122e592b98d
            # should this case warn? It probably means a misconfiguration.
            for g in config.binaries_to_games[binary_key]:
                self.save_info[g] = ""
                return

        # data is now a dict of game mode to game descriptions. If game modes
        # share a save slot, then they will map to the same game.
        for g in config.binaries_to_games[binary_key]:
            if (not config.game_modes[g] in data
                        or not config.games[g].get("show_save_info", False)):
                self.save_info[g] = ""
                continue
            save_dict = data[config.game_modes[g]]
            if not save_dict["loadable"]:
                # the save in this slot is in use.
                self.save_info[g] = "[playing]" # TODO: something better??
            elif config.game_modes[g] == save_dict["game_type"]:
                # save in the slot matches the game type we are
                # checking.
                self.save_info[g] = "[%s]" % save_dict["short_desc"]
            else:
                # There is a save, but it has a different game type.
                # This happens if multiple game types share a slot.
                self.save_info[g] = "[slot full]"

    # collect save info for the player from all binaries that support save
    # info json. Cached on self.save_info. This is asynchronously done using
    # a somewhat involved callback chain; each check requires a subprocess
    # call on which we don't want to block.
    # this code would be much simpler if refactored using async
    def collect_save_info(self, final_callback):
        if not self.username:
            return

        # Tally up the calls we need in order to check relevant save info.
        # For each distinct binary (going by binary keys, cached in
        # `binaries_to_games`), if that binary has modes that should show and
        # update save info, add a call to the list.
        check_calls = []
        for b in config.binaries_to_games:
            games = config.binaries_to_games[b]
            if not games:
                continue
            if not config.game_modes[games[0]]:
                continue # no game mode info for this game
            if not any([config.games[g].get("show_save_info", False) for g in games]):
                # no games modes for this binary have enabled save info
                for g in games:
                    self.save_info[g] = ""
                continue
            if not any([self.save_info.get(g, None) is None for g in games]):
                # no invalid caches for this binary
                continue
            # doesn't matter which element of games we use
            call = config.games[games[0]].get_call_base()
            call += ["-save-json", self.username]
            check_calls.append([b, call])

        # next we need to turn the list of calls into a sequence of non-blocking
        # callbacks

        def build_callback(b, call, next_callback):
            # build a callback that:
            # a. calls check_output with the call info (nonblocking subprocess)
            # b. passes the output to update_save_info, and
            # c. calls next_callback
            def do_update(data, returncode):
                if returncode != 0:
                    # Binary doesn't support save info, print a warning so that
                    # the admin can see there is a misconfiguration
                    # XX it should be possible to check for this case on startup
                    logging.warning("Save info check for '%s' failed" % b)
                    data = None # force error case in update_save_info call

                self.update_save_info(b, data)
                next_callback()

            # the check_output call is non-blocking, e.g. will yield to other
            # IOLoop stuff:
            return lambda: checkoutput.check_output(call, do_update)

        # sequence the calls into a chain of callbacks that end with
        # final_callback
        callback = final_callback
        for l in check_calls:
            callback = build_callback(l[0], l[1], callback)

        # finally, kick things off
        callback()

    def send_lobby_html(self):
        # Rerender Banner
        # TODO: don't really need to do this every time the lobby is loaded?
        if self.ui is None:
            # socket has been finish()ed, and the render_string call will raise.
            # this probably only happens in async race conditions, so don't
            # do anything further (and hope that the close happens properly
            # elsewhere)
            return
        banner_html = to_unicode(self.render_string("banner.html",
                                                    username = self.username))
        self.queue_message("html", id = "banner", content = banner_html)

        if not self.username:
            return
        def disable_check(s):
            return s == "[slot full]"
        def send_game_links():
            global sockets
            if not self in sockets:
                return # socket closed by the time this info was collected
            self.queue_message("html", id = "banner", content = banner_html)
            # TODO: dynamically send this info as it comes in, rather than
            # rendering it all at the end?
            try:
                # post py3.6, this can all be done with a dictionary
                # comprehension, but before that we need to manually keep
                # the order
                games = collections.OrderedDict()
                for g in config.games:
                    if self.game_id_allowed(g):
                        games[g] = config.games[g].templated_dict(self.username)
                play_html = to_unicode(self.render_string("game_links.html",
                                                  games = games,
                                                  save_info = self.save_info,
                                                  disabled = disable_check))
                self.send_message("set_game_links", content = play_html)
            except:
                self.logger.warning("Error on send_game_links callback",
                                                                exc_info=True)
        # if no game links at all have been sent, immediately render the
        # empty version. This is so that if the server takes a while on
        # initial connect, the player sees something immediately.
        if len(self.save_info) == 0:
            for g in config.games:
                self.save_info[g] = None
            send_game_links()
        self.collect_save_info(send_game_links)

    def clear_timeouts(self, lobby=None):
        if not lobby and self.timeout:
            IOLoop.current().remove_timeout(self.timeout)
            self.timeout = None
        if lobby != False and self.lobby_timeout:
            IOLoop.current().remove_timeout(self.lobby_timeout)
            self.lobby_timeout = None

    def reset_lobby_timeout(self):
        self.clear_timeouts(lobby=True)
        # warning: there is a somewhat painful interval at game start while
        # self.process is partly set up, but before self.process.process is
        # still unset. Don't call this function during that time...
        time_limit = config.get('max_lobby_idle_time')
        if not self.is_in_lobby() or self.is_admin() or time_limit <= 0:
            # this can be triggered on callbacks; if the player is now playing,
            # leave things with this timeout no longer set at all
            return

        # don't check more often than the overall connection timeout setting
        time_limit = max(time_limit, config.get('connection_timeout'))
        # lobby doesn't track idle time, so we just use a timeout directly;
        # this gets reset on actions
        self.lobby_timeout = IOLoop.current().add_timeout(
                        time.time() + time_limit,
                        self.check_lobby_connection)

    def check_lobby_connection(self):
        if self.is_running(): # sanity check, but this shouldn't be possible
            return
        # unconditionally close if we reach the lobby idle time
        # XX: possibly print a more informative message on the client side?
        self.logger.info("Lobby connection timed out.")
        self.close()

    def reset_timeout(self):
        self.clear_timeouts(lobby=False)

        self.received_pong = False
        self.send_message("ping")
        self.timeout = IOLoop.current().add_timeout(
                                        time.time() + config.get('connection_timeout'),
                                        self.check_connection)

    def reset_idle_timeouts(self):
        self.reset_lobby_timeout()
        self.reset_timeout()

    def check_connection(self):
        self.timeout = None

        if not self.received_pong:
            self.logger.info("Connection timed out.")
            self.close()
        elif self.is_running():
            if self.process.idle_time() > config.get('max_idle_time'):
                self.logger.info("Stopping crawl after idle time limit.")
                self.process.stop()
        elif self.is_in_lobby():
            if not self.lobby_timeout:
                self.reset_lobby_timeout()
        # XX none of these covers watchers

        if not self.client_closed:
            self.reset_timeout()

    def update_db_info(self):
        """update self's user info from the user database, including updating
        any flag changes that may have happened in the db."""
        if not self.username:
            return True # caller needs to check for anon if necessary
        # won't detect a change in hold state on first login...
        old_restriction = self.user_flags is not None and self.account_restricted()
        u = userdb.get_user_info(self.username)
        self.username = u.username # canonicalize
        self.user_id = u.id
        self.user_email = u.email
        self.user_flags = u.flags
        self.logger.extra["username"] = self.username
        if userdb.dgl_is_banned(self.user_flags):
            return False
        if old_restriction and not self.account_restricted():
            self.logger.info("[Account] Hold cleared for user %s (IP: %s)",
                                        self.username, self.request.remote_ip)

        return True

    def game_id_allowed(self, game_id):
        if game_id not in config.games:
            return False
        if not self.account_restricted():
            return True
        game = config.games[game_id]
        # for now, if this isn't set, default to allow. However, if the
        # version doesn't support `-no-player-bones` it will still fail to
        # start.
        return "allowed_with_hold" not in game or game["allowed_with_hold"]

    def start_crawl(self, game_id):
        if config.get('dgl_mode') and game_id not in config.games:
            self.go_lobby()
            return

        if config.get('dgl_mode'):
            if self.username == None:
                if self.watched_game:
                    self.stop_watching()
                self.send_message("login_required",
                    game=config.games[game_id].templated("name", username=self.username))
                return

        if self.process:
            # ignore multiple requests for the same game, can happen when
            # logging in with cookies
            if self.game_id != game_id:
                self.go_lobby()
            return

        self.game_id = game_id

        # invalidate cached save info for lobby
        # TODO: invalidate for other sockets of the same player?
        self.invalidate_saveslot_cache(game_id)

        # update flags in case an account hold has been released or added, or
        # the player has been banned.
        if config.get('dgl_mode') and (not self.update_db_info() # ban check happens here
                or not self.game_id_allowed(game_id)):
            self.go_lobby()
            return

        from webtiles import process_handler

        if config.get('dgl_mode'):
            self.process = process_handler.CrawlProcessHandler(
                            config.games[game_id], self.username, self.logger)
        else:
            self.process = process_handler.DGLLessCrawlProcessHandler(self.logger)

        # now that `process` is set, clear the lobby timeout and reset the
        # play timeout
        self.reset_idle_timeouts()
        # self.clear_timeouts(lobby=True)
        self.process.end_callback = self._on_crawl_end
        self.process.add_watcher(self)
        try:
            self.process.start()
        except Exception:
            self.logger.warning("Exception starting process!", exc_info=True)
            self.process = None
            self.go_lobby()
        else:
            if self.process is None: # Can happen if the process creation fails
                # lobby should be handled in exit callback
                return

            self.send_message("game_started")
            self.restore_blocklist()
            self.process.handle_notification(self.process.username,
                            "'/help' to see available chat commands")
            if not config.get('max_chat_length'):
                self.process.handle_notification(self.process.username,
                            "Regular chat is disabled on this server")


            if config.get('dgl_mode'):
                if self.process.where == {}:
                    # If location info was found, the lobbys were already
                    # updated by set_where_data
                    update_all_lobbys(self.process)
                update_global_status()

    def _on_crawl_end(self):
        if self.process:
            if config.get('dgl_mode'):
                remove_in_lobbys(self.process)

            reason = self.process.exit_reason
            message = self.process.exit_message
            dump_url = self.process.exit_dump_url
        else:
            # XX under what circumstance is this reachable?
            reason = None

        self.process = None

        if not self.client_closed:
            if shutting_down:
                self.close()
            else:
                # Go back to lobby
                if reason:
                    self.send_message("game_ended", reason = reason,
                                      message = message, dump = dump_url)

                self.invalidate_saveslot_cache(self.game_id)

                if config.get('dgl_mode'):
                    if not self.watched_game:
                        self.send_message("go_lobby")
                        self.send_lobby()
                else:
                    self.start_crawl(None)

        if config.get('dgl_mode'):
            # queues a callback to update the status list
            update_global_status()

        self.try_cleanup()

    def init_user(self, callback):
        # this would be more cleanly implemented with wait_for_exit, but I
        # can't get code for that to work in a way that supports all currently
        # in-use versions. TODO: clean up once old Tornado versions are out of
        # the picture.
        with open("/dev/null", "w") as f:
            if tornado.version_info[0] < 3:
                # before tornado 3, an async approach would have to be done
                # differently, and given that we're deprecating tornado 2.4
                # it doesn't seem worth implementing right now. Just stick with
                # the old synchronous approach for backwards compatibility.
                p = subprocess.Popen([config.get('init_player_program'), self.username],
                                         stdout = f, stderr = subprocess.STDOUT)
                callback(p.wait())
            else:
                # TODO: do we need to care about the streams at all here?
                p = tornado.process.Subprocess(
                        [config.get('init_player_program'), self.username],
                        stdout = f, stderr = subprocess.STDOUT)
                p.set_exit_callback(callback)

    def stop_watching(self):
        if self.watched_game:
            self.logger.info("%s stopped watching %s.",
                                    self.username and self.username or "[Anon]",
                                    self.watched_game.username)
            self.watched_game.remove_watcher(self)
            self.watched_game = None

    def shutdown(self):
        if not self.client_closed:
            self.logger.info("Shutting down user %s id %d",
                                    self.username and self.username or "[Anon]",
                                    self.id)
            msg = to_unicode(self.render_string("shutdown.html", game=self))
            self.send_message("close", reason = msg)
            self.close()
        if self.is_running():
            self.process.stop()

    def do_login(self, username):
        self.username = username
        self.user_flags = None
        if not self.update_db_info():
            # XX consolidate with other ban check / login fail code somehow.
            # Also checked in userdb.user_passwd_match.
            fail_reason = 'Account is disabled.'
            self.logger.warning("[Account] Failed login for user %s: %s", self.username,
                                    fail_reason)
            self.send_message("login_fail", reason=fail_reason)
            return

        def login_callback(result):
            success = result == 0
            if not success:
                msg = ("Could not initialize your rc and morgue!<br>" +
                       "This probably means there is something wrong " +
                       "with the server configuration.")
                self.send_message("close", reason = msg)
                self.logger.warning("User initialization returned an error for user %s!",
                                    self.username)
                self.username = None
                self.close()
                return

            self.queue_message("login_success", username=username,
                               admin=self.is_admin())
            if self.account_restricted():
                self.queue_message("set_account_hold")
            if self.watched_game:
                self.watched_game.update_watcher_description()
            elif self.is_admin():
                self.send_lobby()
            else:
                self.send_lobby_html()

            self.reset_lobby_timeout()

        self.init_user(login_callback)

    def login(self, username, password):
        success, real_username, fail_reason = userdb.user_passwd_match(username, password)
        if success and real_username:
            self.logger.info("User %s logging in from %s.",
                                        real_username, self.request.remote_ip)
            self.do_login(real_username)
        elif fail_reason:
            self.logger.warning("Failed login for user %s (IP: %s): %s",
                            real_username, self.request.remote_ip, fail_reason)
            self.send_message("login_fail", reason = fail_reason)
        else:
            self.logger.warning("Failed login for user %s (IP: %s).", username,
                            self.request.remote_ip)
            self.send_message("login_fail")

    def token_login(self, cookie):
        username, ok = auth.check_login_cookie(cookie)
        if ok:
            auth.forget_login_cookie(cookie)
            self.logger.info("User %s logging in from %s (via token).",
                username, self.request.remote_ip)
            self.do_login(username)
        else:
            self.logger.warning("Wrong login token for user %s. (IP: %s)",
                username, self.request.remote_ip)
            self.send_message("login_fail")

    def set_login_cookie(self):
        if self.username is None:
            return
        cookie = auth.log_in_as_user(self, self.username)
        self.send_message("login_cookie", cookie = cookie,
                          expires = config.get('login_token_lifetime'))

    def forget_login_cookie(self, cookie):
        auth.forget_login_cookie(cookie)

    def restore_blocklist(self):
        if not self.username:
            return
        receiver = None
        if self.process:
            receiver = self.process
        elif self.watched_game:
            receiver = self.watched_game

        if not receiver:
            return

        db_string = userdb.get_blocklist(self.username)
        if db_string is None:
            db_string = ""
        # list constructor here is for forward compatibility with python 3.
        blocked = list([_f for _f in db_string.strip().split(' ') if _f])
        receiver.restore_blocklist(self.username, blocked)

    def save_blocklist(self, blocked):
        db_string = " ".join(blocked).strip()
        userdb.set_blocklist(self.username, db_string)

    def is_admin(self):
        return self.username is not None and userdb.dgl_is_admin(self.user_flags)

    def pong(self):
        self.received_pong = True

    def rcfile_path(self, game_id):
        if game_id not in config.games:
            return None
        if not self.username:
            return None
        path = config.game_param(game_id, "rcfile_path", username=self.username)
        return os.path.join(path, self.username + ".rc")

    def send_json_options(self, game_id, player_name):
        def do_send(data, returncode):
            if returncode != 0:
                # fail silently for returncode 1 for now, probably just an old
                # version missing the command line option
                if returncode != 1:
                    self.logger.warning("Error while getting JSON options!")
                return
            self.append_message('{"msg":"options","watcher":true,"options":'
                                + data + '}')

        if not self.username: return
        if game_id not in config.games: return

        game = config.games[game_id]
        if "send_json_options" in game and not game["send_json_options"]:
            return

        call = game.get_call_base()

        call += ["-name", player_name,
                 "-rc", self.rcfile_path(game_id)]
        if "options" in game:
            call += game.templated("options", username=self.username)
        call.append("-print-webtiles-options")

        checkoutput.check_output(call, do_send)

    def watch(self, username):
        if self.is_running():
            self.process.stop()

        if not self.update_db_info():
            self.go_lobby()
            return

        if self.username and self.account_restricted():
            self.go_lobby(message="Your account is restricted; spectating unavailable")
            return

        if not self.username and not config.get('allow_anon_spectate'):
            self.send_message("auth_error",
                        reason="Anonymous spectating disabled")
            self.go_lobby()
            return

        from webtiles.process_handler import processes
        procs = [process for process in list(processes.values())
                 if process.username.lower() == username.lower()]
        if len(procs) >= 1:
            process = procs[0]
            # has the spectatee blocked this user?
            if process.is_blocked(self.username) and not self.is_admin():
                self.send_message("auth_error",
                            reason="Spectating this player is restricted.")
                self.go_lobby()
                return
            r = process.get_primary_receiver()
            # the spectatee has a restricted account; you can only get to this
            # by manually typing a url
            if r and r.account_restricted():
                self.go_lobby()
                return

            if self.watched_game:
                if self.watched_game == process:
                    return
                self.stop_watching()
            self.logger.info("%s started watching %s (%s).",
                                self.username and self.username or "[Anon]",
                                process.username, process.id)

            self.watched_game = process
            process.add_watcher(self)
            self.send_message("watching_started", username = process.username)
            if not config.get('max_chat_length'):
                process.handle_notification(self.username,
                                            "Chat is disabled on this server")
        else:
            if self.watched_game:
                self.stop_watching()
            self.go_lobby()

    def post_chat_message(self, text):
        max_length = config.get('max_chat_length')
        if max_length:
            max_length = max(100, int(max_length)) # if it's set, enforce a minimum
            if len(text) >= max_length:
                text = text[:max_length - 5] + "[...]"
        receiver = None
        if self.process:
            receiver = self.process
        elif self.watched_game:
            receiver = self.watched_game

        if receiver:
            if self.account_restricted():
                self.send_message("chat",
                        content='Your account is restricted; chat unavailable')
                return
            if self.username is None:
                self.send_message("chat",
                        content='You need to log in to send messages!')
                return

            if not receiver.handle_chat_command(self, text):
                if not max_length:
                    return # chat disabled!
                receiver.handle_chat_message(self.username, text)

    def register(self, username, password, email):
        error = userdb.register_user(username, password, email)
        if error is None:
            self.logger.info("[Account] Registered user %s (IP: %s, email: %s).",
                            username, self.request.remote_ip, email)
            self.do_login(username)
        else:
            self.logger.info("[Account] Registration attempt failed for user %s (IP: %s): %s",
                             username, self.request.remote_ip, error)
            self.send_message("register_fail", reason = error)

    def start_change_password(self):
        self.reset_lobby_timeout()
        self.send_message("start_change_password")

    def change_password(self, cur_password, new_password):
        self.reset_lobby_timeout()

        if not self.update_db_info():
            self.send_message("change_password_fail", reason="Account is disabled")
            return

        if self.username is None:
            self.send_message("change_password_fail",
                    reason="You need to log in to change your password.")
            return

        if self.account_restricted():
            self.send_message("change_password_fail",
                    reason="Your account is restricted; change password unavailable.")
            return

        if not userdb.user_passwd_match(self.username, cur_password)[0]:
            self.send_message("change_password_fail", reason = "Your password didn't match.")
            self.logger.info("Non-matching current password during password change for %s", self.username)
            return

        error = userdb.change_password(self.user_id, new_password)
        if error is None:
            self.logger.info("User %s changed password.", self.username)
            self.send_message("change_password_done")
        else:
            self.logger.info("Failed to change username for %s: %s", self.username, error)
            self.send_message("change_password_fail", reason = error)


    def start_change_email(self):
        self.reset_lobby_timeout()
        self.send_message("start_change_email", email = self.user_email)

    def change_email(self, email):
        self.reset_lobby_timeout()

        if not self.update_db_info():
            self.send_message("change_email_fail", reason="Account is disabled")
            return

        if self.username is None:
            self.send_message("change_email_fail", reason = "You need to log in to change your email")
            return
        if self.account_restricted():
            self.send_message("change_email_fail",
                    reason="Your account is restricted; change email unavailable.")
            return
        error = userdb.change_email(self.user_id, email)
        if error is None:
            self.update_db_info()
            self.logger.info("User %s changed email to %s.",
                self.username, self.user_email if self.user_email else "null")
            self.send_message("change_email_done", email = self.user_email)
        else:
            self.logger.info("Failed to change username for %s: %s", self.username, error)
            self.send_message("change_email_fail", reason = error)

    def forgot_password(self, email):
        if not config.get("allow_password_reset", False):
            return
        self.reset_lobby_timeout()

        sent, error = userdb.send_forgot_password(email)
        if error is None:
            if sent:
                self.logger.info("Sent password reset email to %s.", email)
            else:
                self.logger.info("User requested a password reset, but email "
                                 "is not registered (%s).", email)
            self.send_message("forgot_password_done")
        else:
            self.logger.info("Failed to send password reset email for %s: %s",
                             email, error)
            self.send_message("forgot_password_fail", reason = error)

    def reset_password(self, token, password):
        self.reset_lobby_timeout()

        username, error = userdb.update_user_password_from_token(token,
                                                                 password)
        if error is None:
            self.logger.info("User %s has completed their password reset.",
                             username)
            self.send_message("reload_url")
        else:
            if username is None:
                self.logger.info("Failed to update password for token %s: %s",
                                 token, error)
            else:
                self.logger.info("Failed to update password for user %s: %s",
                                 username, error)
            self.send_message("reset_password_fail", reason = error)

    def go_lobby(self, message=None):
        if not config.get('dgl_mode'):
            return

        if message:
            # a bit hacky: puts the message above the login box / username
            self.send_message("auth_error", reason=message)

        if self.is_running():
            self.process.stop()

        # note -- lobby timer is not guaranteed to be started here, because
        # process stop is async
        self.reset_idle_timeouts()

        if self.username and userdb.dgl_is_banned(self.user_flags):
            # force a logout. Note that this doesn't check the db at this point
            # in order to reduce i/o a bit.
            fail_reason = 'Account is disabled.'
            self.logger.warning("[Account] Logging out user %s: %s",
                                    self.username, fail_reason)
            self.username = self.user_email = self.user_flags = self.user_id = None
            self.send_message("go_lobby")
            self.send_lobby_html()
            self.send_message("logout", reason=fail_reason)
            return

        if self.watched_game:
            self.stop_watching()
            self.send_message("go_lobby")
            self.send_lobby()
        else:
            self.send_message("go_lobby")

    def go_admin(self):
        self.go_lobby()
        self.send_message("go_admin")

    def get_rc(self, game_id):
        if game_id not in config.games: return

        self.reset_lobby_timeout()

        path = self.rcfile_path(game_id)
        try:
            with util.SlowWarning("Slow IO: read rc '%s'" % path):
                with open(path, 'r') as f:
                    contents = f.read()
        # Handle RC file not existing. IOError for py2, OSError for py3
        except (OSError, IOError):
            contents = ''
        self.send_message("rcfile_contents", contents = contents)

    def set_rc(self, game_id, contents):
        rcfile_path = self.rcfile_path(game_id)

        self.reset_lobby_timeout()

        try:
            with util.SlowWarning("Slow IO: write rc '%s'" % rcfile_path):
                with open(rcfile_path, 'wb') as f:
                    # TODO: is binary + encode necessary in py 3?
                    f.write(utf8(contents))
        except Exception:
            self.logger.warning(
                    "Couldn't save rcfile for %s!",
                    self.username, exc_info=True)
            return False
        return True

    def on_message(self, message): # type: (Union[str, bytes]) -> None
        try:
            obj = json_decode(message) # type: Dict[str, Any]
            if obj["msg"] in self.message_handlers:
                handler = self.message_handlers[obj["msg"]]
                del obj["msg"]
                handler(**obj)
            elif self.process:
                self.process.handle_input(message)
            elif (not self.watched_game
                    and obj["msg"] != 'ui_state_sync'
                    and obj["msg"] != 'key'):
                # ui_state_sync can get queued by the js client just before
                # shutdown, and have its sending delayed by enough that the
                # process has stopped. I do not currently think there's a
                # principled way to suppress it with this timing on the js
                # side (because it's basically just a consequence of general
                # ui code), so have an ugly exception here. Otherwise, various
                # game ending conditions log a warning. TODO: fixes?
                self.logger.warning("Didn't know how to handle msg (user %s): %s",
                                    self.username and self.username or "[Anon]",
                                    obj["msg"])
            if self.failed_on_messages > 1:
                self.logger.warning("%d more on_message errors...", self.failed_on_messages)
            self.failed_on_messages = 0
        except OSError as e:
            # maybe should throw a custom exception from the socket call rather
            # than rely on these cases?
            if self.failed_on_messages == 0:
                excerpt = message[:50] if len(message) > 50 else message
                trunc = "..." if len(message) > 50 else ""
                if e.errno == errno.EAGAIN or e.errno == errno.ENOBUFS:
                    # errno is different on mac vs linux, maybe also depending on
                    # python version
                    self.logger.warning(
                        "Socket buffer full; skipping JSON message ('%r%s')!",
                                    excerpt, trunc)
                else:
                    self.logger.warning(
                                    "Error while handling JSON message ('%r%s')!",
                                    excerpt, trunc,
                                    exc_info=True)
            self.failed_on_messages += 1
        except Exception:
            if self.failed_on_messages == 0:
                excerpt = message[:50] if len(message) > 50 else message
                trunc = "..." if len(message) > 50 else ""
                self.logger.warning("Error while handling JSON message ('%r%s')!",
                                    excerpt, trunc,
                                    exc_info=True)
            self.failed_on_messages += 1

    def _encode_for_send(self, msg, deflate):
        try:
            binmsg = utf8(msg)
            if deflate:
                # Compress like in deflate-frame extension:
                # Apply deflate, flush, then remove the 00 00 FF FF
                # at the end
                # note: a compressed stream is stateful, you can't use this
                # compressobj for other sockets
                compressed = self._compressobj.compress(binmsg)
                compressed += self._compressobj.flush(zlib.Z_SYNC_FLUSH)
                compressed = compressed[:-4]
                return MessageBundle(binmsg, compressed)
            else:
                return MessageBundle(binmsg, None)
        except:
            # might happen with weird utf-8 stuff...can this be handled more
            # precisely?
            self.logger.warning("Exception trying to encode message.", exc_info = True)
            return MessageBundle(None, None)


    # send a single message batch, encoding and compressing it if necessary
    def _send_raw_message(self, msg):
        if self.client_closed or not msg:
            return False

        bundle = self._encode_for_send(msg, self.deflate)
        if not bundle:
            self.failed_messages += 1
            return False

        try:
            self.total_message_bytes += len(bundle.binmsg)
            if self.deflate:
                self.compressed_bytes_sent += len(bundle.compressed)
                f = self.write_message(bundle.compressed, binary=True)
            else:
                self.uncompressed_bytes_sent += len(bundle.binmsg)
                f = self.write_message(bundle.binmsg)

            import traceback
            cur_stack = traceback.format_stack()

            # handle any exceptions lingering in the Future
            # TODO: this whole call chain should be converted to use coroutines
            def after_write_callback(f):
                try:
                    f.result()
                except tornado.websocket.StreamClosedError as e:
                    # not supposed to be raised here in current versions of
                    # tornado, but in some older versions it is.
                    if self.failed_messages == 0:
                        self.logger.warning("Connection closed during async write_message")
                    self.failed_messages += 1
                    if self.ws_connection is not None:
                        self.ws_connection._abort()
                except tornado.websocket.WebSocketClosedError as e:
                    if self.failed_messages == 0:
                        self.logger.warning("Connection closed during async write_message")
                    self.failed_messages += 1
                    if self.ws_connection is not None:
                        self.ws_connection._abort()
                except Exception as e:
                    self.logger.warning("Exception during async write_message, stack at call:")
                    self.logger.warning("".join(cur_stack))
                    self.logger.warning(e, exc_info=True)
                    self.failed_messages += 1
                    if self.ws_connection is not None:
                        self.ws_connection._abort()

            # extreme back-compat try-except block, `f` should be None in
            # ancient tornado versions
            try:
                f.add_done_callback(after_write_callback)
            except:
                pass
            # true means that something was queued up to send, but it may be
            # async
            return True
        except tornado.websocket.WebSocketClosedError as e:
            if self.failed_messages == 0:
                self.logger.warning("Connection closed during write_message")
            self.failed_messages += 1
            if self.ws_connection is not None:
                self.ws_connection._abort()
        except:
            self.logger.warning("Exception trying to send message.", exc_info = True)
            self.failed_messages += 1
            if self.ws_connection is not None:
                self.ws_connection._abort()
            return False

    # send anything in the per-socket queue
    def flush_messages(self):
        # type: () -> bool
        if self.client_closed or len(self.message_queue) == 0:
            return False

        batch = ("{\"msgs\":["
            + ",".join(self.message_queue)
            + "]}")
        self.message_queue = [] # always empty the queue
        return self._send_raw_message(batch)

    # n.b. this looks a lot like superclass write_message, but has a static
    # type signature that is not compatible with it, so we do not override
    # that function.
    def append_message(self,
                       msg,      # type: str
                       send=True # type: bool
                       ):
        # type: (...) -> bool
        if self.client_closed:
            return False
        self.message_queue.append(msg)
        if send:
            return self.flush_messages()
        return False

    def send_message(self, msg, **data):
        # type: (str, Any) -> bool
        """Sends a JSON message to the client."""
        data["msg"] = msg
        return self.append_message(json_encode(data), True)

    def queue_message(self, msg, **data):
        # type: (str, Any) -> bool
        data["msg"] = msg
        return self.append_message(json_encode(data), False)

    def try_cleanup(self):
        global sockets
        # try to do what cleanup can be done on game end or close at the
        # current point in time.
        # If there is a running process, some cleanup will need to be
        # deferred, and if the client is not closed only the process will
        # be cleaned up.
        if not self.is_running():
            self.process = None
            if self.client_closed:
                self.clear_timeouts()
                if self in sockets:
                    sockets.remove(self)
            else:
                self.reset_idle_timeouts()

        if self.watched_game:
            self.watched_game.remove_watcher(self)

    def on_close(self):
        # at this point, self.client_closed is guaranteed to be true
        extra = []
        if self.is_running():
            # this will (sooner or later) trigger _on_crawl_end, which will
            # remove `self` from `sockets`
            # XX would it be better to handle on_close messaging at that point?
            self.process.stop()
        elif self in sockets and self.process:
            # buggy state (I think): `self.process` did not get set to
            # None, but process is not actually running.
            extra += ["stale process"]

        self.try_cleanup()

        if self.total_message_bytes == 0:
            comp_ratio = "N/A"
        else:
            comp_ratio = 100 - 100 * (self.compressed_bytes_sent + self.uncompressed_bytes_sent) / self.total_message_bytes
            comp_ratio = round(comp_ratio, 2)

        if self.failed_messages > 0 or self.failed_on_messages > 0:
            extra += ["%d (client) + %d (process) failed messages" % (self.failed_messages, self.failed_on_messages)]

        extra_msg = ""
        if len(extra):
            extra_msg = ", " + ", ".join(extra)

        self.logger.info("Socket closed. (%s sent, compression ratio %s%%%s)",
                         util.humanise_bytes(self.total_message_bytes),
                         comp_ratio,
                         extra_msg)
