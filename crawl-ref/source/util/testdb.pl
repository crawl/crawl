#!/usr/bin/env perl
use warnings;

use DB_File;

tie %descriptions, 'DB_File', "descriptions.db";
while (($k, $v) = each %descriptions)
   { print "$k -> $v\n" }

untie %descriptions ;
