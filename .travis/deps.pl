#!/usr/bin/env perl
use strict;
use warnings;

if ($ENV{BUILD_ALL}) {
    retry(qw(git submodule update --init --recursive));
}
