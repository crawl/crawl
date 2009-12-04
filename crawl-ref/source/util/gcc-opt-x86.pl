#!/usr/bin/env perl
#
# GCC optimization flag generator
#

use strict;
use warnings;

my $gcc = $ARGV[0];
my $cpuinfo = $ARGV[1];

if ( ! $gcc ) {
	die "Can't generate optimization flags (no compiler specified)\n";
}

if ( ! `which $gcc 2> /dev/null` ) {
	die "Can't generate optimization flags ($gcc is missing?)\n";
}

#
# We allow people to provide their own cpuinfo-compatible
# descriptor. This may be useful later if we want to have
# specific architecture-specific optimizations for release
# builds or something.
#
if ( ! $cpuinfo ) {
	$cpuinfo = "/proc/cpuinfo";
}

my %features;
my ($family, $model);
my $uname_M = `uname -m`;
my $uname_P = `uname -p`;
my $uname_S = `uname -s`;

#
# Collect CPU feature list
#

# The Linux way
if ( -e $cpuinfo ) {
	open(FH, "< $cpuinfo");
	my @cpuinfo = <FH>;
	close FH;

	my @line = grep(/^cpu family/, @cpuinfo);
	$line[0] =~ /([0-9]*)$/;
	$family = $1;

	@line = grep(/^model/, @cpuinfo);
	$line[0] =~ /([0-9]*)$/;
	$model = $1;

	my @flags = grep(/^flags/, @cpuinfo);
	$flags[0] =~ s/^flags[ \t]*[:][ \t]*//;
	@flags = split(' ',$flags[0]);
	foreach (@flags) {
		$features{$_} = 1;
	}
}

# The Mac OS X way
if ( $uname_S =~ /^Darwin/ ) {
	# All released Intel macs have CMOV, and
	# sysctl doesn't provide this one.
	$features{"cmov"} = 1;

	$features{"mmx"} = `sysctl -n hw.optional.mmx`;
	$features{"sse"} = `sysctl -n hw.optional.sse`;
	$features{"sse2"} = `sysctl -n hw.optional.sse2`;
	$features{"pni"} = `sysctl -n hw.optional.sse3`;
	$features{"ssse3"} = `sysctl -n hw.optional.supplementalsse3`;
}

# TODO architectures:
# * athlon-4, athlon-xp, athlon-mp
# * k8, opteron, athlon64, athlon-fx
# * k8-sse3, opteron-sse3, athlon64-sse3
# * winchip-c6, winchip2

my $gcc_gte_4_2_0 = `util/gcc-gte.pl $gcc 4.2.0`;
my $gcc_gte_4_3_0 = `util/gcc-gte.pl $gcc 4.2.0`;
my $gcc_gte_4_5_0 = `util/gcc-gte.pl $gcc 4.5.0`;

#
# Check the minimum march/mtune value
#
my $march = "i386";
my $fpmath;

if ($uname_M eq "i586" || $uname_M eq "i686")
{
	# Pentium and PentiumPro
	$march = $uname_M;
}
if ( $uname_M eq "i586" && $features{"mmx"} )
{
	$march = "pentium-mmx";
}
if ( $features{"cmov"} && $features{"mmx"} ) {
	$march = "pentium2";
}
if ( $features{"sse"} ) {
	$march = "pentium3";
	$fpmath = "sse"
}
if ( $features{"sse2"} ) {
	$march = "pentium-m";
}
if ( $features{"pni"} ) { # a.k.a. SSE3
	$march = "prescott";
}
if ( $features{"ssse3"} ) {
	if ( $gcc_gte_4_3_0 ) {
		$march = "core2";
	} else {
		$march = "nocona";
	}
}
if ( $gcc_gte_4_5_0 && $features{"movbe"} ) {
	$march = "atom";
}
if ( $uname_P =~ /Athlon/ )
{
	$march = "athlon";

	if ( $features{"abm"} && $features{"sse4a"} )
	{
		$march = "amdfam10";
	}
}

# It's important to specify 'march=pentium4' for the
# Pentium 4, because it has vastly different optimization
# rules than other x86 processors. What's optimal for the
# Pentium 3 is extremely sub-optimal for the Pentium 4.
if ( $family == 15 ) {
	$march = "pentium4";
}

print "-march=$march -mtune=$march";
if ( $fpmath ) {
	print " -mfpmath=$fpmath";
}

print "\n";

exit 0
