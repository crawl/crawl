###############################################################################
# make-dbm.py
# Make a dbm file from 2 input files, where the first file contains
# strings in the source language and the 2nd contains strings 
# in the target language
###############################################################################

import os
import re
import sys


#####################################
# Main
#####################################

if len(sys.argv) < 3 or len(sys.argv) > 4:
    sys.stderr.write("Usage: dbm-create.py <keyfile> <valuefile> [<output file>]\n")
    sys.exit(1)

keyfile_name = sys.argv[1]
valuefile_name = sys.argv[2]
outfile_name = "dbm.txt"
if len(sys.argv) > 3:
    outfile_name = sys.argv[3]

keyfile = open(keyfile_name)
valuefile = open(valuefile_name)
outfile = open(outfile_name, "w")

line1 = keyfile.readline()
line2 = valuefile.readline()

while line1 and line2:

    if re.match('^ *$', line1) or re.match('^ *#', line1):
        # blank line or comment
        outfile.write(line1)
    else:
        outfile.write("%%%%\n");
        outfile.write(line1)
        outfile.write(line2)

    # read next line
    line1 = keyfile.readline()
    line2 = valuefile.readline()

# close final entry
outfile.write("%%%%\n");
