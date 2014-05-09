#!/usr/bin/env python

import json
import config
import re

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
    return data

def games():
    return [convert_game(id=id, **data) for (id, data) in config.games.items()]

def milestones():
    if config.milestone_file is None:
        return []
    elif type(config.milestone_file) is list:
        return config.milestone_file
    else:
        return [config.milestone_file]

conf = {
    "dgl_mode": config.dgl_mode,
    "binds": binds(),
    "logging_config": config.logging_config,
    "password_db": config.password_db,
    "static_path": config.static_path,
    "template_path": config.template_path,
    "server_socket_path": config.server_socket_path,
    "server_id": config.server_id,
    "game_data_no_cache": config.game_data_no_cache,
    "watch_socket_dirs": config.watch_socket_dirs,
    "games": games(),
    "dgl_status_file": config.dgl_status_file,
    "milestone_files": milestones(),
    "status_file_update_rate": config.status_file_update_rate,
    "recording_term_size": config.recording_term_size,
    "max_connections": config.max_connections,
    "init_player_program": config.init_player_program,
    "ssl_options": config.ssl_options,
    "ssl_binds": sslbinds(),
    "connection_timeout": config.connection_timeout,
    "max_idle_time": config.max_idle_time,
    "http_connection_timeout": config.http_connection_timeout,
    "kill_timeout": config.kill_timeout,
    "nick_regex": config.nick_regex,
    "max_passwd_length": config.max_passwd_length,
    "crypt_algorithm": config.crypt_algorithm,
    "crypt_salt_length": config.crypt_salt_length,
    "login_token_lifetime": config.login_token_lifetime,
    "uid": config.uid,
    "gid": config.gid,
    "umask": config.umask,
    "chroot": config.chroot,
    "pidfile": config.pidfile,
    "daemon": config.daemon,
    "player_url": config.player_url,
    "no_cache": config.no_cache,
    "autologin": config.autologin
    }

print json.dumps(conf, indent=4, separators=(',', ': '))
