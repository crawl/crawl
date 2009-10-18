#!/usr/bin/perl
#
# GCC optimization flag generator
#

use strict;
use warnings;

my $gcc = $ARGV[0];

if ( ! $gcc ) {
	die "Can't generate optimization flags (no compiler specified)\n";
}

if ( ! `which $gcc 2> /dev/null` ) {
	die "Can't generate optimization flags ($gcc is missing?)\n";
}


#
# Detect architecture
#

my $arch = `uname -m`;


# Intel x86
#
# Matches ix86, i386, i486, i586, i686
#
my $pattern = "i([x3-6])86";
if ($arch =~ $pattern) {
	system("util/gcc-opt-x86.pl $gcc");
	exit 0
}


exit 0
