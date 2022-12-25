import collections
import logging
import os
import subprocess

import yaml
from tornado.escape import json_decode


# TODO: consider just reintegrating all this code into webtiles.config; its
# existence predates that module

# load game/template dicts from a single yaml file
def load_from_yaml(path):
    # some day, turn this into a data class
    result = dict(
        templates = collections.OrderedDict(),
        games = collections.OrderedDict(),
        source = path
        )

    try:
        with open(path) as f:
            yaml_text = f.read()
    except OSError as e:
        logging.warn("Couldn't read file %s: %s", path, e)
        yaml_text = None

    try:
        data = yaml.safe_load(yaml_text)
    except yaml.YAMLError as e:
        logging.warning("Failed to load games from %s, skipping (parse failure: %s).",
                        file_name, e)
        return result
    if not isinstance(data, dict) or ('games' not in data and 'templates' not in data):
        logging.warning("Failed to load games from %s, skipping (no 'games'/'templates' key).",
                        file_name)
        return result

    extra_keys = ",".join(key for key in data.keys() if key != 'games' and key != 'templates')
    if extra_keys:
        logging.warning(
            "Found extra top-level keys '%s' in %s, ignoring them"
            " (only 'games'/'templates' keys will be parsed).",
            extra_keys, file_name)


    for t in ('templates', 'games'):
        if t in data:
            for game in data[t]:
                if not 'id' in game:
                    logging.error("Definition missing id in %s! Skipping '%s'", t, repr(game))
                    continue
                if game['id'] in result[t]:
                    logging.warning(
                        "Duplicate id %s in %s (source %s), skipping.",
                        game['id'], t, path)
                    continue
                result[t][game['id']] = game
    return result


def merge_games(accum, new):
    import webtiles.config

    for t in ('templates', 'games'):
        if new[t] is None: # make it easy to clear via yml
            continue
        if len(new[t]):
            logging.info("Reading %d %s from %s", len(new[t]), t, new['source'])
        for k in new[t]:
            if k in accum[t]:
                logging.warning(
                    "Duplicate id %s in %s (source %s), skipping.",
                    k, t, new['source'])
                continue
            accum[t][k] = webtiles.config.GameConfig(new[t][k], game_id=k)
            if t == 'templates' and k != 'default' and webtiles.config.is_metatemplate(k):
                raise ValueError("Cannot redefine reserved template name `%s`" % k)
    return accum


def load_games(reloading=False):
    """
    Load game definitions from games.d/*.yaml and merge with existing config.

    This function will be called on startup and every time the webserver is sent
    a USR1 signal (eg `kill -USR1 <pid>`). This allows dynamic modification of
    the games list without interrupting active game sessions.

    The format of the source YAML files is: `games: [<game>, ...]`. Each game is
    a dictionary matching the format of entries in the `games` variable above,
    with an extra key `id` for the game's ID. Directory paths support %n as
    described above.

    Dynamic update notes and caveats:
    1. If you do modify a game entry, the changed settings only affect new game
       sessions (including new spectators). Existing sessions use the old config
       until they close.
    2. Some settings affect spectators. If you modify a running game's config,
       the mismatch of settings between the player and new spectators might
       cause spectating to fail until the player restarts with new settings.
    3. If something breaks on the reload, everything *should* roll back to the
       previous state. But it's going to be a lot safer not to rely on this.
    """
    import webtiles.config
    accum = dict(
        templates = collections.OrderedDict(),
        games = collections.OrderedDict(),
        )
    try:
        # first: load any games config info from the module
        # this will include any game defs in config.yml (or debug-config.yml)
        from_module = dict(
            templates = webtiles.config.get('templates'),
            games = webtiles.config.get('games'),
            source = "base config"
            )
        accum = merge_games(accum, from_module)

        # second: if circumstances warrant it, load games from yaml files in
        # the games.d subdirectory. By default, this is prevented if the
        # config module sets games.
        use_gamesd = webtiles.config.using_games_dir()
        if use_gamesd is None and not webtiles.config.get('games') or use_gamesd:
            base_path = os.path.join(webtiles.config.server_path, "games.d")
            if os.path.exists(base_path):
                for file_name in sorted(os.listdir(base_path)):
                    path = os.path.join(base_path, file_name)
                    accum = merge_games(accum, load_from_yaml(path))
            else:
                logging.warn("Skipping non-existent YAML configuration directory: '%s'" % base_path)
        else:
            logging.warn("Skipping game definitions in `games.d/` based on config")

    except:
        if not reloading:
            raise # fail on initial load
        logging.error("Errors in game data, reverting to previous game configuration!", exc_info=True)
        return False

    old = dict(
        templates = webtiles.config.game_templates,
        games = webtiles.config.games)
    old_default = webtiles.config.get_template('default')

    # replace any existing config with what we have just loaded
    webtiles.config.game_templates = accum['templates']
    webtiles.config.games = accum['games']

    # do validation. We postpone this because success may rely on having an
    # updated `defaults` value in place.
    ok = True
    if ('default' in webtiles.config.game_templates and not
                            webtiles.config.define_default(
                                webtiles.config.game_templates.pop('default'))):
        ok = False

    for g in webtiles.config.game_templates.values():
        ok = g.validate() and ok

    if ok:
        # only validate games if templates are ok, since the main error case
        # leading to `not ok` here is a loop in templating (which will break
        # games that use that template)
        for g in webtiles.config.games.values():
            ok = g.validate_game() and ok

    if not ok:
        if not reloading:
            raise ValueError("Errors in game data!")
        logging.error("Errors in game data, reverting to previous game configuration!")
        webtiles.config.game_templates = old['templates']
        webtiles.config.metatemplates['default'] = old_default
        webtiles.config.games = old['games']
        return False

    # Done making changes. Print a diff message if reloading
    if reloading:
        # if we are reloading, print some helpful diff info
        for t in ('templates', 'games'):
            removed = [k for k in old[t] if k not in accum[t]]
            added = [k for k in accum[t] if k not in old[t]]
            # does the equality check work?
            updated = [k for k in accum[t] if k in old[t] and old[t][k] != accum[t][k]]
            if len(removed):
                logging.info("Removed %s: %s", t, ", ".join(removed))
            if len(added):
                logging.info("Added %s: %s", t, ", ".join(added))
            if len(updated):
                logging.info("Updated %s: %s", t, ", ".join(updated))
            else:
                logging.info("No updated %s", t)

    return True


def validate_game_dict(game):
    """Validate a game dictionary.

    Log warnings about issues and return validity.
    """
    if 'id' not in game:
        # Log the full game definition to help identify the game
        logging.warn("Missing 'id' from game definition %r", game)
        return False
    found_errors = False
    required = ('id', 'name', 'crawl_binary', 'rcfile_path', 'macro_path',
                'morgue_path', 'inprogress_path', 'ttyrec_path',
                'socket_path')
    optional = ('dir_path', 'cwd', 'morgue_url', 'milestone_path',
                'send_json_options', 'options', 'env', 'separator',
                'show_save_info', 'allowed_with_hold', 'version',
                'template', 'pre_options', 'client_path')
    # XX less ad hoc typing
    boolean = ('send_json_options', 'show_save_info', 'allowed_with_hold')
    string_array = ('options', 'pre_options')
    string_dict = ('env', )

    # values where %n can't be expanded
    # XX not sure this list gets everything
    username_invalid = ('pre_options', 'crawl_binary', 'socket_path')
    # XX should %v be validated here too? Currently handled in
    # GameConfig.validate_game

    allow_none = {'morgue_url'}
    for prop in required:
        if prop not in game:
            found_errors = True
            logging.warn("Missing required property '%s' in game '%s'",
                         prop, game['id'])
    for prop, value in game.items():
        expected = prop in required or prop in optional
        if not expected:
            # Don't count this as an error
            logging.warn("Unknown property '%s' in game '%s'",
                         prop, game['id'])
        if value is None and prop in allow_none:
            continue # short-circuit other checks

        if prop in boolean:
            if not isinstance(value, bool):
                found_errors = True
                logging.warn("Property '%s' value should be boolean in game '%s'",
                             prop, game['id'])
        elif prop in string_array:
            if not isinstance(value, list):
                found_errors = True
                logging.warn("Property '%s' value should be list of strings in game '%s'",
                             prop, game['id'])
                continue
            for item in value:
                if not isinstance(item, str):
                    found_errors = True
                    logging.warn(
                      "Item '%s' in property '%s' should be a string in game '%s'",
                      item, prop, game['id'])
            if prop in username_invalid and " ".join(value).find("%n") >= 0:
                found_errors = True
                logging.warn(
                    "'%%n' will not be expanded in %s (game %s): '%s'" % (
                                    rop, game['id'], repr(value)))
        elif prop in string_dict:
            if not isinstance(value, dict):
                found_errors = True
                logging.warn(
                  "Property '%s' value should be a map of string: string in game '%s'",
                  prop, game['id'])
                continue
            for item_key, item_value in value.items():
                if not isinstance(item_key, str):
                    found_errors = True
                    logging.warn(
                      "Item key '%s' in property '%s' should be a string in game '%s'",
                      item_key, prop, game['id'])
                if not isinstance(item_value, str):
                    found_errors = True
                    logging.warn("Item value '%s' of key '%s' in property '%s'"
                                 " should be a string in game '%s'",
                                 item_value, item_key, prop, game['id'])
                if prop in username_invalid and item_value.find("%n") >= 0:
                    found_errors = True
                    logging.warn(
                        "'%%n' will not be expanded in %s.%s (game %s): '%s'" % (
                                    prop, item_key, game['id'], repr(value)))
        else:
            # String property
            if not isinstance(value, str):
                found_errors = True
                logging.warn("Property '%s' value should be string in game '%s'",
                             prop, game['id'])
            if prop in username_invalid and value.find("%n") >= 0:
                found_errors = True
                logging.warn(
                    "'%%n' will not be expanded in %s (game %s): '%s'" % (
                                    prop, game['id'], repr(value)))
    return not found_errors


binaries = {}
def collect_game_modes():
    import webtiles.config
    # figure out what game modes are associated with which game in the config.
    # Basically: try to line up options in the game config with game types
    # reported by the binary. If the binary doesn't support `-gametypes-json`
    # then the type will be `None`. This is unfortunately fairly post-hoc, and
    # there would be much better ways of doing this if I weren't aiming for
    # backwards compatibility.
    #
    # This is very much a blocking call, especially with many binaries.
    # Changing crawl binaries in a way that affects this will require a restart
    # to get accurate save info.
    global binaries
    for g in webtiles.config.games:
        key = webtiles.config.games[g].get_binary_key()
        if not webtiles.config.games[g].get("show_save_info", False):
            binaries[key] = None
            continue
        if key in binaries and binaries[key]:
            continue
        call = webtiles.config.games[g].get_call_base()

        # "dummy" is here for the sake of the dgamelaunch-config launcher
        # scripts, which choke badly if there is no second argument. The actual
        # crawl binary just ignores it. (TODO: this is not an ideal thing about
        # these wrapper scripts...)
        call += ["-gametypes-json", "dummy"]
        try:
            m_json = subprocess.check_output(call, stderr=subprocess.STDOUT)
            binaries[key] = json_decode(m_json)
        except subprocess.CalledProcessError:  # return value 1
            binaries[key] = None
        except ValueError:  # JSON decoding issue?
            logging.warn("JSON error with output '%s'", repr(m_json))
            binaries[key] = None

    # Example output of this call:
    # {"normal":"","tutorial":"-tutorial","arena":"-arena","sprint":"-sprint","seeded":"-seed"}
    # we want to take the options specified in this map and reconstruct the
    # game modes as defined in the game configs.

    game_modes = {}
    for g in webtiles.config.games:
        key = webtiles.config.games[g].get_binary_key()
        if binaries[key] is None:
            # this binary either has no enabled save infos, opr does not
            # support -gamemode-json, so we know nothing about its game modes.
            # (Pre-0.24 dcss.)
            # XX should we always try to collect game modes?
            game_modes[g] = None
            continue

        # A gametype may specify "" as the options string (e.g. "normal"). We
        # can't look directly for this, since there could be other options.
        # So first find the relevant mode if any, and then use it as a fallback
        # for any calls where no modes are found.
        no_options_mode = None
        for m in binaries[key]:
            if binaries[key][m] == "":
                no_options_mode = m
                break

        mode_found = False
        for mode in binaries[key]:
            for opt in webtiles.config.game_param(g, "options", default=[]):
                if opt == binaries[key][mode]:
                    game_modes[g] = mode
                    mode_found = True
                    break
            if mode_found:
                break
        if not mode_found and no_options_mode is not None:
            game_modes[g] = no_options_mode

    return game_modes
