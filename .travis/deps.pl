#!/usr/bin/env perl
use strict;
use warnings;

if ($ENV{BUILD_ALL}) {
    retry(qw(git submodule update --init --recursive));
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
