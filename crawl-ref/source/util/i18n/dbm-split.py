###############################################################################
# dbm-split.py
# Split dbm into keys and values
###############################################################################

import re
import sys

import dbm

#####################################
# Main
#####################################

if len(sys.argv) < 2:
    sys.stderr.write("Usage: dbm-check.py <input file> [[<key file>] <values file>]\n")
    sys.exit(1)

keyfile_name = "dbm-keys.txt"
valuefile_name = "dbm-values.txt"

infile_name = sys.argv[1];
if len(sys.argv) > 2:
    keyfile_name = sys.argv[2];
if len(sys.argv) > 3:
    valuefile_name = sys.argv[3];

data = {}
dbm.read_dbm(infile_name, data)

keyfile = open(keyfile_name, "w")
valuefile = open(valuefile_name, "w")

for key, value in data.items():
    keyfile.write(key)
    keyfile.write("\n")
    valuefile.write(value)
    valuefile.write("\n")
