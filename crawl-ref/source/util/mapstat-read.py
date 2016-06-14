#!/usr/bin/env python3

"""Load mapstats.*.log files and write a summary csv of their basic info."""

import os
import re
import sys

LINE_PAT = re.compile('^ .*,.*,.*:')
OUTPUT_CSV = 'mapstat.csv'


class MapStats(object):
    def __init__(self, successful, placed_incl_veto, tried):
        self.successful = successful
        self.placed_incl_veto = placed_incl_veto
        self.tried = tried

    def __add__(self, other):
        successful = self.successful + other.successful
        placed_incl_veto = self.placed_incl_veto + other.placed_incl_veto
        tried = self.tried + other.tried
        return MapStats(successful, placed_incl_veto, tried)


data = {}

files = []
for i in os.scandir():
    if i.is_file() and re.match('mapstat\..*\.log$', i.name):
        files.append(i.name)

if not files:
    print("No mapstat.*.log files found.")
    sys.exit(1)

for f in files:
    print("Parsing %s" % f)
    for line in open(f):
        if not line.strip():
            continue
        if not re.match(LINE_PAT, line):
            continue
        name = line.split(':')[1].strip()
        stats = MapStats(
            *[int(f.strip()) for f in line.split(':')[0].split(',')])
        if name not in data:
            data[name] = stats
        else:
            data[name] += stats


print("Writing %s" % OUTPUT_CSV)
with open(OUTPUT_CSV, 'w') as f:
    f.write('Name,Successful,Placed Including Veto,Tried\n')
    for name, stats in data.items():
        f.write('{},{},{},{}\n'.format(name, stats.successful,
                                       stats.placed_incl_veto, stats.tried))
