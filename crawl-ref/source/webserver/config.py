import logging

bind_address = ""
bind_port = 8080

logging_config = {
    "filename": "webtiles.log",
    "level": logging.INFO,
    "format": "%(asctime)s %(levelname)s: %(message)s"
}

password_db = "./webserver/passwd.db3"

static_path = "./webserver/static"
template_path = "./webserver/"

crawl_binary = "./crawl"

rcfile_path = "./rcs/"
macro_path = "./rcs/"
morgue_path = "./rcs/"

running_game_path = "./rcs/running/"

dgl_status_file = "./rcs/status"

status_file_update_rate = 5

max_connections = 100

init_player_program = "/bin/echo"

# ssl_options = None # No SSL
ssl_options = {
    "certfile": "./webserver/localhost.crt",
    "keyfile": "./webserver/localhost.key"
}
ssl_address = ""
ssl_port = 8081

connection_timeout = 600

kill_timeout = 10 # Seconds until crawl is killed after HUP is sent

nick_regex = r"^[a-zA-Z0-9]{3,20}$"
max_passwd_length = 20
