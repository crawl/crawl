import logging
import os

import tornado.web

from conf import config


class SaveHandler(tornado.web.RequestHandler):

    """Handle save backup requests.

    These can be:
    1. Back up a current save
    2. Serve a backed-up save (to devs).
    3. List currently backed-up saves.
    """

    def _listsaves(self):
        """List all files in the save backup directory."""
        yield (item for item in os.listdir(config['save_backup_path']) if
               os.path.isfile(item))

    def _getsave(self, name):
        """Serve a save file.

        TODO: check request is authorised.
        """
        logging.debug("Sending save %s", name)
        path = os.path.join(config['save_backup_path'], name)
        self.set_header("Content-Type", "application/octet-stream")
        self.set_header("Content-Disposition",
                        "attachment; filename=" + name)
        fh = open(path, "rb")
        self.write(fh.read())
        fh.close()
        self.flush()

    def _createsave(self, name, gameid):
        """Back up the current save for name in game gameid."""
        raise NotImplementedError("_createsave")
        savename = '{date}-{gameid}-{name}'

    def get(self, command):
        """Handle requests."""
        if not command:
            self.write('<br>\n'.join(self._listsaves()))
        elif command == 'get':
            if 'id' not in self.request.arguments:
                self.send_error(400, message="Missing id!")
            saveid = self.request.arguments['id'][0]
            savename = saveid + '.cs'
            self._getsave(savename)
        else:
            self.send_error(400, message="Unknown command!")

    def write_error(self, status_code, **kwargs):
        """Handler override for error page rendering."""
        if "message" in kwargs:
            msg = "{0}: {1}".format(status_code, kwargs["message"])
            self.write("<html><body>{0}</body></html>".format(msg))
