"""DCSS Webtiles server config."""

from __future__ import print_function

import csv
import glob
import logging
import os
import re
import string
import sys
from copy import copy, deepcopy

import toml


class ConfigError(Exception):

    """Simple exception for errors when parsing the config file."""

    def __init__(self, msg, conf_file=None):
        """Exception with msg referring to conf_file."""
        self.msg = msg
        if not conf_file and config:
            conf_file = config.path
        if conf_file:
            self.msg = "Config file {0}: {1}".format(conf_file, self.msg)
        Exception.__init__(self, msg)


class Conf(object):

    """Object representing webtiles server configuration.

    One instance of this class should exist in the application, as conf.config.
    """

    def __init__(self, path=""):
        """Initialise, then load config.toml from path.

        If path is "", use the value of WEBTILES_CONF envvar or cwd.
        """
        self.data = None
        self.logging = True
        self.admins = set()
        self.janitors = set()
        self.devteam = {}
        self.player_titles = {}
        self.janitor_commands = {}
        self.games = {}
        self.title_images = []

        if path:
            self.path = path
        elif os.environ.get("WEBTILES_CONF"):
            self.path = os.environ["WEBTILES_CONF"]
        else:
            # Try to find the config file
            if os.path.exists("./config.toml"):
                self.path = "./config.toml"
            else:
                dirname = os.path.dirname(os.path.abspath(__file__))
                self.path = os.path.join(dirname, "config.toml")

        if not os.path.exists(self.path):
            errmsg = "Couldn't find the config file (config.toml)!"
            if os.path.exists(self.path + ".sample"):
                errmsg += " Maybe copy config.toml.sample to config.toml."
            raise ConfigError(errmsg)

        self.read()

    def __getitem__(self, *args, **kwargs):
        """Pass through to self.data dict."""
        return self.data.__getitem__(*args, **kwargs)

    def __setitem__(self, *args, **kwargs):
        """Pass through to self.data dict."""
        return self.data.__setitem__(*args, **kwargs)

    def __contains__(self, *args, **kwargs):
        """Pass through to self.data dict."""
        return self.data.__contains__(*args, **kwargs)

    def get(self, *args, **kwargs):
        """Pass through to self.data dict."""
        return self.data.get(*args, **kwargs)

    def __missing__(self, key):
        """Log a warning if we access an unset config variable."""
        self._warn("Missing config option %s" % key)
        raise KeyError(key)

    def _warn(self, msg):
        """Log a warning, through logging if available, or stderr if not."""
        if self.logging:
            logging.warning(msg)
        else:
            print(msg, file=sys.stderr)

    def _info(self, msg):
        """Log a message, through logging if available, or stdout if not."""
        if self.logging:
            logging.info(msg)
        else:
            print(msg)

    def read(self):
        """Read the main toml configuration data from self.path.

        This makes self.data available but does not check the data for
        consistency.
        """

        # XXX the toml module uses the base Exception class, so we
        # have to catch it in a different block.
        try:
            fh = open(self.path, "r")
        except EnvironmentError as e:
            raise ConfigError("Couldn't open config file "
                              "({0})".format(e.strerror))
        else:
            try:
                self.data = toml.load(self.path)
            except Exception as e:
                raise ConfigError("Couldn't parse config file "
                                  "({1})".format(e.args[0]))
            finally:
                fh.close()

    def check_logging(self):
        """Check logging config, raise ConfigError if there are problems."""
        if not self.get("logging_config"):
            raise ConfigError("logging_config table undefined.")

        log_config = self["logging_config"]
        if log_config.get("filename"):
            if not log_config.get("max_bytes"):
                raise ConfigError("In logging_config, filename enabled but "
                                  "max_bytes undefined.")
            if not log_config.get("backup_count"):
                raise ConfigError("In logging_config, filename enabled but "
                                  "backup_count undefined.")
        if not log_config.get("format"):
            raise ConfigError("In logging_config, format undefined.")

    def load(self):
        """Load the toml configuration from the toml data in self.data.

        Should be called after the toml config is read with self.read() and
        after any chroot and privilege dropping is done by the server.
        """

        if self.get("devs_are_server_janitors") \
           and self.get("dev_nicks_can_be_registered"):
            raise ConfigError("devs_are_server_janitors is true but "
                              "dev_nicks_can_be_registered is not false. I "
                              "can't let you do that, Dave.")

        if not self.get("password_db"):
            raise ConfigError("Password database (password_db) undefined")

        if not self.get("http_connection_timeout"):
            raise ConfigError("HTTP connection timeout "
                              "(http_connection_timeout) undefined.")

        if not self.get("static_path"):
            raise ConfigError("static_path is undefined.")
        try:
            self.scan_titles()
        except EnvironmentError as e:
            raise ConfigError("Couldn't read title_path directory '{0}' "
                              "({1}).".format(self["static_path"], e.strerror))

        if not self.title_images:
            raise ConfigError("No title images (title_*.png) found in "
                              "static_path ({0}).".format(self["static_path"]))

        init_prog = self.get("init_player_program")
        if init_prog and not os.access(init_prog, os.X_OK):
            raise ConfigError("init_player_program ({0}) is not "
                              "executable".format(init_prog))
        # crawl usernames are case-insensitive
        self.admins = set()
        if self.get("server_admins"):
            self.admins = set([a.lower() for a in self.get("server_admins")])
        self.janitors = set()
        if self.get("server_janitors"):
            self.janitors = set([j.lower() for j in
                                 self.get("server_janitors")])
        self.janitors.update(self.admins)
        self._load_devteam_file()
        self._load_player_titles()
        self._load_janitor_commands()
        self._load_games()

    def _load_devteam_file(self):
        """Load the devteam file if its location is specified.

        Devteam file stores the nicks (primary nick, *alternates) for every
        dev.
        """
        self.devteam = {}
        devteam_file = self.get("devteam_file")
        if not devteam_file:
            return

        try:
            devteam_fh = open(devteam_file, "r")
        except EnvironmentError as e:
            raise ConfigError("Couldn't open devteam file {0} "
                              "({1})".format(devteam_file, e.strerror))

        devteam_r = csv.reader(devteam_fh, delimiter=" ",
                               quoting=csv.QUOTE_NONE)
        for row in devteam_r:
            # We leave case intact on the primary account here since it's used
            # for display purposes.
            self.devteam[row[0]] = [u.lower() for u in row[1:]]
            if self.get("devs_are_server_janitors"):
                self.janitors.add(row[0])
        devteam_fh.close()

    def _load_janitor_commands(self):
        """Load janitor commands from the currently loaded config file."""
        self.janitor_commands = {}
        if not self.data.get("janitor_commands"):
            return

        for cmd in self.data["janitor_commands"]:
            required_fields = ("id", "name", "action", "argument")
            for field in required_fields:
                if field not in cmd:
                    raise ConfigError("Janitor command entries must have {0} "
                                      "defined.".format(field))

            cmd["params"] = set()
            for token in string.Formatter().parse(cmd["argument"]):
                if token[1]:
                    cmd["params"].add(token[1])
            self.janitor_commands[cmd["id"]] = cmd

    def _load_games(self):
        """Load game specifications from the currently loaded config file."""
        self.games = {}
        # Load any games specified in main config file first
        if self.data.get("games"):
            for game in self.data["games"]:
                if game["id"] in self.games:
                    self._warn("Skipping duplicate game definition '{0}' "
                               "in {1}".format(game["id"], self.path))
                    continue
                self._load_game_data(game)
        else:
            # Need this to exist since we're sending the final games config
            # array to the client.
            self.data["games"] = []
        self._load_game_conf_dir()

    def _load_game_data(self, game):
        """Load ancillary game data for game (specified by game's key)."""
        if not os.path.exists(game["crawl_binary"]):
            raise ConfigError("Crawl executable {0} doesn't "
                              "exist!".format(game["crawl_binary"]))

        if "client_path" in game and not os.path.exists(game["client_path"]):
            raise ConfigError("Client data path {0} doesn't "
                              "exist!".format(game["client_path"]))
        if type(game.get("options", [])) is not list:
            raise ConfigError("The options field should be a list!")
        if type(game.get("pre_options", [])) is not list:
            raise ConfigError("The pre_options field should be a list!")
        self._load_game_map(game)
        self.games[game["id"]] = game
        self._info("Loaded game '{0}'".format(game["id"]))

    def _load_game_conf_dir(self):
        """Load from the toml files in games_conf_d."""
        conf_dir = self.get("games_conf_d")
        if not conf_dir:
            return
        if not os.path.isdir(conf_dir):
            self._warn("games_conf_d is not a directory, ignoring")
            return

        for f in sorted(glob.glob(conf_dir + "/*.toml")):
            if not os.path.isfile(f):
                self._warn("Skipping non-file {0}".format(f))
                continue
            try:
                fh = open(f, "r")
            except EnvironmentError as e:
                raise ConfigError("Unable to open game file: "
                                  "{0}".format(e.strerror), f)
            else:
                try:
                    data = toml.load(fh)
                except Exception as e:
                    raise ConfigError("Couldn't parse game config file: "
                                      "{0}".format(e.args[0]), f)
                finally:
                    fh.close()

            if "games" not in data:
                self._warn("No games specifications found in game config "
                           "file {0}, skipping.".format(f))
                continue
            for game in data["games"]:
                if game["id"] in self.games:
                    self._warn("Skipping duplicate game definition '{0}' in "
                               "game config file {1}".format(game["id"], f))
                    continue
                self._load_game_data(game)
                # Needed to send the full games config array to the client.
                self.data["games"].append(game)

    def _load_player_titles(self):
        """Load player titles from the player_title_file."""
        self.player_titles = {}
        # Don't bother loading titles if we don't have the title sets defined.
        title_names = self.get("title_names")
        title_file = self.get("player_title_file")
        if not (title_names and title_file):
            return

        try:
            title_fh = open(title_file, "r")
        except EnvironmentError as e:
            raise ConfigError("Couldn't open player title file {0} "
                              "({1})".format(title_file, e.strerror))

        title_r = csv.reader(title_fh, delimiter=" ", quoting=csv.QUOTE_NONE)
        for row in title_r:
            self.player_titles[row[0].lower()] = [u.lower() for u in row[1:]]
        title_fh.close()

    def _load_game_map(self, game):
        """Load game map data into game."""
        if "map_path" not in game:
            return
        if "score_path" not in game:
            self._warn("Not parsing game map for game definition '{0}' "
                       "because it has no score_path".format(game["id"]))
            return
        try:
            data = toml.load(game["map_path"])
        except Exception as e:
            raise ConfigError("Couldn't load game map file {0} "
                              "({1})".format(game["map_path"], e.args[0]))
        if "maps" not in data:
            raise ConfigError("maps section missing from game map file "
                              "{0}".format(game["map_path"]))
        game["game_maps"] = data["maps"]

    def scan_titles(self):
        """Find all title images and load them into self.title_images."""
        self.title_images = []
        if not self["static_path"]:
            return
        title_regex = re.compile(r"^title_.*\.png$")
        for f in os.listdir(self["static_path"]):
            if title_regex.match(f):
                self.title_images.append(f)

    def reload(self):
        """Try to reload the WebTiles configuration from self.path.

        If the new configuration fails to load or parse, we rollback to the
        current configuration. Not that any changes to configuration data that
        is used before chroot or privilege dropping are ignored. This would be
        the [[binds]], [[ssl_binds]], [logging_config] tables, and any of the
        permissions and chroot options.
        """

        # XXX Find a nice way to avoid an explicit list of what to copy
        data = copy(self.data)
        admins = copy(self.admins)
        devteam = deepcopy(self.devteam)
        janitors = copy(self.janitors)
        player_titles = deepcopy(self.player_titles)
        janitor_commands = deepcopy(self.janitor_commands)
        games = deepcopy(self.games)
        title_images = copy(self.title_images)

        try:
            self.read()
            self.load()
        except ConfigError as e:
            self.data = data
            self.admins = admins
            self.devteam = devteam
            self.janitors = janitors
            self.player_titles = player_titles
            self.janitor_commands = janitor_commands
            self.games = games
            self.title_images = title_images
            self._warn("Unable to reload configuration: {0}; rolling back "
                       "configuration to previous state".format(e.msg))

    def reload_player_titles(self):
        """Reload player titles.

        This is called on config reload an on SIGUSR2. The titles are rolled
        back to the current ones if there's an error.
        """
        player_titles = deepcopy(self.player_titles)

        try:
            self._load_player_titles()
        except ConfigError as e:
            self.player_titles = player_titles
            self._warn("Unable to reload player titles: {0}; rolling back "
                       "to previous title data".format(e.msg))

    def get_devname(self, username):
        """Return the canonical username for an account.

        Devs have a primary name with possible alternate names. We return the
        primary name when an alt is checked.
        """
        if not self.devteam:
            return None
        lname = username.lower()
        for d in self.devteam.keys():
            if d.lower() == lname or lname in self.devteam[d]:
                return d
        return None

    def _get_player_title(self, username):
        """Return username's most important title, or None if it lacks one."""
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
        """Return True if username is a server admin."""
        return self.admins and username.lower() in self.admins

    def is_server_janitor(self, username):
        """Return True if username is a server janitor."""
        return self.janitors and username.lower() in self.janitors

    def get_nerd(self, username):
        """Return {title=str, canonical_name=str} for a username."""
        if self.is_server_admin(username):
            return {"type": "admins", "devname": None}
        devname = self.get_devname(username)
        if devname:
            return {"type": "devteam", "devname": devname}
        title = self._get_player_title(username)
        if title:
            return {"type": title, "devname": None}
        return {"type": "normal", "devname": None}

    def get_game(self, game_version, game_mode):
        """Return game data matching game_version & game_mode (or None)."""
        if not self.games:
            return None

        for id, game in self.games.iteritems():
            if "mode" not in game or "version" not in game:
                continue
            if game["mode"] == game_mode and game["version"] == game_version:
                return game
        return None

    def game_map(self, game_id, map_name):
        """Return game_map matching game_id & map_name (or None)."""
        if "game_maps" not in self.games[game_id]:
            return None
        for m in self.games[game_id]["game_maps"]:
            if m["name"] == map_name:
                return m
        return None

config = None
try:
    config = Conf()
except ConfigError as e:
    sys.exit(e.msg)
