#!/usr/bin/env python3

"""
Works with both Python 2 & 3.
"""

import subprocess
import sys

if len(sys.argv) < 3:
    sys.stderr.write('Error: missing arguments\n')
    sys.exit(1)

cxx = sys.argv[1]
includesStr = sys.argv[2]

includes = includesStr.split(';')
includes = [x.strip() for x in includes]
includes = [x for x in includes if x]

arguments = '"' + cxx + '"'
if len(includes) != 0:
    arguments += ' /I "'
    arguments += '" /I "'.join(includes)
    arguments += '"'

python = sys.executable
command = [python, 'util/configure.py', arguments]
result = subprocess.call(command)
if(result != 0):
    sys.exit(result)
