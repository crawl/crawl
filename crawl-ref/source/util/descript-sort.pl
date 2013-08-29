#!/usr/bin/env perl
use warnings;
use strict;

@ARGV == 1 or die "usage: descript-sort.pl file\n";

my $FNAME  = $ARGV[0];

open(INFILE, '<', $ARGV[0])
    or die "Couldn't open file '$ARGV[0]' for reading: $!\n";

undef $/;

my $content = <INFILE>;
close(INFILE);

my @entries = split(/\n?%%%%\s*\n?/m, $content);

my($entry, %entry_table);

foreach $entry (@entries)
{
    chomp(my @lines = split(/\n/m, $entry));
    next if (@lines < 1);

    s/\s+$// for @lines;

    my $key = shift(@lines);

    shift(@lines) while (@lines && length($lines[0]) == 0);

    if (@lines == 0)
    {
        print "'$key' has no definition\n";
        $entry_table{$key} = "";
        next;
    }

    $entry_table{$key} = join("\n", @lines);
}

my @entry_order = sort keys(%entry_table);

open(OUTFILE, '>', $ARGV[0])
    or die "Couldn't open file '$ARGV[0]' for writing: $!\n";

foreach $entry (@entry_order)
{
    print OUTFILE "%%%%\n";
    print OUTFILE "$entry\n\n";

    print OUTFILE "$entry_table{$entry}\n" if ($entry_table{$entry} ne "");
}

print OUTFILE "%%%%\n";
close(OUTFILE);
