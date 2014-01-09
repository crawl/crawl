import logging
try:
    from collections import OrderedDict
except ImportError:
    from ordereddict import OrderedDict

dgl_mode = True

bind_nonsecure = True # Set to false to only use SSL
bind_address = ""
bind_port = 8080
# Or listen on multiple address/port pairs (overriding the above) with:
# bind_pairs = (
#     ("127.0.0.1", 8080),
#     ("localhost", 8082),
#     ("", 8180), # All addresses
# )

logging_config = {
#    "filename": "webtiles.log",
    "level": logging.INFO,
    "format": "%(asctime)s %(levelname)s: %(message)s"
}

password_db = "./webserver/passwd.db3"

static_path = "./webserver/static"
template_path = "./webserver/templates/"

# Path for server-side unix sockets (to be used to communicate with crawl)
server_socket_path = None # Uses global temp dir

# Server name, so far only used in the ttyrec metadata
server_id = ""

# Disable caching of game data files
game_data_no_cache = True

# Watch socket dirs for games not started by the server
watch_socket_dirs = False

# Game configs
# %n in paths is replaced by the current username
games = OrderedDict([
    ("dcss-web-trunk", dict(
        name = "DCSS trunk",
        crawl_binary = "./crawl",
        rcfile_path = "./rcs/",
        macro_path = "./rcs/",
        morgue_path = "./rcs/%n",
        inprogress_path = "./rcs/running",
        ttyrec_path = "./rcs/ttyrecs/%n",
        socket_path = "./rcs",
        client_path = "./webserver/game_data/",
        send_json_options = True)),
    ("sprint-web-trunk", dict(
        name = "Sprint trunk",
        crawl_binary = "./crawl",
        rcfile_path = "./rcs/",
        macro_path = "./rcs/",
        morgue_path = "./rcs/%n",
        inprogress_path = "./rcs/running",
        ttyrec_path = "./rcs/ttyrecs/%n",
        socket_path = "./rcs",
        client_path = "./webserver/game_data/",
        send_json_options = True,
        options = ["-sprint"])),
    ("zd-web-trunk", dict(
        name = "Zot Defense trunk",
        crawl_binary = "./crawl",
        rcfile_path = "./rcs/",
        macro_path = "./rcs/",
        morgue_path = "./rcs/%n",
        inprogress_path = "./rcs/running",
        ttyrec_path = "./rcs/ttyrecs/%n",
        socket_path = "./rcs",
        client_path = "./webserver/game_data/",
        send_json_options = True,
        options = ["-zotdef"])),
    ("tut-web-trunk", dict(
        name = "Tutorial trunk",
        crawl_binary = "./crawl",
        rcfile_path = "./rcs/",
        macro_path = "./rcs/",
        morgue_path = "./rcs/%n",
        inprogress_path = "./rcs/running",
        ttyrec_path = "./rcs/ttyrecs/%n",
        socket_path = "./rcs",
        client_path = "./webserver/game_data/",
        send_json_options = True,
        options = ["-tutorial"])),
])

dgl_status_file = "./rcs/status"

# Set to None not to read milestones
milestone_file = "./milestones"

status_file_update_rate = 5

recording_term_size = (80, 24)

max_connections = 100

# Script to initialize a user, e.g. make sure the paths
# and the rc file exist. This is not done by the server
# at the moment.
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

connection_timeout = 600
max_idle_time = 5 * 60 * 60

# Seconds until stale HTTP connections are closed
# This needs a patch currently not in mainline tornado.
http_connection_timeout = None

kill_timeout = 10 # Seconds until crawl is killed after HUP is sent

nick_regex = r"^[a-zA-Z0-9]{3,20}$"
max_passwd_length = 20

# crypt() algorithm, e.g. "1" for MD5 or "6" for SHA-512; see crypt(3). If
# false, use traditional DES (but then only the first eight characters of the
# password are significant). If set to "broken", use traditional DES with
# the password itself as the salt; this is necessary for compatibility with
# dgamelaunch, but should be avoided if possible because it leaks the first
# two characters of the password's plaintext.
crypt_algorithm = "broken"

# The length of the salt string to use. If crypt_algorithm is false, this
# setting is ignored and the salt is two characters.
crypt_salt_length = 16

login_token_lifetime = 7 # Days

uid = None  # If this is not None, the server will setuid to that (numeric) id
gid = None  # after binding its sockets.

umask = None # e.g. 0077

chroot = None

pidfile = None
daemon = False # If true, the server will detach from the session after startup

# Set to a URL with %s where lowercased player name should go in order to
# hyperlink WebTiles spectator names to their player pages.
# For example: "http://crawl.akrasiac.org/scoring/players/%s.html"
# Set to None to disable player page hyperlinks
player_url = None
