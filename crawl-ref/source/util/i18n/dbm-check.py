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

data = {}
for file_name in sys.argv[1:]:
    dbm.read_dbm(file_name, data)


