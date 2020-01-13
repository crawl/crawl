#!/usr/bin/env python

import argparse
import os
import subprocess
import sys

parser = argparse.ArgumentParser(description="Install packages required to build DCSS")
parser.add_argument("--coverage", action="store_true")
parser.add_argument("--tiles", action="store_true")

args = parser.parse_args()

packages = set(
    [
        "build-essential",
        "libncursesw5-dev",
        "bison",
        "flex",
        "liblua5.1-0-dev",
        "libsqlite3-dev",
        "libz-dev",
        "pkg-config",
        "python-yaml",
        "ccache",
        "git",
    ]
)

if args.coverage:
    packages.add("gcov")

if args.tiles:
    packages.update(
        "libsdl2-image-dev",
        "libsdl2-mixer-dev",
        "libsdl2-dev",
        "libfreetype6-dev",
        "libpng-dev",
        "ttf-dejavu-core",
    )

cmd = ["apt-get", "install"]

if os.geteuid() != 0:
    cmd.insert(0, "sudo")

cmd.extend(packages)

rc = subprocess.call(cmd)
sys.exit(rc)
