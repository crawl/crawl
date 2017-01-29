#!/usr/bin/env perl

use warnings;
use strict;

# Intended to read FAQ.txt (input file in the database format) and
# output its content as an html file, including internal and external links.

my $infile  = shift || "../dat/database/FAQ.txt";
my $outfile = shift || "../../docs/FAQ.html";

open (FILE, $infile) or die "Cannot read file '$infile'.\n";

my @keys;
my %questions;
my %answers;
my $key     = "";
my $current = "";
my $text    = "";
my $list    = 0;
my $bullet  = 0;
my $listct  = 0;
my $title   = "no T:html key!";
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
    elsif ($key eq "" && $line =~ /^T:(.+)$/)
    {
        $key = $1;
        $current = "t";
    }
    elsif ($line =~ /^%%%%/)
    {
        $_ = $text;
        s/&/&amp;/g;
        s/</&lt;/g;
        s/>/&gt;/g;
        s/"/&quot;/g;

        if ($current eq "a")
        {
            $_ = $text;

            # Transform * lists into proper lists.
            s|\n+\* |\n<li>|g;
            # Separate paragraphs.
            s/\n\n/\n<p>/g;
            s|((?:<li>[^<]+)+)|<ul>\n$&</ul>\n|g;

            # Hyperlink URLs.
            s{http(?:|s)://[^\s\),]+}{<a href="$&">$&</a>}g;

            # Highlight commands.
            s|'([^\s']+)'|<strong>$1</strong>|g;

            # Replace *emphasis* by italics.
            s|\*(.*)\*|<em>$1</em>|g;

            # Also use italics for mentioned txt/png files.
            s{[^\s]+\.(?:txt|png)}{<em>$&</em>}g;


            $answers{$key} = $_;
        }

        $questions{$key} = $_ if ($current eq "q");
        $title = $_ if ($current eq "t" && $key eq "html");
        $text = "";
        $key  = "";
        $listct = 0;
    }
    else
    {
        $text .= $line;
    }
}
close FILE;

open (OUTFILE, ">$outfile") or die "Cannot write to file '$outfile'.\n";

# Print the header.
print OUTFILE <<END;
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
        "http://www.w3.org/TR/html4/strict.dtd">
<head>
 <title>$title</title>
 <meta http-equiv="Content-Type" content="text/html;charset=utf-8">
 <style type="text/css">
dt {font-weight:bold; margin-bottom:1em}
dd {margin-left:0}
ul {margin-top:0.5em}
 </style>
</head>
<body>
<p><a name=\"top\"></a>
 <h4>$title</h4>
 <ul style='list-style-type:none; padding-left:0'>
END

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

    print OUTFILE "<li><a href=\"#$k\">$question</a>\n";
}
print OUTFILE <<END;
</ul>
<hr>

<p><dl>
END

# Print all question/answer pairs.
foreach my $k (@keys)
{
    next if (not (exists $questions{$k}) || not (exists $answers{$k}));

    my $question = $questions{$k};
    my $answer   = $answers{$k};

    print OUTFILE <<END;
<dt><a name=\"$k\">$question</a></dt>
<dd>$answer<hr></dd>
END
}
print OUTFILE <<END;

</dl>
<p><a href=\"#top\">Back to top</a></p>
</body>
END
