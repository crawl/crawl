#!/usr/bin/perl

use strict;
use warnings;
use File::Basename;
use open ':encoding(utf8)';

die "Usage: $0 ini_files\n" unless (@ARGV);

main();

sub main {
   foreach my $file (@ARGV) {
        my $basename = basename($file, '.ini');
        open IN, $file;
        open OUT, ">$basename.txt";
        while (<IN>) {
            my ($key, $value) = /(.*?)=(.*)/;
            print OUT "%%%%\n";
            print OUT "$key\n\n";
            $value =~ s/\\n/\n/g;
            print OUT $value, "\n";
        }
        print OUT "%%%%\n";
        close OUT;
   }
}
