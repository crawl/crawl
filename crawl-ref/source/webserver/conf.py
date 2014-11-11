import csv
import os
import toml
import logging
import sys

from util import TornadoFilter


if os.environ.get('WEBTILES_DEBUG'):
    logging.getLogger().setLevel(logging.DEBUG)

class Conf(object):
    def __init__(self, path=''):
        self.data = None
        self.dev_team = None
        self.player_titles = None

        if path:
            self.path = path
        elif os.environ.get("WEBTILES_CONF"):
            self.path = os.environ["WEBTILES_CONF"]
        else:
            # Try to find the config file
            if os.path.exists("./config.toml"):
                self.path = "./config.toml"
            else:
                self.path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "config.toml")

        if not os.path.exists(self.path):
            errmsg = "Could not find the config file (config.toml)!"
            if os.path.exists(self.path + ".sample"):
                errmsg += " Maybe copy config.toml.sample to config.toml."
            logging.error(errmsg)
            sys.exit(errmsg)

        self.load()
        self.init_logging()
        self.init_games()

    def load(self):
        try:
            self.data = toml.load(open(self.path, "r"))
        except OSError as e:
            errmsg = ("Couldn't open config file %s (%s)" % (self.path, e))
            if os.path.exists(path + ".sample"):
                errmsg += (". Maybe copy config.toml.sample to config.toml.")
            logging.error(errmsg)
            sys.exit(1)

        devteam_file = self.get("devteam_file")
        if devteam_file:
            if not os.path.exists(devteam_file):
                errmsg = "Could not find devteam file {0}".format(devteam_file)
                logging.error(errmsg)
                sys.exit(errmsg)

            devteam_fh = open(devteam_file, "r")
            devteam_r = csv.reader(devteam_fh, delimiter=' ',
                                   quoting=csv.QUOTE_NONE)
            self.devteam = {}
            for row in devteam_r:
                self.devteam[row[0]] = row[1:]
            devteam_fh.close()
        else:
            self.devteam = None

        self.load_player_titles()

    def init_games(self):
        self.games = {}
        # If there are no games in config.toml, this won't be defined
        if not self.data.get('games'):
            self.data['games'] = []
        # Load games specified in main config file first
        for game in self.data["games"]:
            if game["id"] in self.games:
                logging.warning("Skipping duplicate game definition '%s' in '%s'" % (game["id"], self.path))
                continue
            self.games[game["id"]] = game
            logging.info("Loaded game '%s'" % game["id"])

        # Load from games_conf_d
        games_conf_d = self.get("games_conf_d")
        if games_conf_d and os.path.isdir(games_conf_d):
            for f in os.listdir(games_conf_d):
                f_path = os.path.join(games_conf_d, f)
                if not os.path.isfile(f_path):
                    logging.warning("Skipping non-file in games_conf_d: %s" % f_path)
                    continue
                if not f.endswith('.toml'):
                    logging.info("Skipping non-toml file in games_conf_d: %s" % f_path)
                    continue

                logging.debug("Loading %s" % f_path)
                data = toml.load(open(f_path, "r"))
                if "games" not in data:
                    logging.warning("No games specifications found in %s, skipping." % f_path)
                    continue

                games = {data["id"]: data for data in data["games"]}
                for game in games.values():
                    if game["id"] in self.games:
                        logging.warning("Skipping duplicate game definition '%s' in '%s'" % (game["id"], f_path))
                        continue

                    self.games[game["id"]] = game
                    # Note: We also have to append these the raw TOML
                    # representation of config.toml, which is used for
                    # determining lobby display order.
                    self.data['games'].append(game)
                    logging.info("Loaded game '%s'" % game["id"])
        elif games_conf_d and not os.path.isdir(games_conf_d):
            logging.warning("games_conf_d is not a directory, ignoring")

    def init_logging(self):
        logging_config = self.logging_config
        filename = logging_config.get("filename")
        if filename:
            max_bytes = logging_config.get("max_bytes", 10*1000*1000)
            backup_count = logging_config.get("backup_count", 5)
            hdlr = logging.handlers.RotatingFileHandler(
                filename, maxBytes=max_bytes, backupCount=backup_count)
        else:
            hdlr = logging.StreamHandler(None)
        fs = logging_config.get("format", "%(levelname)s:%(name)s:%(message)s")
        dfs = logging_config.get("datefmt", None)
        fmt = logging.Formatter(fs, dfs)
        hdlr.setFormatter(fmt)
        logging.getLogger().addHandler(hdlr)
        level = logging_config.get("level")
        if level is not None:
            logging.getLogger().setLevel(level)
        logging.getLogger().addFilter(TornadoFilter())
        logging.addLevelName(logging.DEBUG, "DEBG")
        logging.addLevelName(logging.WARNING, "WARN")

        if not logging_config.get("enable_access_log"):
            logging.getLogger('tornado.access').setLevel(logging.FATAL)

    def load_player_titles(self):
        # Don't bother loading titles if we don't have the title sets defined.
        title_names = self.get("title_names")
        if not title_names:
            return

        title_file = self.get("player_title_file")
        if not title_file:
            return
        if not os.path.exists(title_file):
            errmsg = "Could not find player title file {0}".format(title_file)
            logging.error(errmsg)
            sys.exit(errmsg)

        try:
            title_fh = open(title_file, "r")
        except:
            errmsg = "Could not open player title file {0}".format(title_file)
            logging.error(errmsg)
            return

        title_r = csv.reader(title_fh, delimiter=' ', quoting=csv.QUOTE_NONE)
        self.player_titles = {}
        for row in title_r:
            self.player_titles[row[0]] = row[1:]
        title_fh.close()

    def get_devname(self, username):
        if not self.devteam:
            return None
        for d in self.devteam.keys():
            if d == username or username in self.devteam[d]:
                return d
        return None

    def get_player_title(self, username):
        if not self.player_titles:
            return None

        title_names = self.get("title_names")
        if not title_names:
            return None

        # The titles are mutually exclusive, so we use the last title
        # applicable to the player.
        title = None
        for t in title_names:
            if t not in self.player_titles:
                continue
            if username.lower() in self.player_titles[t]:
                title = t
        return title

    def is_server_admin(self, username):
        server_admins = self.get("server_admins")
        if server_admins and username in server_admins:
            return True
        return False

    def __getattr__(self, name):
        try:
            return self.data[name]
        except KeyError:
            raise AttributeError(name)

    def get(self, *args):
        return self.data.get(*args)

config = Conf()
