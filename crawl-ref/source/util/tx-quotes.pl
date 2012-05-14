#!/usr/bin/env perl
use strict;
use warnings;

die "usage: $0 quote_file output_file additional_files\n" if $#ARGV < 2;

my $quotes = shift;
my $quotes_all = shift;
open IN, $quotes or die "Cannot find $quotes\n";
open OUT, ">$quotes_all";

my %Quotes;
while (<IN>) {
    next if (/^#/);
    print OUT $_;
    my ($key) = /(.*?)=/;
    $Quotes{$key} = 1;
}
close IN;

foreach my $file (@ARGV) {
    next if ($file eq $quotes);
    open IN, $file;
    while(<IN>) {
        next if (/^#/);
        my ($key) = /(.*?)=/;
        print OUT "$key=$key\n" unless $Quotes{$key};
    }
    close IN;
}
close OUT;
