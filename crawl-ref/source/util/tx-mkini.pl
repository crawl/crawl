#!/usr/bin/env perl
# Generate a .ini file (to be pushed to transifex)
# from a .txt file (database or description)
# Use option -u unwrap the file (remove all newlines except if the next line
# starts with a space). This option is meant to be used for description files
# but not for database files
# -d: specify output file or directory
# with the -m (merge) option, a .ini pulled from transifex is necessary.
# Commented text is read and compated to the one found in the .txt. If they
# differ, the text from the .ini is updated and uncommented. This is useful to
# updated the english fake translations on transifex when they have been changed
# in git.

use strict;
use warnings;
use File::Basename;
use Getopt::Std;
if ($^O ne 'msys') {
    use open ':encoding(utf8)';
}

die "Usage: $0 description_files\n" unless (@ARGV);

getopts('d:mu');
our($opt_d, $opt_m, $opt_u);

foreach my $file (@ARGV) {
    unless (-e $file) {
        print "$file not found\n";
        next;
    }
    my ($basename,$path) = fileparse($file, '.txt');
    my $out_file = "$path/$basename.ini";
    if ($opt_d) {
        if (-d $opt_d) {
            $out_file = "$opt_d/$basename.ini";
        } else {
            $out_file = $opt_d;
        }
    }

    my %Original;
    if ($opt_m) {
        open INI, $out_file or next;
        while (<INI>) {
            chomp;
               my ($comment, $key, $value) = /(# )?(.*?)=(.*)$/;
               $value =~ s/&quot;/"/g;
               $Original{$key} = $value;
        }
        close INI;
    }

    open IN, $file or die "Cannot open $file";
    open OUT, ">$out_file";
    my ($key, $value);
    while(<IN>) {
        next if (/^#/);
        chomp;
        if (/^%+$/) {
            if ($key and (not $opt_m or ($Original{$key} and $Original{$key} ne $value))) {
                print OUT "$key=$value\n";
            }
            $key = "";
            $value = "";
        }
        elsif ($key) {
            if ($value) {
                if (not $opt_u or /^\s*$/ or /^\s+/ or /^[{}]{2}/ or substr($value,-2) eq '\n') {
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
