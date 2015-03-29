import tornado.web
import os.path
import logging

import config


class GameDataHandler(tornado.web.StaticFileHandler):

    """Handle requests for game data URLs.

    Game data is DCSS-build-specific js, css & png stuff. We sha1 the game-id
    to use as a unique identifier to map to the real backend filesystem path.
    """

    def initialize(self):
        """Initialise needs to be overridden, but don't do anything special."""
        super(GameDataHandler, self).initialize("/gamedata/")

    @classmethod
    def get_absolute_path(self, root, path):
        """Return the filesystem path that a given URL represents."""
        clientversion, staticfile = path.split('/', 1)
        if clientversion in GameDataHandler._client_paths:
            return GameDataHandler._client_paths[clientversion] + \
                '/' + staticfile
        else:
            raise tornado.web.HTTPError(404)

    def set_extra_headers(self, path):
        if config.game_data_no_cache:
            self.set_header("Cache-Control",
                            "no-cache, no-store, must-revalidate")
            self.set_header("Pragma", "no-cache")
            self.set_header("Expires", "0")

    # We need to override this because Tornado forbids
    # our filesystem path otherwise. Or something...
    def validate_absolute_path(self, root, absolute_path):
        """Validate a filesystem path.

        Needs to be overridden because we tell the StaticFileHandler the root
        dir is '.' but the resources can be loaded from anywhere on the
        filesystem.
        """
        return absolute_path

    _client_paths = {}

    @classmethod
    def add_version(cls, version, path):
        """Add a crawl version to our map.

        The map is shared between all GameDataHandler instances for performance.
        There's only one instance of the handler right now though.
        """
        cls._client_paths[version] = os.path.abspath(path)
