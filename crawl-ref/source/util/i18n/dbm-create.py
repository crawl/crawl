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

if len(sys.argv) != 3:
    sys.stderr.write("Usage: make-dbm.py <keyfile> <valuefile>\n")
    sys.exit(1)

keyfile_name = sys.argv[1]
valuefile_name = sys.argv[2]

keyfile = open(keyfile_name)
valuefile = open(valuefile_name)
outfile = open("dbm.txt", "w")

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
    
