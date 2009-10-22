#! /usr/bin/perl

use strict;
use warnings;
use Encode;

binmode STDOUT, ':utf8';

print "Fetching all historical contributors for CREDITS\n";
my @shas = qx/git log --pretty=oneline --follow CREDITS.txt/;
s/^(\w+).*/$1/ for @shas;

my $contributors = '';

fetch_contributors($_) for @shas;

print "Going back to master\n";
system "git checkout master" and die "git checkout master failed\n";

open my $outf, '>>', 'CREDITS.txt' or die "Can't write CREDITS.txt\n";
binmode $outf, ':utf8';
print $outf "\n$contributors\n";
close $outf;

print "Contributors appended to CREDITS.txt!\n";

sub fetch_contributors {
  my $sha = shift;
  print "Visiting $sha credits.\n";
  system "git checkout $sha" and die "git checkout $sha failed\n";
  $contributors .= credits_names();
}

sub find_credits {
  my @tests = grep(-f, ('CREDITS.txt', 'CREDITS'));
  die "Cannot find CREDITS file\n" unless @tests;
  return $tests[0];
}

sub decodeText {
  my $bytes = shift;
  eval {
    $bytes = decode('utf-8', $bytes, 1);
    return $bytes;
  };
  eval {
    $bytes = decode('iso-8859-1', $bytes, 1);
    return $bytes;
  };
  $bytes
}

sub credits_names {
  my $file = find_credits();
  open my $inf, '<', $file or die "Can't read $file: $!\n";
  binmode $inf;
  my @lines = <$inf>;
  close $inf;
  my $res = '';
  my $incontributors;
  for (@lines) {
    chomp;
    $_ = decodeText($_);
    $_ .= "\n";
    $res .= $_ if $incontributors && /\S/;
    $incontributors = 1 if !$incontributors && /contributed.*:\s*$/;
  }
  $res
}
