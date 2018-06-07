import os, sys

def extract_block(block):
    name_candidates = [i for i in block if i.startswith('enum')]
    if not name_candidates:
        return
    name_candidates = name_candidates[0].split(' ')
    name = [i for i in name_candidates if i not in ('enum', 'class')][0].strip()
    assert(name)
    name_slug = name.replace('_', '-')
    print("Creating %s.h" % name_slug)
    if os.path.exists('%s.h' % name_slug):
        return
    f = open('%s.h' % name_slug, 'w')
    f.writelines(['#pragma once\n', '\n'])
    f.writelines(block)

#f = open('test.cc')
f = open('enum.h')
mode = None
skip = 0
lines = f.readlines()
for n in range(len(lines)):
    if skip:
        skip -= 1
        continue
    line = lines[n]
    # print('Processing line %s: %r' % (n, line))
    if mode == None and line.strip():
        # print("Starting a block")
        mode = 'collecting_block'
        current_block = []
        current_block.append(line)
    elif mode == 'collecting_block':
        current_block.append(line)
        if line.startswith('};'):
            # Snarf up all lines until the next whitespace
            while True:
                if (n+skip+1) < len(lines) and lines[n+skip+1].strip():
                    # print("Adding trailing line to block: %r" % lines[n+1])
                    current_block.append(lines[n+skip+1])
                    skip += 1
                else:
                    break
            # print("Here's a block: %r" % current_block)
            extract_block(current_block)
            del current_block
            mode = None
