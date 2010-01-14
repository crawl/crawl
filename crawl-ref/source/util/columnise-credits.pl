#!/usr/bin/perl

use strict;
use warnings;

my $CREDITS = 'CREDITS.txt';

my $NAMEHEAD = qr/contributed to .*Stone Soup:\s*$/;

binmode STDOUT, ':utf8';
open my $inf, '<:utf8', $CREDITS
    or die "Unable to read $CREDITS: $!\n";
my @text = <$inf>;
close $inf;

my @recol = recolumnise(@text);

open my $outf, '>:utf8', $CREDITS or die "Can't write CREDITS.txt: $!\n";
for (@text) {
  print $outf $_;

  if (/$NAMEHEAD/o) {
    print $outf "\n";
    print $outf @recol, "\n";
    last;
  }
}
close $outf;

warn "Wrote new $CREDITS\n";

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
