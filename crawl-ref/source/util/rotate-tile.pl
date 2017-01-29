#!/usr/bin/env perl

use warnings;
use strict;

use File::Basename;

die "usage: $0 tile.png\n" unless @ARGV == 1;

my $TILE  = shift;

die "Couldn't find $TILE: $!\n" unless -f $TILE;

my %orient =
(
     90 => "w",
    180 => "n",
    270 => "e",
);

my ($name,$path) = fileparse($TILE, ".png");

for my $deg (90, 180, 270)
{
    print `convert -rotate $deg $TILE $path/${name}_$orient{$deg}.png\n`
        or die "$!\n";
}

rename $TILE, "$path/${name}_s.png";
