#!/usr/bin/env python3

"""Check the Debian changelog and add an automatic entry if needed."""

import sys
import re
import datetime

if __name__ == "__main__":

    if len(sys.argv) != 3:
        sys.exit("Usage: {} path/to/changelog version".format(sys.argv[0]))

    filename = sys.argv[1]
    version = sys.argv[2]
    if not re.compile("^\d+\.\d+\.\d+$").match(version):
        sys.exit("Invalid version: {}".format(version))

    with open(filename, 'r', encoding='utf-8') as file:
        changelog = file.readlines()

    if not re.compile("^crawl \(2:"+version+"-\d\).*$").match(changelog[0]):
        print("Adding changelog entry for version {}".format(version))

        now = datetime.datetime.utcnow()
        timestamp = now.strftime("%a, %d %b %Y %R:%S ")+"+0000"

        changelog.insert(0, "\n")
        changelog.insert(0, " -- Crawl bot <bot@crawl.develz.org>  {}\n".format(timestamp))
        changelog.insert(0, "  * New upstream release.\n")
        changelog.insert(0, "\n")
        changelog.insert(0, "crawl (2:{}-1) unstable; urgency=low\n".format(version))

        with open(filename, 'w', encoding='utf-8') as file:
            file.writelines(changelog)

    else:
        print("Changelog already on version {}".format(version))
