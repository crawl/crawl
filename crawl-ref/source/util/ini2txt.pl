#!/usr/bin/env perl
# Generate or update a .txt file (database or description) from a .ini file
# (normally downloaded from transifex). If the .txt file already exists,
# the values of its keys found in the .ini will be updated, and the rest of the
# file will be preserved (comments and keys not in the .ini).
# This behaviour is desired for updating the original files in english.
# For translated files, use the -o option (overwrite). This way, keys deleted
# in the original files will also be deleted in translated files.
# Use the -w option to automatically wrap text at 80 columns (for description
# files, not databasa ones).

use strict;
use warnings;
use File::Basename;
use Getopt::Std;
use Text::WrapI18N qw(wrap);

if ($^O ne 'msys') {
    use open ':encoding(utf8)';
}

die "Usage: $0 ini_files\n" unless (@ARGV);

getopts('ow');
our($opt_o, $opt_w);

foreach my $file (@ARGV) {
    unless (-e $file) {
        print "$file not found\n";
        next;
    }
    my ($basename,$path) = fileparse($file, '.ini');
    my $out_file = "$path/$basename.txt";
    open IN, $file;
    if (-e $out_file and not $opt_o) {
        my $original_file = $out_file . "~";
        rename $out_file, $original_file;
        my %Text;
        while (<IN>) {
            chomp;
            next if (/^#/);
            my ($key, $value) = /(.*?)=(.*)$/;
            $value =~ s/\\n/\n/g;
            $value =~ tr/\r//d;
            $value = wrap("", "", $value) if $opt_w;

            # WrapIl8N adds spaces at the end of lines and don't support the
            # unexpand option.
            $value =~ s/ +$//mg;
            $value =~ s/\t/ {8}/mg;
            $Text{$key} = $value;
        }
        close IN;

        my $key;
        open IN, $original_file;
        open OUT, ">$out_file";
        while (<IN>) {
            chomp;
            my $new_text = $key && $Text{$key};
            next if (!/^%+$/ and $new_text);
            if (/^%+$/) {
                print OUT "\n", $Text{$key}, "\n" if ($new_text);
                $key = "";
            }
            elsif (!$key and !/^#/) {
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
            $value = wrap("", "", $value) if $opt_w;
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
