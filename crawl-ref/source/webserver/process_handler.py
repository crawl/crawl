import os, os.path, errno
import subprocess
import datetime, time
import hashlib

import config

from tornado.escape import json_decode, json_encode

from terminal import TerminalRecorder
from connection import WebtilesSocketConnection
from util import DynamicTemplateLoader
from game_data_handler import GameDataHandler

class CrawlProcessHandlerBase(object):
    def __init__(self, game_params, username, logger, io_loop):
        self.game_params = game_params
        self.username = username
        self.logger = logger
        self.io_loop = io_loop

        self.process = None
        self.client_path = game_params.get("client_path")
        self.where = None
        self.wheretime = time.time()
        self.kill_timeout = None

        self.end_callback = None
        self._watchers = set()
        self._receivers = set()
        self.last_activity_time = time.time()

    def idle_time(self):
        return time.time() - self.last_activity_time

    def send_to_all(self, msg):
        for receiver in self._receivers:
            receiver.handle_message(msg)

    def handle_process_end(self):
        if self.kill_timeout:
            self.io_loop.remove_timeout(self.kill_timeout)
            self.kill_timeout = None

        for watcher in list(self._watchers):
            watcher.stop_watching()

        if self.end_callback:
            self.end_callback()

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

    def add_watcher(self, watcher, hide = False):
        if not hide:
            self._watchers.add(watcher)
        self._receivers.add(watcher)
        if self.client_path:
            self._send_client(watcher)
        self.update_watcher_description()

    def remove_watcher(self, watcher):
        if watcher in self._watchers:
            self._watchers.remove(watcher)
        self._receivers.remove(watcher)
        self.update_watcher_description()

    def watcher_count(self):
        return len(self._watchers)

    def _send_client(self, watcher):
        v = hashlib.sha1(os.path.abspath(self.client_path)).hexdigest()
        GameDataHandler.add_version(v,
                                    os.path.join(self.client_path, "static"))

        templ_path = os.path.join(self.client_path, "templates")
        loader = DynamicTemplateLoader.get(templ_path)
        templ = loader.load("game.html")
        game_html = templ.generate(version = v)
        watcher.handle_message("delay_timeout = 1;$('#game').html(" +
                               json_encode(game_html) +
                               ");delay_ended();")

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

    def note_activity(self):
        self.last_activity_time = time.time()

    def handle_input(self, msg):
        raise NotImplementedError()

class CrawlProcessHandler(CrawlProcessHandlerBase):
    def __init__(self, game_params, username, logger, io_loop):
        super(CrawlProcessHandler, self).__init__(game_params, username,
                                                  logger, io_loop)
        self.socketpath = None
        self.conn = None
        self.ttyrec_filename = None
        self.ttyrec_filename_only = None

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
        self.process.activity_callback = self.note_activity

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

        self.handle_process_end()

    def add_watcher(self, watcher, hide = False):
        super(CrawlProcessHandler, self).add_watcher(watcher, hide)

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

                self.process.write_input(data)

            elif self.conn:
                self.conn.send_message(msg.encode("utf8"))

        else:
            self.process.write_input(msg.encode("utf8"))

    def _on_process_output(self, line):
        self.check_where()

        self.send_to_all(line)

    def _on_socket_message(self, msg):
        # stdout data is only used for compatibility to wrapper
        # scripts -- so as soon as we receive something on the socket,
        # we stop using stdout
        self.process.output_callback = None

        self.check_where()

        self.send_to_all(msg)



class DGLLessCrawlProcessHandler(CrawlProcessHandler):
    def __init__(self, logger, io_loop):
        game_params = dict(
            name = "DCSS",
            ttyrec_path = "./",
            socket_path = "./",
            client_path = "./webserver/game_data")
        super(DGLLessCrawlProcessHandler, self).__init__(game_params,
                                                         "game",
                                                         logger, io_loop)

    def _base_call(self):
        return ["./crawl"]

    def check_where(self):
        pass


class CompatCrawlProcessHandler(CrawlProcessHandlerBase):
    def __init__(self, game_params, username, logger, io_loop):
        super(CompatCrawlProcessHandler, self).__init__(game_params, username,
                                                        logger, io_loop)
        self.client_path = game_params["client_prefix"]

    def start(self):
        game = self.game_params
        call = self._base_call()

        self.logger.info("Starting crawl (compat-mode).")

        self.process = subprocess.Popen(call,
                                        stdin = subprocess.PIPE,
                                        stdout = subprocess.PIPE,
                                        stderr = subprocess.PIPE)

        self.io_loop.add_handler(self.process.stdout.fileno(), self.on_stdout,
                                self.io_loop.READ | self.io_loop.ERROR)
        self.io_loop.add_handler(self.process.stderr.fileno(), self.on_stderr,
                                self.io_loop.READ | self.io_loop.ERROR)

        self.last_activity_time = time.time()

        self.create_mock_ttyrec()

    def create_mock_ttyrec(self):
        now = datetime.datetime.utcnow()
        running_game_path = self.game_params["running_game_path"]
        self.ttyrec_filename = os.path.join(running_game_path,
                                            self.username + ":" + now.strftime("%Y-%m-%d.%H:%M:%S")
                                            + ".ttyrec")
        f = open(self.ttyrec_filename, "w")
        f.close()

    def delete_mock_ttyrec(self):
        if self.ttyrec_filename:
            os.remove(self.ttyrec_filename)
            self.ttyrec_filename = None

    def poll_crawl(self):
        if self.process is not None and self.process.poll() is not None:
            self.io_loop.remove_handler(self.process.stdout.fileno())
            self.io_loop.remove_handler(self.process.stderr.fileno())
            self.process.stdout.close()
            self.process.stderr.close()
            self.process = None

            self.logger.info("Crawl terminated. (compat-mode)")

            self.delete_mock_ttyrec()
            self.handle_process_end()

    def add_watcher(self, watcher, hide = False):
        super(CompatCrawlProcessHandler, self).add_watcher(watcher, hide)

        if self.process:
            self.process.write_input("^r")

    def handle_input(self, msg):
        if msg.startswith("{"):
            obj = json_decode(msg)

            self.note_activity()

            if obj["msg"] == "input" and self.process:
                self.last_action_time = time.time()

                data = ""
                for x in obj["data"]:
                    data += chr(x)

                self.process.stdin.write(data)

            elif obj["msg"] == "key" and self.process:
                self.process.stdin.write("\\" + str(obj["keycode"]) + "\n")

            elif self.conn:
                self.conn.send_message(msg.encode("utf8"))

        elif msg == "^":
            self.note_activity()
            self.process.write_input("\\94\n")

        else:
            if not msg.startswith("^"):
                self.note_activity()
            self.process.stdin.write(msg.encode("utf8"))

    def on_stderr(self, fd, events):
        if events & self.io_loop.ERROR:
            self.poll_crawl()
        elif events & self.io_loop.READ:
            s = self.process.stderr.readline()

            if not (s.isspace() or s == ""):
                self.logger.info("ERR: %s", s.strip())

            self.poll_crawl()

    def on_stdout(self, fd, events):
        if events & self.io_loop.ERROR:
            self.poll_crawl()
        elif events & self.io_loop.READ:
            msg = self.process.stdout.readline()

            self.send_to_all(msg)

            self.poll_crawl()
            self.check_where()

    def _send_client(self, watcher):
        templ_path = os.path.join(config.template_path, self.client_path)
        loader = DynamicTemplateLoader.get(templ_path)
        templ = loader.load("game.html")
        game_html = templ.generate(prefix = self.client_path)
        watcher.handle_message("delay_timeout = 1;$('#game').html(" +
                               json_encode(game_html) +
                               ");delay_ended();")
