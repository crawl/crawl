#!/usr/bin/env perl
# Generate or update a .txt file (database or description) from a .ini file
# (normally downloaded from transifex). If the .txt file already exists,
# the values of its keys found in the .ini will be updated, and the rest of the
# file will be preserved (comments and keys not in the .ini).
# This behaviour is desired for updating the original files in english.
# For translated files, use the -o option (overwrite). This way, keys deleted
# in the original files will also be deleted in translated files.
# If the output file won't be changed, it won't be overwritten with the same
# content, even with -o.
# Other options are:
# -d: specify output file or directory
# -t: only check if the output file would be changed and exit with an
#     error if so.
# -v: verbose output
# -w: automatically wrap text at 76 columns (doesn't work on windows).

use strict;
use warnings;
use File::Basename;
use Getopt::Std;
use Text::WrapI18N qw(wrap);
# WrapI18N isn't supported under windows. Neither msys nor activeperl.
#use Text::Wrap qw(wrap);

if ($^O ne 'msys') {
    use open ':encoding(utf8)';
}

die "Usage: $0 ini_files\n" unless (@ARGV);

getopts('d:otvw');
our($opt_d, $opt_o, $opt_t, $opt_v, $opt_w);

sub clean_value {
    $_ = shift;

    # transifex returns html encoded quotes
    s/&quot;/"/mg;

    # convert literal \n to actual newlines
    s/\\n/\n/g;

    # In case we have some windows newline format, remove them
    tr/\r//d;

    # If using -w, wrap the text
    if ($opt_w) {
        $_ = wrap("", "", $_) ;

        # WrapIl8N adds spaces at the end of lines and doesn't support the
        # unexpand option.
        s/ +$//mg;
        s/\t/        /mg;
    }

    return $_;
}

my $total_changed;

foreach my $file (@ARGV) {
    unless (-e $file) {
        print "$file not found\n";
        next;
    }
    my ($basename,$path) = fileparse($file, '.ini');
    my $out_file = "$path/$basename.txt";
    if ($opt_d) {
        if (-d $opt_d) {
            $out_file = "$opt_d/$basename.txt";
        } else {
            $out_file = $opt_d;
        }
    }
    open IN, $file;
    if (-e $out_file) {
        my %Text;
        while (<IN>) {
            chomp;
            next if (/^#/);
            my ($key, $value) = /(.*?)=(.*)$/;
            $Text{$key} = clean_value($value);
        }
        close IN;

        my ($key, $value, $out_text, $line_number, $changed);
        $value = "";
        open IN, $out_file;
        while (<IN>) {
            chomp;
            $line_number++;
            if (/^#/) {
                $out_text .= "$_\n";
                next;
            }
            if (/^%+$/) {
                if ($key and $Text{$key} and $Text{$key} ne $value) {
                    if ($opt_t) {
                        print STDERR "$file: line $line_number mismatch\n";
                        if ($opt_v) {
                            print "$file: $Text{$key}\n";
                            print "$out_file: $value\n";
                        }
                        $total_changed++;
                        last;
                    }
                    $changed = 1;
                    $out_text .= "$Text{$key}";
                } else {
                    $out_text .= $value;
                }
                $out_text .= "\n$_\n";
                $key = "";
                $value = "";
            }
            elsif (!$key) {
                $key = $_;
                $out_text .= "$_\n\n";
                $_ = <IN>;
                $line_number++;
                print STDERR "$file: missing empty line at $line_number\n" if ($_ ne "\n");
            }
            else {
                $value .= "\n" if $value;
                $value .= $_;
            }
        }
        close IN;
        if ($changed and not $opt_t) {
            open OUT, ">$out_file";
            local $/;
            print OUT $out_text;
            close OUT;
        }
    } else {
        my $empty = 1;
        open OUT, ">$out_file";
        while (<IN>) {
            next if (/^#/);
            $empty = 0;
            my ($key, $value) = /(.*?)=(.*)/;
            print OUT "%%%%\n";
            print OUT "$key\n\n";
            print OUT clean_value($value), "\n";
        }
        close IN;
        if ($empty) {
            close OUT;
            unlink $out_file;
        } else {
            print OUT "%%%%\n";
            close OUT;
        }
    }
}

if ($opt_t and $total_changed) {
    printf STDERR "%d file%s changed\n", $total_changed, $total_changed > 1 ? "s" : "";
    exit 1;
}
