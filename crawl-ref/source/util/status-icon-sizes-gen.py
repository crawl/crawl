#!/usr/bin/env python3

"""
Generate status-icon-sizes.h, and status-icon-sizes.js

Works with both Python 2 & 3. If that changes, update how the Makefile calls
this.
"""

from __future__ import print_function

import sys

def parse_icon_sizes(file_name, icon_sizes):
    with open(file_name) as input_file:
        line_num = 0
        for line in input_file:
            line_num += 1
            trimmed_line = line.strip()
            if trimmed_line == '' or trimmed_line.startswith('#'):
                continue
            values = trimmed_line.split(':', 1)
            if len(values) < 2:
                print('Error: failed to parse line ', line_num, ' of ',
                      file_name, file=sys.stderr)
                return False

            icon_name = values[0].strip()
            icon_width = values[1].strip()
            if icon_width in icon_sizes:
                icon_sizes[icon_width].append(icon_name)
            else:
                icon_sizes[icon_width] = [icon_name]

    return True

def output_icon_sizes(icon_sizes, input_file_name):
    with open('rltiles/status-icon-sizes.h', 'w') as file:
        file.write('#pragma once\n')
        file.write('\n')
        file.write('// This file is auto generated, edit ' + input_file_name
                   + ' instead.\n')
        file.write('\n')
        file.write('#include "tiledef-icons.h"\n')
        file.write('\n')
        file.write('inline int status_icon_size(tileidx_t icon)\n')
        file.write('{\n')
        file.write('    switch (icon)\n')
        file.write('    {\n')
        for size, names in icon_sizes.items():
            for name in names:
                file.write('    case TILEI_' + name.upper() + ':\n')
            file.write('        return ' + size + ';\n')
        file.write('    default:\n')
        file.write('        return -1;\n')
        file.write('    }\n')
        file.write('}\n')

    with open('rltiles/status-icon-sizes.js', 'w') as file:
        file.write('define(["./tileinfo-icons"], function (icons) {\n')
        file.write('// This file is auto generated, edit ' + input_file_name
                   + ' instead.\n')
        file.write('\n')
        file.write('var exports = {};\n')
        file.write('\n')
        file.write('exports.status_icon_size = function (icon)\n')
        file.write('{\n')
        file.write('    switch (icon)\n')
        file.write('    {\n')
        for size, names in icon_sizes.items():
            for name in names:
                file.write('    case icons.' + name.upper() + ':\n')
            file.write('        return ' + size + ';\n')
        file.write('    default:\n')
        file.write('        return -1;\n')
        file.write('    }\n')
        file.write('}\n')
        file.write('\n')
        file.write('return exports;\n')
        file.write('});\n')

def main():
    if len(sys.argv) < 2:
        print('Error: Missing input file', file=sys.stderr)
        sys.exit(1)

    # List of icons by size
    icon_sizes = {}
    succeeded = parse_icon_sizes(sys.argv[1], icon_sizes)
    if not succeeded:
        sys.exit(1)

    output_icon_sizes(icon_sizes, sys.argv[1])

if __name__ == '__main__':
    main()
