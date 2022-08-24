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
from tornado.ioloop import PeriodicCallback

from webtiles import config, connection, game_data_handler, inotify, terminal, util, ws_handler
from webtiles.connection import WebtilesSocketConnection
from webtiles.game_data_handler import GameDataHandler
from webtiles.inotify import DirectoryWatcher
from webtiles.terminal import TerminalRecorder
from webtiles.util import DynamicTemplateLoader, dgl_format_str, parse_where_data
from webtiles.ws_handler import CrawlWebSocket, remove_in_lobbys, update_all_lobbys

try:
    from typing import Dict, Set, Tuple, Any
except:
    pass

last_game_id = 0

processes = dict() # type: Dict[str,CrawlProcessHandler]
unowned_process_logger = logging.LoggerAdapter(logging.getLogger(), {})

def find_game_info(socket_dir, socket_file):
    game_id = socket_file[socket_file.index(":")+1:-5]
    if (game_id in config.games and
        os.path.abspath(config.games[game_id]["socket_path"]) == os.path.abspath(socket_dir)):
        config.games[game_id]["id"] = game_id
        return config.games[game_id]

    game_info = None
    for game_id in list(config.games.keys()):
        gi = config.games[game_id]
        if os.path.abspath(gi["socket_path"]) == os.path.abspath(socket_dir):
            game_info = gi
            break
    game_info["id"] = game_id
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

        process.logger.info("Found a %s game.", game_info["id"])

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
    for game_id in list(config.games.keys()):
        game_info = config.games[game_id]
        socket_dir = os.path.abspath(game_info["socket_path"])
        if socket_dir in added_dirs: continue
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

        self.muted = set()

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
        base_path = self.format_path(self.game_params[key])
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

    def write_to_all(self, msg, send): # type: (str, bool) -> None
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
        self.chat_help_message(source, "/help", "show chat command help.")
        self.chat_help_message(source, "/hide", "hide the chat window.")
        if self.is_player(source):
            self.chat_help_message(source, "/mute <name>", "add <name> to the mute list.")
            self.chat_help_message(source, "", "Must be present in channel.")
            self.chat_help_message(source, "/mutelist", "show your entire mute list.")
            self.chat_help_message(source, "/unmute <name>", "remove <name> from the mute list.")
            self.chat_help_message(source, "/unmute *", "clear your mute list.")

    def handle_chat_command(self, source_ws, text):
        # type: (CrawlWebSocket, str) -> bool
        source = source_ws.username
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
        if command == "/mute":
            self.mute(source, remainder)
        elif command == "/unmute":
            self.unmute(source, remainder)
        elif command == "/mutelist":
            self.show_mute_list(source)
        elif command == "/help":
            self.chat_command_help(source)
        elif command == "/hide":
            self.hide_chat(source_ws, remainder.strip())
        else:
            return False
        return True

    def handle_chat_message(self, username, text): # type: (str, str) -> None
        if username in self.muted: # TODO: message?
            return
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
                watcher.go_lobby()

        if self.end_callback:
            self.end_callback()

    def get_watchers(self, chatting_only=False):
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
        watchers.sort(key=lambda s:s.lower())
        return (player_name, watchers)

    def is_player(self, username):
        # TODO: probably doesn't work for console players spectating themselves
        # TODO: let admin accounts mute as well?
        player_name, watchers = self.get_watchers()
        return (username == player_name)

    def hide_chat(self, receiver, param):
        if param == "forever":
            receiver.send_message("super_hide_chat")
            receiver.chat_hidden = True # currently only for super hidden chat
            self.update_watcher_description()
        else:
            receiver.send_message("toggle_chat")

    def restore_mutelist(self, source, l):
        if not self.is_player(source) or l is None:
            return
        if len(l) == 0:
            return
        self.muted = {u for u in l if u != source}
        self.handle_notification(source, "Restoring mute list.")
        self.show_mute_list(source)
        self.logger.info("Player '%s' restoring mutelist %s" %
                                            (source, repr(list(self.muted))))

    def save_mutelist(self, source):
        if not self.is_player(source):
            return
        receiver = self.get_primary_receiver()
        if receiver is not None:
            receiver.save_mutelist(list(self.muted))

    def mute(self, source, target):
        if not self.is_player(source):
            self.handle_notification(source,
                            "You do not have permission to mute spectators.")
            return False
        if (source == target):
            self.handle_notification(source, "You can't mute yourself!")
            return False
        player_name, watchers = self.get_watchers()
        watchers = set(watchers)
        if not target in watchers:
            self.handle_notification(source, "Mute who??")
            return False
        self.logger.info("Player '%s' has muted '%s'" % (source, target))
        self.handle_notification(source,
                            "Spectator '%s' has now been muted." % target)
        self.muted |= {target}
        self.save_mutelist(source)
        self.update_watcher_description()
        return True

    def unmute(self, source, target):
        if not self.is_player(source):
            self.handle_notification(source,
                            "You do not have permission to unmute spectators.")
            return False
        if (source == target):
            self.handle_notification(source,
                                    "You can't unmute (or mute) yourself!")
            return False
        if target == "*":
            if (len(self.muted) == 0):
                self.handle_notification(source, "No one is muted!")
                return False
            self.logger.info("Player '%s' has cleared their mute list." % (source))
            self.handle_notification(source, "You have cleared your mute list.")
            self.muted = set()
            self.save_mutelist(source)
            self.update_watcher_description()
            return True

        if not target in self.muted:
            self.handle_notification(source, "Unmute who??")
            return False

        self.logger.info("Player '%s' has unmuted '%s'" % (source, target))
        self.handle_notification(source, "You have unmuted '%s'." % target)
        self.muted -= {target}
        self.save_mutelist(source)
        self.update_watcher_description()
        return True

    def show_mute_list(self, source):
        if not self.is_player(source):
            return False
        names = list(self.muted)
        names.sort(key=lambda s: s.lower())
        if len(names) == 0:
            self.handle_notification(source, "No one is muted.")
        else:
            self.handle_notification(source, "You have muted: " +
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
            if (watcher in self.muted):
                n += " (muted)"
            if player_url_template is None:
                return "<span class='{0}'>{1}</span>".format(class_type, n)
            player_url = player_url_template.replace("%s", watcher.lower())
            username = "<a href='{0}' target='_blank' class='{1}'>{2}</a>".format(player_url, class_type, n)
            return username

        player_name, watchers = self.get_watchers(True)

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
                watcher.send_json_options(self.game_params["id"], self.username)
        self._receivers.add(watcher)
        self.update_watcher_description()

    def remove_watcher(self, watcher):
        self._receivers.remove(watcher)
        self.update_watcher_description()

    def watcher_count(self):
        return len([w for w in self._receivers if w.watched_game and not w.chat_hidden])

    def send_client_to_all(self):
        for receiver in self._receivers:
            self._send_client(receiver)
            if receiver.watched_game == self:
                receiver.send_json_options(self.game_params["id"],
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

    def stop(self):
        if self.process:
            self.process.send_signal(subprocess.signal.SIGHUP)
            t = time.time() + config.get('kill_timeout')
            self.kill_timeout = IOLoop.current().add_timeout(t, self.kill)

    def kill(self):
        if self.process:
            self.logger.info("Killing crawl process after SIGHUP did nothing.")
            self.process.send_signal(subprocess.signal.SIGABRT)
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
            "game_id": self.game_params["id"],
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


        call  = [game["crawl_binary"]]

        if "pre_options" in game:
            call += game["pre_options"]

        call += ["-name",   self.username,
                 "-rc",     os.path.join(self.config_path("rcfile_path"),
                                         self.username + ".rc"),
                 "-macro",  os.path.join(self.config_path("macro_path"),
                                         self.username + ".macro"),
                 "-morgue", self.config_path("morgue_path")]

        if self.account_restricted():
            call += ["-no-player-bones"]

        if "options" in game:
            call += game["options"]

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

    def start(self):
        self._purge_locks_and_start(True)

    def stop(self):
        super(CrawlProcessHandler, self).stop()
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
                    hup_wait = 10
                    self.send_to_all("stale_processes",
                                     timeout=hup_wait, game=self.game_params["name"])
                    to = IOLoop.current().add_timeout(time.time() + hup_wait,
                                                      self._kill_stale_process)
                    self._process_hup_timeout = to
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

    @util.note_blocking_fun
    def _find_lock(self):
        for path in os.listdir(self.config_path("inprogress_path")):
            if (path.startswith(self.username + ":") and
                path.endswith(".ttyrec")):
                return os.path.join(self.config_path("inprogress_path"),
                                    path)
        return None

    def _kill_stale_process(self, signal=subprocess.signal.SIGHUP):
        self._process_hup_timeout = None
        if self._stale_pid == None: return
        if signal == subprocess.signal.SIGHUP:
            self.logger.info("Purging stale lock at %s, pid %s.",
                             self._stale_lockfile, self._stale_pid)
        elif signal == subprocess.signal.SIGABRT:
            self.logger.warning("Terminating pid %s forcefully!",
                                self._stale_pid)
        try:
            os.kill(self._stale_pid, signal)
        except OSError as e:
            if e.errno == errno.ESRCH:
                # Process doesn't exist
                self._purge_stale_lock()
            else:
                self.logger.error("Error while killing process %s.", self._stale_pid,
                                  exc_info=True)
                errmsg = ("Error while trying to terminate a stale process.\n" +
                          "Please contact an administrator.")
                self.exit_reason = "error"
                self.exit_message = errmsg
                self.exit_dump_url = None
                self.handle_process_end()
                return
        else:
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
        if ttyrec_path:
            self.ttyrec_filename = os.path.join(ttyrec_path, self.lock_basename)

        processes[os.path.abspath(self.socketpath)] = self

        if config.get('dgl_mode'):
            self.logger.info("Starting %s.", game["id"])
        else:
            self.logger.info("Starting game.")

        try:
            self.process = TerminalRecorder(call, self.ttyrec_filename,
                                            self._ttyrec_id_header(),
                                            self.logger,
                                            config.get('recording_term_size'),
                                            env_vars = game.get("env", {}),
                                            game_cwd = game.get("cwd", None),)
            self.process.end_callback = self._on_process_end
            self.process.output_callback = self._on_process_output
            self.process.activity_callback = self.note_activity
            self.process.error_callback = self._on_process_error

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

            if self.process:
                self.stop()
            else:
                self._on_process_end()

    def connect(self, socketpath, primary = False):
        self.socketpath = socketpath
        self.conn = WebtilesSocketConnection(self.socketpath, self.logger)
        self.conn.message_callback = self._on_socket_message
        self.conn.close_callback = self._on_socket_close
        self.conn.username = self.username
        self.conn.connect(primary)

    @util.note_blocking_fun
    def gen_inprogress_lock(self):
        self.inprogress_lock = os.path.join(self.config_path("inprogress_path"),
                                            self.username + ":" + self.lock_basename)
        f = open(self.inprogress_lock, "w")
        fcntl.lockf(f.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        self.inprogress_lock_file = f
        cols, lines = self.process.get_terminal_size()
        f.write("%s\n%s\n%s\n" % (self.process.pid, lines, cols))
        f.flush()

    @util.note_blocking_fun
    def remove_inprogress_lock(self):
        if self.inprogress_lock_file is None: return
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
                 utf8("Game: %s" % self.game_params["name"]) + crlf +
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
        if line.startswith("ERROR"):
            self.exit_reason = "crash"
            if line.rfind(":") != -1:
                self.exit_message = line[line.rfind(":") + 1:].strip()
        elif line.startswith("We crashed!"):
            self.exit_reason = "crash"
            if self.game_params["morgue_url"] != None:
                match = re.search(r"\(([^)]+)\)", line)
                if match is not None:
                    self.exit_dump_url = self.game_params["morgue_url"].replace("%n", self.username)
                    self.exit_dump_url += os.path.splitext(os.path.basename(match.group(1)))[0]
        elif line.startswith("Writing crash info to"): # before 0.15-b1-84-gded71f8
            self.exit_reason = "crash"
            if self.game_params["morgue_url"] != None:
                url = None
                if line.rfind("/") != -1:
                    url = line[line.rfind("/") + 1:].strip()
                elif line.rfind(" ") != -1:
                    url = line[line.rfind(" ") + 1:].strip()
                if url is not None:
                    self.exit_dump_url = self.game_params["morgue_url"].replace("%n", self.username) + os.path.splitext(url)[0]

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
                    self.client_path = self.format_path(msgobj["path"])
                    if "version" in msgobj:
                        self.crawl_version = msgobj["version"]
                        self.logger.info("Crawl version: %s.", self.crawl_version)
                    self.send_client_to_all()
            elif msgobj["msg"] == "flush_messages":
                # only queue, once we know the crawl process asks for flushes
                self.queue_messages = True;
                self.flush_messages_to_all()
            elif msgobj["msg"] == "dump":
                if "morgue_url" in self.game_params and self.game_params["morgue_url"]:
                    url = self.game_params["morgue_url"].replace("%n", self.username) + msgobj["filename"]
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

            self.write_to_all(msg, not self.queue_messages)



class DGLLessCrawlProcessHandler(CrawlProcessHandler):
    def __init__(self, logger):
        game_params = dict(
            name = "DCSS",
            ttyrec_path = "./",
            inprogress_path = "./",
            socket_path = "./",
            client_path = "./webserver/game_data")
        super(DGLLessCrawlProcessHandler, self).__init__(game_params,
                                                         "game",
                                                         logger)

    def _base_call(self):
        return ["./crawl"]

    def check_where(self):
        pass
