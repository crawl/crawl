#!/usr/bin/perl -w

use strict;

# Intended to read FAQ.txt (input file in the database format) and
# output its content as an html file, including internal and external links.

my $infile  = shift || "../dat/database/FAQ.txt";
my $outfile = shift || "../../docs/FAQ.html";

# Sometimes, you want an additional linebreak, sometimes you don't.
my $BR   = shift || "</br>";

open (FILE, $infile) or die "Cannot read file '$infile'.\n";

my @keys;
my %questions;
my %answers;
my $key     = "";
my $current = "";
my $text    = "";
my $list    = 0;
while (my $line = <FILE>)
{
    next if ($line =~ /^#/);

    if ($line =~ /^Q:(.+)$/)
    {
        # Line includes the question key.
        $key = $1;
        $key =~ s/\s/_/;
        push @keys, $key if (not exists $answers{$key});
        $current = "q";
    }
    elsif ($key eq "" && $line =~ /^A:(.+)$/)
    {
        # Line includes the answer key.
        # NOTE: The answers themselves also start with "A:", which is
        #       why we need to check whether we already have a key, above.
        $key = $1;
        $key =~ s/\s/_/;

        push @keys, $key if (not exists $questions{$key});
        $current = "a";
    }
    elsif ($line =~ /^%%%%/)
    {
        # Line contains the border sign.
        # -> Put currently stored text into the appropriate hash.
        # -> Clear key and text.
        if ($list)
        {
            # Properly close list tags.
            chomp $text;
            $text .= "</li></ul>\n";
            $list = 0;
        }
        $questions{$key} = $text if ($current eq "q");
        $answers{$key}   = $text if ($current eq "a");
        $text = "";
        $key  = "";
    }
    elsif ($line !~ /^\s+$/ || $text ne "")
    {
        $line =~ s/^A:\s*//;

        # Transform * lists into proper lists.
        if ($line =~ /^\*/)
        {
            $line =~ s/^\*/<li>/;
            if (not $list)
            {
                $text =~ s/<\/br>\n$/\n/;
                $line =~ s/^<li>/<ul><li>/;
                $list = 1;
            }
            else
            {
                chomp $text;
                $text .= "<\/li>\n";
            }
        }
        elsif ($list && $line =~ /^\s*$/)
        {
            # An empty line, end list section.
            chomp $text;
            $text .= "</li></ul>\n";
            $list = 0;
        }
        elsif ($current eq "a")
        {
            # If this line is part of an answer, properly translate
            # empty lines.
            if ($line =~ /^\s*$/)
            {
                $line =~ s/\n/<\/br>$BR\n/;
            }
            else
            {
                # Replace newlines with spaces, so the text can run across
                # multiple lines.
                $line =~ s/\n/ /;
            }
        }

        # Turn web addresses into hyperlinks.
        # NOTE: We need the array in case there are several links on the
        #       same line.
        my @links;
        my $count = 0;
        while ($line =~ /^(.*)\b(http[^\s\)\,]+)([\s\)\,].*)/g)
        {
            $count++;

            my $a = $1 || "";
            my $b = $2;
            my $c = $3 || "";
            $b =~ s/\/$//;

            push @links, $b;
            my $l = "link$count";
            $line = "$a<a href=$l>$l</a>$c ";
        }
        $count = 0;
        foreach my $l (@links)
        {
            $count++;
            $line =~ s/link$count/$l/g;
        }

        # Highlight commands.
        while ($line =~ /^(.*)'([^\s']+)'(.*)$/)
        {
            my $a = $1;
            my $b = $2;
            my $c = $3;
            $line = "$a<b>$b</b>$c";
        }

        # Replace *emphasis* by italics.
        if ($line =~ /^(.*)\*(.*)\*(.*)$/)
        {
            my $a = $1;
            my $b = $2;
            my $c = $3;
            $line = "$a<i>$b</i>$c";
        }

        # Specialcase the '&' symbol.
        if ($line =~ /^(.*)&(.*)$/)
        {
            my $a = $1;
            my $b = $2;
            $line = "$a&amp;$b";
        }

        $text .= $line;
    }
}
close FILE;

open (OUTFILE, ">$outfile") or die "Cannot write to file '$outfile'.\n";

# Print the header.
print OUTFILE "<a name=\"top\">\n";
print OUTFILE "<b>Dungeon Crawl Stone Soup - Frequently Asked Questions</b></br>\n";
print OUTFILE "$BR\n";

# Print all questions, and link them to their answer.
my $count = 0;
foreach my $k (@keys)
{
    if (not exists $questions{$k})
    {
        print STDERR "No question for key '$k'. Skip key.\n";
        next;
    }
    if (not exists $answers{$k})
    {
        print STDERR "No answer for key '$k'. Skip key.\n";
        next;
    }
    $count++;
    my $question = $questions{$k};
    my $answer   = $answers{$k};

    # Prefix question with "Q#".
    chomp $question;
    $question = "Q$count".". $question";
    $questions{$k} = $question;

    print OUTFILE "<a href=\"#$k\">";
    print OUTFILE $question;
    print OUTFILE "</a> $BR\n";
}
print OUTFILE "$BR<hr>$BR\n";

# Print all question/answer pairs.
foreach my $k (@keys)
{
    next if (not (exists $questions{$k}) || not (exists $answers{$k}));

    my $question = $questions{$k};
    my $answer   = $answers{$k};

    print OUTFILE "<a name=\"$k\"></a>\n";
    print OUTFILE "<b>$question</b>$BR\n";
    print OUTFILE "$BR\n";
    print OUTFILE "$answer\n";
    print OUTFILE "$BR\n" if ($answer !~ /<\/ul>\n$/);
    print OUTFILE "<hr>$BR\n";
}
print OUTFILE "\n<a href=\"#top\">Back to top</a>\n";
