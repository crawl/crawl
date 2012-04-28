#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;
use open ':encoding(utf8)';

die "Usage: $0 description_files\n" unless (@ARGV);

main();

sub main {
   foreach my $file (@ARGV) {
        my ($basename,$path) = fileparse($file, '.txt');
        my %DESCRIPTIONS;
        load_file($file, \%DESCRIPTIONS);
        open OUT, ">$path/$basename.ini";
        foreach (sort keys %DESCRIPTIONS) {
            print OUT $_, "=", $DESCRIPTIONS{$_}, "\n";
        }
        close OUT;
   }
}

sub load_file {
    my ($input, $hash) = @_;
    open IN, $input or die "Cannot open $input";
    my ($key, $value);
    while(<IN>) {
        next if (/^#/);
        chomp;
        if (/^%%%%$/) {
            if ($key) {
                $hash->{$key} = $value;
            }
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
}
