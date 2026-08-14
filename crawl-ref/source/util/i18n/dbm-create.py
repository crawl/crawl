###############################################################################
# make-dbm.py
# Make a dbm file from 2 input files, where the first file contains
# keys, and the second file contains values
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

keys = []
values = []
multiline_values = False

with open(keyfile_name) as keyfile:
    for line in keyfile:
        line = line.rstrip('\n')
        if len(line) == 0 or line == '%%%%' or line.startswith('#'):
            continue
        keys.append(line)

separators = False
with open(valuefile_name) as valuefile:
     if '%%%%\n' in valuefile.read():
         separators = True

new_value = True
with open(valuefile_name) as valuefile:
    for line in valuefile:
        line = line.rstrip('\n')
        if len(line) == 0 or line.startswith('#'):
            continue
        if line == '%%%%':
            new_value = True
            continue
        if new_value:
            values.append(line)
            if separators:
                new_value = False
        else:
            values[-1] += '\n' + line
            multiline_values = True

outfile = open(outfile_name, "w")

for i, key in enumerate(keys):
    outfile.write('%%%%\n')
    outfile.write(key + '\n')
    if multiline_values:
        outfile.write('\n')
    if i < len(values):
        outfile.write(values[i])
    outfile.write('\n')

# close final entry
outfile.write("%%%%\n");
