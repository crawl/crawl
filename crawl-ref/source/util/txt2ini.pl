#!/usr/bin/env perl
# Generate an ini file from a txt file (database or description)
# Use option -u unwrap the file (remove all newlines except if the next line
# starts with a space).

use strict;
use warnings;
use File::Basename;
use Getopt::Std;
if ($^O ne 'msys') {
    use open ':encoding(utf8)';
}

die "Usage: $0 description_files\n" unless (@ARGV);

getopts('u');
our($opt_u);

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
                if ($opt_u or /^\s*$/ or /^\s+/ or substr($value,-2) eq '\n') {
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
