###############################################################################
# dbm-sort.py
# Sort DBM by keys
###############################################################################

import re
import sys

import dbm

outfile = None

#####################################
# Main
#####################################


if len(sys.argv) != 2:
    sys.stderr.write("Usage: dbm-sort.py\n")
    sys.exit(1)

file_name = sys.argv[1]

key = ""
entry = ""
dbm = {}

with open(file_name) as infile:
    for line in infile:
        line = line.strip()
        if line == "%%%%":
            if key != "":
                dbm[key] = entry
            key = ""
            entry = ""
        else:
            entry += line + "\n"
            if key == "" and line != "" and not line.startswith("#"):
                key = line

if key != "":
    dbm[key] = entry

outfile = open("dbm.txt", "w")
for key, entry in sorted(dbm.items()):
    outfile.write("%%%%\n")
    outfile.write(entry)
outfile.write("%%%%\n")
outfile.close()
