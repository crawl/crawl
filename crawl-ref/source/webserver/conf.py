import csv, os, os.path, toml, logging, sys

class Conf(object):
    def __init__(self, path=''):
        self.data = None
        self.dev_team = None
        self.player_titles = None

        if path:
            self.path = path
        elif os.environ.get("WEBTILES_CONF"):
            path = os.environ["WEBTILES_CONF"]
        else:
            if os.path.exists("./config.toml"):
                path = "./config.toml"
            else:
                path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "config.toml")

        if not os.path.exists(path):
            errmsg = "Could not find the config file (config.toml)!"
            if os.path.exists(path + ".sample"):
                errmsg += " Maybe copy config.toml.sample to config.toml."
            logging.error(errmsg)
            sys.exit(errmsg)

        self.load()

    def load(self):
        try:
            self.data = toml.load(open(self.path, "r"))
        except OSError as e:
            errmsg = ("Couldn't open config file %s (%s)" % (self.path, e))
            if os.path.exists(path + ".sample"):
                errmsg += (". Maybe copy config.toml.sample to config.toml.")
            logging.error(errmsg)
            sys.exit(1)

        self.load_games()

        try:
            devteam_file = self.get("devteam_file")
        except:
            devteam_file = None
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


    def load_player_titles(self):
        ## Don't bother loading titles if we don't have the title sets defined.
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

        ## The titles are mutually exclusive, so we use the last title
        ## applicable to the player.
        title = None
        for t in title_names:
            if not t in self.player_titles:
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
