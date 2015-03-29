#!/bin/sh

set -eu

retry () {
    local cmd=$1
    shift
    $cmd $* || (sleep 10 && $cmd $*) || (sleep 10 && $cmd $*)
}

add-apt-repository ppa:ubuntu-toolchain-r/test -y
add-apt-repository ppa:zoogie/sdl2-snapshots -y

retry apt-get update -qq

if [ "$CXX" = "clang++" ]; then
    retry apt-get install -qq libstdc++-4.8-dev
elif [ "$CXX" = "g++" ]; then
    retry apt-get install -qq g++-4.8
    export CXX="g++-4.8" CC="gcc-4.8"
fi
