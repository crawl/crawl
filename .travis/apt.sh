#!/bin/bash

retry () {
    local cmd="$@"
    until $cmd; do
        sleep 5
    done
}

retry add-apt-repository ppa:ubuntu-toolchain-r/test -y
retry add-apt-repository ppa:zoogie/sdl2-snapshots -y

retry apt-get update -qq

if [ "$CXX" = "clang++" ]; then
    retry apt-get install -qq libstdc++-4.8-dev
elif [ "$CXX" = "g++" ]; then
    retry apt-get install -qq g++-4.8
    export CXX="g++-4.8" CC="gcc-4.8"
fi
