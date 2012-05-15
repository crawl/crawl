#!/usr/bin/env perl
# Generate or update a .txt file (database or description) from a .ini file
# (normally downloaded from transifex). If the .txt file already exists,
# and the -m (merge) option is used, the values of its keys found in the .ini
# will be updated, and the rest of the file will be preserved (comments and
# keys not in the .ini).
# This behaviour is desired for updating the original files in english,
# but not for translated files, otherwise keys deleted in the original files
# would not be deleted in them.
# If the output file won't be changed, it won't be overwritten with the same
# content.
# Other options are:
# -a: used with -m. Append additional keys to the end of the file.
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

$Text::WrapI18N::columns = 79;

if ($^O ne 'msys') {
    use open ':encoding(utf8)';
}

die "Usage: $0 ini_files\n" unless (@ARGV);

getopts('ad:mtvw');
our($opt_a, $opt_d, $opt_m, $opt_t, $opt_v, $opt_w);

if ($opt_a) {
    $opt_m = 1;
}

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
    open INI, $file;

    if (-e $out_file) {
        my %Text;
        my @Keys;
        my $out_text = "";

        # First, we load the ini file.
        while (<INI>) {
            chomp;
            next if (/^#/);
            my ($key, $value) = /(.*?)=(.*)$/;
            $Text{$key} = clean_value($value);
            push @Keys, $key;
            $out_text .= "%%%%\n";
            $out_text .= "$key\n\n";
            $out_text .= clean_value($value) . "\n";
        }
        close INI;

        if ($out_text) {
            $out_text .= "%%%%\n";
        }

        my ($key, $value, $merged_text, $line_number, $changed);
        $value = "";

        # Now we load the .txt file and compare it with what's in the .ini
        open TXT, $out_file;
        while (<TXT>) {
            chomp;
            $line_number++;

            # Comments are preserved
            if (/^#/) {
                $merged_text .= "$_\n";
                next;
            }

            # end of section, time to compare values
            if (/^%+$/) {
                if ($key and $Text{$key} and $Text{$key} ne $value) {

                    # In test mode, we report the mismatch and stop here.
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
                    $merged_text .= "$Text{$key}\n";
                } elsif ($value) {
                    $merged_text .= "$value\n";
                }

                # When not in merged mode, we also check for new/deleted keys.
                if ($key) {
                    if ($Text{$key}) {
                        delete $Text{$key};
                    } elsif (not $opt_m) {
                        # The key has been deleted from the .ini, so the .txt
                        # will need to be updated.
                        $changed = 1;
                    }
                }

                $merged_text .= "$_\n";
                $key = "";
                $value = "";
            }

            # If key is unset, then we have a new key. Read it and skip the
            # blank line.
            elsif (!$key) {
                $key = $_;
                $merged_text .= "$_\n\n";
                $_ = <TXT>;
                $line_number++;
                print STDERR "$file: missing empty line at $line_number\n" if ($_ ne "\n");
            }

            # Otherwise, we're reading a value.
            else {
                $value .= "\n" if $value;
                $value .= $_;
            }
        }
        close TXT;

        if (%Text) {
            # With -a, we append the remaining entries at the end of the file.
            if ($opt_a) {
                foreach $key (@Keys) {
                    if ($Text{$key}) {
                        $merged_text .= "$key\n\n$Text{$key}\n%%%%\n";
                        $changed = 1;
                    }
                }
            }

            # When not in merge mode, if %Text isn't empty, it means that we
            # have new keys, so we need to write the file.
            if (not $opt_m) {
                $changed = 1;
            }
        }

        # File needs to be updated.
        if ($changed and not $opt_t) {
            open TXT, ">$out_file";
            local $/;
            if ($opt_m) {
                print TXT $merged_text;
            } else {
                print TXT $out_text;
            }
            close TXT;
        }
    } else {
        # Destination file doesn't exists, so we just create it.
        my $empty = 1;
        open TXT, ">$out_file";
        while (<INI>) {
            next if (/^#/);
            $empty = 0;
            my ($key, $value) = /(.*?)=(.*)/;
            print TXT "%%%%\n";
            print TXT "$key\n\n";
            print TXT clean_value($value), "\n";
        }
        close INI;
        if ($empty) {
            close TXT;
            unlink $out_file;
        } else {
            print TXT "%%%%\n";
            close TXT;
        }
    }
}

if ($opt_t and $total_changed) {
    printf STDERR "%d file%s changed\n", $total_changed, $total_changed > 1 ? "s" : "";
    exit 1;
}
