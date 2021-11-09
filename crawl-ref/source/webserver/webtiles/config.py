# utilities for dealing with webtiles configuration. The actual configuration
# data does *not* go in here.

import collections
import os.path

from webtiles import load_games

# classic config: everything is just done in a module
# (TODO: add some alternative)
def init_config_from_module(module):
    global server_config
    server_config = module


server_path = None
games = collections.OrderedDict()
game_modes = {}  # type: Dict[str, str]

def get(key, default=None):
    global server_config
    return getattr(server_config, key, default)

def set(key, val):
    global server_config
    setattr(server_config, key, val)

def clear(key):
    global server_config
    delattr(server_config, key)

def has_key(key):
    global server_config
    return hasattr(server_config, key)

def check_keys_all(required, raise_on_missing=False):
    # accept either a single str, or an iterable for `required`
    if isinstance(required, str):
        required = [required]
    for k in required:
        if not has_key(k):
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
                                                        ", ".join(required))
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
    if has_key('bind_nonsecure'):
        check_keys_any(['bind_pairs', ['bind_address', 'bind_port']], True)
    if has_key('ssl_options'):
        check_keys_any(['ssl_bind_pairs', ['ssl_address', 'ssl_port']], True)

    required = ['static_path', 'template_path', 'logging_config',
        'kill_timeout', 'recording_term_size', 'server_id',
        'crypt_algorithm', 'crypt_salt_length', 'max_passwd_length',
        'nick_regex', 'dgl_status_file', 'status_file_update_rate',
        'max_connections', 'connection_timeout', 'max_idle_time',
        'init_player_program', 'login_token_lifetime']
    if get('allow_password_reset', False) or get('admin_password_reset', False):
        required.add('lobby_url')

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
