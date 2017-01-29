#!/usr/bin/env perl
use warnings;

use DB_File;

open(TEXT,"../dat/descriptions.txt");
unlink("../dat/descriptions.db");
tie %descriptions, 'DB_File', "../dat/descriptions.db";

my $state = 0;
my $title = "";
my $entry = "";
while (<TEXT>)
{
    $thisLine = $_;
    if ($thisLine =~ /^#/)
    {
       # It's a comment.  continue.
       next;
    }
    if ($thisLine =~ /^%%%%/)
    {
        $state=1;
        # Push existing entry, if any, to the database.
        if ($title ne "")
        {
            $descriptions{"$title"} = "$entry";
            # Clear and set up for next run.
            $title = "";
            $entry = "";
        }
        next;
    }
    if (1 == $state)
    {
        # Lowercase the title, to canonicalize the key.
        $title = lc($thisLine);
        chomp($title);
        #        print ("I just read $title\n");
        $state++;
        next;
    }
    if (2 == $state)
    {
        $entry .= $thisLine;
        next;
    };

}
$descriptions{"$title"} = "$entry";

while (($k, $v) = each %descriptions)
   { print "$k -> $v\n" }


untie %descriptions;
