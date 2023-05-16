import tornado.web
from tornado.escape import json_encode

import webtiles
from webtiles import config, ws_handler, server

class LobbyHandler(tornado.web.RequestHandler):
    def get(self):
        games = []
        url_base = config.get('lobby_url')
        for s in list(ws_handler.sockets):
            if not s.username or not s.process or not s.show_in_lobby():
                continue

            # XX the following doesn't perfectly match the regular lobby
            # json, arguably one or the other should be changed before this
            # gets to be widely used
            game = dict(
                name=s.username,
                game_id=s.game_id,
                idle_time=s.process.idle_time(),
                viewers=s.process.watcher_count(),
                )
            if url_base:
                # otherwise constructed on js side, see client.js
                game['watch_url'] = url_base + "#watch-" + s.username
            # copy some selected info from `where`. This is a bit more than
            # CrawlProcessHandlerBase.interesting_info.
            # XX game type isn't in here except via game_id, which isn't
            # standardized across servers
            for k in ('v', 'vlong', 'tiles', 'race', 'cls', 'char', 'xl',
                        'title', 'place', 'god', 'turn', 'dur'):
                # the following will force keys not present in the milestone
                # to appear here, e.g. the crawl process doesn't set `god` if
                # there is none. (Maybe this is a bug?)
                game[k] = s.process.where.get(k, "")

            if s.process.last_milestone:
                game["milestone"] = s.process.last_milestone.get("milestone", "")
            else:
                game["milestone"] = ""

            games.append(game)
        self.write(json_encode(games))

class VersionHandler(tornado.web.RequestHandler):
    def get(self):
        self.write(json_encode(server.version_data()))
