# utilities for dealing with webtiles configuration. The actual configuration
# data does *not* go in here.

import collections
import os.path
import logging
import re
import sys
import yaml

from webtiles import load_games, bans

server_config = {}
source_file = None
source_module = None

# rudimentary code for log msgs before server.py:init_logging is called
# probably better just to avoid using rather than to make more sophisticated..
_early_logging = []

def early_log(s):
    _early_logging.append(s)

def do_early_logging():
    global _early_logging
    for s in _early_logging:
        logging.info(s)
    _early_logging = []

# light wrapper class that maps get/set/etc to getattr/setattr/etc
# doesn't bother to implement most of the dict interface...
class ConfigModuleWrapper(object):
    def __init__(self, module):
        self.module = module

        # default override location for config settings
        self._load_override_file(os.path.join(
                        self.get("server_path", ""), "config.yml"))

        # default override locations for ban lists. This first filename
        # is intentionally the same as the scoring db's ban list filename.
        load_banfile(os.path.join(
                        self.get("server_path", ""), "banned_players.yml"))
        load_banfile(os.path.join(
                        self.get("server_path", ""), "banned_players.txt"))

    def _load_override_file(self, path):
        if not os.path.isfile(path):
            return
        with open(path) as f:
            override_data = yaml.safe_load(f)
            if not isinstance(override_data, dict):
                raise ValueError("'%s' must be a map" % path)
            for key, value in override_data.items():
                if key == 'games':
                    raise ValueError("Can't override 'games' in %s. Use games.d/ instead." % path)
                elif key == 'banned':
                    # we probably don't want to overwrite any ban lists set
                    # in the config module
                    add_ban_list(value)
                setattr(self.module, key, value)
            early_log("Loading config overrides from: '%s'" % path)

    def get(self, key, default):
        return getattr(self.module, key, default)

    def __setitem__(self, key, val):
        setattr(self.module, key, val)

    def pop(self, key):
        r = getattr(self.module, key)
        delattr(self.module, key)
        return r

    def __contains__(self, key):
        return hasattr(self.module, key)

# temporary compatibility shim for config calls in templates
allow_password_reset = False
admin_password_reset = False

# classic config: everything is just done in a module
# (TODO: add some alternative)
def init_config_from_module(module):
    global server_config, source_file, source_module
    source_module = module
    server_config = ConfigModuleWrapper(module)
    source_file = os.path.abspath(module.__file__)
    global allow_password_reset, admin_password_reset
    allow_password_reset = get('allow_password_reset')
    admin_password_reset = get('admin_password_reset')


def init_config_timeouts():
    from webtiles import ws_handler, util
    # anything added here should be robust to reload()...
    util.set_slow_callback_logging(get('slow_callback_alert'))
    ws_handler.do_load_logging(True)


def reload_namespace_resets():
    # ensure that the ban list always gets rebuilt
    set('banned', [])


def reload():
    from webtiles import util
    with util.note_blocking("config.reload"):
        global source_file, source_module
        try:
            logging.warning("Reloading config from %s", source_file)
            try:
                from importlib import reload
            except:
                from imp import reload
            # major caveat: this will not reset the namespace before doing the
            # reload. So to return something to the default value requires an
            # explicit setting. XX is there anything better to do about this?
            reload_namespace_resets()
            reload(source_module)
            init_config_from_module(source_module)
            do_early_logging()
            init_config_timeouts()
            try:
                load_game_data(not get('games'))
            except ValueError:
                logging.error("Game data reload failed!", exc_info=True)
                # if you get to here, game data is probably messed up. But there's
                # nothing to be done, probably...
            # XX it might be good to revalidate here, but I'm not sure how to
            # recover from errors
        except:
            logging.error("Config reload failed!", exc_info=True)


server_path = None
# note: get('games') should not be used to get this value! This gets only
# whatever games are defined in the config module.
games = collections.OrderedDict()
game_modes = {}  # type: Dict[str, str]


# templating for strings in game config
def dgl_format_str(s, username, game_params):  # type: (str, str, Any) -> str
    # XX this should probably implement %% as an escape

    def tsub(s, code, name, base, xform=lambda x: x):
        if s.find(code) >= 0:
            if base is None:
                raise ValueError("%s used in config templating but not set in config: %s" % (name, s))
            s = s.replace(code, xform(base))
        return s

    # sometimes this is called without a username available, so gracefully
    # ignore if it is not provided
    if username is not None:
        tsub(s, "%n", 'username', username)

    # provide the version info in various ways useful for constructing paths
    version = game_params.get('version')

    s = tsub(s, "%v", 'version', version)
    s = tsub(s, "%V", 'version', version, lambda x: x.capitalize())
    # %r is the version as a string that follows a "0." (if not found, the
    # entire version string is used). This is used for building directory
    # names on some dgl servers.
    s = tsub(s, "%r", 'version', version, lambda x: x.split("0.", 1)[-1]) # XX a bit brittle
    return s


# use this to get templated game config values
def game_param(game_key, param, username=None, default=""):
    global games
    game = games[game_key]
    val = game.get(param, default)
    if isinstance(val, str):
        return dgl_format_str(val, username, game)
    elif isinstance(val, (list, tuple)): # ugh, but we need this for pre_options
        return [dgl_format_str(e, username, game)
                if isinstance(e, str) else e for e in val]
    else:
        return val


# return a copy with everything templated that can be templated
# XX could make a better interface with a custom game class rather than dict
def game_params(game_key, username=None):
    global games
    r = {}
    for k in games[game_key]:
        r[k] = game_param(game_key, k, username=username)
    return r


# for values not in this dict, the default is None
defaults = {
    'dgl_mode': True,
    'logging_config': {
        "level": logging.INFO,
        "format": "%(asctime)s %(levelname)s: %(message)s"
    },
    'server_socket_path': None,
    'watch_socket_dirs': False,
    'milestone_file': [],
    'status_file_update_rate': 5,
    'lobby_update_rate': 2,
    'recording_term_size': (80, 24),
    'max_connections': 100,
    'connection_timeout': 600,
    'max_idle_time': 5 * 60 * 60,
    'use_gzip': True,
    'kill_timeout': 10,
    'nick_regex': r"^[a-zA-Z0-9]{3,20}$",
    'max_passwd_length': 20,
    'allow_password_reset': False,
    'admin_password_reset': False,
    'crypt_algorithm': "broken", # should this be the default??
    'crypt_salt_length': 16,
    'login_token_lifetime': 7, # Days
    'daemon': False,
    'development_mode': False,
    'no_cache': False,
    'live_debug': False,
    'lobby_update_rate': 2,
    'load_logging_rate': 0,
    'slow_callback_alert': None,
    'games': collections.OrderedDict([]),
    'use_game_yaml': None, # default: load games.d if games is empty
    'banned': [],
    'bot_accounts': False,
    'wizard_accounts': False,
}

def get(key, default=None):
    global server_config
    return server_config.get(key, defaults.get(key, default))

def set(key, val):
    global server_config
    server_config[key] = val

def pop(key):
    global server_config
    return server_config.pop(key)

def has_key(key):
    global server_config
    return key in server_config

def check_keys_all(required, raise_on_missing=False):
    # accept either a single str, or an iterable for `required`
    if isinstance(required, str):
        required = [required]
    for k in required:
        if not has_key(k) or get(k) is None:
            if raise_on_missing:
                raise ValueError("Webtiles config: Missing configuration key: %s" % k)
            return False
    return True

def check_keys_any(required, raise_on_missing=False):
    # use `has_keys`: if any member of required is itself a list, require
    # all keys in the list
    if not any([check_keys_all(key) for key in required]):
        if raise_on_missing:
            raise ValueError("Webtiles config: Need at least one of %s!" %
                                        ", ".join([repr(r) for r in required]))
        return False
    return True

def check_game_config():
    success = True
    for (game_id, game_data) in get('games').items():
        if not os.path.exists(game_data["crawl_binary"]):
            logging.warning("Crawl executable for %s (%s) doesn't exist!",
                            game_id, game_data["crawl_binary"])
            success = False

        if ("client_path" in game_data and
                        not os.path.exists(game_data["client_path"])):
            logging.warning("Client data path %s doesn't exist!", game_data["client_path"])
            success = False

    return success

def using_games_dir():
    # backwards compatibility: deprecated games_config_dir was either None to
    # disable, or a str, but treat it now as just a bool-like. (Slightly
    # awkward as None has a different semantics for use_game_yaml.)
    if has_key('games_config_dir'):
        return bool(get('games_config_dir'))
    else:
        return get('use_game_yaml') # True, False, or None (to ignore)


def add_ban_list(ban_list):
    cur = get('banned')
    cur.extend(ban_list)
    # explicitly set, in case we started with the default
    set('banned', cur)


# check a username against various config options that may disallow it
def check_name(username):
    if not re.match(get('nick_regex'), username):
        return False
    if get('nick_check_fun') and not get('nick_check_fun')(username):
        return False
    return not bans.do_ban_check(username, get('banned'))


def load_banfile(path, require_exists=False):
    if require_exists and not os.path.isfile(path):
        raise ValueError("Webtiles config: banfile '%s' does not exist", path)
    # otherwise, will be a noop on a non-existent file
    l = bans.load_bans(path)
    if l:
        add_ban_list(l)
        early_log("Loading ban list from: '%s'" % path)


def load_game_data(reloading=False):
    # TODO: should the `load_games` module be refactored into config?
    global games

    # note: if `games` is defined in the config module, reloading is always
    # False here.
    if not reloading:
        games = get('games')
        if len(games):
            logging.info("Reading %d games from config module", len(games))

    # now try to load games from any `games.d/` yaml files.
    # if `games` from config is non-empty, by default do not try to load from
    # games.d. a server that is using both can override this by setting the
    # `use_game_yaml` option to True. Setting this option to False will also
    # prevent games.d loading, which is a bit useless, but may be helpful for
    # validation purposes.
    use_gamesd = using_games_dir()
    if use_gamesd is None and not get('games') or use_gamesd:
        # this does not modify `games` until the assignment on return, so is
        # exception safe.
        # Awkward interaction: if a yaml game overrides a config module game,
        # it is just described as "Updated". This isn't a recommended use case
        # so I haven't figured out how to get a warning to happen.
        games = load_games.load_games(games, reloading)
    else:
        logging.warn("Skipping game definitions in `games.d/` based on config")

    # hopefully we've found some games by this point; do a bit of validation:
    if len(games) == 0:
        raise ValueError("No games defined!")
    if not check_game_config():
        raise ValueError("Errors in game data!")

    # finally, collect info about the games we found (which will also validate
    # the binaries, for versions that support this call at least).
    global game_modes
    game_modes = load_games.collect_game_modes()

def validate():
    check_keys_any(['bind_nonsecure', 'ssl_options'], True)
    if has_key('bind_nonsecure') and get('bind_nonsecure'):
        check_keys_any(['bind_pairs', ['bind_address', 'bind_port']], True)
    if has_key('ssl_options') and get('ssl_options'):
        check_keys_any(['ssl_bind_pairs', ['ssl_address', 'ssl_port']], True)

    required = ['static_path', 'template_path', 'server_id',
        'dgl_status_file', 'init_player_program',]
    if get('allow_password_reset') or get('admin_password_reset'):
        required.append('lobby_url')

    check_keys_all(required, raise_on_missing=True)

    smpt_opts = ['smtp_host', 'smtp_port', 'smtp_from_addr']
    if check_keys_any(smpt_opts):
        check_keys_all(smpt_opts, True)
    if has_key('smtp_user'):
        check_keys_all('smtp_password', True)

    bans.validate(get('banned'))

    try:
        # simplest is just to run the ban check against a name that isn't
        # valid and so shouldn't ever be banned, which exercises the whole
        # thing
        bans.do_ban_check('', get('banned'))
    except:
        raise ValueError("Webtiles config: malformed ban list ('%s')" %
                                                        repr(get('banned')))

    # set up defaults that are conditioned on other values
    if not has_key('settings_db'):
        set('settings_db', os.path.join(os.path.dirname(get('password_db')),
                                        "user_settings.db3"))
