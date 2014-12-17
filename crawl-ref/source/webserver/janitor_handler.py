"""Handler for Janitor requests."""

import string
import os
import re
import subprocess
import shlex

import tornado.web
import tornado.escape
import tornado.gen

from conf import config
import userdb
import logging


class JanitorHandler(tornado.web.RequestHandler):

    """Handle Janitor requests."""

    def __init__(self, application, request):
        """See tornado.web.RequestHandler.__init__."""
        # NOTE: this regex MUST be anchored on ^both sides$
        self.PARAM_REGEX = r'^[a-zA-Z0-9_-]+$'
        super(JanitorHandler, self).__init__(application, request)

    def _resolve_params(self, raw_string, provided_args):
        """Return a validated dict of provided params.

        Handles the following security concerns:
        * Only required params are in the returned dict
        * All params were specified with exactly one value
        """
        required_params = set(token[1] for token in
                              string.Formatter().parse(raw_string)
                              if token[1]
                              )
        validated_params = {}
        for param in required_params:
            if param not in self.request.arguments:
                raise ValueError("Missing param %s" % param)
            if len(self.request.arguments[param]) == 0:
                # this is a sanity check for &arg=
                raise ValueError("No values for param %s" % param)
            if len(self.request.arguments[param]) > 1:
                raise ValueError(">1 values for param %s" % param)

            param_value = self.request.arguments[param][0]
            if not re.match(self.PARAM_REGEX, param_value):
                raise ValueError("Bad characters in param %s" % param)
            validated_params[param] = param_value
        return validated_params

    def _dispatch_request(self, action, args):
        """
        Dispatch the actual janitor command.

        Responsible for validating 'action' is valid.
        """
        if action == 'source':
            logging.debug("Returning the static file %s" % args)
            self._action_source(args)
        elif action == 'command':
            logging.debug("Running command %s" % args)
            self._action_command(args)
        else:
            assert False  # check_config should ensure this is unreachable

    def _get_command(self, arg, janitor_commands):
        """Return the configuration for a given janitor ID.

        May return None.
        """
        cmd = [cmd for cmd in janitor_commands if cmd['id'] == arg]
        assert len(cmd) in (0, 1)  # sanity check: check_config ensures this
        if cmd:
            return cmd[0]
        else:
            return None

    def _action_source(self, path):
        if not os.path.isfile(path):
            logging.debug("Requested file does not exist: %s" % path)
            self.send_error(404)
            return
        if os.path.abspath(path) != path:
            # probably someone trying to be clever with a /../ component
            logging.debug("Requested path is invalid")
            self.send_error(400)
            return
        filename = os.path.basename(path)
        self.set_header('Content-Type', 'application/octet-stream')
        self.set_header('Content-Disposition',
                        'attachment; filename=' + filename)
        with open(path, 'rb') as f:
            self.write(f.read())
            self.flush()

    def _action_command(self, command):
        args = shlex.split(command)
        p = tornado.process.Subprocess(args)
        p.set_exit_callback(self._handle_command_exit)
        self.write('Kicked off command "%s".<br>' % command)

    def _handle_command_exit(self, returncode):
        # This handler makes Tornado reap completed processes
        # XXX: there is probably a better way to handle this, but the entire
        # command processing should become async anyway.
        logging.debug("Some janitor command exited: %s" % returncode)

    def fail(self, code, message):
        """Handler override for error page pre-render tasks."""
        logging.debug("%s: %s: %s", self.request.remote_ip, code, message)
        self.send_error(code, message=message)

    def get(self, arg=None):
        """Handler override for GET requests."""
        janitor_commands = config.get('janitor_commands')

        # XXX: should clean up this session management
        # factor it all out to util?
        sid = self.get_cookie("sid")
        if not sid:
            self.fail(403, "You must be logged in to access this.")
            return
        session = userdb.session_info(sid)
        if not session:
            self.fail(403, "You must be logged in to access this.")
            return
        janitors = config.janitors
        if session['username'].lower() not in janitors:
            self.fail(403, "You do not have permission to access this.")
            return

        if not janitor_commands:
            # This is also checked by check_config
            self.fail(404, "No janitor_commands defined")
            return
        if not arg:
            self.fail(400, "No specific janitor command requested")
            return

        cmd = self._get_command(arg, janitor_commands)
        if not cmd:
            self.fail(404, "No janitor_command matches %s" % arg)
            return

        try:
            safe_params = self._resolve_params(cmd['args'],
                                               self.request.arguments)
        except ValueError as e:
            self.fail(400, e)
            return

        args_formatted = cmd['args'].format(**safe_params)
        self._dispatch_request(cmd['action'], args_formatted)

    def write_error(self, status_code, **kwargs):
        """Handler override for error page rendering."""
        if 'message' in kwargs:
            msg = "%s: %s" % (status_code, kwargs['message'])
            self.write('<html><body>%s</body></html>' % msg)
