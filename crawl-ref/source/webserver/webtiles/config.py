# utilities for dealing with webtiles configuration. The actual configuration
# data does *not* go in here.

import builtins
import collections
import itertools
import os.path
import logging
import re
import sys
import yaml
try:
    from collections.abc import MutableMapping
except:
    from collections import MutableMapping

from webtiles import load_games, bans

server_config = {}
source_file = None
source_module = None
server_path = None

# "temporary" compatibility shim for config calls in templates
allow_password_reset = False
admin_password_reset = False

# rudimentary code for log msgs before server.py:init_logging is called
# probably better just to avoid using as much as possible rather than to make
# it any more sophisticated. (Or, refactor early server setup so it's not
# needed..)
_early_logging = []


def early_log(s):
    global _early_logging
    _early_logging.append(s)

def do_early_logging():
    global _early_logging
    for s in _early_logging:
        logging.info(s)
    _early_logging = []


# game definitions and configuration
# note: get('games') should not be used to get game data! That call would get
# only whatever games are defined in the config module.
games = collections.OrderedDict()
game_templates = collections.OrderedDict()
# see also `metatemplates`, defined below

# mapping of game keys to game mode names
game_modes = {}  # type: Dict[str, str]

# mapping of binary keys to game keys
binaries_to_games = {}  # type: Dict[str, str]

# default values for module-level get calls
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
    'status_file_update_rate': 30,
    'lobby_update_rate': 2,
    'recording_term_size': (80, 24),
    'max_connections': 100,
    'connection_timeout': 10 * 60,
    'max_idle_time': 5 * 60 * 60,
    'max_lobby_idle_time': 3 * 60 * 60,
    'use_gzip': True,
    'kill_timeout': 10,
    'nick_regex': r"^[a-zA-Z0-9]{3,20}$",
    'max_passwd_length': 20,
    'allow_password_reset': False,
    'admin_password_reset': False,
    'crypt_algorithm': "broken", # should this be the default??
    'crypt_salt_length': 16,
    'login_token_lifetime': 7, # Days; set to <= 0 to disable
    'recovery_token_lifetime': 12, # hours
    'daemon': False,
    'development_mode': False,
    'no_cache': False,
    'live_debug': False,
    'lobby_update_rate': 2,
    'load_logging_rate': 0,
    'milestone_interval': 1000, # ms
    'slow_callback_alert': None,
    'slow_io_alert': 0.250,
    'games': collections.OrderedDict([]),
    'templates': collections.OrderedDict([]),
    'use_game_yaml': None, # default: load games.d if games is empty
    'banned': [],
    'bot_accounts': False,
    'wizard_accounts': False,
    'allow_anon_spectate': True,
    'enable_ttyrecs': True,
    'max_chat_length': 1000,
}


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
            early_log("Loading config overrides from: '%s'" % path)
            for key, value in override_data.items():
                if key == 'games':
                    # right now this completely overwrites anything set in the
                    # module, so issue a warning
                    if self.get('games', {}):
                        early_log("Warning: overriding `games` with value from " + path)
                    # fallthrough
                elif key == 'banned':
                    # we probably don't want to overwrite any ban lists set
                    # in the config module
                    add_ban_list(value)
                    continue

                setattr(self.module, key, value)

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


# classic config: everything is just done in a module
# TODO: allow this to be loaded directly from yml somehow? Right now, there
# always must be a config module, and then an override from yml is possible
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
            load_game_data(True)
        except ValueError:
            logging.error("Game data reload failed!", exc_info=True)
            # if you get to here, game data is probably messed up. But there's
            # nothing to be done, probably...
        # XX it might be good to revalidate here, but I'm not sure how to
        # recover from errors
    except:
        logging.error("Config reload failed!", exc_info=True)


class GameConfig(MutableMapping):
    def __init__(self, params, use_template=True, game_id=None):
        template_name = None
        if isinstance(params, GameConfig): # copy constructor, essentially
            # possibly should be a deep copy?
            self._store = params._store.copy()
            if not game_id:
                game_id = params.id
            template_name = params.template if use_template else None
            #self.template = params.template if use_template else None
        else:
            self._store = params
            # note: this prevents the `template` param from being modified via
            # the mapping interface
            # self.template = self._store.pop('template', 'default') if use_template else None

        if game_id:
            self.id = game_id
            self._store['id'] = game_id
        else:
            self.id = params['id'] # must be set
        if use_template and template_name is None and not is_metatemplate(self.id):
            template_name = self._store.pop('template', 'default')
        self.template = template_name

    def copy(self):
        return GameConfig(self)

    def __repr__(self):
        params = self._store.copy()
        extras = ""
        if self.template:
            params['template'] = self.template
        else:
            extras = ", use_template=False"
        return "GameConfig(%s%s)" % (repr(params), extras)

    def get_defaults(self):
        if self.template:
            return get_template(self.template)
        else:
            return {}

    def templated(self, key, username=None, default=""):
        val = self.get(key, default)
        if isinstance(val, str):
            return dgl_format_str(val, username, self)
        elif isinstance(val, (list, tuple)): # ugh, but we need this for pre_options
            return [dgl_format_str(e, username, self)
                    if isinstance(e, str) else e for e in val]
        else:
            return val

    def templated_dict(self, username):
        r = {}
        for k in self:
            r[k] = self.templated(k, username=username)
        return r

    def get_call_base(self):
        # these params can't be templated with a username...
        call = [self.templated("crawl_binary")]
        if "pre_options" in self:
            call += self.templated("pre_options")
        return call

    def get_binary_key(self):
        # return a value that is suitable for using as a hash key for a crawl
        # binary. This intentionally accommodates `pre_options`, because for
        # dgamelaunch-config setups, stable is called via a wrapper script
        # with the version set via `pre_options`, so this is necessary to
        # differentiate different stable versions.
        return " ".join(self.get_call_base())

    def validate(self):
        # check for loops in templating
        seen = builtins.set()
        t = self.get_defaults()
        while isinstance(t, GameConfig) and t.template:
            if t.id in seen:
                logging.error("Loop in template dependencies from game %s: %s", self.id, t.id)
                return False
            seen.add(t.id)
            t = t.get_defaults()

        # warn if this is a non-templatable GameConfig
        if 'template' in self._store:
            logging.warning("Ignoring template value '%s' in %s",
                                            self._store['template'], self.id)
        return True

    def validate_game(self):
        # check for templating errors in parameter values. We want to do this
        # only for full games, not templates. Note that %n isn't validated
        # here, but is checked against a hardcoded list of keys in the
        # validate_game_dict call.
        for k in self:
            try:
                self.templated(k, username="test")
            except:
                logging.error("Error when templating game %s, key %s: %s",
                    self.id, k, repr(self[k]), exc_info=True)
                return False

        if not self.validate():
            return False
        return load_games.validate_game_dict(self)

    def __getitem__(self, key):
        # the exception handling pattern here is so that error messages are
        # cleaner with multiple levels of templating. Otherwise, we get a bunch
        # of hard to parse `During handling ...` traces, and a messy final
        # trace. XX once we can 100% assume py3.3, this would be cleaner if
        # redone using `raise from None`.
        try:
            return self._store[key]
        except KeyError:
            pass
        try:
            return self.get_defaults()[key]
        except KeyError:
            pass
        raise KeyError(key)

    def __setitem__(self, key, val):
        self._store[key] = val

    def __delitem__(self, key):
        del self._store[key]

    def clear(self):
        # override the mixin behavior. This really does clear everything,
        # because it clears `template`.
        self._store.clear() # override

    def __iter__(self):
        return itertools.chain(
            iter(self._store),
            filter(lambda k: k not in self._store, iter(self.get_defaults())))

    def __len__(self):
        return len(self._store) + len([k for k in self.get_defaults() if not k in self._store])


# metatemplates that ground out recursion. `default` can be adjusted using
# the `define_default` function below, `base` should be left alone.
# outside this file, only modified in load_games.py:load_games
metatemplates = dict(
    base = GameConfig({}, use_template=False, game_id='base'),
    default = GameConfig({}, use_template=False, game_id='default')
    )


def is_metatemplate(k):
    global metatemplates
    return k in metatemplates


def define_default(params):
    global metatemplates
    new = GameConfig(params, use_template=False, game_id='default')
    if not new.validate():
        logging.warning("Ignoring invalid defaults")
        return False
    if len(metatemplates['default']) > 1 and new != metatemplates['default']:
        logging.warning("Replacing existing default game template")
    metatemplates['default'] = new
    return True


def get_template(k):
    global metatemplates, game_templates
    if k is None:
        k = 'default'
    if is_metatemplate(k):
        return metatemplates[k]
    else:
        return game_templates[k]


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
        s = tsub(s, "%n", 'username', username)

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
    return game.templated(param, username=username, default=default)


def get(key, default=None):
    global server_config, defaults
    return server_config.get(key, defaults.get(key, default))


# note: shadows type `set`, use `builtins.set` if you need the type name
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
    global games
    for game in games.values():
        if not os.path.exists(game["crawl_binary"]):
            logging.warning("Crawl executable for %s ('%s') doesn't exist!",
                            game.id, game["crawl_binary"])
            success = False

        if ("client_path" in game and
                        not os.path.exists(game["client_path"])):
            logging.warning("Client data path for %s ('%s') doesn't exist!",
                            game.id, game["client_path"])
            success = False

    return success


# aggregate check on a bunch of options values to determine whether we should
# load yaml files from games.d/
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
    # note: it's possible that nick_regex should be handled differently, since
    # it is always set and used as a syntactic check; the others are used
    # check for bad names of some kind
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


# build config.games, as well as various cached bits of game data
def load_game_data(reloading=False):
    global games, game_modes, binaries_to_games
    if not load_games.load_games(reloading=reloading):
        return # failed config reload

    # hopefully we've found some games by this point; do a bit more validation:
    if len(games) == 0:
        raise ValueError("No games defined!")
    if not check_game_config():
        raise ValueError("Errors in game data!")

    # finally, collect info about the games we found (which will also validate
    # the binaries, for versions that support this call at least).
    game_modes = load_games.collect_game_modes()
    binaries_to_games = dict()
    for g in games:
        k = games[g].get_binary_key()
        if k not in binaries_to_games:
            binaries_to_games[k] = []
        binaries_to_games[k].append(g)


def validate():
    check_keys_any(['bind_nonsecure', 'ssl_options'], True)
    if has_key('bind_nonsecure') and get('bind_nonsecure'):
        check_keys_any(['bind_pairs', ['bind_address', 'bind_port']], True)
    if has_key('ssl_options') and get('ssl_options'):
        check_keys_any(['ssl_bind_pairs', ['ssl_address', 'ssl_port']], True)
    if has_key('bind_nonsecure') and get('bind_nonsecure') == "redirect":
        if not check_keys_any(['ssl_options']):
            raise ValueError("bind_nonsecure='redirect' requires ssl ports to redirect to")

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
