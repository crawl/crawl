#!/usr/bin/perl

use strict;
use warnings;
use Config::Tiny;
use File::Basename;
#use open ':encoding(utf8)';

die "Usage: $0 yaml_files\n" unless (@ARGV);

main();

sub main {
   foreach my $file (@ARGV) {
        my $basename = basename($file, '.ini');
        open OUT, ">$basename.txt";
        my $Config = Config::Tiny->new;
        $Config = Config::Tiny->read($file);
        foreach (sort keys %{$Config->{_}}) {
            print OUT "%%%%\n";
            print OUT "$_\n\n";
            my $value = $Config->{_}->{$_};
            $value =~ s/\\n/\n/g;
            print OUT $value, "\n";
        }
        print OUT "%%%%\n";
        close OUT;
   }
}
