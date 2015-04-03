#!/usr/bin/env perl
use strict;
use warnings;
use 5.010;

use Capture::Tiny 'capture';
use Path::Class;

my $file = file($ARGV[0]);
my @lines = $file->slurp;

my @remove;
for my $i (0..$#lines) {
    my $line = $lines[$i];
    if ($line =~ /^\s*#\s*include\s+"(.*)"/) {
        my $header = $1;
        next if $header eq 'AppHdr.h';
        $file->spew(join('', @lines[0..($i-1)], @lines[($i+1)..$#lines]));
        my $ret;
        capture {
            $ret = system("make -j5");
        };
        push @remove, $i unless $ret;
    }
}

for my $remove (reverse @remove) {
    splice(@lines, $remove, 1);
}
$file->spew(join('', @lines));
my $ret;
my $output = capture {
    $ret = system("make -j5");
};
print $output if $ret;
exit $ret;
