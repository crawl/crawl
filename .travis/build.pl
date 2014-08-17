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

if ($ENV{FULLDEBUG} && !$ENV{TILES}) {
    try("make test");
}

sub try {
    my ($cmd) = @_;
    print "$cmd\n";
    my $exit = system $cmd;
    exit $exit if $exit;
}
