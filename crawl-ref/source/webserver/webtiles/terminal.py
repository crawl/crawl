import asyncio
import fcntl
import os
import pty
import resource
import signal
import struct
import sys
import termios
import time

import tornado.ioloop
from tornado.escape import to_unicode
from tornado.ioloop import IOLoop

from webtiles import util, config

BUFSIZ = 2048

class TerminalRecorder(object):
    def __init__(self,
                 command, # type: List[str]
                 logger,
                 termsize,
                 env_vars, # type: Dict[str, str]
                 game_cwd, # type: Optional[str]
                 ):
        """
        Args:
            command: argv of command to run, eg [cmd, args, ...]
            env_vars: dictionary of environment variables to set. The variables
                COLUMNS, LINES, and TERM cannot be overridden.
        """
        self.command = command
        self.ttyrec = None
        self.desc = "TerminalRecorder"
        self.returncode = None
        self.output_buffer = b""
        self.termsize = termsize
        self.env_vars = env_vars
        self.game_cwd = game_cwd

        self.pid = None
        self.child_fd = None

        self.end_callback = None
        self.output_callback = None
        self.activity_callback = None
        self.error_callback = None

        self.errpipe_read = None
        self.error_buffer = b""

        self.logger = logger

    def start(self, ttyrec_filename, id_header):
        if ttyrec_filename:
            self.desc = ttyrec_filename
            with util.SlowWarning("Slow IO: open '%s'" % self.desc):
                self.ttyrec = open(ttyrec_filename, "wb") # type: Optional[BinaryIO]
        if id_header:
            self.write_ttyrec_chunk(id_header, flush=True)

        self._spawn()

    def is_started(self):
        return self.pid is not None and self.pid != 0

    def _spawn(self):
        self.errpipe_read, errpipe_write = os.pipe()

        self.pid, self.child_fd = pty.fork()

        if self.pid == 0:
            # We're the child

            # attempt to avoid some race conditions in global effects that can
            # be triggered by early signals (before execvpe replaces process
            # state)
            config.set("pidfile", None)

            # remove server's signal handling for the interim.
            # Note: I haven't found a reliable way to do this, because shared
            # resources are copied on fork. See:
            #   https://github.com/python/cpython/issues/66197
            # so, sometimes, a race condition here leads to the main process
            # reloading its config...currently some key cases are handled
            # elsewhere by a brute force delay.
            asyncio.get_event_loop().remove_signal_handler(signal.SIGHUP)

            # Set window size
            cols, lines = self.get_terminal_size()
            s = struct.pack("HHHH", lines, cols, 0, 0)
            fcntl.ioctl(sys.stdout.fileno(), termios.TIOCSWINSZ, s)

            os.close(self.errpipe_read)
            os.dup2(errpipe_write, 2)

            # Make sure not to retain any files from the parent
            max_fd = resource.getrlimit(resource.RLIMIT_NOFILE)[0]
            for i in range(3, max_fd):
                try:
                    os.close(i)
                except OSError:
                    pass

            # And exec
            env            = dict(os.environ)
            env.update(self.env_vars)
            env["COLUMNS"] = str(cols)
            env["LINES"]   = str(lines)
            env["TERM"]    = "linux"
            if self.game_cwd:
                os.chdir(self.game_cwd)
            try:
                os.execvpe(self.command[0], self.command, env)
            except OSError:
                sys.exit(1)

        # We're the parent
        os.close(errpipe_write)
        if not self.ttyrec:
            self.desc = "TerminalRecorder (fd %d)" % self.child_fd

        IOLoop.current().add_handler(self.child_fd,
                                     self._handle_read,
                                     IOLoop.ERROR | IOLoop.READ)

        IOLoop.current().add_handler(self.errpipe_read,
                                     self._handle_err_read,
                                     IOLoop.READ)

    def _handle_read(self, fd, events):
        if events & IOLoop.READ:
            try:
                with util.SlowWarning("Slow IO: os.read (session '%s')" % self.desc):
                    buf = os.read(fd, BUFSIZ)
            except (OSError, IOError):
                self.poll() # fd probably closed?
                return

            if len(buf) > 0:
                try:
                    self.write_ttyrec_chunk(buf)
                except OSError as e:
                    # should something more happen?
                    self.logger.error("Failed to write ttyrec chunk! (%s)" % e)

                if self.activity_callback:
                    self.activity_callback()

                self.output_buffer += buf
                self._do_output_callback()

            self.poll()

        if events & IOLoop.ERROR:
            self.poll()

    def _handle_err_read(self, fd, events):
        if events & IOLoop.READ:
            with util.SlowWarning("Slow IO: os.read stderr (session '%s')" % self.desc):
                buf = os.read(fd, BUFSIZ)

            if len(buf) > 0:
                self.error_buffer += buf
                self._log_error_output()

            self.poll()

    def write_ttyrec_header(self, sec, usec, l):
        if self.ttyrec is None:
            return
        s = struct.pack("<iii", sec, usec, l)
        self.ttyrec.write(s)

    def write_ttyrec_chunk(self, data, flush=False):
        if self.ttyrec is None:
            return
        with util.SlowWarning("Slow IO: write_ttyrec_chunk '%s'" % self.desc):
            t = time.time()
            self.write_ttyrec_header(int(t), int((t % 1) * 1000000), len(data))
            self.ttyrec.write(data)
        if flush:
            self.flush_ttyrec()

    def flush_ttyrec(self):
        if self.ttyrec is None:
            return
        with util.SlowWarning("Slow IO: flush '%s'" % self.desc):
            self.ttyrec.flush()

    def _do_output_callback(self):
        pos = self.output_buffer.find(b"\n")
        while pos >= 0:
            line = self.output_buffer[:pos]
            self.output_buffer = self.output_buffer[pos + 1:]

            if len(line) > 0:
                if line[-1] == b"\r": line = line[:-1]

                if self.output_callback:
                    self.output_callback(to_unicode(line))

            pos = self.output_buffer.find(b"\n")

    def _log_error_output(self):
        pos = self.error_buffer.find(b"\n")
        while pos >= 0:
            line = self.error_buffer[:pos]
            self.error_buffer = self.error_buffer[pos + 1:]

            if len(line) > 0:
                if line[-1] == b"\r": line = line[:-1]

                self.logger.info("ERR: %s", to_unicode(line))
                if self.error_callback:
                    self.error_callback(to_unicode(line))

            pos = self.error_buffer.find(b"\n")


    def send_signal(self, signal):
        if not self.is_started():
            raise RuntimeError("Can't send a signal without a child process to send it to!")
        os.kill(self.pid, signal)

    def poll(self):
        if self.returncode is None:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid == self.pid:
                if os.WIFSIGNALED(status):
                    self.returncode = -os.WTERMSIG(status)
                elif os.WIFEXITED(status):
                    self.returncode = os.WEXITSTATUS(status)
                else:
                    # Should never happen
                    raise RuntimeError("Unknown child exit status!")

            if self.returncode is not None:
                IOLoop.current().remove_handler(self.child_fd)
                IOLoop.current().remove_handler(self.errpipe_read)

                os.close(self.child_fd)
                os.close(self.errpipe_read)

                if self.ttyrec:
                    with util.SlowWarning("Slow IO: close '%s'" % self.desc):
                        self.ttyrec.close()

                if self.end_callback:
                    self.end_callback()

                # accessed in the default end callback for logging
                self.pid = None

        return self.returncode

    def get_terminal_size(self):
        return self.termsize

    def write_input(self, data):
        if self.poll() is not None: return

        while len(data) > 0:
            written = os.write(self.child_fd, data)
            data = data[written:]
