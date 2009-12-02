#!/usr/bin/env perl

use strict;
use warnings;

my $outfile = $ARGV[0];
my $cflags = $ARGV[1];
my $ldflags = $ARGV[2];

$cflags =~ s/\"/\\"/g;
$ldflags =~ s/\"/\\"/g;

while ($cflags =~ s/[ \t]{2,}/ /g) {}
while ($ldflags =~ s/[ \t]{2,}/ /g) {}

$cflags =~ s/^[ \t]+|[ \t]$//;
$ldflags =~ s/^[ \t]+|[ \t]$//;

my $prefix   = "CRAWL";
my $smprefix = "crawl";

open OUT, ">", "$outfile" or die $!;
print OUT <<__eof__;
#ifndef __included_${smprefix}_compile_flags_h
#define __included_${smprefix}_compile_flags_h

#define ${prefix}_CFLAGS "${cflags}"
#define ${prefix}_LDFLAGS "${ldflags}"

#endif

__eof__
close OUT or die $!;
