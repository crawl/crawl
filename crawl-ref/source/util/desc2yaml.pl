#!/usr/bin/perl

use strict;
use warnings;
use YAML ();
use File::Basename;

die "Usage: $0 description_files\n" unless (@ARGV);

main();

sub main {
   foreach (@ARGV) {
        my $basename = basename($_, '.txt');
        open OUT, ">$basename.yaml";
        my %DESCRIPTIONS;
        load_file($_, \%DESCRIPTIONS);
        print OUT YAML::Dump({en_AU => \%DESCRIPTIONS});
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
                if (/^\s*$/ or /^\s+/ or substr($value,-1) eq "\n") {
                    $value .= "\n";
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
