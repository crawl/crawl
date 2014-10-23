#!/usr/bin/env python

from __future__ import print_function
import argparse, csv, os, os.path, signal, sys, toml, urllib2

## set up help options
parser = argparse.ArgumentParser(description="Fetch the public Sequell nickmap "
                                 "and make a player title file for WebTiles")
parser.add_argument('config_file', metavar='<config-file>', nargs='?',
                    default=None)
parser.add_argument('-q', dest='quiet', action='store_true', default=False,
                    help='suppress all messages but warnings/errors.')

args = parser.parse_args()
config_file = args.config_file

## This is the logic WebTiles uses to search for the TOML configuration.
if config_file is None:
    config_file = os.environ.get("WEBTILES_CONF")
    if config_file is None:
        if os.path.exists("./config.toml"):
            config_file = "./config.toml"
        else:
            config_dir = os.path.dirname(os.path.abspath(__file__))
            config_file = os.path.join(config_dir, "config.toml")
        if not os.path.exists(config_file):
            errmsg = "Could not find WebTiles config file!"
            sys.exit(errmsg)

## Get the WebTiles TOML config data and check that the necessary options are
## set.
config = toml.load(open(config_file, "r"))
if config.get('pidfile'):
    try:
        pid_fh = open(config['pidfile'], 'r')
        pid = int(pid_fh.readline())
        pid_fh.close()
    except:
        print(("Warning: Can't open WebTiles server "
               "pidfile {0}!".format(config['pidfile'])), file=sys.stderr)
        pid = None
else:
     print("Warning: No server pid file set in WebTiles configuration!",
           file=sys.stderr)
     pid = None


if not config.get('nickdb_url'):
    errmsg = "Sequell nickdb URL (option nickdb_url) not set in configuration!"
    sys.exit(errmsg)
if not config['player_title_file']:
    errmsg = ("Player title file (option player_title_file) not set in"
              "configuration!")

    sys.exit(errmsg)
if not config.get('title_names'):
    errmsg = "Title names (option title_names) not set in configuration!"
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
row_count = 0
for row in nick_r:
    if row[0] in config['title_names']:
        titles[row[0]] = row[1:]
    row_count += 1
nick_fh.close()
if not args.quiet:
    print("Fetched {0} lines from {1}".format(row_count, config['nickdb_url']))

## Filter out the title nick data and write to the player title file
title_fh = open(config['player_title_file'], 'w')
title_w = csv.writer(title_fh, delimiter=' ', quoting=csv.QUOTE_NONE)
title_count = 0
for t in config['title_names']:
    if t in titles:
        title_w.writerow([t] + titles[t])
        title_count += 1
title_fh.close()
if not args.quiet:
    print("Wrote {0} title entries to {1}".format(title_count,
                                                  config['player_title_file']))

## Tell WebTiles server to reload the player titles
if pid:
    if not args.quiet:
        print("Sending SIGUSR2 to WebTiles process {0}".format(pid))
    try:
        os.kill(pid, signal.SIGUSR2)
    except:
        print(("Warning: Could not send SIGUSR2 to WebTiles process id "
               "{0}".format(pid)), file=sys.stderr)
        
