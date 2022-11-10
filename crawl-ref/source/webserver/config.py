# ### Server Operators ###
# If you want to customise any settings in this file, you'll need to work around
# the fact that is version controlled. Some options:
# 1. Use a local commit and always do a rebase pull on top of it. (This may
#    affect versioning, though.)
# 2. Create config.yml and write your overrides in there.
# 3. Use a more complicated scripting system; for example, dgamelaunch-config
#    supports templating this file and automatically handles merging the
#    server config with the repository files.
#
# ### Developers ###
# Warning! Servers will not update or merge with the version controlled copy of
# this file, so any parameters here should be presented as recommendations or
# documentation of the default, not a default value, and option name changes
# here will require manual intervention on the part of server admins. New
# values should always come with a default that does not lead to crashes, so
# that something sensible happens on DGL upgrade.
#
# To add new default values, see the `webtiles.config` module. Note that
# webtiles.config.get(x) returns `None` if `x` is not present in the config
# data, so this can be taken as a default default value.
#
# Bad assumptions about config defaults particularly impacts templated html
# files, which are loaded and called dynamically, so *do* get updated
# immediately on a rebuild (unlike python code in this module or
# webtiles.config). If something like client.html raises an exception, this
# will trigger 500 errors across the whole server.

import logging
import os

import yaml

# Where to look for `games.d/`, `config.yml`, and other things.
server_path = os.path.dirname(os.path.abspath(__file__))

# Set to False to enable `local webtiles` mode: the regular webtiles lobby
# is disabled, and opening webtiles loads the game main menu. (default: True)
# dgl_mode = False

bind_nonsecure = True # Set to false to only use SSL
bind_address = ""
bind_port = 8080
# Or listen on multiple address/port pairs (overriding the above) with:
# bind_pairs = (
#     ("127.0.0.1", 8080),
#     ("localhost", 8082),
#     ("", 8180), # All addresses
# )

# to log to a file directly, uncomment and add something like:
#    "filename": "webtiles.log",
# logging_config = {
#     "level": logging.INFO,
#     "format": "%(asctime)s %(levelname)s: %(message)s"
# }

# sometimes useful for debugging:
# logging.getLogger('asyncio').setLevel(logging.DEBUG)

password_db = "./webserver/passwd.db3"
# Uncomment and change if you want this db somewhere separate from the
# password_db location.
#settings_db = "./webserver/user_settings.db3"

static_path = "./webserver/static"
template_path = "./webserver/templates/"

# Path for server-side unix sockets (to be used to communicate with crawl)
# server_socket_path = None # None: Uses global temp dir

# Server name, so far only used in the ttyrec metadata
server_id = ""

# Disable caching of game data files
game_data_no_cache = True

# Watch socket dirs for games not started by the server
# watch_socket_dirs = False

################
# Game configs
################
#
# The webtiles server needs some game modes configured in order to run. The
# default in-repo configuration enables regular mode, seeded, tutorial, and
# sprint for the `crawl` binary as built in the repository. (Build with
# `WEBTILES=y`.)
#
# You can define game configs in two ways:
# 1. As *.yml files in `games.d/`. (default; see games.d/base.yml)
# 2. With a dictionary `games` in this file
#
# Both of these formats are essentially the same. See the commented examples
# below, as well as the comments and examples in games.d/base.yml for more
# information on the format and how it is used. Examples from production
# servers are also available in branches of the repository:
#   https://github.com/crawl/dgamelaunch-config
# (These are further templated using the scripts in that package.)

# to override yml loading, simply set `games` in this file; see the example
# below. To force using both sources, set `use_game_yaml` to True. Games
# defined in this file will precede games defined in games.d. Templates
# defined in this file are always loaded.
# use_game_yaml = True

# Templating game configurations:
# You can define a set of parameters to use as a template across multiple
# game configs. To use a template by name, use the `template` parameter.
# Recursive templating is supported. If a template named `default` is defined,
# it will be used as the template for any game definitions that specify no
# template at all (see example below). If `default` is defined, it can be
# explicitly overriden by using the template name `base` (which cannot be
# redefined).

# Templating string values:
# if the `version` key is set, most game parameters will support templating
# with %v, %V, and %r.
#   %v: the version value as-is
#   %V: the capitalized version value (e.g. "trunk" -> "Trunk")
#   %r: the 'raw' version, which strips off an initial "0.". So for example,
#       "0.29" would give just "29". If this substring does not occur, %r
#       is equivalent to %v. (Implementation note: characters before a "0."
#       will be dropped as well. Not relevant for normal crawl versions of
#       course.)
#
# Most fields (though not all, in particular, not any game-general ones and
# not socket_path) also support templating with %n, which gives the player's
# username.
#
# The bare minimum necessary fields are what it takes to get the crawl binary
# running. Games are called with the following argv; some of these fields can
# be inferred when not set.
# [
#   $crawl_binary,
#   *$pre_options,
#   "-name", "%n",
#   "-rc", "$rcfile_path/%n.rc",
#   "-macro", "$macro_path/%n.macro",
#   "-morgue", "$morgue_path,"
#   *$options,
#   "-dir", "$dir_path"
#   "-webtiles-socket", "$socket_path/%n:$timestamp.sock",
#   "-await-connection"
# ]
#
# The example below illustrates basic uses of both of these.

# Example of defining `games` directly via a default template and a dictionary,
# and sets all the required paths:
# templates = dict(
#     default = dict(
#         crawl_binary = "./crawl", # relative paths are relative to CWD from running server.py
#         rcfile_path = "./rcs/",
#         macro_path = "./rcs/",
#         morgue_path = "./rcs/%n",
#         inprogress_path = "./rcs/running",
#         ttyrec_path = "./rcs/ttyrecs/%n",
#         socket_path = "./rcs",
#         client_path = "./webserver/game_data/",
#         morgue_url = "http://crawl.akrasiac.org/rawdata/%n/",
#         # the `dir_path` field uses local crawl defaults when unset. This
#         # should almost always be set on a production server, but not for
#         # development.
#         # dir_path = ".",
#         # cwd = ".", # override the CWD inherited when starting the server
#         # morgue_url = None, # public-facing URL, if set
#         show_save_info = True,
#         allowed_with_hold = True,
#         # milestone_path = "./rcs/milestones",
#         # env = {"LANG": "en_US.UTF8"},
#         ),
#     )

# # `games` needs to be an OrderedDict pre python 3.6, so this example uses that
# # for compatibility. Needs the default template above.
# import collections
# games = collections.OrderedDict([
#     ("dcss-web-trunk",   dict(version = "trunk", name="Play %v",)),
#     ("seeded-web-trunk", dict(version = "trunk", name="Seeded", options=["-seed"])),
#     ("tut-web-trunk",    dict(version = "trunk", name="Tutorial", options=["-tutorial"])),
#     ("sprint-web-trunk", dict(version = "trunk", name="Sprint %v", options=["-sprint"])),
# ])
#
# Non-templated example: it's possible just to set every field directly.
# games = collections.OrderedDict([
#     ("dcss-web-trunk", dict(
#         version = "trunk",
#         name = "Play %v",
#         crawl_binary = "./crawl",
#         rcfile_path = "./rcs/",
#         macro_path = "./rcs/",
#         morgue_path = "./rcs/%n",
#         inprogress_path = "./rcs/running",
#         ttyrec_path = "./rcs/ttyrecs/%n",
#         socket_path = "./rcs",
#         client_path = "./webserver/game_data/",
#         morgue_url = None,
#         show_save_info = True,
#         allowed_with_hold = True,
#         # milestone_path = "./rcs/milestones",
#         )),
# ])

dgl_status_file = "./rcs/status"

# Extra paths to tail for milestone updates. This is a legacy setting, you
# should use `milestone_path` or `dir_path` for each game in the games dict.
# (This setting can be a string or list of strings.)
# milestone_file = ["./milestones"]

# status_file_update_rate = 5
# lobby_update_rate = 2

# recording_term_size = (80, 24)

# max_connections = 100

# Script to initialize a user, e.g. make sure the paths
# and the rc file exist. This is not done by the server
# at the moment. This value
init_player_program = "./util/webtiles-init-player.sh"

ssl_options = None # No SSL
#ssl_options = {
#    "certfile": "./webserver/localhost.crt",
#    "keyfile": "./webserver/localhost.key"
#}
ssl_address = ""
ssl_port = 8081
# Or listen on multiple address/port pairs (overriding the above) with:
# ssl_bind_pairs = (
#     ("127.0.0.1", 8081),
#     ("localhost", 8083),
# )

# connection_timeout = 600
# max_idle_time = 5 * 60 * 60

# use_gzip = True

# Seconds until stale HTTP connections are closed
# This needs a patch currently not in mainline tornado.
# http_connection_timeout = None

# Set this to true if you are behind a reverse proxy
# Your proxy must set header X-Real-IP
#
# Enabling this option when webtiles is NOT protected behind a reverse proxy
# introduces a security risk. An attacker could inject a false address into the
# X-Real-IP header. Do not enable this option if the webtiles server is
# directly exposed to users.
# http_xheaders = None

# kill_timeout = 10 # Seconds until crawl is killed after HUP is sent

# max_passwd_length = 20

# Username management options.
#
# This options are used on a running webserver only for new account creation!
# It is possible to run a ban query against an existing userdb with them as
# well, see wtutil.py options --check-config-bans and --run-config-bans. For
# banning offensive names, skip to `banned` below. Crawl nicknames are case
# preserving but not case sensitive. To be an acceptable username, a string
# must pass all three of these checks (if defined).
#
# 1. Use `nick_regex` to modify the basic format for valid nicknames.
# a nick is allowed if it matches this regex. It is not recommended to modify
# this unless you really know what you're doing, and this default should not
# ever be modified to something less restrictive for official servers.
# nick_regex = r"^[a-zA-Z0-9]{3,20}$"
#
# 2. `nick_check_fun` if defined, is a function that returns true on valid
# nicknames. You can use this for arbitrary custom nick checks in a server
# config. You will need to do case management manually in this funciton.
# def nick_check_fun(s):
#     return s.lower() != "plog" and s.lower() != "muggle"
#
# 3. For easily excluding a larger list of usernames from account creation, you
# can use the `banned` option. This can take a simple list of names, or some
# more complex things. Instead of or in addition to putting banned names in
# config, you can put them in files named `banned_players.txt` and
# `banned_players.yml`. The latter must have a single dict entry `banned`
# that is allowed to use the complex options below.
#
# Simple example:
# banned = ['plog', 'muggle', 'muggles', 'ihatemuggles']
#
# Complex example. This will ban many things, for example,
# plain 'muggle', leetspeak variants like 'mugg13s', versions of the phrase
# embedded into other contexts with different letter repeats such as
# 'xx1hatemugggglesxx', etc. It is recommended to be fairly cautious when using
# these extended options, because of
# https://en.wikipedia.org/wiki/Scunthorpe_problem; there are very few English
# slurs where it is a good idea to turn on all three of these options.
# You can mix plain strings with complex entries, as shown here:
#
# banned = ['plog',
#           {'options': {'leet': True, 'repeats': True, 'part': True},
#            'names': ['muggle']}]
# it is allowed to mix in plain strings with dicts in this list,

# Example to demonstrate this same list in yml form:
# banned:
#   - plog
#   - options:
#       leet: True
#       repeats: True
#       part: True
#     names:
#       - muggle
#
# It is recommended to use an external file rather than defining this option
# directly in config. (Among other reasons, this means that server admins are
# only viewing/editing a file likely filled with slurs when they are actually
# intending to...)

# Set to True to allow users to request a password reset email. Some settings
# must be properly configured for this to work:
# allow_password_reset = False
# Set to True to allow dgl admin users to generate password reset tokens in the
# admin panel. Only use if you really trust your admin users!
# admin_password_reset = False

# Set to the primary URL where a player would reach the main lobby
# For example: "http://crawl.akrasiac.org/"
# This is required for for password reset, as it will be the base URL for
# recovery URLs. Use "http://localhost:8080/" for testing.
lobby_url = None

# Proper SMTP settings are required for password reset to function properly.
# if smtp_host is anything other than `localhost`, you may need to adjust the
# timeout settings (see server.py, calls to ioloop.set_blocking_log_threshold).
# TODO: set_blocking_log_threshold is deprecated in tornado 5+...
# Ideally, test out these settings carefully in a non-production setting
# before enabling this, as there's a bunch of ways for this to go wrong and you
# don't want to get your SMTP server blacklisted.
smtp_host = "localhost"
smtp_port = 25
smtp_use_ssl = False
smtp_user = "" # set to None for no auth
smtp_password = ""
smtp_from_addr = "noreply@crawl.example.org" # The address from which automated
                                             # emails will be sent

# crypt() algorithm, e.g. "1" for MD5 or "6" for SHA-512; see crypt(3). If
# false, use traditional DES (but then only the first eight characters of the
# password are significant). If set to "broken", use traditional DES with
# the password itself as the salt; this is necessary for compatibility with
# dgamelaunch, but should be avoided if possible because it leaks the first
# two characters of the password's plaintext.
# crypt_algorithm = "broken"

# The length of the salt string to use. If crypt_algorithm is false, this
# setting is ignored and the salt is two characters.
# crypt_salt_length = 16

# login_token_lifetime = 7 # Days

uid = None  # If this is not None, the server will setuid to that (numeric) id
gid = None  # after binding its sockets.

umask = None # e.g. 0077

chroot = None

pidfile = None
# daemon = False # If true, the server will detach from the session after startup

# Set to a URL with %s where lowercased player name should go in order to
# hyperlink WebTiles spectator names to their player pages.
# For example: "http://crawl.akrasiac.org/scoring/players/%s.html"
# Set to None to disable player page hyperlinks
player_url = None

# set one of these for various moderation modes. Disabled preempts hold. In
# account hold mode, new accounts cannot use chat, cannot spectate, and do
# not appear in the lobby until explicitly approved by an admin. They can still
# play. (Of course, they can still log out and spectate as anon.)
# new_accounts_disabled = True
# new_accounts_hold = True

# If set to True, a SIGHUP triggers an attempt to reload the config and game
# data. Some values cannot be reloaded (including this one), and to reset a
# value to its default, you need to explicitly set the value rather than
# comment it out.
# If not explicitly set, this defaults to False.
hup_reloads_config = True

# set to do periodic logging of user load
# load_logging_rate = 10 # seconds, set to 0 to explicitly disable
# slow_callback_alert = 0.25 # seconds, set to None to explicitly disable

# Only for development:
# This is insecure; do not set development_mode = True in production!
# development_mode = False

# Disable caching of static files which are not part of game data.
# no_cache = development_mode
# Automatically log in all users with the username given here.
autologin = None
