import ctypes
import ctypes.util
import errno
import os
import os.path
import struct
import sys

import tornado.ioloop
import tornado.platform.posix
from tornado.ioloop import IOLoop

try:
    from typing import Any, Callable, Dict
except ImportError:
    pass

# The below class is from pyinotify, released under the MIT license
# Copyright (c) 2010 Sebastien Martini <seb@dbzteam.org>
class _CtypesLibcINotifyWrapper():
    def __init__(self):  # type: () -> None
        self._libc = None
        self._get_errno_func = None

    def init(self):
        assert ctypes
        libc_name = None
        try:
            libc_name = ctypes.util.find_library('c')
        except (OSError, IOError):
            pass  # Will attempt to load it with None anyway.

        if sys.version_info >= (2, 6):
            self._libc = ctypes.CDLL(libc_name, use_errno=True)
            self._get_errno_func = ctypes.get_errno
        else:
            self._libc = ctypes.CDLL(libc_name)
            try:
                location = self._libc.__errno_location
                location.restype = ctypes.POINTER(ctypes.c_int)
                self._get_errno_func = lambda: location().contents.value
            except AttributeError:
                pass

        # Eventually check that libc has needed inotify bindings.
        if (not hasattr(self._libc, 'inotify_init') or
            not hasattr(self._libc, 'inotify_add_watch') or
            not hasattr(self._libc, 'inotify_rm_watch')):
            return False

        self._libc.inotify_init.argtypes = []
        self._libc.inotify_init.restype = ctypes.c_int
        self._libc.inotify_add_watch.argtypes = [ctypes.c_int, ctypes.c_char_p,
                                                 ctypes.c_uint32]
        self._libc.inotify_add_watch.restype = ctypes.c_int
        self._libc.inotify_rm_watch.argtypes = [ctypes.c_int, ctypes.c_int]
        self._libc.inotify_rm_watch.restype = ctypes.c_int
        return True

    def _get_errno(self):
        if self._get_errno_func is not None:
            return self._get_errno_func()
        return None

    def _inotify_init(self):  # type: () -> int
        assert self._libc is not None
        return self._libc.inotify_init()

    def _inotify_add_watch(self, fd, pathname, mask):
        # type: (int, bytes, int) -> int
        assert self._libc is not None
        pathname_buf = ctypes.create_string_buffer(pathname)
        return self._libc.inotify_add_watch(fd, pathname_buf, mask)

    def _inotify_rm_watch(self, fd, wd):  # type: (int, int) -> int
        assert self._libc is not None
        return self._libc.inotify_rm_watch(fd, wd)

class DirectoryWatcher(object):
    CREATE = 0x100 # IN_CREATE
    DELETE = 0x200 # IN_DELETE

    def __init__(self):  # type: () -> None
        self.inotify = _CtypesLibcINotifyWrapper()
        self.enabled = self.inotify.init()
        if self.enabled:
            self.fd = self.inotify._inotify_init()
            tornado.platform.posix._set_nonblocking(self.fd)
            tornado.platform.posix.set_close_exec(self.fd)
            IOLoop.current().add_handler(self.fd, self._handle_read,
                                         IOLoop.ERROR | IOLoop.READ)
        self.handlers = dict() # type: Dict[int, Callable[[str, int], Any]]
        self.paths = dict()  # type: Dict[int, str]
        self.buffer = bytes()

    def watch(self, path, handler):  # type: (str, Callable[[str, int], Any]) -> None
        if self.enabled:
            w = self.inotify._inotify_add_watch(self.fd, path.encode('utf-8'),
                                                DirectoryWatcher.CREATE |
                                                DirectoryWatcher.DELETE)
            self.handlers[w] = handler
            self.paths[w] = path

    def _handle_read(self, fd, event):
        if event & IOLoop.ERROR:
            return

        try:
            bufsize = 1024
            data = os.read(self.fd, bufsize)
            i = 0
            while i < len(data):
                (w,) = struct.unpack_from("@i", data, i)
                i += struct.calcsize("@i")
                (mask, cookie, l) = struct.unpack_from("=III", data, i)
                i += 3*4
                (name,) = struct.unpack_from("%ds" % l, data, i)
                name = name.rstrip(b"\x00").decode('utf-8')
                i += l
                self.handlers[w](os.path.join(self.paths[w], name), mask)
        except OSError as e:
            if e.errno in (errno.EWOULDBLOCK, errno.EAGAIN):
                return
            else:
                raise
