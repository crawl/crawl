#!/usr/bin/env python

import toml
import config
import re
from collections import OrderedDict

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
        if key in data and data[key] is not None: d[key] = data[key]
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
    ("daemon", config.daemon),
    ("logging_config", config.logging_config),
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

    ("max_idle_time", config.max_idle_time),
    ("connection_timeout", config.connection_timeout),
    ("kill_timeout", config.kill_timeout),

    ("recording_term_size", list(config.recording_term_size)),

    ("watch_socket_dirs", config.watch_socket_dirs),
    ("status_file_update_rate", config.status_file_update_rate),
    ("milestone_files", milestones()),

    ("games", games())
])

binds_list = binds()
ssl_binds_list = sslbinds()

if binds_list: conf["binds"] = binds_list
if config.ssl_options: conf["ssl_options"] = config.ssl_options
if ssl_binds_list: conf["ssl_binds"] = ssl_binds_list
if config.uid: conf["uid"] = config.uid
if config.gid: conf["gid"] = config.gid
if config.umask: conf["umask"] = config.umask
if config.chroot: conf["chroot"] = config.chroot
if config.pidfile: conf["pidfile"] = config.pidfile
if config.http_connection_timeout: conf["http_connection_timeout"] = config.http_connection_timeout
if config.server_socket_path: conf["server_socket_path"] = config.server_socket_path
if config.player_url: conf["player_url"] = config.player_url
if config.game_data_no_cache: conf["game_data_no_cache"] = True
if config.no_cache: conf["no_cache"] = True
if config.autologin: conf["no_cache"] = config.autologin

print toml.dumps(conf)
