#!/usr/bin/env perl
use strict;
use warnings;

my @deps = qw(
    gdb
);

if ($ENV{BUILD_ALL} || $ENV{CROSSCOMPILE}) {
    retry(qw(git fetch --unshallow));
    retry(qw(git submodule update --init --recursive));

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

retry(qw(sudo apt-get install), @deps);

sub retry {
    my (@cmd) = @_;

    my $tries = 5;
    my $sleep = 5;

    my $ret;
    while ($tries--) {
        print "Running '@cmd'\n";
        $ret = system(@cmd);
        return if $ret == 0;
        print "'@cmd' failed ($ret), retrying in $sleep seconds...\n";
        sleep $sleep;
    }

    print "Failed to run '@cmd' ($ret)\n";
    # 1 if there was a signal or system() failed, otherwise the exit status.
    exit($ret & 127 ? 1 : $ret >> 8);
}
