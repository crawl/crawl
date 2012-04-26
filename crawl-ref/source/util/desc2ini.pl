#!/usr/bin/perl

use strict;
use warnings;
use Config::Tiny;
use File::Basename;

die "Usage: $0 description_files\n" unless (@ARGV);

main();

sub main {
   foreach my $file (@ARGV) {
        my $basename = basename($file, '.txt');
        my %DESCRIPTIONS;
        load_file($file, \%DESCRIPTIONS);
        my $Config = Config::Tiny->new;
        foreach (keys %DESCRIPTIONS) {
            $Config->{_}->{$_} = $DESCRIPTIONS{$_};
        }
        $Config->write("$basename.ini");
   }
}

sub load_file {
    my ($input, $hash) = @_;
    open IN, $input or die "Cannot open $input";
    my ($key, $value);
    while(<IN>) {
        next if (/^#/);
        chomp;
        if (/^%%%%$/) {
            if ($key) {
                $hash->{$key} = $value;
            }
            $key = "";
            $value = "";
        }
        elsif ($key) {
            if ($value) {
                if (/^\s*$/ or /^\s+/ or substr($value,-2) eq '\n') {
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
}
