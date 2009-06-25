#!/usr/bin/perl -w

use strict;

my $line_num      = 0;
my @errors        = ();
my @all_artefacts = ();
my %used_names    = ();
my %used_appears  = ();
my %used_enums   = ();

my %field_type = (
    AC       => "num",
    ACC      => "num",
    ANGRY    => "num",
    APPEAR   => "str",
    BERSERK  => "num",
    BLINK    => "num",
    BRAND    => "enum",
    CANTELEP => "num",
    COLD     => "num",
    COLOUR   => "enum",
    CURSED   => "num",
    DAM      => "num",
    DESC     => "str",
    DESC_END => "str",
    DESC_ID  => "str",
    DEX      => "num",
    ELEC     => "num",
    EV       => "num",
    FIRE     => "num",
    INT      => "num",
    INV      => "num",
    LEV      => "num",
    LIFE     => "num",
    MAGIC    => "num",
    MAPPING  => "num",
    METAB    => "num",
    MP       => "num",
    MUTATE   => "num",
    NAME     => "str",
    NOISES   => "num",
    NOSPELL  => "num",
    NOTELEP  => "num",
    POISON   => "num",
    RND_TELE => "num",
    SEEINV   => "num",
    STEALTH  => "num",
    STR      => "num",

    plus      => "num",
    plus2     => "num",
    base_type => "enum",
    sub_type  => "enum"
);

my @field_list = keys(%field_type);

sub error
{
    my ($artefact, $str) = @_;

    my $msg = "";

    my $name = $artefact->{NAME} || $artefact->{APPEAR} || "NAMELESS";

    if ($artefact->{_FINISHING})
    {
        $msg .= "Artefact $name starting at line "
              . $artefact->{_START_LINE} . ": ";
    }
    else
    {
        $msg = "Artefact $name at line $line_num: "
    }

    $msg .= $str;

    $artefact->{_ERRROR} = 1;

    push(@errors, $msg);
}

sub is_number
{
    my ($num) = @_;
    return ($num =~ /^[+-]?\d+$/);
}

sub finish_art
{
    my ($artefact) = @_;

    $artefact->{_FINISHING} = 1;

    my $must;
    foreach $must ("NAME", "APPEAR", "OBJ", "COLOUR")
    {
        if (!defined($artefact->{$must}))
        {
            error($artefact, "Required field '$must' missing");
            $artefact->{$must} = "_MISSING_";
        }
    }

    # Prevent further errors caused by absence of OBJ
    $artefact->{base_type} ||= "";
    $artefact->{sub_type}  ||= "";

    if (!exists($artefact->{BRAND}))
    {
        my $type = $artefact->{base_type} || "";

        if ($type eq "OBJ_WEAPONS")
        {
            $artefact->{BRAND} = "SPWPN_NORMAL";
        }
        elsif($type eq "OBJ_ARMOUR")
        {
            $artefact->{BRAND} = "SPARM_NORMAL";
        }
        else
        {
            $artefact->{BRAND} = "0";
        }
    }

    my $field;
    foreach $field (@field_list)
    {
        if (!exists($artefact->{$field}))
        {
            # Give default values for fields not specified.
            my $type = $field_type{$field} || "";
            if ($type eq "str")
            {
                $artefact->{$field} = "";
            }
            elsif($type eq "num")
            {
                $artefact->{$field} = "0";
            }
            elsif($type eq "enum")
            {
                error($artefact, "No enumeration for field '$field'");
            }
            else
            {
                error($artefact, "Unknown type '$type' for field '$field'");
            }
        }
        elsif (!defined($artefact->{$field}))
        {
            error($artefact, "Field '$field' not defined");
        }
    }

    delete($artefact->{_FINISHING});
    $artefact->{_FINISHED} = 1;
}

sub process_line
{
    my ($artefact, $line) = @_;

    # A line can start with whitespace if it's a continuation of a field
    # with a string value.
    if ($line =~ /^\s/)
    {
        my $prev_field = $artefact->{_PREV_FIELD} || "";
        if ($field_type{$prev_field} eq "str")
        {
            $line =~ s/^\s*//;
            $artefact->{$prev_field} .= " " . $line;
        }
        else
        {
            error($artefact, "line starts with whitespace");
        }
        return;
    }

    $artefact->{_START_LINE} ||= $line_num;

    my ($field, $value) = ($line =~ /([^:]+):\s*(.*)/);

    $field ||= "";
    $value ||= "";

    # Strip leading and trailing white space.
    $field =~ s/^\s*|\s*$//g;
    $value =~ s/^\s*|\s*$//g;

    if ($field eq "")
    {
        error($artefact, "No field");
        return;
    }

    if (defined($artefact->{$field}))
    {
        error($artefact, "Field '$field' already set");
        return;
    }

    if ($value eq "true" && $field_type{$field} eq "num")
    {
        $value = "1";
    }

    $artefact->{_PREV_FIELD} = $field;
    $artefact->{$field}      = $value;

    if ($value eq "")
    {
        error($artefact, "Field '$field' has no value");
        return;
    }

    if ($field eq "OBJ")
    {
        my @parts = split(m!/!, $value);

        if (@parts > 2)
        {
            error($artefact, "Too many parts to OBJ");
            return;
        }
        elsif (@parts == 1)
        {
            error($artefact, "Too few parts to OBJ");
            return;
        }

        if ($parts[0] !~ /^OBJ_/)
        {
            error($artefact, "OBJ base type must start with 'OBJ_'");
            return;
        }
        $artefact->{base_type} = $parts[0];
        $artefact->{sub_type}  = $parts[1];
    }
    elsif($field eq "PLUS")
    {
        my @parts = split(m!/!, $value);

        if (@parts > 2)
        {
            error($artefact, "Too many parts to PLUS");
            return;
        }

        if (!is_number($parts[0]))
        {
            error($artefact, "'$parts[0]' in PLUS is not a number");
            return;
        }
        if (@parts == 2 && !is_number($parts[1]))
        {
            error($artefact, "'$parts[1]' in PLUS is not a number");
            return;
        }
        $artefact->{plus}  = $parts[0];
        $artefact->{plus2} = $parts[1] if (@parts == 2);
    }
    elsif ($field eq "ENUM")
    {
    }
    else
    {
        if (!exists($field_type{$field}) || $field =~ /^[a-z]/)
        {
            error($artefact, "No such field as '$field'");
            return;
        }

        my $num =  is_number($value);
        my $type = $field_type{$field} || "";

        if (($type eq "num" && !$num) || ($type eq "str" && $num))
        {
            error($artefact, "'$value' invalid value type for field '$field'.");
            return;
        }
    }

    my $enum = "";

    if ($field eq "NAME")
    {
        if (exists($used_names{$value}))
        {
            error($artefact, "Name \"$value\" already used at line " .
                             $used_names{$value});
            return;
        }
        $used_names{$value} = $line_num;

        if (!exists($artefact->{_ENUM}))
        {
            $enum = $value;
            $enum =~ s/'//g;
            # If possible, make the enum literal the part of the name between
            # double quotes, or the part of the name after " of " or
            # " of the ".
            if ($enum =~ /"(.*)"/)
            {
                $enum = $1;
            }
            elsif ($enum =~ / of (?:the )?(.*)/)
            {
                $enum = $1;
            }
            $enum = uc($enum);
            $enum =~ s/[ -]/_/g;
        }
    }
    elsif ($field eq "APPEAR")
    {
        if (exists($used_appears{$value}))
        {
            error($artefact, "Name '$value' already used at line " .
                             $used_appears{$value});
            return;
        }
        $used_appears{$value} = $line_num;
    }
    elsif ($field eq "ENUM")
    {
        if (exists($artefact->{NAME}))
        {
            eror($artefact, "ENUM must be before NAME");
            return;
        }
        $enum = "$value";
    }

    if ($enum ne "")
    {
        if (exists($used_enums{$enum}))
        {
            error($artefact, "Enum \"$enum\" already used by artefact " .
                             "\"$used_enums{$enum}\"");
            return;
        }

        $used_enums{$enum} = $value;
        $artefact->{_ENUM} = $enum;
    }
}

my @art_order = (
    "NAME", "APPEAR", "\n",
    "base_type", "sub_type", "plus", "plus2", "COLOUR",

    "{", "BRAND", "AC", "EV", "STR", "INT", "DEX", "\n",
    "FIRE", "COLD", "ELEC", "POISON", "LIFE", "MAGIC", "\n",
    "SEEINV", "INV", "LEV", "BLINK", "CANTELEP", "BERSERK", "\n",
    "MAPPING", "NOISES", "NOSPELL", "RND_TELE", "NOTELEP", "\n",
    "ANGRY", "METAB", "MUTATE", "ACC", "DAM", "\n",
    "CURSED", "STEALTH", "MP", "}",

    "DESC", "\n",
    "DESC_ID", "\n",
    "DESC_END"
);

sub art_to_str
{
    my ($artefact) = @_;

    my $indent = 1;

    my $str = "{\n    ";

    my $part;
    foreach $part (@art_order)
    {
        if (length($part) == 1)
        {
            if ($part eq "{")
            {
                $str .= "\n" . (" " x ($indent * 4)) . "{";
                $indent++;
                $str .= "\n" . (" " x ($indent * 4));
            }
            elsif($part eq "}")
            {
                $indent--;
                $str .= "\n" . (" " x ($indent * 4)) . "},";
                $str .= "\n" . (" " x ($indent * 4));
            }
            else
            {
                $str .= "\n" . (" " x ($indent * 4));
            }
            next;
        }

        if (!defined($field_type{$part}))
        {
            print STDERR "No field type for part '$part'\n";
            next;
        }

        if ($field_type{$part} eq "str")
        {
            my $temp = $artefact->{$part};
            $temp =~ s/"/\\"/g;
            $str .= "\"$temp\"";
        }
        else
        {
            $str .= $artefact->{$part};
        }
        $str .= ", ";
    }

    $str .= "\n},\n\n";

    return ($str);
}

sub write_data
{
    unless (open(HEADER, ">art-data.h"))
    {
        die "Couldn't open 'art-data.h' for writing: $!\n";
    }

    print HEADER <<"ENDofTEXT";
/*
 * File:       art-data.h
 * Summary:    Definitions for unrandom artefacts.
 * Written by: ????
 *
 *  Modified for Crawl Reference by \$Author: dploog \$ on \$Date: 2009-06-17 22:29:07 -0700 (Wed, 17 Jun 2009) \$
 */

/*
 * This file is automatically generated from art-data.txt via
 * util/art-data.pl.  Do not directly edit this file, but rather change
 * art-data.txt.
 *
 * If the unrandart_entry struct is changed or a new artefact property is
 * added to artefact_prop_type, then change art-data.pl so that the
 * art-data.h file it produces matches up with the new structure.
 */

#ifdef ART_DATA_H
#error "art-data.h included twice!"
#endif

#define ART_DATA_H

ENDofTEXT

    my $artefact;
    my $art_num = 1;
    foreach $artefact (@all_artefacts)
    {
        print HEADER "/* $art_num: UNRAND_$artefact->{_ENUM} */\n";
        print HEADER art_to_str($artefact);
        $art_num++;
    }

    close(HEADER);
}

sub write_enums
{
    unless (open(ENUM_IN, "<artefact.h"))
    {
        die "Couldn't open 'artefact.h' for reading: $!";
    }

    my $changed = 0;
    my $out     = "";

    while(<ENUM_IN>)
    {
        last if (/^#define NO_UNRANDARTS/);
        $out .= $_;
    }

    if (!/^#define NO_UNRANDARTS (\d+)/)
    {
        die "Couldn't find NO_UNRANDARTS in artefact.h\n";
    }

    my $orig_num = $1;
    my $new_num  = scalar(@all_artefacts);

    if ($orig_num == $new_num)
    {
        print "Number of unrandarts unchanged.\n";
        $out .= $_;
    }
    else
    {
        print "Number of unrandarts changed from $orig_num to $new_num\n";
        $out .= "#define NO_UNRANDARTS $new_num\n";
        $changed = 1;
    }

    while(<ENUM_IN>)
    {
        $out .= $_;
        last if (/^enum unrand_type/);
    }

    if (!/^enum unrand_type/)
    {
        die "Couldn't find 'enum unrand_type' in artefact.h\n";
    }

    $out .= <ENUM_IN>; # {
    $out .= <ENUM_IN>; #     UNRAND_START = 180,

    my @enum_list = ();
    while(<ENUM_IN>)
    {
        last if (/\bUNRAND_LAST\b/);
        /^\s*(\w+)/;
        push(@enum_list, $1);
    }

    <ENUM_IN>; # discard "};"

    # Suck in rest of file
    undef($/);
    my $remainder = <ENUM_IN>;
    close(ENUM_IN);

    if (@enum_list != @all_artefacts)
    {
        print "Enumeration list changed.\n";
        $changed = 1;
    }
    else
    {
        my $i;
        for ($i = 0; $i < @enum_list; $i++)
        {
            if ($enum_list[$i] ne "UNRAND_$all_artefacts[$i]->{_ENUM}")
            {
                print "Enumeration list changed.\n";
                $changed = 1;
                last;
            }
        }
    }

    if (!$changed)
    {
        print "No changes made to artefact.h\n";
        return;
    }

    print "Updating artefact.h...\n";

    unless(open(ENUM_OUT, ">artefact.h"))
    {
        die "Couldn't open 'artefact.h' for writing: $!\n";
    }

    print ENUM_OUT $out;

    my $i;
    my $longest_enum = 0;
    for ($i = 0; $i < @all_artefacts; $i++)
    {
        my $enum = $all_artefacts[$i]->{_ENUM};
        my $len  = length($enum);
        $longest_enum = $len if ($len > $longest_enum);
    }

    my $enum;
    for ($i = 0; $i < @all_artefacts; $i++)
    {
        $enum = $all_artefacts[$i]->{_ENUM};

        print ENUM_OUT "    UNRAND_$enum";

        if ($i == 0)
        {
            print ENUM_OUT " = UNRAND_START,\n";
        }
        else
        {
            print ENUM_OUT ", ";
            print ENUM_OUT " " x ($longest_enum - length($enum));
            print ENUM_OUT "// $all_artefacts[$i]->{NAME}\n";
        }
    }

    print ENUM_OUT "    UNRAND_LAST = UNRAND_$enum\n";
    print ENUM_OUT "};\n";

    print ENUM_OUT $remainder;

    close(ENUM_OUT);
}

###############################################################3
###############################################################3
###############################################################3

chdir("..")     if (-e "../art-data.txt");
chdir("source") if (-e "source/art-data.txt");

die "Couldn't find art-data.txt\n" unless (-e "art-data.txt");
die "Couldn't find artefact.h\n"   unless (-e "artefact.h");
die "Can't read art-data.txt\n"    unless (-r "art-data.txt");
die "Can't read artefact.h\n"      unless (-r "artefact.h");
die "Can't write to artefact.h\n"  unless (-w "artefact.h");
die "Can't write to art-data.h\n"  if (-e "art-data.h" && !-w "art-data.h");

unless(open(INPUT, "<art-data.txt"))
{
    die "Couldn't open art-data.txt for reading: $!\n";
}

my $prev_line = "";
my $curr_art  = {};

while (<INPUT>)
{
    chomp;
    $line_num++;

    # Skip comment-only lines
    next if (/^#/);

    # Strip comments.
    s/#.*//;

    # Strip trailing whitspace; leading whitespace indicates the
    # continuation of a string field.
    s/\s*$//;

    if ($_ =~ /^\s*$/)
    {
        if ($prev_line !~ /^\s*$/)
        {
            finish_art($curr_art);
            push(@all_artefacts, $curr_art);
            $curr_art = {};
        }
    }
    else
    {
        process_line($curr_art, $_);
    }
    $prev_line = $_;
}
close(INPUT);

if (keys(%$curr_art) > 0)
{
    finish_art($curr_art);
    push(@all_artefacts, $curr_art);
}

if (@errors > 0)
{
    print STDERR "Error(s) processing art-data.txt:\n\n";

    my $err;
    foreach $err (@errors)
    {
        print STDERR "$err\n";
    }
    exit (1);
}

write_data();
write_enums();

exit (0);
