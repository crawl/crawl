import tornado.web
import os.path

import config

class GameDataHandler(tornado.web.StaticFileHandler):
    def initialize(self):
        super(GameDataHandler, self).initialize(".")

    def head(self, version, path):
        self.get(version, path, include_body=False)

    def get(self, version, path, include_body=True):
        if version not in GameDataHandler._client_paths:
            raise tornado.web.HTTPError(404)
        self.root = GameDataHandler._client_paths[version]
        super(GameDataHandler, self).get(path, include_body)

    def set_extra_headers(self, path):
        if config.game_data_no_cache:
            self.set_header("Cache-Control",
                            "no-cache, no-store, must-revalidate")
            self.set_header("Pragma", "no-cache")
            self.set_header("Expires", "0")

    _client_paths = {}

    @classmethod
    def add_version(cls, version, path):
        cls._client_paths[version] = os.path.abspath(path)
