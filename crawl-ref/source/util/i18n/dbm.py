###############################################################################
# dbm.py
# DBM methods
###############################################################################

import re
import sys

def strip_quotes_if_allowed(line):
    if len(line) > 2 and line[0] == '"' and line[-1] == '"':
        if line[1] not in ' \t' and line[-2] not in ' \t':
            if not (line[1] == '"' and line[-2] == '"'):
                return line[1:-1]
    return line

def add_entry(data, key, value):
    key = strip_quotes_if_allowed(key)
    value = strip_quotes_if_allowed(value)
    if key in data:
        print('WARNING: multiple definitions for: "{0}"'.format(key))
    else:
        data[key]=value


def read_dbm(filename, data = {}):

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
                add_entry(data, key, value)
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
        add_entry(data, key, value)

    return data

