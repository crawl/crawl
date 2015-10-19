#!/usr/bin/env python
"""
usage: parse_tiles.py [options]

DESCRIPTION
    Attempt to find a vault-defined tiles for monsters and create a look-up
    table. Requires access to dc-mon.txt and monster-trunk.

OPTIONS:
    -v  --verbose       Print more information.
    -o  --output file   Output to file instead of the default.
    -h  --help          Print this text.

DEFAULTS:
    output              %s
    dc-mon.txt          %s
"""

import re, sys, parse_des, os, subprocess

DEFAULT_CRAWL_FOLDER = "crawl-ref/crawl-ref/source"
DEFAULT_OUTPUT = "tile_info.txt"
DC_MON_LOCATION = os.path.join(DEFAULT_CRAWL_FOLDER, "rltiles", "dc-mon.txt")

class TileParseError (Exception):
    """
    If an error is encountered while attempting to generate the list, this
    exception will be raised.
    """
    pass

GET_TILE = re.compile("tile:([^ ]*)")

def parse_tile_data (data):
    tiles = {}
    cur_dir = ""
    last_tile = ""

    for line in data:
        line = line.strip()

        if line.startswith("#"):
            continue
        elif line.strip() == "":
            continue
        elif line.startswith("%sdir"):
            cur_dir = line.split(" ", 1)[1]
        elif line.startswith("%"):
            continue

        if " " in line:
            fname, tile = line.split(" ", 1)
            fname = os.path.join(cur_dir, fname)
            tiles[tile] = fname+".png"
            last_tile = tile

    return tiles

def main (args):
    if "-h" in args or "--help" in args:
        print main.__doc__.lstrip()
        return

    if "-o" in args or "--output" in args:
        if "-o" in args:
            index = args.index("-o")
        elif "--output" in args:
            index = args.index("--output")
        args.pop(index)
        output_file=args.pop(index)
    else:
        output_file=DEFAULT_OUTPUT

    verbose = False
    if "-v" in args or "--verbose" in args:
        verbose = True

    try:
        assert os.path.exists(parse_des.DEFAULT_OUTPUT)
        assert os.path.exists("monster-trunk")
    except AssertionError:
        if verbose:
            raise

    tile_data_path = open(DC_MON_LOCATION)
    tile_data = parse_tile_data(tile_data_path.readlines())
    tile_data_path.close()


    if verbose:
        print "GEN %s" % output_file

    fn = open(parse_des.DEFAULT_OUTPUT)
    data = fn.readlines()
    fn.close()

    check_lines = []

    for line in data:
        if "tile:" in line:
            check_lines.append(line)

    output = open(output_file, "w")

    done = []

    for line in check_lines:
        line = line.split('"', 1)[1].split('"', 1)[0]
        result = subprocess.Popen(["./monster-trunk", line], stdout=subprocess.PIPE)
        name = result.stdout.read().split(" (", 1)[0].lower().replace("'", "")
        tile = GET_TILE.findall(line)[0].upper()
        if name in done:
            continue
        else:
            done.append(name)

        try:
            output.write("%s,%s\n" % (name, tile_data[tile]))
        except:
            pass
        else:
            print "GEN %s" % name

    output.close()

main.__doc__ = __doc__ % (DEFAULT_OUTPUT, DC_MON_LOCATION)

if __name__=="__main__":
    main (sys.argv)
