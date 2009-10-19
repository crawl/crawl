#!/usr/bin/perl

use strict;
use warnings;

my $outfile = $ARGV[0];
my $cflags = $ARGV[1];
my $cflags_l = $ARGV[2];
my $ldflags = $ARGV[3];

$cflags =~ s/\"/\\"/g;
$cflags_l =~ s/\"/\\"/g;
$ldflags =~ s/\"/\\"/g;

while ($cflags =~ s/[ \t]{2,}/ /g) {}
while ($cflags_l =~ s/[ \t]{2,}/ /g) {}
while ($ldflags =~ s/[ \t]{2,}/ /g) {}

$cflags =~ s/^[ \t]+|[ \t]$//;
$cflags_l =~ s/^[ \t]+|[ \t]$//;
$ldflags =~ s/^[ \t]+|[ \t]$//;

my $prefix   = "CRAWL";
my $smprefix = "crawl";

open OUT, ">", "$outfile" or die $!;
print OUT <<__eof__;
#ifndef __included_${smprefix}_compile_flags_h
#define __included_${smprefix}_compile_flags_h

#define ${prefix}_CFLAGS "${cflags}"
#define ${prefix}_CFLAGS_L "${cflags_l}"
#define ${prefix}_LDFLAGS "${ldflags}"

#endif

__eof__
close OUT or die $!;
