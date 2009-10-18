#!/usr/bin/perl
#
# Simple GCC version check
#
# I have a feeling this could be better-implemented, because
# it seems like the regex results should be available in an
# array (i.e $RESULT[1] == $1 or something), but whatever.
# But for now, this works fine. - Steven
#

use strict;
use warnings;

my $gcc = $ARGV[0];
my $min = $ARGV[1];

if ( `which $gcc 2> /dev/null` ) {
} else {
	die "Can't detect GCC version ($gcc is missing?)\n";
}

my $local = `$gcc -dumpversion`;
my $pattern = "([0-9]+).([0-9]+).([0-9]+)";

if ($local =~ $pattern) {
} else {
	die "Version '$local' is malformed.\n";
}

my $local_major = $1;
my $local_minor = $2;
my $local_patch = $3;

if ($min =~ $pattern) {
} else {
	die "Version '$min' is malformed.\n";
}

my $min_major = $1;
my $min_minor = $2;
my $min_patch = $3;

if ($local_major > $min_major) {
	print "1";
	exit 0;
}

if ($local_major < $min_major) {
	print "0";
	exit 0;
}

if ($local_minor > $min_minor) {
	print "1";
	exit 0;
}

if ($local_minor < $min_minor) {
	print "0";
	exit 0;
}

if ($local_patch < $min_patch) {
	print "0";
	exit 0;
}

print "1";
exit 0;
