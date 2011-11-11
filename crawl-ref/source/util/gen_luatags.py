#!/usr/bin/env python

import os
import re
import sys

TAG_VERSION = "tag-version.h"
TAG_FINDER = re.compile("((?:NUM_)?TAG_MINOR[^, =]*)")

INITIAL = {"TAG_MINOR_INVALID": -1, "TAG_MINOR_RESET": 0}

TAG_OUTPUT = "dat/dlua/tags.lua"

TAG_OUTPUT_TEMPLATE = """------------------------------------------------------------------------------
-- tags.lua:
-- Tag and version compatibility functions and info.
------------------------------------------------------------------------------
tags = {}

------------------------------------------------------------------------------
-- simpler access to tag_major_version
------------------------------------------------------------------------------

function tags.tag_major_version ()
  return file.tag_major_version()
end"""

def uniquify (data):
    res = []

    for k in data:
        if k in res:
            continue

        res.append(k)

    return res

def main (args):
    """usage: gen_luatags.py"""

    CURRENT = INITIAL["TAG_MINOR_RESET"] + 1

    if not os.path.exists(TAG_VERSION):
        print main.__doc__
        print
        print "couldn't open tag-version.h for reading"
        return

    with open(TAG_VERSION) as fn:
        v_data = fn.read()

    taglist = uniquify(TAG_FINDER.findall(v_data))

    for tag in taglist:
        if INITIAL.has_key(tag):
            continue

        INITIAL[tag] = CURRENT
        CURRENT += 1

    # to reflect tag-version.h
    INITIAL["TAG_MINOR_VERSION"] = INITIAL["NUM_TAG_MINORS"] - 1

    with open(TAG_OUTPUT, "w") as fn:
        fn.write(TAG_OUTPUT_TEMPLATE+"\n\n")

        for tag in taglist:
            fn.write("tags.%s = %s\n" % (tag, INITIAL[tag]))

        fn.write("\n")

    print "Done!"

if __name__=="__main__":
    main(sys.argv)
