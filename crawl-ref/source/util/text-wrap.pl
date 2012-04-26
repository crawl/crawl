#!/usr/bin/env perl

use strict;
use warnings;
use Getopt::Std;
use Text::Wrap;
$Text::Wrap::columns = 80;

my $ln = "\n";

# unwrap option
getopts('u');
our($opt_u);
if ($^O eq 'msys') {
    $/ = "\cM\cJ";
    $\ = $/;
    $Text::Wrap::separator=$\;
}
else {
    $\ = "\cM";
}

main();

sub main {
    foreach my $file (@ARGV) {
        my $tmp = "$file~";
        rename $file, $tmp;
        open IN, $tmp;
        open OUT, ">$file";
        my $name = "";
        my $desc = "";
        while (<IN>) {
            chomp;
            my $blank = /^\s*$/;
            my $end_section = /^%%%%$/;
            if (/^#/ or $blank and not $desc) {
                print OUT $_;
            } elsif ($end_section or $blank) {
                if ($name and not $opt_u) {
                    $desc = wrap("", "", $desc);
                }
                if ($desc) {
                    print OUT $desc;
                }
                print OUT $_;
                if ($end_section) {
                    $name = "";
                }
                $desc = "";
            } elsif ($name) {
                if ($desc) {
                    $desc .= /^\s+/ ? "\n" : " ";
                }
                $desc .= $_;
            } else {
                print OUT;
                $name = $_;
            }
        }
        close IN;
        close OUT;
        unlink $tmp;
   }
}
