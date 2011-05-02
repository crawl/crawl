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

max_connections = 100

# ssl_options = None # No SSL
ssl_options = {
    "certfile": "./webserver/localhost.crt",
    "keyfile": "./webserver/localhost.key"
}
ssl_address = ""
ssl_port = 8081
