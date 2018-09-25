import re
import logging
import tornado.template
import tornado.ioloop
import os.path
import time
import smtplib

from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

from config import (smtp_use_ssl, smtp_host, smtp_port,
                    smtp_user, smtp_password, smtp_from_addr)

class TornadoFilter(logging.Filter):
    def filter(self, record):
        if record.module == "web" and record.levelno <= logging.INFO:
            return False
        return True

class DynamicTemplateLoader(tornado.template.Loader):
    def __init__(self, root_dir):
        tornado.template.Loader.__init__(self, root_dir)

    def load(self, name, parent_path=None):
        name = self.resolve_path(name, parent_path=parent_path)
        if name in self.templates:
            template = self.templates[name]
            path = os.path.join(self.root, name)
            if os.path.getmtime(path) > template.load_time:
                del self.templates[name]
            else:
                return template

        template = super(DynamicTemplateLoader, self).load(name, parent_path)
        template.load_time = time.time()
        return template

    _instances = {}

    @classmethod
    def get(cls, path):
        if path in cls._instances:
            return cls._instances[path]
        else:
            l = DynamicTemplateLoader(path)
            cls._instances[path] = l
            return l

class FileTailer(object):
    def __init__(self, filename, callback, interval_ms = 1000, io_loop = None):
        self.file = None
        self.filename = filename
        self.callback = callback
        self.io_loop = io_loop or tornado.ioloop.IOLoop.instance()
        self.scheduler = tornado.ioloop.PeriodicCallback(self.check, interval_ms,
                                                         io_loop = self.io_loop)
        self.scheduler.start()

    def check(self):
        if self.file is None:
            if os.path.exists(self.filename):
                self.file = open(self.filename, "r")
                self.file.seek(os.path.getsize(self.filename))
            else:
                return

        while True:
            pos = self.file.tell()
            line = self.file.readline()
            if line.endswith("\n"):
                self.callback(line)
            else:
                self.file.seek(pos)
                return

    def stop(self):
        self.scheduler.stop()

def dgl_format_str(s, username, game_params):
    s = s.replace("%n", username)

    return s

where_entry_regex = re.compile("(?<=[^:]):(?=[^:])")

def parse_where_data(data):
    where = {}

    for entry in where_entry_regex.split(data):
        if entry.strip() == "": continue
        field, _, value = entry.partition("=")
        where[field.strip()] = value.strip().replace("::", ":")
    return where

def send_email(to_address, subject, body_plaintext, body_html):
    if not to_address: return

    # establish connection
    if smtp_use_ssl:
        email_server = smtplib.SMTP_SSL(smtp_host, smtp_port)
    else:
        email_server = smtplib.SMTP(smtp_host, smtp_port)

    try:
        # authenticate
        email_server.login(smtp_user, smtp_password)

        # build multipart message
        msg = MIMEMultipart('alternative')
        msg['Subject'] = subject
        msg['From'] = smtp_from_addr
        msg['To'] = to_address

        part1 = MIMEText(body_plaintext, 'plain')
        part2 = MIMEText(body_html, 'html')

        msg.attach(part1)
        msg.attach(part2)

        # send
        email_server.sendmail(smtp_from_addr, to_address, msg.as_string())
    finally:
        # end connection
        if email_server: email_server.quit()

def validate_email_address(address): # Returns an error string describing the problem, or None
    if not address: return "Email address can't be empty"
    if " " in address: return "Email address can't contain a space"
    if "@" not in address: return "Expected email address to contain the @ symbol"
    if not re.match(r"[^@]+@[^@]+\.[^@]+", address): return "Invalid email address"
    return None
