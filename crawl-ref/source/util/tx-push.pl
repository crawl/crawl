#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $lang = shift or die "usage: $0 language\n";
chomp (my $base_dir = `git rev-parse --show-toplevel`);
my $descript_dir = "$base_dir/crawl-ref/source/dat/descript";
my $src_dir = "$descript_dir/$lang";
my $dest_dir = $src_dir;
my $util_dir = "$base_dir/crawl-ref/source/util";
my $options = "-u";
if ($lang eq "en") {
    $src_dir = "$descript_dir";
    $options .= "md $dest_dir";
}

foreach my $file (<$src_dir/*.txt>) {
    my $res = basename($file, '.txt');
    print "Generating ini file for $res\n";
    print `$util_dir/tx-mkini.pl $options $file`;
    if (-e "$dest_dir/$res.ini") {
        print `tx push -t -l $lang -r dcss.$res`;
    }
}
