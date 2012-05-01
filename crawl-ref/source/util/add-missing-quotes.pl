#!/usr/bin/env perl
use strict;
use warnings;

chomp (my $base_dir = `git rev-parse --show-toplevel`);
my $desc_dir = "$base_dir/crawl-ref/source/dat/descript";
my $quotes =  "$desc_dir/quotes.ini";
open IN, $quotes or die "Cannot find $quotes\n";
my %Quotes;
while (<IN>) {
    next if (/^#/);
    my ($key) = /(.*?)=/;
    $Quotes{$key} = 1;
}
close IN;

open OUT, ">>$quotes";
foreach my $file (<$desc_dir/*.ini>) {
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
