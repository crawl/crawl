import errno
import logging
import os.path
import re
import smtplib
import time
import itertools
import functools
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText

import tornado.ioloop
import tornado.template

from webtiles import config

try:
    from typing import Any, Callable, Dict, Optional
    from typing.IO import TextIO
except ImportError:
    pass


# utility code for explicit debugging of potentially blocking I/O. We use this
# because while asyncio debugging is good at detecting when a blocking call
# happens, it is terrible at identifying what tornado 6+ is actually doing at
# the time.


class SlowWarning(object):
    def __init__(self, desc, time=None):
        if time is None:
            time = config.get('slow_io_alert')
        self.time = time
        self.desc = desc

    def __enter__(self):
        if self.time:
            self.start = time.monotonic()

    def __exit__(self, exc_type, exc_value, exc_traceback):
        if self.time:
            duration = time.monotonic() - self.start
            if duration >= self.time:
                logger = logging.getLogger("server.py")
                logger.warning('%s: %.3fs', self.desc, duration)
        return False


# backwards compatibility for older python, older Tornado. Also, future-proofing
# for internal API calls that could change. This check disables the slow
# callback diagnostic code if the tries fail.
_asyncio_available = True
try:
    def f():
        pass
    f.__qualname__ # for good measure..
    from asyncio.base_events import _format_handle
    import asyncio.events
    import reprlib
except:
    _asyncio_available = False


def func_repr(func):
    # from asyncio.format_helps._format_callback, not used for rendering
    if hasattr(func, '__qualname__') and func.__qualname__:
        return func.__qualname__
    elif hasattr(func, '__name__') and func.__name__:
        return func.__name__
    else:
        return repr(func)

def callback_arg_repr(f):
    # strip any instances of vacuous functools.partial, for readability (there
    # are a lot of these in practice)
    if isinstance(f, functools.partial) and not f.args:
        f = f.func
    return reprlib.repr(f)

_original_asyncio_run = None

# this function originally based on code from aiodebug:
#   https://gitlab.com/quantlane/libs/aiodebug
# License: Apache 2.0
def set_slow_callback_logging(slow_duration):
    global _asyncio_available
    if not _asyncio_available:
        return

    from webtiles import ws_handler

    global _original_asyncio_run
    if not slow_duration:
        if _original_asyncio_run:
            # de-monkeypatch
            asyncio.events.Handle._run = _original_asyncio_run
            _original_asyncio_run = None
        return

    # the default repr call for asyncio Handles will limit argument strings
    # substantially (default, 20 chars) via reprlib, making it impossible to
    # get any useful info.
    reprlib.aRepr.maxother = 1000

    def on_slow_callback(name, duration):
        if name is None:
            return
        logger = logging.getLogger("server.py")
        logger.warning('Slow callback (%.3fs): %s', duration, name)
        global last_blocking_description
        last_blocking_description = "None"

    def format_tornado_handle(h):
        from tornado.platform.asyncio import BaseAsyncIOLoop

        # to see only certain callbacks when debugging, you can add a filter here.

        # the code here uses a certain level of undocumented / private APIs,
        # and ad-hoc introspection, so do try/except wrapping for future
        # proofing
        try:
            if (h._callback is not None
                            and func_repr(h._callback).startswith("IOLoop") # can this be done with introspection?
                            and h._args):
                # simplify down IOLoop callbacks to just the part that matters.
                # this will typically strip off something like:
                # `<TimerHandle when=X IOLoop._run_callback(functools.partial(...))>`
                # based on asyncio.format_helpers._format_args_and_kwargs
                # possibly items is always size 1 for an IOLoop callback?
                items = [callback_arg_repr(arg) for arg in h._args]
                s = 'IOLoop callback: {}'.format(', '.join(items))
                if h._cancelled:
                    s += " cancelled!"
                return s
            elif (h._callback is not None
                        # check for bound instance method: I cannot for the life of
                        # me figure out how to directly get the `method` type
                        and getattr(h._callback, '__self__', None)
                        and isinstance(h._callback.__self__, BaseAsyncIOLoop)
                        and getattr(h._callback, '__func__', None)
                        and h._callback.__func__ == BaseAsyncIOLoop._handle_events):
                # for event handlers, h._callback is a bound method instance on
                # BaseAsyncIOLoop, which is not very informative. Instead, find
                # the actual callback that Tornado used by introspecting on the
                # method object.
                s = "BaseAsyncIOLoop handler: {}".format(
                    repr(h._callback.__self__.handlers[h._args[0]]))
                if h._cancelled:
                    s += " cancelled!"
                return s
        except:
            pass

        # otherwise, fallback on the default (with the reprlib tweak). The
        # existence of this asyncio.base_events internal function is already
        # checked by a global try..except block.
        return _format_handle(h)

    if not _original_asyncio_run:
        _original_asyncio_run = asyncio.events.Handle._run

    def instrumented(self):
        global _original_asyncio_run
        t0 = time.monotonic()
        return_value = _original_asyncio_run(self)
        dt = time.monotonic() - t0
        if dt >= slow_duration:
            on_slow_callback(format_tornado_handle(self), dt)
        return return_value

    asyncio.events.Handle._run = instrumented


class TornadoFilter(logging.Filter):
    def filter(self, record):  # noqa A003: ignore shadowing builtin
        if record.module == "web" and record.levelno <= logging.INFO:
            return False
        return True


class DynamicTemplateLoader(tornado.template.Loader):  # type: ignore
    def __init__(self, root_dir):  # type: (str) -> None
        tornado.template.Loader.__init__(self, root_dir)

    def load(self, name, parent_path=None):
        name = self.resolve_path(name, parent_path=parent_path)
        # Fall back to template.html.default if template.html does not exist
        if not os.path.isfile(os.path.join(self.root, name)):
            name = name + '.default'

        if name in self.templates:
            template = self.templates[name]
            path = os.path.join(self.root, name)
            if os.path.getmtime(path) > template.load_time:  # type: ignore
                del self.templates[name]
            else:
                return template

        parent_cls = super(DynamicTemplateLoader, self)
        template = parent_cls.load(name, parent_path)  # type: ignore
        template.load_time = time.time()  # type: ignore
        return template

    _instances = {}  # type: Dict[str, DynamicTemplateLoader]

    @classmethod
    def get(cls, path):  # type: (str) -> DynamicTemplateLoader
        if path in cls._instances:
            return cls._instances[path]
        else:
            loader = DynamicTemplateLoader(path)
            cls._instances[path] = loader
            return loader


# light subclass to improve logging for the FileTailer scheduler
class PeriodicCallback(tornado.ioloop.PeriodicCallback):
    def __init__(self, check, interval_ms, source_desc=None):
        super(PeriodicCallback, self).__init__(check, interval_ms)
        self._source_desc = source_desc if source_desc else repr(check)

    def __repr__(self):
        return "PeriodicCallback(%s)" % self._source_desc


class FileTailer(object):
    def __init__(self, filename, callback):
        # type: (str, Callable[[str], Any], int) -> None
        self.file = None  # type: Optional[TextIO]
        self.filename = filename
        self.callback = callback
        self.scheduler = PeriodicCallback(self.check,
                config.get('milestone_interval'),
                "FileTailer('%s').check" % filename)
        self.scheduler.start()

    def check(self):  # type: () -> None
        if self.file is None:
            try:
                with SlowWarning("Slow IO: open '%s'" % self.filename):
                    self.file = open(self.filename, "r")
            except (IOError, OSError) as e:  # noqa
                if e.errno == errno.ENOENT:
                    return
                else:
                    raise

            with SlowWarning("Slow IO: seek '%s'" % self.filename):
                self.file.seek(os.path.getsize(self.filename))

        while True:
            pos = self.file.tell()
            with SlowWarning("Slow IO: readline '%s'" % self.filename):
                line = self.file.readline()
            if line.endswith("\n"):
                self.callback(line)
            else:
                self.file.seek(pos)
                return

    def stop(self):  # type: () -> None
        self.scheduler.stop()


_WHERE_ENTRY_REGEX = re.compile("(?<=[^:]):(?=[^:])")


def parse_where_data(data):  # type: (str) -> Dict[str, str]
    where = {}

    for entry in _WHERE_ENTRY_REGEX.split(data):
        if not entry.strip():
            continue
        field, _, value = entry.partition("=")
        where[field.strip()] = value.strip().replace("::", ":")
    return where


def send_email(to_address, subject, body_plaintext, body_html):
    # type: (str, str, str, str) -> None
    if not to_address:
        return

    logging.info("Sending email to '%s' with subject '%s'", to_address, subject)
    connected = False
    try:
        # establish connection
        # TODO: this should not be a blocking call at all...
        if config.get('smtp_use_ssl'):
            email_server = smtplib.SMTP_SSL(
                config.get('smtp_host'), config.get('smtp_port'))  # type: smtplib.SMTP
        else:
            email_server = smtplib.SMTP(config.get('smtp_host'), config.get('smtp_port'))
        connected = True

        # authenticate
        if config.get('smtp_user'):
            email_server.login(config.get('smtp_user'), config.get('smtp_password'))

        # build multipart message
        msg = MIMEMultipart('alternative')
        msg['Subject'] = subject
        msg['From'] = config.get('smtp_from_addr')
        msg['To'] = to_address

        part1 = MIMEText(body_plaintext, 'plain')
        part2 = MIMEText(body_html, 'html')

        msg.attach(part1)
        msg.attach(part2)

        # send
        email_server.sendmail(config.get('smtp_from_addr'), to_address, msg.as_string())
    finally:
        # end connection
        if connected:
            email_server.quit()


def validate_email_address(address):  # type: (str) -> Optional[str]
    # Returns an error string describing the problem, or None.
    # NOTE: a blank email address validates successfully.
    if not address:
        return None
    if " " in address:
        return "Email address can't contain a space"
    if "@" not in address:
        return "Expected email address to contain the @ symbol"
    if not re.match(r"[^@]+@[^@]+\.[^@]+", address):
        return "Invalid email address"
    if len(address) >= 80:
        return "Email address can't be more than 80 characters"
    return None


def humanise_bytes(num):  # type: (int) -> str
    """Convert raw bytes into human-friendly measure."""
    # TODO: Replace with pypi/humanize when we remove python 2 support.
    units = ["kilobytes", "megabytes", "gigabytes"]
    for index, unit in reversed(list(enumerate(units))):
        index += 1
        n = float(num) / 1000**index
        if n > 1:
            return "{} {}".format(round(n, 1), unit)
    return "{} bytes".format(num)
