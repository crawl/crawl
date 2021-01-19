import aiofiles
import asyncio
import codecs
import datetime
import logging
import os
import random
import signal
import subprocess
import sys
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

import auth
import config
import userdb
import util
import load_games
from util import *

try:
    from typing import Dict, Set, Tuple, Any, Union, Optional
except:
    pass

sockets = set() # type: Set[CrawlWebSocket]
current_id = 0
shutting_down = False
rand = random.SystemRandom()

async def shutdown():
    global shutting_down
    shutting_down = True
    await asyncio.gather(*[socket.shutdown() for socket in list(sockets)])

async def update_global_status():
    await write_dgl_status_file()

async def update_all_lobbys(game):
    lobby_entry = game.lobby_entry()
    if lobby_entry:
        await asyncio.gather(*[socket.send_message("lobby_entry", **lobby_entry)
                            for socket in list(sockets)
                            if socket.is_in_lobby()])


async def remove_in_lobbys(process):
    remove_dict = dict(id=process.id,
                        reason=process.exit_reason,
                        message=process.exit_message,
                        dump=process.exit_dump_url)
    await asyncio.gather(*[socket.send_message("lobby_remove", **remove_dict)
                            for socket in list(sockets)
                            if socket.is_in_lobby()])

async def global_announce(text):
    await asyncio.gather(*[socket.send_announcement(text)
                            for socket in list(sockets)])

async def write_dgl_status_file():
    f = None
    process_info = ["%s#%s#%s#0x0#%s#%s#" %
                            (socket.username, socket.game_id,
                             (socket.process.human_readable_where()),
                             str(socket.process.idle_time()),
                             str(socket.process.watcher_count()))
                        for socket in list(sockets)
                        if socket.username and socket.is_running()]
    try:
        async with aiofiles.open(config.dgl_status_file, mode="w") as f:
            await f.write("\n".join(process_info))
    except:
        logging.warning("Could not write dgl status file", exc_info=True)

async def status_file_timeout():
    while True:
        await write_dgl_status_file()
        await tornado.gen.sleep(config.status_file_update_rate)


def find_user_sockets(username):
    for socket in list(sockets):
        if socket.username and socket.username.lower() == username.lower():
            yield socket


def find_running_game(charname, start):
    from process_handler import processes
    for process in list(processes.values()):
        if (process.where.get("name") == charname and
            process.where.get("start") == start):
            return process
    return None


def _milestone_files():
    # First we collect all milestone files explicitly specified in the config.
    files = set()

    top_level_milestones = getattr(config, 'milestone_file', None)
    if top_level_milestones is not None:
        if not isinstance(top_level_milestones, list):
            top_level_milestones = [top_level_milestones]
        files.update(top_level_milestones)

    for game_config in config.games.values():
        milestone_file = game_config.get('milestone_file')
        if milestone_file is None and 'dir_path' in game_config:
            # milestone appears in this dir by default
            milestone_file = os.path.join(game_config['dir_path'], 'milestones')
        if milestone_file is not None:
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
        milestone_file_tailers.append(FileTailer(f, handle_new_milestone))

def handle_new_milestone(line):
    data = parse_where_data(line)
    if "name" not in data:
        return
    game = find_running_game(data.get("name"), data.get("start"))
    if game:
        game.log_milestone(data)

# decorator for admin calls
def admin_required(f):
    async def wrapper(self, *args, **kwargs):
        if not self.is_admin():
            logging.error("Non-admin user '%s' attempted admin function '%s'" %
                (self.username and self.username or "[Anon]", f.__name__))
            return
        return await f(self, *args, **kwargs)
    return wrapper

class CrawlWebSocket(tornado.websocket.WebSocketHandler):
    def __init__(self, app, req, **kwargs):
        tornado.websocket.WebSocketHandler.__init__(self, app, req, **kwargs)
        self.username = None
        self.user_id = None
        self.user_email = None
        self.timeout = None
        self.watched_game = None
        self.process = None
        self.game_id = None
        self.received_pong = None
        self.save_info = dict()

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
    async def admin_announce(self, text):
        self.logger.info("User '%s' sent serverwide announcement: %s", self.username, text)
        await global_announce(text)
        await self.send_message("admin_log", text="Announcement made ('" + text + "')")

    @admin_required
    async def admin_pw_reset(self, username):
        user_info = await userdb.get_user_info(username)
        if not user_info:
            await self.send_message("admin_pw_reset_done", error="Invalid user")
            return
        ok, msg = await userdb.generate_forgot_password(username)
        if not ok:
            await self.send_message("admin_pw_reset_done", error=msg)
        else:
            self.logger.info("Admin user '%s' set a password token on account '%s'", self.username, username)
            await self.send_message("admin_pw_reset_done", email_body=msg, username=username, email=user_info[1])

    @admin_required
    async def admin_pw_reset_clear(self, username):
        ok, err = await userdb.clear_password_token(username)
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

    async def open(self):
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

        await self.reset_timeout()

        if config.max_connections < len(sockets):
            await self.append_message("connection_closed('The maximum number of "
                              + "connections has been reached, sorry :(');")
            self.close()
        elif shutting_down:
            self.close()
        else:
            if config.dgl_mode:
                if hasattr(config, "autologin") and config.autologin:
                    self.do_login(config.autologin)
                await self.send_lobby()
            else:
                await self.start_crawl(None)

    def check_origin(self, origin):
        return True

    def idle_time(self):
        return self.process.idle_time()

    def is_running(self):
        return self.process is not None

    def is_in_lobby(self):
        return not self.is_running() and self.watched_game is None

    async def send_lobby(self):
        self.queue_message("lobby_clear")
        from process_handler import processes
        for process in list(processes.values()):
            self.queue_message("lobby_entry", **process.lobby_entry())
        await self.send_message("lobby_complete")
        await self.send_lobby_html()

    async def send_announcement(self, text):
        # TODO: something in lobby?
        if not self.is_in_lobby():
            # show in chat window
            await self.send_message("server_announcement", text=text)
            # show in player message window
            if self.is_running():
                await self.process.handle_announcement(text)

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

    async def send_game_links(self):
        def disable_check(s):
            return s == "[slot full]"

        play_html = to_unicode(self.render_string("game_links.html",
                                              games = config.games,
                                              save_info = self.save_info,
                                              disabled = disable_check))
        await self.send_message("set_game_links", content = play_html)

    async def update_save_info(self, g):
        game = config.games[g] # throws exception on bad game id
        if not game.get("show_save_info", False):
            self.save_info[g] = ""
        elif self.save_info.get(g, None) is None:
            call = [game["crawl_binary"]]
            if "pre_options" in game:
                call += game["pre_options"]
            call += ["-save-json", self.username]
            returncode, data = await util.check_output(call)
            if returncode == 0:
                try:
                    save_dict = json_decode(data)[load_games.game_modes[g]]
                    if not save_dict["loadable"]:
                        # the save in this slot is in use.
                        self.save_info[g] = "[playing]" # TODO: something better??
                    elif load_games.game_modes[g] == save_dict["game_type"]:
                        # save in the slot matches the game type we are
                        # checking.
                        self.save_info[g] = "[" + save_dict["short_desc"] + "]"
                    else:
                        # There is a save, but it has a different game type.
                        # This happens if multiple game types share a slot.
                        self.save_info[g] = "[slot full]"
                except Exception:
                    # game key missing (or other error). This will mainly
                    # happen if there are no saves at all for the player
                    # name. It can also happen under some dgamelaunch-config
                    # setups if escape codes are incorrectly inserted into
                    # the output for some calls. See:
                    # https://github.com/crawl/dgamelaunch-config/commit/6ad788ceb5614b3c83d65b61bf26a122e592b98d
                    self.save_info[g] = ""
            else:
                # error in the subprocess: this will happen if the binary
                # does not support `-save-json`. Print a warning so that
                # the admin can see that they have save info enabled
                # incorrectly for this binary.
                logging.warn("Save info check for '%s' failed" % g)
                self.save_info[g] = ""
        # else: don't update the cached version

    async def update_all_save_info(self):
        await asyncio.gather(*[self.update_save_info(g) for g in config.games])

    async def send_lobby_html(self):
        # Rerender Banner
        # TODO: don't really need to do this every time the lobby is loaded?
        banner_html = to_unicode(self.render_string("banner.html",
                                                    username = self.username))
        asyncio.create_task(self.send_message(
                        "html", id = "banner", content = banner_html))

        if not self.username:
            return

        # if no game links at all have been sent, immediately render the
        # empty version. This is so that if the server takes a while on
        # initial connect, the player sees something immediately.
        # TODO: dynamically send this info as it comes in, rather than
        # rendering it all at the end?
        if len(self.save_info) == 0:
            for g in config.games:
                self.save_info[g] = None
            await self.send_game_links()

        await self.update_all_save_info()
        await self.send_game_links()

    async def reset_timeout(self):
        if self.timeout:
            IOLoop.current().remove_timeout(self.timeout)

        self.received_pong = False
        await self.send_message("ping")
        self.timeout = IOLoop.current().call_later(config.connection_timeout,
                                        self.check_connection)

    async def check_connection(self):
        self.timeout = None

        if not self.received_pong:
            self.logger.info("Connection timed out.")
            self.close()
        else:
            if self.is_running() and self.process.idle_time() > config.max_idle_time:
                self.logger.info("Stopping crawl after idle time limit.")
                await self.process.stop()

        if not self.client_closed:
            await self.reset_timeout()

    async def start_crawl(self, game_id):
        if config.dgl_mode and game_id not in config.games:
            await self.go_lobby()
            return

        if config.dgl_mode:
            game_params = dict(config.games[game_id])
            if self.username == None:
                if self.watched_game:
                    await self.stop_watching()
                await self.send_message("login_required", game = game_params["name"])
                return

        if self.process:
            # ignore multiple requests for the same game, can happen when
            # logging in with cookies
            if self.game_id != game_id:
                await self.go_lobby()
            return

        self.game_id = game_id

        # invalidate cached save info for lobby
        # TODO: invalidate for other sockets of the same player?
        self.invalidate_saveslot_cache(game_id)

        import process_handler

        #TODO: asyncify
        if config.dgl_mode:
            game_params["id"] = game_id
            args = (game_params, self.username, self.logger)
            self.process = process_handler.CrawlProcessHandler(*args)
        else:
            self.process = process_handler.DGLLessCrawlProcessHandler(self.logger)

        self.process.end_callback = self._on_crawl_end
        await self.process.add_watcher(self)
        try:
            await self.process.start()
        except Exception:
            self.logger.warning("Exception starting process!", exc_info=True)
            self.process = None
            await self.go_lobby()
        else:
            if self.process is None: # Can happen if the process creation fails
                await self.go_lobby()
                return

            await self.send_message("game_started")
            await self.restore_mutelist()

            if config.dgl_mode:
                if self.process.where == {}:
                    # If location info was found, the lobbys were already
                    # updated by set_where_data
                    asyncio.create_task(update_all_lobbys(self.process))
                asyncio.create_task(update_global_status())

    async def _on_crawl_end(self):
        if config.dgl_mode:
            # must await here -- process will shortly be wiped out
            await remove_in_lobbys(self.process)

        reason = self.process.exit_reason
        message = self.process.exit_message
        dump_url = self.process.exit_dump_url
        self.process = None

        if self.client_closed:
            sockets.remove(self)
        else:
            if shutting_down:
                self.close()
            else:
                # Go back to lobby
                await self.send_message("game_ended", reason = reason,
                                  message = message, dump = dump_url)

                self.invalidate_saveslot_cache(self.game_id)

                if config.dgl_mode:
                    if not self.watched_game:
                        await self.send_message("go_lobby")
                        await self.send_lobby()
                else:
                    await self.start_crawl(None)

        if config.dgl_mode:
            await update_global_status()

        # TODO: double check shutdown stuff
        if shutting_down and len(sockets) == 0:
            # The last crawl process has ended, now we can go
            IOLoop.current().stop()

    async def stop_watching(self):
        if self.watched_game:
            self.logger.info("%s stopped watching %s.",
                                    self.username and self.username or "[Anon]",
                                    self.watched_game.username)
            await self.watched_game.remove_watcher(self)
            self.watched_game = None

    async def shutdown(self):
        if not self.client_closed:
            self.logger.info("Shutting down user %s id %d",
                                    self.username and self.username or "[Anon]",
                                    self.id)
            msg = to_unicode(self.render_string("shutdown.html", game=self))
            await self.send_message("close", reason = msg)
            self.close()
        if self.is_running():
            await self.process.stop()

    async def do_login(self, username):
        self.username = username
        self.user_id, self.user_email, self.user_flags = await userdb.get_user_info(username)
        self.logger.extra["username"] = username

        p = await asyncio.create_subprocess_exec(
                                config.init_player_program, self.username,
                                stdout=asyncio.subprocess.DEVNULL)
        result = await p.wait()

        success = result == 0
        if not success:
            msg = ("Could not initialize your rc and morgue!<br>" +
                   "This probably means there is something wrong " +
                   "with the server configuration.")
            await self.send_message("close", reason = msg)
            self.logger.warning("User initialization returned an error for user %s!",
                                self.username)
            self.username = None
            self.close()
            return

        await self.send_message("login_success", username=username,
                           admin=self.is_admin())
        if self.watched_game:
            await self.watched_game.update_watcher_description()
        else:
            await self.send_lobby_html()

    async def login(self, username, password):
        real_username = await userdb.user_passwd_match(username, password)
        if real_username:
            self.logger.info("User %s logging in from %s.",
                                        real_username, self.request.remote_ip)
            await self.do_login(real_username)
        else:
            self.logger.warning("Failed login for user %s.", username)
            await self.send_message("login_fail")

    async def token_login(self, cookie):
        username, ok = auth.check_login_cookie(cookie)
        if ok:
            auth.forget_login_cookie(cookie)
            self.logger.info("User %s logging in (via token).", username)
            await self.do_login(username)
        else:
            self.logger.warning("Wrong login token for user %s.", username)
            await self.send_message("login_fail")

    async def set_login_cookie(self):
        if self.username is None:
            return
        cookie = auth.log_in_as_user(self, self.username)
        await self.send_message("login_cookie", cookie = cookie,
                          expires = config.login_token_lifetime)

    async def forget_login_cookie(self, cookie):
        auth.forget_login_cookie(cookie)

    async def restore_mutelist(self):
        if not self.username:
            return
        receiver = None
        if self.process:
            receiver = self.process
        elif self.watched_game:
            receiver = self.watched_game

        if not receiver:
            return

        db_string = await userdb.get_mutelist(self.username)
        if db_string is None:
            db_string = ""

        muted = list([_f for _f in db_string.strip().split(' ') if _f])
        await receiver.restore_mutelist(self.username, muted)

    async def save_mutelist(self, muted):
        db_string = " ".join(muted).strip()
        await userdb.set_mutelist(self.username, db_string)

    def is_admin(self):
        return self.username is not None and userdb.dgl_is_admin(self.user_flags)

    async def pong(self):
        self.received_pong = True

    def rcfile_path(self, game_id):
        if game_id not in config.games: return None
        if not self.username: return None
        path = dgl_format_str(config.games[game_id]["rcfile_path"],
                                     self.username, config.games[game_id])
        return os.path.join(path, self.username + ".rc")

    async def send_json_options(self, game_id, player_name):
        def do_send(data, returncode):
            if returncode != 0:
                # fail silently for returncode 1 for now, probably just an old
                # version missing the command line option
                if returncode != 1:
                    self.logger.warning("Error while getting JSON options!")
                return
            self.append_message('{"msg":"options","watcher":true,"options":'
                                + data + '}')

        if not self.username:
            return
        if game_id not in config.games:
            return

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

        returncode, data = await util.check_output(call)
        if returncode != 0:
            # fail silently for returncode 1 for now, probably just an old
            # version missing the command line option
            if returncode != 1:
                self.logger.warning("Error while getting JSON options!")
            return

        self.append_message('{"msg":"options","watcher":true,"options":'
                            + data + '}')

    async def watch(self, username):
        if self.is_running():
            await self.process.stop()

        from process_handler import processes
        procs = [process for process in list(processes.values())
                 if process.username.lower() == username.lower()]
        if len(procs) >= 1:
            process = procs[0]
            if self.watched_game:
                if self.watched_game == process:
                    return
                await self.stop_watching()
            self.logger.info("%s started watching %s (%s).",
                                self.username and self.username or "[Anon]",
                                process.username, process.id)

            self.watched_game = process
            await process.add_watcher(self)
            await self.send_message("watching_started", username = process.username)
        else:
            if self.watched_game:
                await self.stop_watching()
            await self.go_lobby()

    async def post_chat_message(self, text):
        receiver = None
        if self.process:
            receiver = self.process
        elif self.watched_game:
            receiver = self.watched_game

        if receiver:
            if self.username is None:
                await self.send_message("chat", content
                                  = 'You need to log in to send messages!')
                return

            if not await receiver.handle_chat_command(self, text):
                await receiver.handle_chat_message(self.username, text)

    async def register(self, username, password, email):
        error = await userdb.register_user(username, password, email)
        if error is None:
            self.logger.info("Registered user %s.", username)
            await self.do_login(username)
        else:
            self.logger.info("Registration attempt failed for username %s: %s",
                             username, error)
            await self.send_message("register_fail", reason = error)

    async def start_change_password(self):
        await self.send_message("start_change_password")

    async def change_password(self, cur_password, new_password):
        if self.username is None:
            await self.send_message(
                "change_password_fail",
                reason = "You need to log in to change your password.")
            return

        if not await userdb.user_passwd_match(self.username, cur_password):
            await self.send_message(
                "change_password_fail", reason = "Your password didn't match.")
            self.logger.info(
                "Non-matching current password during password change for %s",
                self.username)
            return

        error = await userdb.change_password(self.user_id, new_password)
        if error is None:
            self.user_id, self.user_email, self.user_flags = await userdb.get_user_info(self.username)
            self.logger.info("User %s changed password.", self.username)
            await self.send_message("change_password_done")
        else:
            self.logger.info("Failed to change username for %s: %s", self.username, error)
            await self.send_message("change_password_fail", reason = error)

    async def start_change_email(self):
        await self.send_message("start_change_email", email = self.user_email)

    async def change_email(self, email):
        if self.username is None:
            await self.send_message(
                "change_email_fail",
                reason = "You need to log in to change your email")
            return
        error = await userdb.change_email(self.user_id, email)
        if error is None:
            self.user_id, self.user_email, self.user_flags = await userdb.get_user_info(self.username)
            self.logger.info("User %s changed email to %s.",
                self.username, email if email else "null")
            await self.send_message("change_email_done", email = email)
        else:
            self.logger.info("Failed to change username for %s: %s",
                self.username, error)
            await self.send_message("change_email_fail", reason = error)

    async def forgot_password(self, email):
        if not getattr(config, "allow_password_reset", False):
            return
        sent, error = await userdb.send_forgot_password(email)
        if error is None:
            self.logger.info("Sent password reset email to %s.", email)
            await self.send_message("forgot_password_done")
        else:
            self.logger.info("Failed to send password reset email for %s: %s",
                             email, error)
            await self.send_message("forgot_password_fail", reason = error)

    async def reset_password(self, token, password):
        username, error = await userdb.update_user_password_from_token(token,
                                                                 password)
        if error is None:
            self.logger.info("User %s has completed their password reset.",
                             username)
            await self.send_message("reload_url")
        else:
            if username is None:
                self.logger.info("Failed to update password for token %s: %s",
                                 token, error)
            else:
                self.logger.info("Failed to update password for user %s: %s",
                                 username, error)
            await self.send_message("reset_password_fail", reason = error)

    async def go_lobby(self):
        if not config.dgl_mode:
            return
        if self.is_running():
            await self.process.stop()
        elif self.watched_game:
            await self.stop_watching()
            await self.send_message("go_lobby")
            await self.send_lobby()
        else:
            await self.send_message("go_lobby")

    async def go_admin(self):
        await self.go_lobby()
        await self.send_message("go_admin")

    async def get_rc(self, game_id):
        if game_id not in config.games: return
        path = self.rcfile_path(game_id)
        try:
            with open(path, 'r') as f:
                contents = f.read()
        # Handle RC file not existing. IOError for py2, OSError for py3
        except (OSError, IOError):
            contents = ''
        await self.send_message("rcfile_contents", contents = contents)

    async def set_rc(self, game_id, contents):
        rcfile_path = dgl_format_str(config.games[game_id]["rcfile_path"],
                                     self.username, config.games[game_id])
        rcfile_path = os.path.join(rcfile_path, self.username + ".rc")
        with open(rcfile_path, 'wb') as f:
            # TODO: is binary + encode necessary in py 3?
            f.write(utf8(contents))

    async def on_message(self, message): # type: (Union[str, bytes]) -> None
        try:
            obj = json_decode(message) # type: Dict[str, Any]
            if obj["msg"] in self.message_handlers:
                handler = self.message_handlers[obj["msg"]]
                del obj["msg"]
                if asyncio.iscoroutinefunction(handler):
                    await handler(**obj)
                else:
                    handler(**obj)
            elif self.process:
                await self.process.handle_input(message)
            elif not self.watched_game and obj["msg"] != 'ui_state_sync':
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
        except Exception:
            excerpt = message[:50] if len(message) > 50 else message
            trunc = "..." if len(message) > 50 else ""
            self.logger.warning("Error while handling JSON message ('%r%s')!",
                                excerpt, trunc,
                                exc_info=True)

    async def flush_messages(self):
        # type: () -> bool
        # TODO: use async queues?
        if self.client_closed or len(self.message_queue) == 0:
            return False
        msg = ("{\"msgs\":["
                + ",".join(self.message_queue)
                + "]}")
        self.message_queue = []

        try:
            binmsg = utf8(msg)
            self.total_message_bytes += len(binmsg)
            if self.deflate:
                # Compress like in deflate-frame extension:
                # Apply deflate, flush, then remove the 00 00 FF FF
                # at the end
                compressed = self._compressobj.compress(binmsg)
                compressed += self._compressobj.flush(zlib.Z_SYNC_FLUSH)
                compressed = compressed[:-4]
                self.compressed_bytes_sent += len(compressed)
                await self.write_message(compressed, binary=True)
            else:
                self.uncompressed_bytes_sent += len(binmsg)
                await self.write_message(binmsg)

            return True
        except tornado.websocket.WebSocketClosedError as e:
            self.logger.warning("Connection closed during write_message")
            if self.ws_connection is not None:
                self.ws_connection._abort()
        except:
            self.logger.warning("Exception trying to send message.", exc_info = True)
            if self.ws_connection is not None:
                self.ws_connection._abort()
        return False

    def append_message(self, msg):
        # type: (str) -> bool
        if self.client_closed:
            return False
        self.message_queue.append(msg)
        return True

    async def send_message(self, msg, **data):
        # type: (str, Any) -> bool
        """Sends a JSON message to the client."""
        data["msg"] = msg
        self.append_message(json_encode(data))
        return await self.flush_messages()

    def queue_message(self, msg, **data):
        # type: (str, Any) -> bool
        data["msg"] = msg
        return self.append_message(json_encode(data))

    async def _on_close(self):
        if self.process is None and self in sockets:
            sockets.remove(self)
            if shutting_down and len(sockets) == 0:
                # The last socket has been closed, now we can go
                IOLoop.current().stop()
        elif self.is_running():
            await self.process.stop()

        if self.watched_game:
            await self.watched_game.remove_watcher(self)

        if self.timeout:
            IOLoop.current().remove_timeout(self.timeout)

        if self.total_message_bytes == 0:
            comp_ratio = "N/A"
        else:
            comp_ratio = 100 - 100 * (self.compressed_bytes_sent + self.uncompressed_bytes_sent) / self.total_message_bytes
            comp_ratio = round(comp_ratio, 2)

        self.logger.info("Socket closed. (%s sent, compression ratio %s%%)",
                         util.humanise_bytes(self.total_message_bytes), comp_ratio)

    # do a little dance because superclass on_close is not actually a
    # coroutine, and needs to support direct calls
    @tornado.gen.coroutine
    def on_close(self):
        try:
            yield self._on_close()
        except:
            self.logger.error("Unhandled exception while closing websocket",
                                exc_info=True)
