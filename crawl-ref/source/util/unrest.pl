#! /usr/bin/env perl
use warnings;

undef $/;
$_=<>;

# Undo Windows newlines.
s/\r\n/\n/sg;

# URLs have damn inconsistent handling in reST.
s|:http: ``(.+)``|$1|g;
s|:telnet: ``(.+)``|telnet:  $1|g;
s|:ssh: ``(.+)``|ssh:     $1|g;
s|:tiles: ``(.+)``|tiles:   $1|g;

# Notes.
s/\.\. note::/Note: /g;

# Local references.
s/`([^_])\.\s+(.*?)`_/$1. "$2"/sg; # added "" for a nicer look
s/`(.+)`_/"$1"/g; # should in principle have s flag, but will be too greedy

# HTML and reST escapes.
s/&lt;/</g;
s/&gt;/>/g;
s/&quot;/"/g;
s/&#0*39;/'/g;
s/&amp;/&/g;
s/\\(.)/$1/g;

# Table of contents.
my $contents = "Contents\n--------\n";
for (/\*{9,}\n(.\. .+)\n\*{9,}/g)
{
    /(.)\. (.+)/;
    $contents .= "\nAppendices\n" if $1 eq "1";
    $contents .= "$1.      $2\n";
}
s/\.\. contents::\n   :depth: 5/$contents/;

# Main headers.
my $DCSShead = <<END;
                       DUNGEON CRAWL Stone Soup
                            - the manual -
END
s/\+{9,}\nDungeon Crawl Stone Soup manual\n\+{9,}\n/$DCSShead/;
s/#{9,}\nManual\n#{9,}\n\n//;

# Make section headers nice and centered.
my $dashes = "-"x72;
my $spaces = " "x36;
s/\*{9,}\n(.)\. (.*)\n\*{9,}/$dashes\n$1.$spaces\ca$2\cb\U$2\n$dashes/g;
1 while s/ \ca[^\cb]{2}/\ca/g;
s/\ca[^\cb]?\cb//g;

# Rewrap overlong lines.
my $ls = "";
my $rem = "";
for (/^.*$/mg)
{
    s/\s*$//;
    /^( *)(.*)/;
    my ($s, $line) = ($1, $2);
    if ($s ne $ls || $line eq "")
    {
        # Different indentation -- needs a separate line.
        $_ = "$ls$rem";
        s/\s+$//;
        print "$_\n" unless $_ eq "";
        $rem = "";
    }
    $ls = $s;
    $_ = "$s$rem$line";
    /(.{0,75})(?:$|\s+)(.*)/;
    ($_, $rem) = ($1, "$2 ");
    s/\s+$//;
    print "$_\n";
    $rem =~ s/^\s+//;
}
