import tornado.web
import os.path

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

    _client_paths = {}

    @classmethod
    def add_version(cls, version, path):
        cls._client_paths[version] = os.path.abspath(path)
