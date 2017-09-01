#!/usr/bin/env perl
use strict;
use warnings;

run(qw(sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y));
run(qw(sudo add-apt-repository ppa:zoogie/sdl2-snapshots -y));

retry(qw(sudo apt-get update -qq));

my @common_libs = qw(xorg-dev);

if ($ENV{CXX} eq "clang++") {
    retry(qw(sudo apt-get install -qq libstdc++6-4.7-dev), @common_libs);
}
elsif ($ENV{CXX} eq "g++") {
    retry(qw(sudo apt-get install -qq g++-4.7), @common_libs);
}

sub run {
    my @cmd = @_;

    print "Running '@cmd'\n";
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
