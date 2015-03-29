#!/usr/bin/env perl
use strict;
use warnings;

run(qw(add-apt-repository ppa:ubuntu-toolchain-r/test -y));
run(qw(add-apt-repository ppa:zoogie/sdl2-snapshots -y));

retry(qw(apt-get update -qq));

if ($ENV{CXX} eq "clang++") {
    retry(qw(retry apt-get install -qq libstdc++-4.8-dev));
}
elsif ($ENV{CXX} eq "g++") {
    retry(qw(retry apt-get install -qq g++-4.8));
}

sub run {
    my @cmd = @_;

    my $ret = system(@cmd);
    exit($ret) if $ret;
    return;
}

sub retry {
    my (@cmd) = @_;

    my $tries = 5;
    my $sleep = 5;

    my $ret;
    while ($tries--) {
        $ret = system(@cmd);
        return if $ret == 0;
        print "'@cmd' failed ($ret), retrying in $sleep seconds...\n";
        sleep $sleep;
    }

    print "Failed to run '@cmd' ($ret)\n";
    exit($ret);
}
