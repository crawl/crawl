import csv
import glob
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
        ## XXX the toml module uses the base Exception class, so we
        ## have to catch it in a different block.
        try:
            fh = open(self.path, "r")
        except EnvironmentError as e:
            logging.error("Couldn't open config file {0} "
                                  "({1})".format(self.path, e.strerror))
            sys.exit(1)
        else:
            try:
                self.data = toml.load(self.path)
            except Exception as e:
                logging.error("Couldn't parse config file {0} "
                              "({1})".format(self.path, e.args))
                sys.exit(1)
            finally:
                fh.close()

        devteam_file = self.get("devteam_file")
        if devteam_file:
            try:
                devteam_fh = open(devteam_file, "r")
            except EnvironmentError as e:
                logging.error("Could not open devteam file {0} "
                              "({1})".format(devteam_file, e.strerror))
                sys.exit(1)

            devteam_r = csv.reader(devteam_fh, delimiter=" ",
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
        # Load any games specified in main config file first
        if self.data.get('games'):
            for game in  self.data["games"]:
                try:
                    self.load_game_data(game)
                except ValueError:
                    logging.warning("Skipping duplicate game definition '{0}' "
                                    "in {1}".format(game["id"], self.path))
        self.load_game_conf_dir()


    def load_game_data(self, game):
        if game["id"] in self.games:
            raise ValueError
        else:
            self.games[game["id"]] = game
            logging.info("Loaded game '{0}'".format(game["id"]))


    # Load from games_conf_d
    def load_game_conf_dir(self):
        conf_dir = self.get("games_conf_d")
        if not conf_dir:
            return
        if not os.path.isdir(conf_dir):
            logging.warning("games_conf_d is not a directory, ignoring")
            return

        for f in sorted(glob.glob(conf_dir + "/*.toml")):
            if not os.path.isfile(f):
                logging.warning("Skipping non-file {0}".format(f))
                continue
            logging.debug("Loading {0}".format(f))
            try:
                fh = open(f, "r")
            except EnvironmentError as e:
                logging.warning("Unable to open game config file {0} "
                                "({1})".format(f, e.strerror))
                sys.exit(1)
            else:
                try:
                    data = toml.load(fh)
                except Exception as e:
                    logging.error("Could parse game config file {0} "
                                "({1})".format(f, e.args))
                finally:
                    fh.close()

            if "games" not in data:
                logging.warning("No games specifications found in game config "
                                "file {0}, skipping.".format(f))
                continue
            for game in data["games"]:
                try:
                    self.load_game_data(game)
                except ValueError:
                    logging.warning("Skipping duplicate definition '{0}' in "
                                    "game config file {1}".format(game["id"], f))


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

        try:
            title_fh = open(title_file, "r")
        except EnvironmentError as e:
            logging.error("Could not open player title file {0} "
                          "({1})".format(title_file, e.strerror))
            sys.exit(1)

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
