###############################################################################
# dbm-empty.py
# Remove values from dbm
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


if len(sys.argv) != 2:
    sys.stderr.write("Usage: dbm-empty.py <file>\n")
    sys.exit(1)

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
            write_entry(key, "")
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
    write_entry(key, "")
    outfile.write("%%%%\n");
