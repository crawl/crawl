#!/usr/bin/env perl

use strict;
use warnings;

use File::Basename;

my $scriptpath = dirname($0);
my $outfile = $ARGV[0];
my $mergebase = $ARGV[1];

$mergebase or $mergebase = "";

mkdir dirname($outfile);

# to build without git or with a shallow clone, you will need to generate
# the file `release_ver` somehow with version information. This file is just
# intended to contain the output of `git describe`, and anything that
# conforms to the regex below is fine. For example, "0.28-a" would be safe.
#
# Source tarbells distributed as part of a release include this file already
# generated with the release version.
$_ = `git describe $mergebase 2> /dev/null`
    || (open(IN, "<", "$scriptpath/release_ver") ? <IN>
        : die "Can't get version information: `git describe` failed (no git, no repository, or shallow clone), and $scriptpath/release_ver doesn't exist.\n")
    or die "couldn't get the version information\n";

chomp;

/v?(([0-9]+\.[0-9]+)(?:\.[0-9]+)?(?:-([a-zA-Z]+[0-9]+))?)(?:-[0-9]+-g[a-fA-F0-9]+)?/
    or die "Version string '$_' is malformed.\n";

my ($major, $tag, $pretyp) = ($2, $1, $3);

my $rel = defined($pretyp) ? $pretyp le "b" ? "ALPHA" : "BETA" : "FINAL";

my $prefix = "CRAWL";

open OUT, ">", "$outfile" or die $!;
print OUT <<__eof__;
#define ${prefix}_VERSION_MAJOR "$major"
#define ${prefix}_VERSION_RELEASE VER_$rel
#define ${prefix}_VERSION_SHORT "$tag"
#define ${prefix}_VERSION_LONG "$_"
__eof__
close OUT or die $!;
