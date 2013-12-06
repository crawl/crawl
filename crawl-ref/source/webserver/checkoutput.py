import os
import subprocess
import errno
import fcntl

BUFSIZ = 1024

def _set_nonblocking(fd):
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

"""A non-blocking version of subprocess.check_output on the tornado ioloop."""
def check_output(call, callback, ioloop):
    out_r, out_w = os.pipe()
    nul_f = open(os.devnull, 'w')
    p = subprocess.Popen(call, stdout=out_w, stderr=nul_f)
    nul_f.close()
    os.close(out_w)
    _set_nonblocking(out_r)
    data = []

    def _poll():
        if p.returncode is None:
            p.poll()

        if p.returncode is not None:
            ioloop.remove_handler(out_r)
            os.close(out_r)

            callback("".join(data), p.returncode)

    def _handle_read(fd, events):
        if events & ioloop.READ:
            try:
                buf = os.read(out_r, BUFSIZ)
            except (IOError, OSError) as e:
                if e.args[0] == errno.EBADF:
                    _poll()
                elif e.args[0] not in (errno.EWOULDBLOCK, errno.EAGAIN):
                    raise

            if not buf:
                _poll()
            else:
                data.append(buf)

        if events & ioloop.ERROR:
            _poll()

    ioloop.add_handler(out_r, _handle_read,
                       ioloop.ERROR | ioloop.READ)
