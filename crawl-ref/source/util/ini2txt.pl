#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;
if ($^O ne 'msys') {
    use open ':encoding(utf8)';
}

die "Usage: $0 ini_files\n" unless (@ARGV);

foreach my $file (@ARGV) {
    my ($basename,$path) = fileparse($file, '.ini');
    my $out_file = "$path/$basename.txt";
    open IN, $file;
    if (-e $out_file) {
        my $original_file = $out_file . "~";
        rename $out_file, $original_file;
        my %Text;
        while (<IN>) {
            chomp;
            next if (/^#/);
            my ($key, $value) = /(.*?)=(.*)$/;
            $value =~ s/\\n/\n/g;
            $value =~ tr/\r//d;
            $Text{$key} = $value;
        }
        close IN;

        my $key;
        open IN, $original_file;
        open OUT, ">$out_file";
        while (<IN>) {
            chomp;
            my $new_text = $key && $Text{$key};
            next if (!/^%%%%$/ and $new_text);
            if (/^%%%%$/) {
                print OUT "\n", $Text{$key}, "\n" if ($new_text);
                $key = "";
            }
            elsif (!$key) {
                chomp ($key = $_);
            }
            print OUT "$_\n";
        }
        close IN;
        close OUT;
        unlink $original_file;
    } else {
        my $empty = 1;
        open OUT, ">$out_file";
        while (<IN>) {
            next if (/^#/);
            $empty = 0;
            my ($key, $value) = /(.*?)=(.*)/;
            print OUT "%%%%\n";
            print OUT "$key\n\n";
            $value =~ s/\\n/\n/g;
            print OUT $value, "\n";
        }
        close IN;
        if ($empty) {
            close OUT;
            unlink $out_file;
        } else {
            print OUT "%%%%\n";
            close OUT;
        }
    }
}
