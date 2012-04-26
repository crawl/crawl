#!/usr/bin/perl

use strict;
use warnings;
use YAML ();
use File::Basename;
use open ':encoding(utf8)';

die "Usage: $0 yaml_files\n" unless (@ARGV);

main();

sub main {
   foreach (@ARGV) {
        my $basename = basename($_, '.yml');
        open OUT, ">$basename.txt";
        my $data = YAML::LoadFile($_)->{'en_AU'};
        foreach (sort keys %$data) {
            print OUT "%%%%\n";
            print OUT "$_\n\n";
            print OUT $data->{$_}, "\n";
        }
        print OUT "%%%%\n";
        close OUT;
   }
}
