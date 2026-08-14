###############################################################################
# dbm-empty.py
# Remove values from dbm
###############################################################################

import re
import sys

import dbm

outfile = ""

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

for line in infile:
    line = line.strip()
    if re.match(r'^$', line):
        if key == "":
            # blank line, and not part of entry
            outfile.write(line + '\n');
    elif line.startswith('#'):
        # comment
        outfile.write(line + '\n');
    elif line == "%%%%":
        outfile.write(line + '\n');
        key = ""
    elif key == "":
        key = line
        outfile.write(line + '\n\n');

outfile.close()
