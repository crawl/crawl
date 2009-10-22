#!/usr/bin/perl

use strict;
use warnings;

my $NAMEHEAD = qr/contributed to .*Stone Soup:\s*$/;

binmode STDOUT, ':utf8';
open my $inf, '<:utf8', 'CREDITS.txt'
    or die "Unable to read CREDITS.txt: $!\n";
my @text = <$inf>;
close $inf;

my @recol = recolumnise(@text);

for (@text) {
  print;

  if (/$NAMEHEAD/o) {
    print "\n";
    print @recol, "\n";
    last;
  }
}

sub last_word {
  my $s = shift;
  my ($word) = $s =~ /.* (\S+)$/;
  $word ||= $s;
  lc($word)
}

sub recolumnise {
  my @text = @_;

  my @columns;
  for (@text) {
    push @columns, $_ if (/$NAMEHEAD/o .. undef);
  }

  # Discard header lines:
  splice @columns, 0, 2;

  my @names = sort { last_word($a) cmp last_word($b) } extract_names(@columns);

  my @recol = resplit(3, @names);
  @recol
}

sub pad_column {
  my ($rcol, $size) = @_;
  my $maxlen;
  for (@$rcol) {
    $maxlen = length() if !$maxlen || length() > $maxlen;
  }

  $maxlen += 6;
  $maxlen = $size if $maxlen < $size;

  @$rcol = map { $_ . (" " x ($maxlen - length())) } @$rcol;
}

sub resplit {
  my ($ncols, @names) = @_;

  my $colsize = int(@names / $ncols);
  $colsize++ if @names % $ncols;

  my @columns;
  my $start = 0;

  for (1 .. ($ncols - 1)) {
    push @columns, [ @names[ $start .. ($start + $colsize - 1) ] ];
    $start += $colsize;
  }

  push @columns, [ @names[ $start .. $#names ] ];

  my $stop = 80 / $ncols;

  pad_column($_, $stop) for @columns;

  my @out;
  for my $row (1 .. $colsize) {
    push @out, join("", map { $columns[$_ - 1][$row - 1] || '' } 1 .. $ncols);
  }
  s/^\s+//, s/\s+$//, $_ .= "\n" for @out;
  @out
}

sub extract_names {
  my @cols = @_;
  my @names;
  for my $line (@cols) {
    push @names, ($line =~ /((?:\S+ )*\S+)/g);
  }
  my %dupe;
  grep(!$dupe{$_}++, @names)
}
