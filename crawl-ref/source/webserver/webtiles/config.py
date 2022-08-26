# utilities for dealing with webtiles configuration. The actual configuration
# data does *not* go in here.

import collections
import os.path
import logging

from webtiles import load_games

server_config = {}
source_file = None
source_module = None

# light wrapper class that maps get/set/etc to getattr/setattr/etc
# doesn't bother to implement most of the dict interface...
class ConfigModuleWrapper(object):
    def __init__(self, module):
        self.module = module

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
            # explicit setting. XX is there anything to do about this?
            reload(source_module)
            init_config_from_module(source_module)
            init_config_timeouts()
            try:
                load_game_data()
            except ValueError:
                logging.error("Game data reload failed!", exc_info=True)
                # if you get to here, game data is probably messed up. But there's
                # nothing to be done, probably...
            # XX it might be good to revalidate here, but I'm not sure how to
            # recover from errors
        except:
            logging.error("Config reload failed!", exc_info=True)


server_path = None
games = collections.OrderedDict()
game_modes = {}  # type: Dict[str, str]

# for values not in this dict, the default is None
defaults = {
    'dgl_mode': True,
    'logging_config': {
        "level": logging.INFO,
        "format": "%(asctime)s %(levelname)s: %(message)s"
    },
    'server_socket_path': None,
    'watch_socket_dirs': False,
    'use_game_yaml': True,
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
    'games_config_dir': 'games.d',
    'load_logging_rate': 0,
    'slow_callback_alert': None,
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

def load_game_data():
    # TODO: should the `load_games` module be refactored into config?
    global games
    games = get('games', collections.OrderedDict())
    if get('use_game_yaml', False):
        games = load_games.load_games(games)
    # TODO: check_games here or in validate?
    if len(games) == 0:
        raise ValueError("No games defined!")
    if not check_game_config():
        raise ValueError("Errors in game data!")
    global game_modes
    game_modes = load_games.collect_game_modes()

def validate():
    # TODO: some way of setting defaults in this module?
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
    if (has_key('smtp_user')):
        check_keys_all('smtp_password', True)

    # set up defaults that are conditioned on other values
    if not has_key('settings_db'):
        set('settings_db', os.path.join(os.path.dirname(get('password_db')),
                                        "user_settings.db3"))
