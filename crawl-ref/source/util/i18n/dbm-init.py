###############################################################################
# dbm-init.py
# Initialise an empty dbm from a list of keys
# optionally, import values from old file
###############################################################################

import os
import re
import sys

def strip_quotes_if_allowed(line):
    if len(line) > 2 and line[0] == '"' and line[-1] == '"':
        if line[1] not in ' \t' and line[-2] not in ' \t':
            if not (line[1] == '"' and line[-2] == '"'):
                return line[1:-1]
    return line

def add_entry(dbm, key, value):
    key = strip_quotes_if_allowed(key)
    value = strip_quotes_if_allowed(value)
    if key in dbm:
        print('WARNING: multiple definitions for: "{0}"'.format(key))
    else:
        dbm[key]=value


def read_dbm(filename):
    dbm = {}
    file = open(filename, "r")

    key = ""
    value = ""

    for line in file:
        line = line.strip()
        if re.match(r'^\s*$', line) or re.match(r'^\s*#', line):
            # blank line or comment
            continue
        elif line == "%%%%":
            if key != "":
                add_entry(dbm, key, value)
            key = ""
            value = ""
        elif key == "":
            key = line
        elif value == "":
            value = line
        else:
            value += "\n"
            value += line

    if key != "":
        add_entry(dbm, key, value)

    return dbm;


#####################################
# Main
#####################################

if len(sys.argv) < 2 or len(sys.argv) > 4:
    sys.stderr.write("Usage: dbm-init.py <keyfile> [<old values>] [<ignore file>]\n")
    sys.exit(1)

old_vals = {}
if len(sys.argv) >= 3:
    old_vals = read_dbm(sys.argv[2])

ignore_vals = {}
if len(sys.argv) == 4:
    ignore_vals = read_dbm(sys.argv[3])


keyfile_name = sys.argv[1]
keyfile = open(keyfile_name)
outfile = open("dbm.txt", "w")

in_entry = False
locnote = ""
source_heading = ""
for line in keyfile:
    if re.match(r'^\s*$', line) or re.match(r'^\s*#', line):
        # blank line or comment
        if line.startswith('# section:'):
            locnote = ''
            continue
        elif line.startswith('# duplicate:'):
            #locnote = ''
            continue
        elif line.startswith('# note:'):
            locnote = line
            continue
        if in_entry:
            outfile.write("%%%%\n");
            in_entry = False
        if '##################' in line:
            locnote = ''
            if source_heading.count('##################') > 1:
                # don't output the source file name if no strings from file in output
                source_heading = ""
            source_heading += line
            continue
        elif source_heading != "":
            source_heading += line
            continue
        outfile.write(line)
    else:
        key = strip_quotes_if_allowed(line.strip())
        if key in ignore_vals:
            locnote = ''
            continue
        if source_heading != "":
            outfile.write(source_heading)
            source_heading = ""
        outfile.write("%%%%\n");
        if locnote != "":
            outfile.write(locnote)
            locnote = ""
        outfile.write(key + "\n")
        if key in old_vals:
            outfile.write(old_vals[key])
        elif key + '%s' in old_vals:
            old = old_vals[key + '%s']
            outfile.write(old[0:-2])
        outfile.write("\n");
        in_entry = True

if in_entry:
    outfile.write("%%%%\n");
