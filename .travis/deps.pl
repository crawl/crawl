#!/usr/bin/env perl
use strict;
use warnings;

if ($ENV{BUILD_ALL}) {
    my @deps = qw(
        libasound2-dev
    );

    exec "sudo apt-get install @deps";
    exec "git submodule update --init --recursive";
}
else {
    my @deps = qw(
        liblua5.1-0-dev
    );

    push @deps, qw(
        libsdl1.2-dev
        libsdl-image1.2-dev
    ) if $ENV{TILES} || $ENV{WEBTILES};

    exec "sudo apt-get install @deps";
}
