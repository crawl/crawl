#!/usr/bin/env perl
use warnings;


%remote_branches;
%merged_branches;

open (REMOTE, "git branch -a|");
while (<REMOTE>)
{
    my ($branch) = /remotes\/origin\/(.*)$/;
    $remote_branches{$branch} = 1;
}
close REMOTE;

open(LOG, "git log master|");
while (<LOG>)
{
    my $branch;
    next unless (($branch) = /Merge branch '(.*)'/);
    next if ($branch eq "master");
    next unless ($remote_branches{$branch});
    next if ($merged_branches{$branch});
    $merged_branches{$branch} = 1;
    print "$branch\n";
}
close LOG;
