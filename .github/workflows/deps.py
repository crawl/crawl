#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys
import shutil


def run(cmd):
    print("%s: Running '%s'..." % (sys.argv[0], " ".join(cmd)), file=sys.stderr)
    subprocess.check_call(cmd)


def build_opts(string):
    """Parse Make opts, eg "DEBUG=1 TILES=1" => {"DEBUG": "1", "TILES": "1"}."""
    if string:
        return {arg: val for arg, val in (opt.split("=") for opt in string.split(" "))}
    else:
        return {}

run(["sudo", "apt-get", "update"])

parser = argparse.ArgumentParser(description="Install packages required to build DCSS")
parser.add_argument("--compiler", choices=("gcc", "clang"))
parser.add_argument("--build-opts", default={}, type=build_opts)
parser.add_argument("--coverage", action="store_true")
parser.add_argument("--crosscompile", action="store_true")

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
    ]
)
if "TILES" in args.build_opts or "WEBTILES" in args.build_opts:
    packages.update(
        [
            "libsdl2-image-dev",
            "libsdl2-mixer-dev",
            "libsdl2-dev",
            "libfreetype6-dev",
            "libpng-dev",
            "ttf-dejavu-core",
        ]
    )
if args.coverage:
    packages.add("lcov")
if args.crosscompile:
    packages.add("mingw-w64")
if args.compiler == "clang":
    packages.update(["lsb-release", "wget", "software-properties-common"])

cmd = ["sudo", "apt-get", "install"]
cmd.extend(packages)

run(cmd)

if args.compiler == "clang":
    run(["wget", "-O", "/tmp/llvm.sh", "https://apt.llvm.org/llvm.sh"])
    run(["sudo", "bash", "/tmp/llvm.sh"])
    for binary in os.scandir("/usr/bin"):
        if binary.name.startswith("clang-") or binary.name.startswith("clang++-"):
            run(
                [
                    "sudo",
                    "ln",
                    "-s",
                    "/usr/bin/ccache",
                    os.path.join("/usr/lib/ccache/", binary.name),
                ],
            )

if args.crosscompile:
    run(
        ["sudo", "ln", "-s", "/usr/bin/ccache", "/usr/lib/ccache/i686-w64-mingw32-gcc"],
    )
    run(
        ["sudo", "ln", "-s", "/usr/bin/ccache", "/usr/lib/ccache/i686-w64-mingw32-g++"],
    )
