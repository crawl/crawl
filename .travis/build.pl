#!/usr/bin/env perl
use strict;
use warnings;

chdir "crawl-ref/source"
    or die "couldn't chdir: $!";

open my $fh, '>', "util/release_ver"
    or die "couldn't open util/release_ver: $!";
$fh->print("v0.0-a0");
$fh->close;

$ENV{TRAVIS} = 1;

# can't set these in .travis.yml because env vars are set before compiler
# selection
$ENV{FORCE_CC} = $ENV{CC};
$ENV{FORCE_CXX} = $ENV{CXX};

try("make -j2");

if (!$ENV{TILES}) {
    if ($ENV{FULLDEBUG}) {
        try("make test");
    }
    else {
        try("make nondebugtest");
    }
}

sub try {
    my ($cmd) = @_;
    print "$cmd\n";
    if (system $cmd) {
        if ($? == -1) {
            print "failed to execute '$cmd': $!\n";
            exit 1;
        }
        elsif ($? & 127) {
            printf "'$cmd' died with signal %d", ($? & 127);
            exit 1;
        }
        elsif ($?) {
            my $exit = $? >> 8;
            printf "'$cmd' returned exit value %d", $exit;
            exit $exit;
        }
    }
}

sub error {
    my ($exitcode) = @_;
    system "cat morgue/crash-*.txt";
    exit $exitcode;
}
