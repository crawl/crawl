#!/usr/bin/env perl

use warnings;

use strict;

unless (@ARGV == 1)
{
    die("usage: grep-mon.pl <pattern>\n");
}

my $data_file;

if (-e "mon-data.h")
{
    $data_file = "mon-data.h";
}
elsif (-e "../mon-data.h")
{
    $data_file = "../mon-data.h";
}
else
{
     die("Can't find 'mon-data.h'\n");
}

unless (open(FILE, "<$data_file"))
{
    die("Couldn't open '$data_file' for reading: $!\n");
}

my $line;

while (<FILE>)
{
    $line = $_;
    last if ($line =~ /static monsterentry mondata/);
}

unless ($line =~ /static monsterentry mondata/)
{
    die("Couldn't find mondata array.\n");
}

# Slurp in the rest of it.
undef($/);
my $content = <FILE>;
close(FILE);

my @mons = split(/^},/m, $content);

my @matches = grep(/$ARGV[0]/s, @mons);

print join("},", @matches) . "\n";
