#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $lang = shift or die "usage: $0 language\n";
chomp (my $base_dir = `git rev-parse --show-toplevel`);
my $tr_dir = "$base_dir/crawl-ref/source/dat/descript/$lang";
my $util_dir = "$base_dir/crawl-ref/source/util";

foreach my $file (<$tr_dir/*.txt>) {
    my $res = basename($file, '.txt');
    print "Generating ini file for $res\n";
    print `$util_dir/txt2ini.pl -u $file`;
    print `tx push -t -l $lang -r dcss.$res`;
}
