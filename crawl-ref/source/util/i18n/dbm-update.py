###############################################################################
# dbm-update.py
# Update DBM with new values
###############################################################################

import re
import sys

import dbm

outfile = ""
new_vals = {}

def write_entry(key, value):
    key = dbm.strip_quotes_if_allowed(key)
    value = dbm.strip_quotes_if_allowed(value)
    outfile.write(key + '\n')
    if key in new_vals:
        outfile.write(new_vals[key])
    else:
        outfile.write(value)
    outfile.write('\n')
    #outfile.write('%%%%\n');

#####################################
# Main
#####################################


if len(sys.argv) < 2 or len(sys.argv) > 3:
    sys.stderr.write("Usage: dbm-update.py <file> [<new file>]\n")
    sys.exit(1)

if len(sys.argv) == 3:
    new_vals = dbm.read_dbm(sys.argv[2])


file_name = sys.argv[1]
infile = open(file_name)
outfile = open("dbm.txt", "w")

key = ""
value = ""

for line in infile:
    line = line.strip()
    if re.match(r'^\s*$', line) or re.match(r'^\s*#', line):
        # blank line or comment
        outfile.write(line + '\n');
    elif line == "%%%%":
        if key != "":
            write_entry(key, value)
        outfile.write(line + '\n')
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
    write_entry(data, key, value)
    outfile.write("%%%%\n");
