#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;
use open ':encoding(utf8)';

die "Usage: $0 description_files\n" unless (@ARGV);

foreach my $file (@ARGV) {
    my ($basename,$path) = fileparse($file, '.txt');
    open IN, $file or die "Cannot open $file";
    open OUT, ">$path/$basename.ini";
    my ($key, $value);
    while(<IN>) {
        next if (/^#/);
        chomp;
        if (/^%%%%$/) {
            print OUT "$key=$value\n" if ($key);
            $key = "";
            $value = "";
        }
        elsif ($key) {
            if ($value) {
                if (/^\s*$/ or /^\s+/ or substr($value,-2) eq '\n') {
                    $value .= '\n';
                } else {
                    $value .= " ";
                }
            }
            $value .= $_;
        } else {
            $key = $_;
        }
    }
    close IN;
    close OUT;
}
