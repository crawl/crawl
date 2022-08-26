import errno
import logging
import os.path
import re
import smtplib
import time
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

# backwards compatibility for python 2.7, remove this some day. Disables the
# manual blocking diagnostic code if we are this old.
_asyncio_available = True
try:
    def f():
        pass
    f.__qualname__ # for good measure..
    from asyncio.base_events import _format_handle
    import asyncio.events
except:
    _asyncio_available = False

# note: this is somewhat wacky because the slow callback logging code will
# always run *after* this context manager has exited, but we do sometimes want
# to nest them. TODO: could timing be done directly in this?
blocking_description = []
last_blocking_description = "None"
class BlockingDescriptionCtx(object):
    def __init__(self):
        self.desc = "None"

    def __enter__(self):
        global blocking_description, last_blocking_description
        blocking_description.append(self.desc)
        last_blocking_description = self.desc

    def __exit__(self, exc_type, exc_value, exc_traceback):
        global blocking_description, last_blocking_description
        blocking_description.pop()
        if len(blocking_description):
            last_blocking_description = blocking_description[-1]
        return False

# just use this as a singleton, don't want to add even *more* overhead to things
# that may be blocking...
cached_blocking_ctx = BlockingDescriptionCtx()

# use the context manager returned by this to explicitly mark some code as
# potentially blocking, for logging purposes. Should have minimal overhead..
def note_blocking(desc):
    global cached_blocking_ctx
    cached_blocking_ctx.desc = desc
    return cached_blocking_ctx

def annotate_blocking_note(a):
    global last_blocking_description, blocking_description, _asyncio_available
    if not _asyncio_available:
        return # don't bother if no asyncio
    if last_blocking_description:
        last_blocking_description += a
        # sanity check: don't let this accrue infinitely if it is called wrong
        last_blocking_description = last_blocking_description[:100]
    for i in range(len(blocking_description)):
        blocking_description[i] += a

# corresponding decorator, automatically uses function name
def note_blocking_fun(f):
    global _asyncio_available
    if not _asyncio_available:
        return f

    def wrapped(*args, **kwargs):
        with note_blocking(f.__qualname__):
            return f(*args, **kwargs)
    return wrapped

def last_noted_blocker():
    global last_blocking_description
    return last_blocking_description

_original_asyncio_run = None

# this function based on code from aiodebug:
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

    def on_slow_callback(name, duration):
        logger = logging.getLogger("server.py")
        logger.warning('Slow callback (%.3fs, %s): %s. Socket state: %s',
            duration, last_noted_blocker(), name, ws_handler.describe_sockets())
        global last_blocking_description
        last_blocking_description = "None"

    if not _original_asyncio_run:
        _original_asyncio_run = asyncio.events.Handle._run

    def instrumented(self):
        global _original_asyncio_run
        t0 = time.monotonic()
        return_value = _original_asyncio_run(self)
        dt = time.monotonic() - t0
        if dt >= slow_duration:
            on_slow_callback(_format_handle(self), dt)
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


class FileTailer(object):
    def __init__(self, filename, callback, interval_ms=1000):
        # type: (str, Callable[[str], Any], int) -> None
        self.file = None  # type: Optional[TextIO]
        self.filename = filename
        self.callback = callback
        self.scheduler = tornado.ioloop.PeriodicCallback(self.check,
                                                         interval_ms)
        self.scheduler.start()

    def check(self):  # type: () -> None
        if self.file is None:
            try:
                self.file = open(self.filename, "r")
            except (IOError, OSError) as e:  # noqa
                if e.errno == errno.ENOENT:
                    return
                else:
                    raise

            self.file.seek(os.path.getsize(self.filename))

        while True:
            pos = self.file.tell()
            line = self.file.readline()
            if line.endswith("\n"):
                self.callback(line)
            else:
                self.file.seek(pos)
                return

    def stop(self):  # type: () -> None
        self.scheduler.stop()


def dgl_format_str(s, username, game_params):  # type: (str, str, Any) -> str
    s = s.replace("%n", username)

    return s


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
