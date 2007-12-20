#!/usr/bin/perl
#
# featname.pl
#
# Checks that the DNGN feature names in luadgn.cc match up with the enum
# constants in enum.h.
#

use strict;
use warnings;

my $FEATFILE = "enum.h";
my $FNAMEFILE = "luadgn.cc";
my ($features, $fnummap) = read_features($FEATFILE);
my @fnames = read_feature_names($FNAMEFILE);

verify_names($features, $fnummap, \@fnames);

sub read_feature_names {
  my $file = shift;
  my $text = do { local(@ARGV, $/) = $file; <> };
  my ($array) = $text =~
    /dngn_feature_names.*?=.*?{([^}]+)}/xs;
  my @names = $array =~ /"([^"]*)"/gs;
  return @names;
}

sub verify_names {
  my ($farr, $fmap, $fnames) = @_;
  for (my $i = 0; $i < @$fnames; ++$i) {
    my $name = $$fnames[$i];
    next unless $name;
    my $feat = "DNGN_\U$name";
    $$fmap{$feat} = -1 unless exists $$fmap{$feat};
    if ($$fmap{$feat} != $i) {
      die "$name is at $i, was expecting $$fmap{$feat} as in enum.\n";
    }
  }
  print "Feature names in $FNAMEFILE and $FEATFILE match ok.\n";
}

sub read_features {
  my $file = shift;
  my @lines = do { local (@ARGV) = $file; <> };

  my @features = ([]) x 500;
  my $in_enum;
  my %fnummap;
  my $currval = 0;
  for my $line (@lines) {
    if (!$in_enum) {
      $in_enum = 1 if $line =~ /enum\s+dungeon_feature_type/;
    }
    else {
      last if $line =~ /^\s*};/;

      s/^\s+//, s{//.*}{}, s/\s+$// for $line;
      next unless $line =~ /\S/;

      my ($key, $val) = $line =~ /(DNGN\w+)(?:\s*=\s*(\w+))?/;
      next unless $key;

      if (defined $val) {
        if ($val =~ /^DNGN/) {
          $val = $fnummap{$val};
        }
      }
      else {
        $val = $currval;
      }
      $currval = $val + 1;

      $fnummap{$key} = $val;
      push @{$features[$val]}, $key;
    }
  }
  return (\@features, \%fnummap);
}
