"""Handler for Janitor requests."""

import string
import os
import re
import shlex

import tornado.web
from tornado.escape import url_unescape
import tornado.gen
from tornado.process import Subprocess

from conf import config
import userdb
import logging


class JanitorHandler(tornado.web.RequestHandler):
    """Handle Janitor requests."""

    def _format_command_argument(self, command_id):
        """Return a formatted janitor command argument using the request
        arguments as parameters.

        Handles the following security concerns:
        * Only required params are in the returned dict.
        * All params are given with exactly one value.
        * Parameter values contain only safe characters.
        """

        valid_params = {}
        command = config.janitor_commands[command_id]
        for param in command["params"]:
            if param not in self.request.arguments \
               or not self.request.arguments[param]:
                raise ValueError("Missing param {0}".format(param))
            if len(self.request.arguments[param]) > 1:
                raise ValueError("Multiple values for param {0}".format(param))
            param_value = self.request.arguments[param][0]
            # Check that the value contains only safe characters.
            if not re.match("^[a-zA-Z0-9_-]+$", param_value):
                raise ValueError("Bad characters in param {0}".format(param))
            valid_params[param] = param_value
        return command["argument"].format(**valid_params)

    def _handle_command_data(self, data):
        self.write(data)
        self.flush()

    def _action_source(self, path):
        filename = os.path.basename(path)
        self.set_header("Content-Type", "application/octet-stream")
        self.set_header("Content-Disposition",
                        "attachment; filename=" + filename)
        try:
            fh = open(path, "rb")
        except EnvironmentError as e:
            self.fail(500, "Could not open file {0}: {1}".format(path,
                                                                 e.strerror))
            return
        self.write(fh.read())
        fh.close()
        self.flush()

    def _action_command(self, command):
        args = shlex.split(command)
        try:
            p = Subprocess(args, stdout=Subprocess.STREAM,
                           stderr=Subprocess.STREAM)
        except OSError as e:
            self.fail(400, "Command {0} could not execute: "
                      "{1}".format(command, e.strerror))
            return
        self.set_header("Content-Type", "text/plain")
        self.write("Kicked off command {0}\n".format(command))
        self.flush()

        def no_handler(data):
            return
        p.stderr.read_until_close(callback=no_handler,
                                  streaming_callback=self._handle_command_data)
        p.stdout.read_until_close(callback=self._handle_command_exit,
                                  streaming_callback=self._handle_command_data)

    def _handle_command_exit(self, data):
        self.write("Janitor command complete!")
        self.finish()

    def fail(self, code, message):
        """Handler override for error page pre-render tasks."""
        logging.debug("{0}: {1}: {2}".format(self.request.remote_ip, code,
                                             message))
        self.send_error(code, message=message)

    @tornado.web.asynchronous
    def get(self, command_id=None):
        """Handler for Janitor HTTP GET requests."""

        sid = self.get_cookie("sid")
        session = None
        if sid:
            session = userdb.session_info(url_unescape(sid))
        if not session:
            self.fail(403, "You must be logged in to access this.")
            return

        if not config.is_server_janitor(session["username"]):
            self.fail(403, "You do not have permission to access this.")
            return

        if not config.janitor_commands or not command_id \
           or command_id not in config.janitor_commands:
            self.fail(400, "Invalid janitor command.")
            return
        try:
            janitor_arg = self._format_command_argument(command_id)
        except ValueError as e:
            self.fail(400, e)
            return
        # The source command is not async, so we finish the session after the
        # call.
        cmd = config.janitor_commands[command_id]
        if cmd["action"] == "source":
            logging.debug("Returning the static file {0}".format(janitor_arg))
            self._action_source(janitor_arg)
            self.finish()
        elif cmd["action"] == "command":
            logging.debug("Running command {0}".format(command_id))
            self._action_command(janitor_arg)

    def write_error(self, status_code, **kwargs):
        """Handler override for error page rendering."""
        if "message" in kwargs:
            msg = "{0}: {1}".format(status_code, kwargs["message"])
            self.write("<html><body>{0}</body></html>".format(msg))
