import os, os.path, errno
import subprocess
import datetime, time

import config

from tornado.escape import json_decode, json_encode

from terminal import TerminalRecorder
from connection import WebtilesSocketConnection

class CrawlProcessHandlerBase(object):
    def __init__(self, game_params, username, logger, io_loop):
        self.game_params = game_params
        self.username = username
        self.logger = logger
        self.io_loop = io_loop

        self.process = None
        self.client_version = None
        self.where = None
        self.wheretime = time.time()
        self.kill_timeout = None

        self.end_callback = None
        self._watchers = set()
        self.last_activity_time = time.time()

    def idle_time(self):
        return time.time() - self.last_activity_time

    def send_to_all(self, msg):
        for watcher in self._watchers:
            watcher.handle_message(msg)

    def update_watcher_description(self):
        watcher_names = [watcher.username for watcher in self._watchers
                         if watcher.username]
        anon_count = len(self._watchers) - len(watcher_names)
        s = ", ".join(watcher_names)
        if len(watcher_names) > 0 and anon_count > 0:
            s = s + ", and %i Anon" % anon_count
        elif anon_count > 0:
            s = "%i Anon" % anon_count
        self.send_to_all("watchers(%i,'%s')" %
                         (len(self._watchers), s))

    def add_watcher(self, watcher):
        self._watchers.add(watcher)
        if self.client_version:
            watcher.set_client(self.game_params, self.client_version)

    def remove_watcher(self, watcher):
        self._watchers.remove(watcher)
        self.update_watcher_description()

    def watcher_count(self):
        return len(self._watchers)

    def stop(self):
        self.process.send_signal(subprocess.signal.SIGHUP)
        t = time.time() + config.kill_timeout
        self.kill_timeout = self.io_loop.add_timeout(t, self.kill)

    def kill(self):
        if self.process:
            self.logger.info("Killing crawl process after SIGHUP did nothing.")
            self.process.send_signal(subprocess.signal.SIGTERM)
            self.kill_timeout = None

    def check_where(self):
        morgue_path = self.game_params["morgue_path"]
        wherefile = os.path.join(morgue_path, self.username,
                                 self.username + ".dglwhere")
        try:
            if os.path.getmtime(wherefile) > self.wheretime:
                self.wheretime = time.time()
                f = open(wherefile, "r")
                _, _, newwhere = f.readline().partition("|")
                f.close()

                newwhere = newwhere.strip()

                if self.where != newwhere:
                    self.where = newwhere
        except (OSError, IOError):
            pass

    def _base_call(self):
        game = self.game_params

        call = [game["crawl_binary"],
                "-name",   self.username,
                "-rc",     os.path.join(game["rcfile_path"],
                                        self.username + ".rc"),
                "-macro",  os.path.join(game["macro_path"],
                                        self.username + ".macro"),
                "-morgue", os.path.join(game["morgue_path"],
                                        self.username)]

        if "options" in game:
            call += game["options"]

        return call

    def handle_input(self, msg):
        raise NotImplementedError()

class CrawlProcessHandler(CrawlProcessHandlerBase):
    def __init__(self, game_params, username, logger,  io_loop):
        super(CrawlProcessHandler, self).__init__(game_params, username,
                                                  logger, io_loop)
        self.socketpath = None
        self.conn = None
        self.ttyrec_filename = None
        self.ttyrec_filename_only = None

        if "client_prefix" in game_params:
            self.client_version = game_params["client_prefix"]


    def start(self):
        self.socketpath = os.path.join(self.game_params["socket_path"],
                                       self.username + ".sock")

        try: # Unlink if necessary
            os.unlink(self.socketpath)
        except OSError, e:
            if e.errno != errno.ENOENT:
                raise

        game = self.game_params

        call = self._base_call() + ["-webtiles-socket", self.socketpath,
                                    "-await-connection"]

        now = datetime.datetime.utcnow()
        ttyrec_path = game.get("ttyrec_path", game.get("running_game_path"))
        tf = self.username + ":" + now.strftime("%Y-%m-%d.%H:%M:%S") + ".ttyrec"
        self.ttyrec_filename_only = tf
        self.ttyrec_filename = os.path.join(ttyrec_path, tf)

        self.logger.info("Starting crawl.")

        self.process = TerminalRecorder(call, self.ttyrec_filename,
                                        self._ttyrec_id_header(),
                                        self.logger, self.io_loop)
        self.process.end_callback = self._on_process_end
        self.process.output_callback = self._on_process_output

        self.conn = WebtilesSocketConnection(self.io_loop, self.socketpath)
        self.conn.message_callback = self._on_socket_message
        self.conn.connect()

        self.last_activity_time = time.time()

    def _ttyrec_id_header(self):
        clrscr = "\033[2J"
        crlf = "\r\n"
        templ = (clrscr + "\033[1;1H" + crlf +
                 "Player: %s" + crlf +
                 "Game: %s" + crlf +
                 "Server: %s" + crlf +
                 "Filename: %s" + crlf +
                 "Time: (%s) %s" + crlf +
                 clrscr)
        tstamp = int(time.time())
        ctime = time.ctime()
        return templ % (self.username, self.game_params["name"],
                        config.server_id, self.ttyrec_filename_only,
                        tstamp, ctime)

    def _on_process_end(self):
        self.logger.info("Crawl terminated.")

        self.process = None

        if self.conn:
            self.conn.close()
            self.conn = None

        if self.kill_timeout:
            self.io_loop.remove_timeout(self.kill_timeout)
            self.kill_timeout = None

        for watcher in list(self._watchers):
            watcher.stop_watching()

        if self.end_callback:
            self.end_callback()

    def add_watcher(self, watcher):
        super(CrawlProcessHandler, self).add_watcher(watcher)

        if self.conn:
            self.conn.send_message('{"msg":"spectator_joined"}')

    def handle_input(self, msg):
        if msg.startswith("{"):
            obj = json_decode(msg)

            if obj["msg"] == "input" and self.process:
                self.last_action_time = time.time()

                data = ""
                for x in obj["data"]:
                    data += chr(x)

                self.process.poll()
                if self.process:
                    self.process.write_input(data)

            elif self.conn:
                self.conn.send_message(msg.encode("utf8"))

        else:
            self.process.poll()
            if self.process:
                self.process.write_input(msg.encode("utf8"))

    def _on_process_output(self, line):
        self.check_where()
        self.last_activity_time = time.time()

        self.send_to_all(line)

    def _on_socket_message(self, msg):
        # stdout data is only used for compatibility to wrapper
        # scripts -- so as soon as we receive something on the socket,
        # we stop using stdout
        self.process.output_callback = None

        self.check_where()
        self.last_activity_time = time.time()

        self.send_to_all(msg)
