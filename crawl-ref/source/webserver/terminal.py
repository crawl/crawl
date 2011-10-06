import pty
import termios
import os
import fcntl
import struct
import resource
import sys
import time

BUFSIZ = 2048

class TerminalRecorder(object):
    def __init__(self, command, filename, io_loop):
        self.io_loop = io_loop
        self.command = command
        self.ttyrec = open(filename, "w", 0)
        self.id = id
        self.returncode = None
        self.output_buffer = ""

        self.pid = None
        self.child_fd = None

        self.end_callback = None
        self.output_callback = None

        self._spawn()

    def _spawn(self):
        self.pid, self.child_fd = pty.fork()

        if self.pid == 0:
            # We're the child
            # Set window size
            cols = 80
            lines = 24
            s = struct.pack("HHHH", lines, cols, 0, 0)
            fcntl.ioctl(sys.stdout.fileno(), termios.TIOCSWINSZ, s)

            # Make sure not to retain any files from the parent
            max_fd = resource.getrlimit(resource.RLIMIT_NOFILE)[0]
            for i in range (3, max_fd):
                try:
                    os.close (i)
                except OSError:
                    pass

            # And exec
            env            = dict(os.environ)
            env["COLUMNS"] = str(cols)
            env["LINES"]   = str(lines)
            env["TERM"]    = "linux"
            os.execvpe(self.command[0], self.command, env)

        # We're the parent
        self.io_loop.add_handler(self.child_fd,
                                 self._handle_read,
                                 self.io_loop.ERROR | self.io_loop.READ)

    def _handle_read(self, fd, events):
        if events & self.io_loop.READ:
            buf = os.read(fd, BUFSIZ)

            if len(buf) > 0:
                self.write_ttyrec_chunk(buf)

                self.output_buffer += buf
                self._do_output_callback()

            self.poll()

        if events & self.io_loop.ERROR:
            self.poll()

    def write_ttyrec_header(self, sec, usec, l):
        s = struct.pack("<iii", sec, usec, l)
        self.ttyrec.write(s)

    def write_ttyrec_chunk(self, data):
        t = time.time()
        self.write_ttyrec_header(int(t), int((t % 1) * 1000000), len(data))
        self.ttyrec.write(data)

    def _do_output_callback(self):
        pos = self.output_buffer.find("\n")
        while pos >= 0:
            line = self.output_buffer[:pos]
            self.output_buffer = self.output_buffer[pos + 1:]

            if len(line) > 0:
                if line[-1] == "\r": line = line[:-1]

                if self.output_callback:
                    self.output_callback(line)

            pos = self.output_buffer.find("\n")

    def send_signal(self, signal):
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
                self.io_loop.remove_handler(self.child_fd)
                os.close(self.child_fd)

                self.ttyrec.close()

                if self.end_callback:
                    self.end_callback()

        return self.returncode

    def write_input(self, data):
        while len(data) > 0:
            written = os.write(self.child_fd, data)
            data = data[written:]
