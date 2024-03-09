#!/usr/bin/env perl

use strict;
use warnings;

my $CXX = shift @ARGV;
my $includesStr = shift @ARGV;
my @includes = split(/;+/, $includesStr);
my $fixedIncludes = "\/I \"" . join("\" /I \"", @includes) . "\"";
system("python.exe", "util/configure.py \"" . $CXX . "\" " . $fixedIncludes);
