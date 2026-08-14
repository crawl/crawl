###############################################################################
# dbm-check.py
# Check dbm
###############################################################################

import re
import sys

import dbm

#####################################
# Main
#####################################

if len(sys.argv) < 2:
    sys.stderr.write("Usage: dbm-check.py <files>\n")
    sys.exit(1)

for file_name in sys.argv[1:]:
    data = {}
    dbm.read_dbm(file_name, data)
    empties = [key for key, value in data.items() if value == ""]
    print("{}: {} entries, {} empty values".format(file_name, len(data), len(empties)))
    if len(empties) < 100:
        for key in empties:
            print("  " + key)
