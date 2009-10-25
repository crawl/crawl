#!/usr/bin/perl

use strict;
use warnings;

use File::Basename;

use Cwd;
my $cwd = cwd;

my $in_git = 1;
my $scriptpath = dirname($0);
my $outfile = $ARGV[0];
my $mergebase = $ARGV[1];

if (!$mergebase) {
	$mergebase = "";
}

my $releasever;

open IN, "<", "$scriptpath/release_ver";
read IN, $releasever, 32;
close IN;

mkdir dirname($outfile);

my $verstring = "";

$verstring = `git describe --tags --long $mergebase 2> /dev/null || git describe --tags $mergebase 2> /dev/null`;

if (!$verstring) {
	print STDERR "WARNING: Couldn't get revision information from Git. Using $scriptpath/release_ver.\n";
	$verstring = $releasever;
	$in_git = 0;
}

if (!$verstring) {
	die "couldn't get the version information\n";
}

chomp($verstring);

my $component_pattern = "[v]?([0-9]+)[.]([0-9]+)[.]([0-9]+)(?:[.]([0-9]+))?(?:(?:-(?:([a-zA-Z]+)([0-9]+)))?(?:-([0-9]+)?-g[a-fA-F0-9]+)?)?";

if ($verstring =~ $component_pattern) {
} else {
	die "Version string '$verstring' is malformed...\n";
}

my $major  = $1;
my $minor  = $2;
my $revis  = $3;
my $build  = $4;
my $pretyp = $5;
my $prenum = $6;
my $commit = $7;

# Git didn't give us a --long format?
if ( !$commit ) {
	$commit = 0;
}

# Final releases don't have a prenum.
if ( !$prenum ) {
	$prenum = 0;
}

# This gets us just the tag:
my $tag_pattern = "([v]?[0-9]+[.][0-9]+[.][0-9]+(?:[.][0-9]+)?(?:(?:-[a-zA-Z]+[0-9]+)?))";

if ($verstring =~ $tag_pattern) {
} else {
	die "Version string '$verstring' is malformed...\n";
}

my $tag    = $1;

# We assume here that we must be using a different
# version number convention.
if ( !$build ) {
	$build = $commit;
}

# Old versions of git omit the commits-since-tag number,
# so we can try 'git rev-list' to get this instead.
if ( $commit == 0 && $in_git ) {
	$commit = `git rev-list $tag..$mergebase | wc -l`
}

if ( $commit == 0 ) {
	# If we're at the tag, don't make the long
	# version longer than necessary.
	$verstring = $tag;
	if ( !$pretyp ) {
		$pretyp = "FINAL";
	}
}

if ( $verstring ne $tag || !$pretyp ) {
	$pretyp = "DEV";
} else {
	if ( $pretyp =~ /^a$/ ) {
		$pretyp = "ALPHA";
	}
	if ( $pretyp =~ /^b$/ ) {
		$pretyp = "BETA";
	}
	if ( $pretyp =~ /^rc$/ ) {
		$pretyp = "RC";
	}
}

my $OS        = `uname -s`;
my $OS_ver    = `uname -r`;
my $machine   = `uname -m`;
my $processor = `uname -p`;

chomp($OS);
chomp($OS_ver);
chomp($machine);
chomp($processor);

unlink("$outfile");

my $prefix   = "CRAWL";
my $smprefix = "crawl";

open OUT, ">", "$outfile" or die $!;
print OUT <<__eof__;
#ifndef __included_${smprefix}_build_number_h
#define __included_${smprefix}_build_number_h

#define ${prefix}_VERSION_MAJOR ${major}
#define ${prefix}_VERSION_MINOR ${minor}
#define ${prefix}_VERSION_REVISION ${revis}
#define ${prefix}_VERSION_BUILD ${build}
#define ${prefix}_VERSION_PREREL_TYPE ${pretyp}
#define ${prefix}_VERSION_PREREL_NUM ${prenum}
#define ${prefix}_VERSION_TAG "${tag}"
#define ${prefix}_VERSION_LONG "${verstring}"

#define ${prefix}_RESOURCE_VERSION ${major},${minor},${revis},${build}
#define ${prefix}_RESOURCE_VERSION_STRING "${major}, ${minor}, ${revis}, ${build}"

#define ${prefix}_BUILD_OS "${OS}"
#define ${prefix}_BUILD_OS_VER "${OS_ver}"
#define ${prefix}_BUILD_MACHINE "${machine}"
#define ${prefix}_BUILD_PROCESSOR "${processor}"

#endif

__eof__
close OUT or die $!;
