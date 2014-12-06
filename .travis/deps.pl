#!/usr/bin/env perl
use strict;
use warnings;

my @deps = qw(
    gdb
);

if ($ENV{BUILD_ALL}) {
    system("git submodule update --init --recursive") == 0
        or die "Couldn't update submodules: $?";

    push @deps, qw(
       libegl1-mesa-dev
       libasound2-dev
       libxss-dev
    );
}
else {
    push @deps, qw(
        liblua5.1-0-dev
        liblua5.1-0-dbg
    );

    push @deps, qw(
        libsdl2-dev
        libsdl2-image-dev
        libsdl2-mixer-dev
    ) if $ENV{TILES} || $ENV{WEBTILES};
}

exec "sudo apt-get install @deps";
