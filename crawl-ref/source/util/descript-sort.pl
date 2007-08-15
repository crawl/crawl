#!/usr/bin/perl -w

use strict;

if (@ARGV != 2) {
    die "usage: descript-sort.pl infile outfile\n";
}

my $INNAME  = $ARGV[0];
my $OUTNAME = $ARGV[1];

unless (open(INFILE, "$INNAME")) {
    die "Couldn't open input file '$INNAME' for reading: $!\n";
}

unless (open(OUTFILE, ">$OUTNAME")) {
    die "Couldn't open output file '$OUTNAME' for writing: $!\n";
}

undef $/;

my $content = <INFILE>;
close(INFILE);

my @entries = split(/\n?%%%%\s*\n?/m, $content);

my($entry, %entry_table);

foreach $entry (@entries) {
    chomp(my @lines = split(/\n/m, $entry));
    next if (@lines < 1);

    s/\s+$// for @lines;

    my $key = shift(@lines);

    shift(@lines) while (@lines && length($lines[0]) == 0);

    if (@lines == 0) {
        print "'$key' has no definition\n";
        $entry_table{$key} = "";
        next;
    }

    $entry_table{$key} = join("\n", @lines);
}

my @entry_order = sort keys(%entry_table);

foreach $entry (@entry_order) {
    print OUTFILE "%%%%\n";
    print OUTFILE "$entry\n\n";

    if ($entry_table{$entry} ne "") {
        print OUTFILE "$entry_table{$entry}\n";
    }
}

print OUTFILE "%%%%\n";
close(OUTFILE);
