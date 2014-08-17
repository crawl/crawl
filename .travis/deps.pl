#!/usr/bin/env perl
use strict;
use warnings;

exit 0 if $ENV{BUILD_ALL};

my @deps = qw(
    liblua5.1-0-dev
);

push @deps, qw(
    libsdl1.2-dev
    libsdl-image1.2-dev
) if $ENV{TILES} || $ENV{WEBTILES};

exec "sudo apt-get install @deps";
