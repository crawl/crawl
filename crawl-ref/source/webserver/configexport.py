#!/usr/bin/env python

import json
import config
import re
from collections import OrderedDict

json.encoder.c_make_encoder = None # http://bugs.python.org/issue6105

VERSION_RE = r"trunk$|\d.\d\d$"

def binds():
    if not config.bind_nonsecure:
        return []
    try:
        pairs = list(config.bind_pairs)
    except AttributeError:
        pairs = [(config.bind_address, config.bind_port)]
    return [{"address": address, "port": port}
            for (address, port) in pairs]

def sslbinds():
    if config.ssl_options is None:
        return []
    try:
        pairs = list(config.ssl_bind_pairs)
    except AttributeError:
        pairs = [(config.ssl_address, config.ssl_port)]
    return [{"address": address, "port": port}
            for (address, port) in pairs]

def convert_game(**data):
    versions = re.findall(VERSION_RE, data["name"])
    if versions: data["version"] = versions[0]
    opts = data.get("options", [])
    if "-sprint" in opts:
        mode = "Dungeon Sprint"
    elif "-zotdef" in opts:
        mode = "Zot Defence"
    elif "-tutorial" in opts:
        mode = "Tutorial"
    else:
        mode = "Dungeon Crawl"
    data["mode"] = mode
    d = OrderedDict()
    for key in ["id", "name", "version", "mode", "comment", "morgue_url",
                "crawl_binary", "options", "pre_options", "inprogress_path",
                "rcfile_path", "socket_path", "morgue_path", "client_path",
                "macro_path", "ttyrec_path", "send_json_options"]:
        if key in data: d[key] = data[key]
    return d

def games():
    return [convert_game(id=id, **data) for (id, data) in config.games.items()]

def milestones():
    if config.milestone_file is None:
        return []
    elif type(config.milestone_file) is list:
        return config.milestone_file
    else:
        return [config.milestone_file]

conf = OrderedDict([
    ("binds", binds()),
    ("ssl_options", config.ssl_options),
    ("ssl_binds", sslbinds()),
    ("daemon", config.daemon),
    ("uid", config.uid),
    ("gid", config.gid),
    ("umask", config.umask),
    ("chroot", config.chroot),
    ("pidfile", config.pidfile),
    ("logging_config", config.logging_config),
    ("http_connection_timeout", config.http_connection_timeout),
    ("max_connections", config.max_connections),
    ("dgl_status_file", config.dgl_status_file),
    ("server_id", config.server_id),
    ("init_player_program", config.init_player_program),
    ("password_db", config.password_db),
    ("crypt_algorithm", config.crypt_algorithm),
    ("crypt_salt_length", config.crypt_salt_length),
    ("nick_regex", config.nick_regex),
    ("max_passwd_length", config.max_passwd_length),
    ("login_token_lifetime", config.login_token_lifetime),

    ("static_path", config.static_path),
    ("template_path", config.template_path),
    ("server_socket_path", config.server_socket_path),

    ("max_idle_time", config.max_idle_time),
    ("connection_timeout", config.connection_timeout),
    ("kill_timeout", config.kill_timeout),

    ("recording_term_size", config.recording_term_size),

    ("watch_socket_dirs", config.watch_socket_dirs),
    ("status_file_update_rate", config.status_file_update_rate),
    ("player_url", config.player_url),
    ("milestone_files", milestones()),

    ("games", games())
])

if config.game_data_no_cache: conf["game_data_no_cache"] = True
if config.no_cache: conf["no_cache"] = True
if config.autologin: conf["no_cache"] = config.autologin

print json.dumps(conf, indent=4, separators=(',', ': '))
