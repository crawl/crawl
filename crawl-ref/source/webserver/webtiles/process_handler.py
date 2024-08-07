import datetime
import errno
import fcntl
import hashlib
import logging
import os
import os.path
import re
import subprocess
import time

from tornado.escape import json_decode
from tornado.escape import json_encode
from tornado.escape import to_unicode
from tornado.escape import utf8
from tornado.escape import xhtml_escape
from tornado.ioloop import IOLoop

from webtiles import config, connection, game_data_handler, inotify, terminal, util, ws_handler
from webtiles.config import dgl_format_str
from webtiles.connection import WebtilesSocketConnection
from webtiles.game_data_handler import GameDataHandler
from webtiles.inotify import DirectoryWatcher
from webtiles.terminal import TerminalRecorder
from webtiles.util import DynamicTemplateLoader, parse_where_data, PeriodicCallback
from webtiles.ws_handler import CrawlWebSocket, remove_in_lobbys, update_all_lobbys

try:
    from typing import Dict, Set, Tuple, Any
except:
    pass

last_game_id = 0

processes = dict() # type: Dict[str,CrawlProcessHandler]
unowned_process_logger = logging.LoggerAdapter(logging.getLogger(), {})

# this will not template username...
def find_game_info(socket_dir, socket_file):
    game_id = socket_file[socket_file.index(":")+1:-5]
    if (game_id in config.games and
                    os.path.abspath(config.game_param(game_id, "socket_path")) == os.path.abspath(socket_dir)):
        return config.games[game_id]

    game_info = None
    for game_id in config.games:
        if os.path.abspath(config.game_param(game_id, "socket_path")) == os.path.abspath(socket_dir):
            game_info = config.games[game_id]
            break
    return game_info

def handle_new_socket(path, event):
    dirname, filename = os.path.split(path)
    if ":" not in filename or not filename.endswith(".sock"): return
    username = filename[:filename.index(":")]
    abspath = os.path.abspath(path)
    if event == DirectoryWatcher.CREATE:
        if abspath in processes: return # Created by us

        # Find a game_info with this socket path
        game_info = find_game_info(dirname, filename)

        # Create process handler
        process = CrawlProcessHandler(game_info, username,
                                      unowned_process_logger)
        processes[abspath] = process

        try:
            process.connect(abspath)
        except ConnectionRefusedError:
            process.logger.error(
                "Crawl process socket connection refused for %s, socketpath '%s'.",
                game_info["id"], abspath, exc_info=True)
            del processes[abspath]
            return

        process.logger.info("Found a %s game for user %s.", game_info["id"], username)

        # Notify lobbys
        if config.get('dgl_mode'):
            update_all_lobbys(process)
    elif event == DirectoryWatcher.DELETE:
        if abspath not in processes: return
        process = processes[abspath]
        if process.process: return # Handled by us, will be removed later
        process.handle_process_end()
        process.logger.info("Game ended.")
        remove_in_lobbys(process)
        del processes[abspath]

def watch_socket_dirs():
    watcher = DirectoryWatcher()
    added_dirs = set()
    for game_id in config.games:
        socket_dir = os.path.abspath(config.game_param(game_id, "socket_path"))
        if socket_dir in added_dirs:
            continue
        watcher.watch(socket_dir, handle_new_socket)

class CrawlProcessHandlerBase(object):
    def __init__(self, game_params, username, logger):
        self.game_params = game_params
        self.username = username
        self.logger = logging.LoggerAdapter(logger, {})
        try:
            self.logger.manager
            self.logger.process = self._process_log_msg
        except AttributeError:
            # This is a workaround for a python 3.5 bug with chained
            # LoggerAdapters, where delegation is not handled properly (e.g.
            # manager isn't set, _log isn't available, etc.). This simple fix
            # only handles two levels of chaining. The general fix is to
            # upgrade to python 3.7.
            # Issue: https://bugs.python.org/issue31457
            self.logger = logging.LoggerAdapter(logger.logger, {})
            self.logger.process = lambda m,k: logger.process(*self._process_log_msg(m, k))

        self.queue_messages = False

        self.process = None
        self.client_path = self.config_path("client_path")
        self.crawl_version = None
        self.where = {}
        self.wheretime = 0
        self.last_milestone = None
        self.kill_timeout = None

        self.blocked = set()
        self.kicked = {} # not persisted across sessions!

        now = datetime.datetime.utcnow()
        self.formatted_time = now.strftime("%Y-%m-%d.%H:%M:%S")
        self.lock_basename = self.formatted_time + ".ttyrec"

        self.end_callback = None
        self._receivers = set()
        self.last_activity_time = time.time()
        self.idle_checker = PeriodicCallback(self.check_idle, 10000)
        self.idle_checker.start()
        self._was_idle = False
        self.last_watcher_join = 0
        self.receiving_direct_milestones = False

        global last_game_id
        self.id = last_game_id + 1
        last_game_id = self.id

    def _process_log_msg(self, msg, kwargs):
        return "P%-5s %s" % (self.id, msg), kwargs

    def format_path(self, path):
        return dgl_format_str(path, self.username, self.game_params)

    def config_path(self, key):
        if key not in self.game_params:
            return None
        base_path = self.game_params.templated(key, username=self.username)
        if key == "socket_path" and config.get('live_debug'):
            # TODO: this is kind of brute-force given that regular paths aren't
            # validated at all...
            debug_path = os.path.join(base_path, 'live-debug')
            if not os.path.isdir(debug_path):
                os.makedirs(debug_path)
            return debug_path
        else:
            return base_path

    def idle_time(self):
        return int(time.time() - self.last_activity_time)

    def is_idle(self):
        return self.idle_time() > 30

    def check_idle(self):
        if self.is_idle() != self._was_idle:
            self._was_idle = self.is_idle()
            if config.get('dgl_mode'):
                update_all_lobbys(self)

    def flush_messages_to_all(self):
        for receiver in self._receivers:
            receiver.flush_messages()

    def _is_full_map_msg(self, msg):
        # heuristic: map bundles can be very large (100k+), so we don't want to
        # decode. But the crawl process is guaranteed to put some key info
        # first. Use some brute force regexes to check it.
        tocheck = msg[0:50]
        return (re.search(r'"msg" *: *"map"', tocheck)
                and re.search(r'"clear" *: *true', tocheck))

    def handle_process_message(self, msg, send): # type: (str, bool) -> None
        # special handling for map messages on a new spectator: these can be
        # massive, and the deflate time adds up, so only send it to new
        # spectators. This is all a bit heuristic, and probably could use
        # a new control message instead...
        # TODO: if multiple spectators join at the same time, it's probably
        # possible for this heuristic to fail and send a full map to everyone
        if self._fresh_watchers and self._is_full_map_msg(msg):
            for w in self._fresh_watchers:
                w.append_message(msg, send)
            self._fresh_watchers = set()
            return
        for receiver in self._receivers:
            receiver.append_message(msg, send)

    def send_to_all(self, msg, **data): # type: (str, Any) -> None
        for receiver in self._receivers:
            receiver.send_message(msg, **data)

    def chat_help_message(self, source, command, desc):
        if len(command) == 0:
            self.handle_notification_raw(source,
                        "&nbsp;" * 8 + "<span>%s</span>" % (xhtml_escape(desc)))
        else:
            self.handle_notification_raw(source,
                        "&nbsp;" * 4 + "<span>%s: %s</span>" %
                                (xhtml_escape(command), xhtml_escape(desc)))

    def chat_command_help(self, source):
        # TODO: generalize
        # the chat window is basically fixed width, and these are calibrated
        # to not do a linewrap
        self.handle_notification(source, "The following chat commands are available:")
        self.chat_help_message(source, "/help", "show chat command help")
        self.chat_help_message(source, "/hide", "hide the chat window")
        if self.is_player(source):
            self.chat_help_message(source, "/kick <name> [<minutes>]", "kick <name> for some time")
            self.chat_help_message(source, "", "default: 15 minutes. Not saved across sessions.")
            self.chat_help_message(source, "/block <name>", "kick and block <name>")
            self.chat_help_message(source, "", "Must be present in channel. Special names:")
            self.chat_help_message(source, "", "[anon] disables anonymous spectating")
            self.chat_help_message(source, "", "[all] disables spectating")
            self.chat_help_message(source, "/blocklist", "show your entire blocklist")
            self.chat_help_message(source, "/unblock <name>", "remove <name> from blocklist")
            self.chat_help_message(source, "/unblock *", "clear your blocklist")

    def handle_chat_command(self, source_ws, text):
        # type: (CrawlWebSocket, str) -> bool
        source = source_ws.username
        if len(text) >= 500:
            # sanity check, distinct from chat max length
            text = text[:500]
        text = text.strip()
        if len(text) == 0 or text[0] != '/':
            return False
        splitlist = text.split(None, 1)
        if len(splitlist) == 1:
            command = splitlist[0]
            remainder = ""
        else:
            command, remainder = splitlist
        command = command.lower()
        # TODO: generalize
        if command == "/block" or command == "/mute":
            self.block(source, remainder)
        elif command == "/unblock" or command == "/unmute":
            self.unblock(source, remainder)
        elif command == "/blocklist" or command == "/mutelist":
            self.show_block_list(source)
        elif command == "/help":
            self.chat_command_help(source)
        elif command == "/hide":
            self.hide_chat(source_ws, remainder.strip())
        elif command == "/kick":
            self.kick(source, remainder)
        else:
            return False
        return True

    def is_blocked(self, username):
        if "[all]" in self.blocked:
            return username != self.username
        if not username:
            return "[anon]" in self.blocked
        if username in self.kicked:
            start, interval = self.kicked[username]
            if time.time() - start < interval:
                return True
            else:
                # this doesn't otherwise get cleaned up, so in principle it's
                # a minor memory leak for a long-running process
                del self.kicked[username]
        return username in self.blocked

    def handle_chat_message(self, username, text): # type: (str, str) -> None
        chat_msg = ("<span class='chat_sender'>%s</span>: <span class='chat_msg'>%s</span>" %
                    (username, xhtml_escape(text)))
        self.send_to_all("chat", content = chat_msg)

    def get_receivers_by_username(self, username):
        result = list()
        for w in self._receivers:
            if not w.username:
                continue
            if w.username == username:
                result.append(w)
        return result

    def get_primary_receiver(self):
        # TODO: does this work with console? Probably not...
        if self.username is None:
            return None
        receivers = self.get_receivers_by_username(self.username)
        for r in receivers:
            if not r.watched_game:
                return r
        return None

    def update_receiver_permissions(self):
        for w in self._receivers:
            if self.is_blocked(w.username) and not w.is_admin():
                IOLoop.current().add_callback(
                    lambda w=w: w.go_lobby(message="Spectating this player is restricted."))

    def send_to_user(self, username, msg, **data):
        # type: (str, str, Any) -> None
        # a single user may be viewing from multiple receivers
        for receiver in self.get_receivers_by_username(username):
            receiver.send_message(msg, **data)

    # obviously, don't use this for player/spectator-accessible data. But, it
    # is still partially sanitized in chat.js.
    def handle_notification_raw(self, username, text):
        # type: (str, str) -> None
        msg = ("<span class='chat_msg'>%s</span>" % text)
        self.send_to_user(username, "chat", content=msg, meta=True)

    def handle_notification(self, username, text):
        # type: (str, str) -> None
        self.handle_notification_raw(username, xhtml_escape(text))

    def handle_process_end(self):
        if self.kill_timeout:
            IOLoop.current().remove_timeout(self.kill_timeout)
            self.kill_timeout = None

        self.idle_checker.stop()

        # send game_ended message to watchers. The player is handled in cleanup
        # code in ws_handler.py.
        for watcher in list(self._receivers):
            if watcher.watched_game == self:
                watcher.send_message("game_ended", reason = self.exit_reason,
                                     message = self.exit_message,
                                     dump = self.exit_dump_url)
                # these are individually ok, but with a lot of spectators,
                # doing them in a big loop can easily add up to 100s of ms
                IOLoop.current().add_callback(watcher.go_lobby)

        if self.end_callback:
            self.end_callback()

    def get_watchers(self, chatting_only=False, mark_admins=False):
        # TODO: I don't understand why this code didn't just use self.username,
        # when will this be different than player_name? Maybe for a console
        # player?
        player_name = None
        watchers = list()
        for w in self._receivers:
            if not w.username: # anon
                continue
            if chatting_only and w.chat_hidden:
                continue
            if not w.watched_game:
                player_name = w.username
            else:
                watchers.append(w.username)
                if mark_admins and w.is_admin():
                    watchers[-1] += " (admin)"
        watchers.sort(key=lambda s:s.lower())
        return (player_name, watchers)

    def is_player(self, username):
        # TODO: probably doesn't work for console players spectating themselves
        # TODO: let admin accounts block as well?
        player_name, watchers = self.get_watchers()
        return (username == player_name)

    def hide_chat(self, receiver, param):
        if param == "forever":
            receiver.send_message("super_hide_chat")
            receiver.chat_hidden = True # currently only for super hidden chat
            self.update_watcher_description()
        else:
            receiver.send_message("toggle_chat")

    def restore_blocklist(self, source, l):
        if not self.is_player(source) or l is None:
            return
        if len(l) == 0:
            return
        self.blocked = {u for u in l if u != source}
        self.handle_notification(source, "Restoring blocklist.")
        self.show_block_list(source)
        self.logger.debug("Player '%s' restoring blocklist %s" %
                                            (source, repr(list(self.blocked))))

    def save_blocklist(self, source):
        if not self.is_player(source):
            return
        receiver = self.get_primary_receiver()
        if receiver is not None:
            receiver.save_blocklist(list(self.blocked))

    def kick(self, source, target_args):
        if not self.is_player(source):
            self.handle_notification(source,
                            "You do not have permission to kick spectators.")
            return False

        args = target_args.split(maxsplit=1)
        args.extend([''] * (2 - len(args))) # pad missing args
        target = args[0]
        interval = 15
        try:
            interval = int(args[1])
        except ValueError:
            pass

        if (source == target):
            self.handle_notification(source, "You can't kick yourself!")
            return False
        player_name, watchers = self.get_watchers()
        watchers = set(watchers)

        if target in watchers:
            self.logger.info("Player '%s' has kicked '%s' (%dm)" % (source, target, interval))
            self.handle_notification(source,
                "Spectator '%s' has been kicked for %d minutes." % (target, interval))
        else:
            self.handle_notification(source, "Kick who??")
            return False

        self.kicked[target] = [time.time(), interval * 60]

        receivers = self.get_receivers_by_username(target)
        for w in receivers:
            if not w.is_admin():
                IOLoop.current().add_callback(
                    lambda w=w: w.go_lobby(message="You have been kicked."))
        return True


    def block(self, source, target):
        if not self.is_player(source):
            self.handle_notification(source,
                            "You do not have permission to block spectators.")
            return False
        if (source == target):
            self.handle_notification(source, "You can't block yourself!")
            return False
        player_name, watchers = self.get_watchers()
        watchers = set(watchers)

        if target in watchers:
            self.logger.info("Player '%s' has blocked '%s'" % (source, target))
            self.handle_notification(source,
                                "Spectator '%s' has now been blocked." % target)
        elif target == "[anon]":
            self.logger.info("Player '%s' has disabled anonymous spectating." % source)
            self.handle_notification(source, "Anonymous spectating disabled.")
        elif target == "[all]":
            self.logger.info("Player '%s' has disabled spectating." % source)
            self.handle_notification(source, "Spectating disabled.")
        else:
            # this means that players can't block a username that isn't currently
            # in their chat -- good/bad?
            self.handle_notification(source, "Block who??")
            return False
        self.blocked |= {target}
        self.save_blocklist(source)
        self.update_receiver_permissions() # async
        return True

    def unblock(self, source, target):
        if not self.is_player(source):
            self.handle_notification(source,
                            "You do not have permission to unblock spectators.")
            return False
        if (source == target):
            self.handle_notification(source,
                                    "You can't unblock (or block) yourself!")
            return False
        if target == "*":
            if len(self.kicked):
                self.kicked = {}
                self.handle_notification(source, "Kicks cleared.")
            if (len(self.blocked) == 0):
                self.handle_notification(source, "No one is blocked!")
                return False
            self.logger.info("Player '%s' has cleared their blocklist." % (source))
            self.handle_notification(source, "You have cleared your blocklist.")
            self.blocked = set()
            self.save_blocklist(source)
            self.update_watcher_description()
            return True

        did_something = False
        if target in self.kicked:
            del self.kicked[target]
            self.handle_notification(source, "Removing '%s' from kicks" % target)
            did_something = True

        if not target in self.blocked:
            if not did_something:
                self.handle_notification(source, "Unblock who??")
            return False

        # these messages are more or less ok for [all], [anon]
        self.logger.info("Player '%s' has unblocked '%s'" % (source, target))
        self.handle_notification(source, "You have unblocked '%s'." % target)
        self.blocked -= {target}
        self.save_blocklist(source)
        self.update_watcher_description()
        return True

    def show_block_list(self, source):
        if not self.is_player(source):
            return False
        names = list(self.blocked)
        names.sort(key=lambda s: s.lower())
        if len(names) == 0:
            self.handle_notification(source, "No one is blocked.")
        else:
            self.handle_notification(source, "You have blocked: " +
                                                            ", ".join(names))
        return True

    def get_anon(self):
        return [w for w in self._receivers if not w.username]

    def update_watcher_description(self):
        player_url_template = config.get('player_url')

        def wrap_name(watcher, is_player=False):
            if is_player:
                class_type = 'player'
            else:
                class_type = 'watcher'
            n = watcher
            if player_url_template is None:
                return "<span class='{0}'>{1}</span>".format(class_type, n)
            player_url = player_url_template.replace("%s", watcher.lower())
            username = "<a href='{0}' target='_blank' class='{1}'>{2}</a>".format(player_url, class_type, n)
            return username

        player_name, watchers = self.get_watchers(True, True)

        watcher_names = []
        if player_name is not None:
            watcher_names.append(wrap_name(player_name, True))
        watcher_names += [wrap_name(w) for w in watchers]

        anon_count = len(self.get_anon())
        s = ", ".join(watcher_names)
        if len(watcher_names) > 0 and anon_count > 0:
            s = s + " and %i Anon" % anon_count
        elif anon_count > 0:
            s = "%i Anon" % anon_count
        self.send_to_all("update_spectators",
                         count = self.watcher_count(),
                         names = s)

        if config.get('dgl_mode'):
            update_all_lobbys(self)

    def add_watcher(self, watcher):
        self.last_watcher_join = time.time()
        if self.client_path:
            self._send_client(watcher)
            if watcher.watched_game == self:
                watcher.send_json_options(self.game_params.id, self.username)
        self._receivers.add(watcher)
        self.update_watcher_description()

    def remove_watcher(self, watcher):
        # if both users quit around the same time, this can get out of sync;
        # don't crash in that case
        if watcher in self._receivers:
            self._receivers.remove(watcher)
            self.update_watcher_description()

    def watcher_count(self):
        return len([w for w in self._receivers if w.watched_game and not w.chat_hidden])

    def send_client_to_all(self):
        for receiver in self._receivers:
            self._send_client(receiver)
            if receiver.watched_game == self:
                receiver.send_json_options(self.game_params.id,
                                           self.username)

    def _send_client(self, watcher):
        h = hashlib.sha1(utf8(os.path.abspath(self.client_path)))
        if self.crawl_version:
            h.update(utf8(self.crawl_version))
        v = h.hexdigest()
        GameDataHandler.add_version(v,
                                    os.path.join(self.client_path, "static"))

        templ_path = os.path.join(self.client_path, "templates")
        loader = DynamicTemplateLoader.get(templ_path)
        templ = loader.load("game.html")
        game_html = to_unicode(templ.generate(version = v))
        watcher.send_message("game_client", version = v, content = game_html)

    def stop(self, delay=False):
        if delay:
            IOLoop.current().call_later(0.2, self.stop)
            return
        if self.process:
            self.process.flush_ttyrec()
            try:
                self.process.send_signal(subprocess.signal.SIGHUP)
            except OSError as e:
                self.logger.error(f"Error {repr(e)} on SIGHUP to child process")
                self._on_process_end()
                return
            t = time.time() + config.get('kill_timeout')
            self.kill_timeout = IOLoop.current().add_timeout(t, self.kill)

    def kill(self):
        if self.process:
            self.logger.info("Killing crawl process after SIGHUP did nothing.")
            try:
                self.process.send_signal(subprocess.signal.SIGABRT)
            except OSError as e:
                self.logger.error(f"Error {repr(e)} on SIGKILL to child process")
                self._on_process_end()
                return
            finally:
                self.kill_timeout = None

    interesting_info = ("xl", "char", "place", "turn", "dur", "god", "title")

    def set_where_info(self, newwhere):
        # milestone doesn't count as "interesting" but the field is directly
        # handled when sending lobby info by looking at last_milestone
        milestone = bool(newwhere.get("milestone"))
        interesting = (milestone
                or newwhere.get("status") == "chargen"
                or any([self.where.get(key) != newwhere.get(key)
                        for key in CrawlProcessHandlerBase.interesting_info]))

        # ignore milestone sync messages for where purposes
        if newwhere.get("status") != "milestone_only":
            self.where = newwhere
        if milestone:
            self.last_milestone = newwhere
        if interesting:
            update_all_lobbys(self)

    def check_where(self):
        if self.receiving_direct_milestones:
            return
        morgue_path = self.config_path("morgue_path")
        wherefile = os.path.join(morgue_path, self.username + ".where")
        try:
            if os.path.getmtime(wherefile) > self.wheretime:
                self.wheretime = time.time()
                with open(wherefile, "r") as f:
                    wheredata = f.read()

                if wheredata.strip() == "": return

                try:
                    newwhere = parse_where_data(wheredata)
                except:
                    self.logger.warning("Exception while trying to parse where file!",
                                        exc_info=True)
                else:
                    if (newwhere.get("status") == "active" or
                                        newwhere.get("status") == "saved"):
                        self.set_where_info(newwhere)
        except (OSError, IOError):
            pass

    def account_restricted(self):
        # convenience function to check the ws_handler data.
        # this won't work with dgl games, but an account hold sets a dgl ban
        # flag so they can't log in that way
        r = self.get_primary_receiver()
        return r and r.account_restricted()

    def lobby_entry(self):
        u = self.username
        if self.account_restricted():
            u = "[account hold] " + u
        entry = {
            "id": self.id,
            "username": u,
            "spectator_count": self.watcher_count(),
            "idle_time": (self.idle_time() if self.is_idle() else 0),
            "game_id": self.game_params.id,
            }
        for key in CrawlProcessHandlerBase.interesting_info:
            if key in self.where:
                entry[key] = self.where[key]
        if self.last_milestone and self.last_milestone.get("milestone"):
            entry["milestone"] = self.last_milestone.get("milestone")
        return entry

    def human_readable_where(self):
        try:
            return "L{xl} {char}, {place}".format(**self.where)
        except KeyError:
            return ""

    def _base_call(self):
        game = self.game_params
        call = game.get_call_base()

        call += ["-name",   self.username,
                 "-rc",     os.path.join(self.config_path("rcfile_path"),
                                         self.username + ".rc"),
                 "-macro",  os.path.join(self.config_path("macro_path"),
                                         self.username + ".macro"),
                 "-morgue", self.config_path("morgue_path")]

        if self.account_restricted():
            call += ["-no-player-bones"]

        if "options" in game:
            call += game.templated("options", username=self.username)

        if "dir_path" in game:
            call += ["-dir", self.config_path("dir_path")]

        return call

    def note_activity(self):
        self.last_activity_time = time.time()
        self.check_idle()

    def handle_input(self, msg):
        raise NotImplementedError()

class CrawlProcessHandler(CrawlProcessHandlerBase):
    def __init__(self, game_params, username, logger):
        super(CrawlProcessHandler, self).__init__(game_params, username, logger)
        self.socketpath = None
        self.conn = None
        self.ttyrec_filename = None
        self.inprogress_lock = None
        self.inprogress_lock_file = None

        self.exit_reason = None
        self.exit_message = None
        self.exit_dump_url = None

        self._stale_pid = None
        self._stale_lockfile = None
        self._purging_timer = None
        self._process_hup_timeout = None

        self._fresh_watchers = set()

    def start(self):
        self._purge_locks_and_start(True)

    def stop(self, delay=False):
        # n.b. the super call here is partly async
        super(CrawlProcessHandler, self).stop(delay=delay)
        self._stop_purging_stale_processes()
        self._stale_pid = None

    def _purge_locks_and_start(self, firsttime=False):
        # Purge stale locks
        lockfile = self._find_lock()
        if lockfile:
            try:
                with open(lockfile) as f:
                    pid = f.readline()

                try:
                    pid = int(pid)
                except ValueError:
                    # pidfile is empty or corrupted, can happen if the server
                    # crashed. Just clear it...
                    self.logger.error("Invalid PID from lockfile %s, clearing", lockfile)
                    self._purge_stale_lock()
                    return

                self._stale_pid = pid
                self._stale_lockfile = lockfile
                if firsttime:
                    if self._kill_stale_process(check_pid_only=True):
                        # if we get here, the pid is real
                        hup_wait = 10
                        self.send_to_all("stale_processes",
                                         timeout=hup_wait,
                                         # is name really correct here?
                                         game=self.game_params.templated("name", username=self.username))
                        to = IOLoop.current().add_timeout(time.time() + hup_wait,
                                                          self._kill_stale_process)
                        self._process_hup_timeout = to
                    # else: _purge_stale_locks recurses to this function, so no
                    # need to do anything more here
                else:
                    self._kill_stale_process()
            except Exception:
                self.logger.error("Error while handling lockfile %s.", lockfile,
                                  exc_info=True)
                errmsg = ("Error while trying to terminate a stale process.\n" +
                          "Please contact an administrator.")
                self.exit_reason = "error"
                self.exit_message = errmsg
                self.exit_dump_url = None
                self.handle_process_end()
        else:
            # No more locks, can start
            self._start_process()

    def _stop_purging_stale_processes(self):
        if not self._process_hup_timeout: return
        IOLoop.current().remove_timeout(self._process_hup_timeout)
        self._stale_pid = None
        self._stale_lockfile = None
        self._purging_timer = None
        self._process_hup_timeout = None
        self.handle_process_end()

    def _find_lock(self):
        for path in os.listdir(self.config_path("inprogress_path")):
            if (path.startswith(self.username + ":") and
                path.endswith(".ttyrec")):
                return os.path.join(self.config_path("inprogress_path"),
                                    path)
        return None

    def _kill_stale_process(self, signal=subprocess.signal.SIGHUP, check_pid_only=False):
        if check_pid_only:
            signal = 0
        self._process_hup_timeout = None
        if self._stale_pid == None:
            return
        if signal == subprocess.signal.SIGHUP:
            self.logger.info("Purging stale lock at %s, pid %s.",
                             self._stale_lockfile, self._stale_pid)
        elif signal == subprocess.signal.SIGABRT:
            self.logger.warning("Terminating pid %s forcefully!",
                                self._stale_pid)
        # intentional missing `else`, don't message on 0

        try:
            os.kill(self._stale_pid, signal)
        except ProcessLookupError as e:
            # Process doesn't exist
            self._purge_stale_lock()
            if check_pid_only:
                return False
        except PermissionError as e:
            # Process does exist, but we don't have permissions. Unlikely
            # coincidence between an unrelated process + a stale lock?
            self.logger.error("Clearing stale(?) lock on permission error for pid %i", self._stale_pid)
            self._purge_stale_lock()
            if check_pid_only:
                return False
        except OSError as e:
            self.logger.error("Error while killing process %s.", self._stale_pid,
                              exc_info=True)
            errmsg = ("Error while trying to terminate a stale process.\n" +
                      "Please contact an administrator.")
            self.exit_reason = "error"
            self.exit_message = errmsg
            self.exit_dump_url = None
            self.handle_process_end()
            return False
        else:
            if check_pid_only:
                return True
            if signal == subprocess.signal.SIGABRT:
                self._purge_stale_lock()
            else:
                if signal == subprocess.signal.SIGHUP:
                    self._purging_timer = 10
                else:
                    self._purging_timer -= 1

                if self._purging_timer > 0:
                    IOLoop.current().add_timeout(time.time() + 1,
                                                 self._check_stale_process)
                else:
                    self.logger.warning("Couldn't terminate pid %s gracefully.",
                                        self._stale_pid)
                    self.send_to_all("force_terminate?")
                return
        self.send_to_all("hide_dialog")

    def _check_stale_process(self):
        self._kill_stale_process(0)

    def _do_force_terminate(self, answer):
        if answer:
            self._kill_stale_process(subprocess.signal.SIGABRT)
        else:
            self.handle_process_end()

    def _purge_stale_lock(self):
        if os.path.exists(self._stale_lockfile):
            os.remove(self._stale_lockfile)

        self._purge_locks_and_start(False)

    def _start_process(self):
        self.socketpath = os.path.join(self.config_path("socket_path"),
                                       self.username + ":" +
                                       self.formatted_time + ".sock")

        try: # Unlink if necessary
            os.unlink(self.socketpath)
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise

        game = self.game_params

        call = self._base_call() + ["-webtiles-socket", self.socketpath,
                                    "-await-connection"]

        ttyrec_path = self.config_path("ttyrec_path")
        if ttyrec_path and config.get('enable_ttyrecs'):
            self.ttyrec_filename = os.path.join(ttyrec_path, self.lock_basename)

        processes[os.path.abspath(self.socketpath)] = self

        if config.get('dgl_mode'):
            self.logger.info("Starting %s.", game["id"])
        else:
            self.logger.info("Starting game.")

        connected = False

        try:
            self.process = TerminalRecorder(call,
                                            self.logger,
                                            config.get('recording_term_size'),
                                            env_vars = game.templated("env", username=self.username, default={}),
                                            game_cwd = game.templated("cwd", username=self.username, default=None),)
            self.process.end_callback = self._on_process_end
            self.process.output_callback = self._on_process_output
            self.process.activity_callback = self.note_activity
            self.process.error_callback = self._on_process_error

            self.process.start(self.ttyrec_filename, self._ttyrec_id_header())

            self.gen_inprogress_lock()

            self.connect(self.socketpath, True)

            self.logger.debug("Crawl FDs: fd%s, fd%s.",
                             self.process.child_fd,
                             self.process.errpipe_read)

            self.last_activity_time = time.time()

            self.check_where()
        except Exception:
            self.logger.warning("Error while starting the Crawl process!", exc_info=True)
            self.exit_reason = "error"
            self.exit_message = "Error while starting the Crawl process!\nSomething has gone very wrong; please let a server admin know."
            self.exit_dump_url = None

            if self.process and self.process.is_started():
                # n.b. we delay a bit here so that the process has more
                # time to start up, and avoid race conditions. (I couldn't come
                # up with anything more reliable.) Also, currently
                # if the connection fails the crawl process will be in a state
                # where it ignores HUP, and the kill handler is needed.
                self.stop(delay=True)
            else:
                self._on_process_end()

    def connect(self, socketpath, primary = False):
        self.socketpath = socketpath
        self.conn = WebtilesSocketConnection(self.socketpath, self.logger)
        self.conn.message_callback = self._on_socket_message
        self.conn.close_callback = self._on_socket_close
        self.conn.username = self.username
        self.conn.connect(primary)

    def gen_inprogress_lock(self):
        self.inprogress_lock = os.path.join(self.config_path("inprogress_path"),
                                            self.username + ":" + self.lock_basename)
        with util.SlowWarning("gen_inprogress_lock '%s'" % self.inprogress_lock):
            f = open(self.inprogress_lock, "w")
            fcntl.lockf(f.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.inprogress_lock_file = f
            cols, lines = self.process.get_terminal_size()
            f.write("%s\n%s\n%s\n" % (self.process.pid, lines, cols))
            f.flush()

    def remove_inprogress_lock(self):
        if self.inprogress_lock_file is None: return
        with util.SlowWarning("remove_inprogress_lock '%s'" % self.inprogress_lock):
            fcntl.lockf(self.inprogress_lock_file.fileno(), fcntl.LOCK_UN)
            self.inprogress_lock_file.close()
            try:
                os.remove(self.inprogress_lock)
            except OSError:
                # Lock already got deleted
                pass

    def _ttyrec_id_header(self): # type: () -> bytes
        clrscr = b"\033[2J"
        crlf = b"\r\n"
        tstamp = int(time.time())
        ctime = time.ctime()
        return (clrscr + b"\033[1;1H" + crlf +
                 utf8("Player: %s" % self.username) + crlf +
                 # odd that this uses name
                 utf8("Game: %s" % self.game_params.templated("name", username=self.username)) + crlf +
                 utf8("Server: %s" % config.get('server_id')) + crlf +
                 utf8("Filename: %s" % self.lock_basename) + crlf +
                 utf8("Time: (%s) %s" % (tstamp, ctime)) + crlf +
                 clrscr)

    def _on_process_end(self):
        if self.process:
            self.logger.debug("Crawl PID %s terminated.", self.process.pid)
        else:
            self.logger.error("Crawl process failed to start, cleaning up.")

        self.remove_inprogress_lock()

        try:
            del processes[os.path.abspath(self.socketpath)]
        except KeyError:
            self.logger.warning("Process entry already deleted: %s", self.socketpath)

        self.process = None

        self.handle_process_end()

    def _on_socket_close(self):
        self.conn = None
        self.stop()

    def handle_process_end(self):
        if self.conn:
            self.conn.close_callback = None
            self.conn.close()
            self.conn = None

        super(CrawlProcessHandler, self).handle_process_end()


    def add_watcher(self, watcher):
        super(CrawlProcessHandler, self).add_watcher(watcher)

        if self.conn and self.conn.open:
            self._fresh_watchers.add(watcher)
            self.conn.send_message('{"msg":"spectator_joined"}')

    def handle_input(self, msg): # type: (str) -> None
        obj = json_decode(msg)

        if obj["msg"] == "input" and self.process:
            self.last_action_time = time.time()

            data = ""
            for x in obj.get("data", []):
                data += chr(x)

            data += obj.get("text", "")

            self.process.write_input(utf8(data))

        elif obj["msg"] == "force_terminate":
            self._do_force_terminate(obj["answer"])

        elif obj["msg"] == "stop_stale_process_purge":
            self._stop_purging_stale_processes()

        elif self.conn and self.conn.open:
            self.conn.send_message(utf8(msg))

    def handle_chat_message(self, username, text): # type: (str, str) -> None
        super(CrawlProcessHandler, self).handle_chat_message(username, text)

        if self.conn and self.conn.open:
            self.conn.send_message(json_encode({
                        "msg": "note",
                        "content": "%s: %s" % (username, text)
                        }))

    def handle_announcement(self, text):
        if self.conn and self.conn.open:
            self.conn.send_message(json_encode({
                        "msg": "server_announcement",
                        "content": text
                        }))

    def _on_process_output(self, line): # type: (str) -> None
        self.check_where()

        try:
            json_decode(line)
        except ValueError:
            self.logger.warning("Invalid JSON output from Crawl process: %s",
                                line)

        # send messages from wrapper scripts only to the player
        for receiver in self._receivers:
            if not receiver.watched_game:
                receiver.append_message(line, True)

    def _on_process_error(self, line): # type: (str) -> None
        morgue_url = self.game_params.templated("morgue_url", username=self.username)
        # The msg this is parsing can be found in dbg-asrt.cc:do_crash_dump
        # this is run line-by-line, so multi-line errors (the norm) may trigger
        # this call more than once
        if line.startswith("ERROR"):
            # header line, e.g. `ERROR in 'wizard.cc' at line 79: Intentional crash`
            self.exit_reason = "crash"
            if line.rfind(":") != -1:
                self.exit_message = line[line.rfind(":") + 1:].strip()
        elif line.find("crash report: ") >= 0:
            self.exit_reason = "crash"
            if morgue_url:
                match = re.search(r"crash report: (.*)", line)
                if match is not None and match.group(1):
                    self.exit_dump_url = morgue_url
                    self.exit_dump_url += os.path.splitext(os.path.basename(match.group(1)))[0]
        elif line.startswith("We crashed!"):
            # before 0.19-a0-1226-g81ff5c4599 everything was on one line; this
            # line prefix is still present but the match below fails.
            self.exit_reason = "crash"
            if morgue_url:
                match = re.search(r"\(([^)]+)\)", line)
                if match is not None:
                    self.exit_dump_url = morgue_url
                    self.exit_dump_url += os.path.splitext(os.path.basename(match.group(1)))[0]
        elif line.startswith("Writing crash info to"):
            # before 0.15-b1-84-gded71f8 the message was very minimal
            self.exit_reason = "crash"
            if morgue_url:
                url = None
                if line.rfind("/") != -1:
                    url = line[line.rfind("/") + 1:].strip()
                elif line.rfind(" ") != -1:
                    url = line[line.rfind(" ") + 1:].strip()
                if url is not None:
                    self.exit_dump_url = morgue_url + os.path.splitext(url)[0]

    def _on_socket_message(self, msg): # type: (str) -> None
        # stdout data is only used for compatibility to wrapper
        # scripts -- so as soon as we receive something on the socket,
        # we stop using stdout
        if self.process:
            self.process.output_callback = None

        if msg.startswith("*"):
            # Special message to the server
            msg = msg[1:]
            msgobj = json_decode(msg)
            if msgobj["msg"] == "client_path":
                if self.client_path == None:
                    # XX Why does this string from the msgobj get templated?
                    self.client_path = self.format_path(msgobj["path"])
                    if "version" in msgobj:
                        self.crawl_version = msgobj["version"]
                        self.logger.info("Crawl version: %s.", self.crawl_version)
                    self.send_client_to_all()
            elif msgobj["msg"] == "flush_messages":
                # only queue, once we know the crawl process asks for flushes
                # note: every version since 0.13 supports this
                self.queue_messages = True;
                self.flush_messages_to_all()
            elif msgobj["msg"] == "dump":
                morgue_url = self.game_params.templated("morgue_url", username=self.username)
                if morgue_url:
                    url = morgue_url + msgobj["filename"]
                    if msgobj["type"] == "command":
                        self.send_to_all("dump", url = url)
                    else:
                        self.exit_dump_url = url
            elif msgobj["msg"] == "exit_reason":
                self.exit_reason = msgobj["type"]
                if "message" in msgobj:
                    self.exit_message = msgobj["message"]
                else:
                    self.exit_message = None
            elif msgobj["msg"] == "milestone":
                # milestone/whereis update: milestone fields are right in the
                # message
                self.receiving_direct_milestones = True # no need for .where files
                self.set_where_info(msgobj)
            else:
                self.logger.warning("Unknown message from the crawl process: %s",
                                    msgobj["msg"])
        else:
            self.check_where()
            if time.time() > self.last_watcher_join + 2:
                # Treat socket messages as activity, since it's otherwise
                # hard to determine activity for games found via
                # watch_socket_dirs.
                # But don't if a spectator just joined, since we don't
                # want that to reset idle time.
                self.note_activity()

            self.handle_process_message(msg, not self.queue_messages)



class DGLLessCrawlProcessHandler(CrawlProcessHandler):
    def __init__(self, logger):
        # used when dgl_mode = False
        game_params = config.GameConfig(dict(
            name = "DCSS",
            id = "dcss-webtiles",
            ttyrec_path = "./",
            inprogress_path = "./",
            socket_path = "./",
            client_path = "./webserver/game_data"))
        super(DGLLessCrawlProcessHandler, self).__init__(game_params,
                                                         "game",
                                                         logger)

    def _base_call(self):
        return ["./crawl"]

    def check_where(self):
        pass
