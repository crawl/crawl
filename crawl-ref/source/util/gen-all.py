#!/usr/bin/env python3

from __future__ import print_function

import glob
import os
import shutil
import subprocess
import sys

def get_min_file_modified_time(file_names):
    return min([os.path.getmtime(file_name) for file_name in file_names])

def get_max_file_modified_time(file_names):
    return max([os.path.getmtime(file_name) for file_name in file_names])

def needs_running(generated_files, input_files):
    inputs_last_modified = get_max_file_modified_time(input_files)
    needs_to_run = False
    try:
        output_last_modified = get_min_file_modified_time(generated_files)
        needs_to_run = inputs_last_modified >= output_last_modified
    except OSError:
        needs_to_run = True
    return needs_to_run

def run_if_needed(generated_files, input_files, command):
    needs_to_run = needs_running(generated_files, input_files)
    if(not needs_to_run):
        return

    result = subprocess.call(command)
    if(result != 0):
        sys.exit(result)

def main():
    perl = shutil.which('perl')
    if not perl:
        print('Error: no perl installed', file=sys.stderr)
        sys.exit(1)
    python = sys.executable

    ##########################################################################
    # Based on lines 1464-1481 in Makefile
    #
    generated_files = ['../docs/aptitudes.txt']
    input_files = ['util/gen-apt.pl', '../docs/template/apt-tmpl.txt',
        'species-data.h', 'aptitudes.h']
    command = [perl, input_files[0]] + generated_files + input_files[1:]
    run_if_needed(generated_files, input_files, command)

    generated_files = ['../docs/aptitudes-wide.txt']
    input_files = ['util/gen-apt.pl', '../docs/template/apt-tmpl-wide.txt',
        'species-data.h', 'aptitudes.h']
    command = [perl, input_files[0]] + generated_files + input_files[1:]
    run_if_needed(generated_files, input_files, command)

    generated_files = ['../docs/FAQ.html']
    input_files = ['util/FAQ2html.pl', 'dat/database/FAQ.txt']
    command = [perl] + input_files + generated_files
    run_if_needed(generated_files, input_files, command)

    # Generate a .txt version because some servers need it
    # TODO: actually render this
    generated_files = ['../docs/quickstart.txt']
    input_files = ['../docs/quickstart.md']
    if needs_running(generated_files, input_files):
        shutil.copyfile(input_files[0], generated_files[0])

    generated_files = ['../docs/crawl_manual.txt']
    input_files = ['util/unrest.pl', '../docs/crawl_manual.rst']
    if needs_running(generated_files, input_files):
        with open(generated_files[0], 'wb', 0) as file:
            command = [perl] + input_files
            result = subprocess.call(command, stdout=file)
            if(result != 0):
                sys.exit(result.returncode)

    ##########################################################################
    # Based on lines 1842-1876 in Makefile
    #
    generated_files = ['art-data.h', 'art-enum.h', 'rltiles/dc-unrand.txt',
        'rltiles/tiledef-unrand.cc']
    input_files = ['util/art-data.pl', 'art-data.txt', 'art-func.h', 'rltiles/dc-player.txt']
    command = [perl, input_files[0]]
    run_if_needed(generated_files, input_files, command)

    generated_files = ['mon-mst.h']
    input_files = ['util/gen-mst.pl', 'mon-spell.h', 'mon-data.h']
    command = [perl, input_files[0]]
    run_if_needed(generated_files, input_files, command)

    generated_files = ['cmd-name.h']
    input_files = ['util/cmd-name.pl', 'command-type.h']
    command = [perl, input_files[0]]
    run_if_needed(generated_files, input_files, command)

    generated_files = ['dat/dlua/tags.lua']
    input_files = ['util/gen-luatags.pl', 'tag-version.h']
    command = [perl, input_files[0]]
    run_if_needed(generated_files, input_files, command)

    generated_files = ['mi-enum.h']
    input_files = ['util/gen-mi-enum', 'mon-info.h']
    command = [perl, input_files[0]]
    run_if_needed(generated_files, input_files, command)

    generated_files = ['mon-data.h']
    input_files = (['util/mon-gen.py'] + glob.glob('dat/mons/*.yaml') +
        glob.glob('util/mon-gen/*.txt'))
    command = [python, input_files[0], 'dat/mons/', 'util/mon-gen/'] + generated_files
    run_if_needed(generated_files, input_files, command)

    generated_files = ['species-data.h', 'aptitudes.h', 'species-groups.h', 'species-type.h']
    input_files = (['util/species-gen.py'] + glob.glob('dat/species/*.yaml') +
        glob.glob('util/species-gen/*.txt'))
    command = [python, input_files[0], 'dat/species/', 'util/species-gen/'] + generated_files
    run_if_needed(generated_files, input_files, command)

    generated_files = ['job-data.h', 'job-groups.h', 'job-type.h']
    input_files = (['util/job-gen.py'] + glob.glob('dat/jobs/*.yaml') +
        glob.glob('util/job-gen/*.txt'))
    command = [python, input_files[0], 'dat/jobs/', 'util/job-gen/'] + generated_files
    run_if_needed(generated_files, input_files, command)

if __name__ == '__main__':
    main()
