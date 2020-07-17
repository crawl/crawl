#!/usr/bin/env python3

"""Install DCSS dependencies in GitHub Actions CI."""

import argparse
import os
import subprocess
import sys
import time
from typing import Dict, List, Set


def run(cmd: List[str], max_retries: int = 1) -> None:
    attempt = 1
    while True:
        print("%s: Running '%s'..." % (sys.argv[0], " ".join(cmd)))
        try:
            subprocess.check_call(cmd)
        except Exception as e:
            print(
                "%s: Command failed (%s) (attempt %s of %s)"
                % (sys.argv[0], e, attempt, max_retries),
            )
            attempt += 1
            if attempt > max_retries:
                raise
            else:
                time.sleep(5)
        else:
            return


def make_opts(string: str) -> Dict[str, str]:
    """Parse Make opts, eg "DEBUG=1 TILES=1" => {"DEBUG": "1", "TILES": "1"}."""
    if string:
        return {arg: val for arg, val in (opt.split("=") for opt in string.split(" "))}
    else:
        return {}


def apt_update() -> None:
    run(["sudo", "apt-get", "update"], max_retries=5)


def _packages_to_install(args: argparse.Namespace) -> Set[str]:
    packages = {
        "build-essential",
        "libncursesw5-dev",
        "bison",
        "flex",
        "liblua5.1-0-dev",
        "libsqlite3-dev",
        "libz-dev",
        "pkg-config",
        "ccache",
        "advancecomp",  # used to compress release zips and png sprite sheets
    }
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
    if "FULLDEBUG" in args.debug_opts:
        packages.add("gdb")
    if args.coverage:
        packages.add("lcov")
    if args.crosscompile:
        packages.add("mingw-w64")
        packages.add("nsis")  # makensis used to build Windows installer
    if args.compiler == "clang":
        # dependencies for llvm.sh
        packages.update(["lsb-release", "wget", "software-properties-common"])
    return packages


def apt_install(args: argparse.Namespace) -> None:
    cmd = ["sudo", "apt-get", "install", *_packages_to_install(args)]
    run(cmd, max_retries=5)


def install_llvm() -> None:
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


def setup_msys_ccache_symlinks() -> None:
    run(
        [
            "sudo",
            "ln",
            "-s",
            "/usr/bin/ccache",
            "/usr/lib/ccache/i686-w64-mingw32-gcc",
        ],
    )
    run(
        [
            "sudo",
            "ln",
            "-s",
            "/usr/bin/ccache",
            "/usr/lib/ccache/i686-w64-mingw32-g++",
        ],
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Install packages required to build DCSS"
    )
    parser.add_argument("--compiler", choices=("gcc", "clang"))
    parser.add_argument("--build-opts", default={}, type=make_opts)
    parser.add_argument("--debug-opts", default={}, type=make_opts)
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--crosscompile", action="store_true")

    args = parser.parse_args()

    apt_update()
    apt_install(args)
    if args.compiler == "clang":
        install_llvm()
    if args.crosscompile:
        setup_msys_ccache_symlinks()
