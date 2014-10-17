#!/usr/bin/env python

import argparse, csv, os, os.path, signal, sys, toml, urllib2

## set up help options
parser = argparse.ArgumentParser(description='Fetch the public Sequell nickmap and make a player title file')
parser.add_argument('config_file', metavar='<config-file>', nargs='?',
                    default=None)

args = parser.parse_args()
config_file = args.config_file

## This is the logic WebTiles uses to search for the TOML configuration.
if config_file is None:
    config_file = os.environ.get("WEBTILES_CONF")
    if config_file is None:
        if os.path.exists("./config.toml"):
            config_file = "./config.toml"
        else:
            config_file = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                       "config.toml")
        if not os.path.exists(config_file):
            errmsg = "Could not find WebTiles config file!"
            sys.exit(errmsg)

## Get the WebTiles TOML config data and check that the necessary options are
## set.
config = toml.load(open(config_file, "r"))
if not config['pidfile']:
    errmsg = "WebTiles server pidfile not set in configuration!"
    sys.exit(errmsg)
try:
    pid_fh = open(config['pidfile'], 'r')
except:
    errmsg = "Can't open WebTiles server pidfile {0}!".format(config['pidfile'])
    sys.exit(errmsg)
pid = int(pid_fh.readline())
pid_fh.close()

if not config['nickdb_url']:
    errmsg = "Sequell nickdb URL not set in configuration!"
    sys.exit(errmsg)
if not config['player_title_file']:
    errmsg = "Player title file not set in configuration!"
    sys.exit(errmsg)
if not config['title_names']:
    errmsg = "Title names not set in configuration!"
    sys.exit(errmsg)


## Retreive the nickdb and build a dict of the nick data for the titles we
## want.
try:
    nick_fh = urllib2.urlopen(config['nickdb_url'])
except urllib2.HTTPError as e:
    errmsg = "Unable to retreive nickdb: " + e.reason
    sys.exit(errmsg)

nick_r = csv.reader(nick_fh, delimiter=' ', quoting=csv.QUOTE_NONE)
titles = {}
for row in nick_r:
    if row[0] in config['title_names']:
        titles[row[0]] = row[1:]
nick_fh.close()

## Filter out the title nick data and write to the player title file
title_fh = open(config['player_title_file'], 'w')
title_w = csv.writer(title_fh, delimiter=' ', quoting=csv.QUOTE_NONE)
for t in config['title_names']:
    if t in titles:
        title_w.writerow([t] + titles[t])
title_fh.close()

## Tell WebTiles server to reload the player titles
os.kill(pid, signal.SIGUSR2)
