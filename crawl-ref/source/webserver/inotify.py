import struct
import ctypes
import os, os.path
import errno
import sys
import tornado.ioloop
import tornado.platform.posix

# The below class is from pyinotify, released under the MIT license
# Copyright (c) 2010 Sebastien Martini <seb@dbzteam.org>
class _CtypesLibcINotifyWrapper():
    def __init__(self):
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

    def _inotify_init(self):
        assert self._libc is not None
        return self._libc.inotify_init()

    def _inotify_add_watch(self, fd, pathname, mask):
        assert self._libc is not None
        pathname = ctypes.create_string_buffer(pathname)
        return self._libc.inotify_add_watch(fd, pathname, mask)

    def _inotify_rm_watch(self, fd, wd):
        assert self._libc is not None
        return self._libc.inotify_rm_watch(fd, wd)

class DirectoryWatcher(object):
    CREATE = 0x100 # IN_CREATE
    DELETE = 0x200 # IN_DELETE

    def __init__(self, io_loop=None):
        self.io_loop = io_loop or tornado.ioloop.IOLoop.instance()
        self.inotify = _CtypesLibcINotifyWrapper()
        self.enabled = self.inotify.init()
        if self.enabled:
            self.fd = self.inotify._inotify_init()
            tornado.platform.posix._set_nonblocking(self.fd)
            tornado.platform.posix.set_close_exec(self.fd)
            self.io_loop.add_handler(self.fd, self._handle_read,
                                     self.io_loop.ERROR | self.io_loop.READ)
        self.handlers = dict()
        self.paths = dict()
        self.buffer = bytes()

    def watch(self, path, handler):
        if self.enabled:
            w = self.inotify._inotify_add_watch(self.fd, path,
                                                DirectoryWatcher.CREATE |
                                                DirectoryWatcher.DELETE)
            self.handlers[w] = handler
            self.paths[w] = path

    def _handle_read(self, fd, event):
        if event & self.io_loop.ERROR:
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
                while name[-1] == "\x00":
                    name = name[:-1]
                i += l
                self.handlers[w](os.path.join(self.paths[w], name), mask)
        except OSError as e:
            if e.errno in (errno.EWOULDBLOCK, errno.EAGAIN):
                return
            else:
                raise
