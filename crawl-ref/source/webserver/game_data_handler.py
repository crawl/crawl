import os.path

import tornado
import tornado.web

import config

try:
    from typing import Dict
except:
    pass

class GameDataHandler(tornado.web.StaticFileHandler):
    def initialize(self):
        if tornado.version_info[0] < 3:
            # ugly extreme backward compatibility hack; can hopefully be removed
            # once tornado 2.4 is out of the picture.
            super(GameDataHandler, self).initialize(".")
            self.root = "/" # just override the superclass for these old versions...
        else:
            super(GameDataHandler, self).initialize("/")

    def parse_url_path(self, url_path):
        # the path should already match "([0-9a-f]*\/.*)", from server.py
        import sys
        version, url_path = url_path.split("/", 1)
        if version not in GameDataHandler._client_paths:
            raise tornado.web.HTTPError(404)
        return super(GameDataHandler, self).parse_url_path(
                        GameDataHandler._client_paths[version] + "/" + url_path)

    def set_extra_headers(self, path):
        if config.game_data_no_cache:
            self.set_header("Cache-Control",
                            "no-cache, no-store, must-revalidate")
            self.set_header("Pragma", "no-cache")
            self.set_header("Expires", "0")

    _client_paths = {} # type: Dict[str, str]

    @classmethod
    def add_version(cls, version, path):
        cls._client_paths[version] = os.path.abspath(path)
