#!/usr/bin/perl -w

use strict;

use File::Basename;

if (@ARGV != 1) {
    die "usage: $0 tile.png\n";
}

my $TILE  = shift;

unless (-f $TILE) {
    die "Couldn't find $TILE: $!\n";
}

my %orient = (
     90 => "w",
    180 => "n",
    270 => "e",
);

my ($name,$path) = fileparse($TILE, ".png");

for my $deg (90, 180, 270) {
	print `convert -rotate $deg $TILE $path/${name}_$orient{$deg}.png\n`
		or die "$!\n";
}

rename $TILE, "$path/${name}_s.png";
