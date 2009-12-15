#!/usr/bin/perl -w

use strict;

my $line_num      = 0;
my @warnings      = ();
my @errors        = ();
my @all_artefacts = ();
my %used_names    = ();
my %used_appears  = ();
my %used_enums    = ();
my %found_funcs   = ();

my %field_type = (
    AC       => "num",
    ACC      => "num",
    ANGRY    => "num",
    APPEAR   => "str",
    BERSERK  => "bool",
    BLINK    => "bool",
    BRAND    => "enum",
    CHAOTIC  => "bool",
    COLD     => "num",
    COLOUR   => "enum",
    CURSED   => "num",
    DAM      => "num",
    DEX      => "num",
    ELEC     => "bool",
    EV       => "num",
    EVIL     => "bool",
    FIRE     => "num",
    HOLY     => "bool",
    INT      => "num",
    INV      => "bool",
    LEV      => "bool",
    LIFE     => "bool",
    MAGIC    => "num",
    METAB    => "num",
    MP       => "num",
    MUTATE   => "num",
    NAME     => "str",
    NOISES   => "bool",
    NOSPELL  => "bool",
    NOTELEP  => "bool",
    POISON   => "bool",
    RND_TELE => "bool",
    SEEINV   => "bool",
    SPECIAL  => "bool",
    STEALTH  => "num",
    STR      => "num",
    VALUE    => "num",

    DESC     => "str",
    DESC_END => "str",
    DESC_ID  => "str",
    TILE     => "str",
    TILE_EQ  => "str",
    TILERIM  => "bool",

    UNUSED   => "unused",

    flags     => "flags",

    equip_func        => "func",
    unequip_func      => "func",
    world_reacts_func => "func",
    fight_func_func   => "func",
    melee_effect_func => "func",
    launch_func       => "func",
    evoke_func        => "func",

    plus      => "num",
    plus2     => "num",
    base_type => "enum",
    sub_type  => "enum"
);

my %union_name = (
    melee_effect => "fight_func",
    launch       => "fight_func"
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

    $artefact->{_ERROR} = 1;

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

    # Handled later
    $artefact->{flags} = "";

    # Default ego.
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

    # Fill in function pointers.
    my $enum = $artefact->{_ENUM};
    my $funcs;
    if ($found_funcs{$enum})
    {
        $funcs = $found_funcs{$enum};
        delete($found_funcs{$enum});
    }
    else
    {
        $funcs = {};
    }

    foreach my $func_name ("equip", "unequip", "world_reacts", "fight_func",
                           "evoke")
    {
        my $val;
        if ($funcs->{$func_name})
        {
            $val = "_${enum}_" . $funcs->{$func_name};
        }
        else
        {
            $val = "NULL";
        }
        $artefact->{"${func_name}_func"} = $val;
    }

    # Default values.
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
            elsif($type eq "num" || $type eq "bool")
            {
                $artefact->{$field} = "0";
            }
            elsif($type eq "enum")
            {
                error($artefact, "No enumeration for field '$field'");
            }
            elsif($type eq "func")
            {
                $artefact->{$field} = "NULL";
            }
            elsif ($type eq "unused")
            {
                $artefact->{$field} = "0";
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

    my $flags = "";
    my $flag;
    foreach $flag ("SPECIAL", "HOLY", "EVIL", "CHAOTIC")
    {
        if ($artefact->{$flag})
        {
            $flags .= "UNRAND_FLAG_$flag | ";
        }
    }

    if ($flags eq "")
    {
        $flags = "UNRAND_FLAG_NONE";
    }
    else
    {
        chop($flags);
        chop($flags);
        chop($flags);
    }
    $artefact->{flags} = $flags;

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

    $artefact->{_PREV_FIELD} = $field;
    $artefact->{$field}      = $value;

    if ($value eq "")
    {
        error($artefact, "Field '$field' has no value");
        return;
    }

    if ($field eq "BOOL")
    {
        my @parts = split(/\s*,\s*/, $value);
        my $part;
        foreach $part (@parts)
        {
            my $up = uc($part);
            if ($up eq "CURSED")
            {
                # Start out cursed, but don't re-curse.
                $artefact->{CURSED} = -1;
            }
            elsif (!exists($field_type{$up}))
            {
                error($artefact, "Unknown bool '$part'");
            }
            elsif ($field_type{$up} ne "bool")
            {
                error($artefact, "'$part' is not a boolean");
            }
            else
            {
                $artefact->{$up} = 1;
            }
        }
    }
    elsif ($field eq "OBJ")
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

        if ($field_type{$field} eq "bool")
        {
            error($artefact, "'$field' should be expressed using BOOL");
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
            error($artefact, "ENUM must be before NAME");
            return;
        }
        $enum = "$value";
    }

    if ($enum ne "")
    {
        if (exists($used_enums{$enum}))
        { error($artefact, "Enum \"$enum\" already used by artefact " .
                             "\"$used_enums{$enum}\"");
            return;
        }

        $used_enums{$enum} = $value;
        $artefact->{_ENUM} = $enum;
    }
}

my @art_order = (
    "NAME", "APPEAR", "\n",
    "base_type", "sub_type", "plus", "plus2", "COLOUR", "VALUE", "\n",
    "flags",

    "{", "BRAND", "AC", "EV", "STR", "INT", "DEX", "\n",
    "FIRE", "COLD", "ELEC", "POISON", "LIFE", "MAGIC", "\n",
    "SEEINV", "INV", "LEV", "BLINK", "BERSERK",  "NOISES", "\n",
    "NOSPELL", "RND_TELE", "NOTELEP", "ANGRY", "METAB", "\n",
    "MUTATE", "ACC", "DAM", "CURSED", "STEALTH", "MP", "\n",
    "}",

    "DESC", "\n",
    "DESC_ID", "\n",
    "DESC_END", "\n",

    "equip_func", "unequip_func", "world_reacts_func", "{fight_func_func",
    "evoke_func"
);

sub art_to_str
{
    my ($artefact) = @_;

    my $indent = 1;

    my $str = "{\n    ";

    for (my $i = 0; $i < @art_order; $i++)
    {
        my $part = $art_order[$i];

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

        my $bracketed = 0;
        if ($part =~ /^{(.*)/)
        {
            $part = $1;
            $str .= "{ ";
            $bracketed = 1;
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

        $str .= " }" if ($bracketed);

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

#ifndef ART_FUNC_H
#error "art-func.h must be included before art-data.h"
#endif

#define ART_DATA_H

ENDofTEXT

    my $artefact;
    foreach $artefact (@all_artefacts)
    {
        print HEADER "/* UNRAND_$artefact->{_ENUM} */\n";
        print HEADER art_to_str($artefact);
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

sub write_tiles
{
    my $tilefile = "dc-unrand.txt";
    print "Updating $tilefile...\n";

    die "Can't write to $tilefile\n"  if (-e $tilefile && !-w $tilefile);
    unless (open(TILES, ">$tilefile"))
    {
        die "Couldn't open '$tilefile' for writing: $!\n";
    }

    my %art_by_type = ();
    foreach my $artefact (@all_artefacts)
    {
        next if ($artefact->{NAME} =~ /DUMMY/);

        if ($artefact->{TILE} eq "")
        {
            print STDERR "No TILE defined for '$artefact->{NAME}'\n";
            next;
        }

        # The path always has the form /item/$folder/artefact.
        my $type   = $artefact->{base_type} || "";
        my $folder = "";
        if ($type eq "OBJ_WEAPONS")
        {
            $folder = "weapon";
        }
        elsif ($type eq "OBJ_ARMOUR")
        {
            $folder = "armour";
        }
        elsif ($type eq "OBJ_JEWELLERY")
        {
            if ($artefact->{sub_type} =~ /RING_/)
            {
                $folder = "ring";
            }
            else
            {
                $folder = "amulet";
            }
        }
        else
        {
            next;
        }

        my $definition = "$artefact->{TILE} UNRAND_$artefact->{_ENUM}";
        my $needrim    = ($artefact->{TILERIM} ? "1" : "0");
        if (defined $art_by_type{$folder})
        {
            if (defined $art_by_type{$folder}{$needrim})
            {
                push @{$art_by_type{$folder}{$needrim}}, $definition;
            }
            else
            {
                $art_by_type{$folder}{$needrim} = [$definition];
            }
        }
        else
        {
            $art_by_type{$folder} = {$needrim => [$definition]};
        }
    }

    print TILES << "HEADER_END";
# This file is automatically generated from source/art-data.txt via
# util/art-data.pl.  Do not directly edit this file, but rather change
# art-data.txt.

HEADER_END

    # Output the tile definitions sorted by type (and thus path).
    foreach my $type (keys %art_by_type)
    {
        print TILES "%sdir item/$type/artefact\n";

        foreach my $needrim (sort keys %{$art_by_type{$type}})
        {
            print TILES "%rim 1\n" if ($needrim);
            foreach my $def (@{$art_by_type{$type}{$needrim}})
            {
                print TILES "$def\n";
            }
            print TILES "%rim 0\n" if ($needrim);
        }
        print TILES "\n";
    }
    close(TILES);

    my %parts;
    foreach my $artefact (@all_artefacts)
    {
        next if ($artefact->{NAME} =~ /DUMMY/);
        if (not(defined $artefact->{TILE_EQ}) || $artefact->{TILE_EQ} eq "")
        {
            if ($artefact->{base_type} ne "OBJ_JEWELLERY")
            {
                print STDERR "TILE_EQ not defined for '$artefact->{NAME}'\n";
            }
            next;
        }

        my $part = "BODY";
        if ($artefact->{base_type} eq "OBJ_WEAPONS")
        {
            $part = "HAND1";
        }
        elsif ($artefact->{base_type} ne "OBJ_ARMOUR")
        {
            next;
        }
        elsif ($artefact->{sub_type} =~ /_SHIELD/
               || $artefact->{sub_type} =~ /_BUCKLER/)
        {
            $part = "HAND2";
        }
        elsif ($artefact->{sub_type} =~ /_CLOAK/)
        {
            $part = "CLOAK";
        }
        elsif ($artefact->{sub_type} =~ /_CAP/
               || $artefact->{sub_type} =~ /_HAT/)
        {
            $part = "HELM";
        }
        elsif ($artefact->{sub_type} =~ /_SHIELD/)
        {
            $part = "HAND2";
        }
        elsif ($artefact->{sub_type} =~ /_BOOTS/)
        {
            $part = "BOOTS";
        }
        elsif ($artefact->{sub_type} =~ /_GLOVES/)
        {
            $part = "ARM";
        }

        if (defined $parts{$part})
        {
            push @{$parts{$part}}, $artefact;
        }
        else
        {
            $parts{$part} = [$artefact];
        }
    }

    $tilefile = "dc-player.txt";
    unless (open(TILES, "<$tilefile"))
    {
        die "Couldn't open '$tilefile' for reading: $!\n";
    }

    my $curr_part = "";
    my $content = <TILES>;
    my @lines   = split "\n", $content;
    foreach my $line (@lines)
    {
        if ($line =~ /parts_ctg\s+(\S+)/)
        {
            $curr_part = $1;
            next;
        }
        next if (not defined $parts{$curr_part});

        if ($line =~ /^(\S+)\s+(\S+)/)
        {
            my $name = $1;
            my $enum = $2;

            foreach my $art (@{$parts{$curr_part}})
            {
                if ($art->{TILE_EQ} eq $name)
                {
                    $art->{TILE_EQ_ENUM} = "TILEP_".$curr_part."_".$enum;
                    # Don't break from the loop in case several artefacts
                    # share the same tile.
                }
            }
        }
    }
    close(TILES);

    # Create tiledef-unrand.cc for the function unrandart_to_tile().
    # Should we also create tiledef-unrand.h this way?
    $tilefile = "tiledef-unrand.cc";
    print "Updating $tilefile...\n";

    die "Can't write to $tilefile\n"  if (-e $tilefile && !-w $tilefile);
    unless (open(TILES, ">$tilefile"))
    {
        die "Couldn't open '$tilefile' for writing: $!\n";
    }

    print TILES << "HEADER_END";
/*
 * This file is automatically generated from source/art-data.txt via
 * util/art-data.pl.  Do not directly edit this file, but rather change
 * art-data.txt.
 */

#include "AppHdr.h"
#include "tiledef-unrand.h"

#include "artefact.h"
#include "tiledef-main.h"
#include "tiledef-player.h"

int unrandart_to_tile(int unrand)
{
    switch (unrand)
    {
HEADER_END

    my $longest_enum = 0;
    foreach my $artefact (@all_artefacts)
    {
        my $enum = $artefact->{_ENUM};
        my $len  = length($enum);
        $longest_enum = $len if ($len > $longest_enum);
    }
    $longest_enum += length("UNRAND_");

    foreach my $artefact (@all_artefacts)
    {
        next if ($artefact->{NAME} =~ /DUMMY/);
        next if ($artefact->{TILE} eq "");

        my $enum = "UNRAND_$artefact->{_ENUM}";
        print TILES (" " x 4) . "case $enum:"
            . " " x ($longest_enum - length($enum) + 2) . "return TILE_$enum;\n";
    }
    print TILES (" " x 4) . "default: return -1;\n";
    print TILES (" " x 4) . "}\n";
    print TILES "}\n\n";

    print TILES "int unrandart_to_doll_tile(int unrand)\n{\n";
    print TILES (" " x 4) . "switch (unrand)\n";
    print TILES (" " x 4) . "{\n";
    foreach my $part (sort keys %parts)
    {
        print TILES (" " x 4) . "// $part\n";
        foreach my $artefact (@{$parts{$part}})
        {
            if (not defined $artefact->{TILE_EQ_ENUM})
            {
                print STDERR "Tile '$artefact->{TILE_EQ}' for part '$part' not "
                           . "found in 'dc-player.txt'.\n";
                next;
            }
            my $enum   = "UNRAND_$artefact->{_ENUM}";
            my $t_enum = $artefact->{TILE_EQ_ENUM};
            print TILES (" " x 4) . "case $enum:"
                . " " x ($longest_enum - length($enum) + 2) . "return $t_enum;\n";
        }
    }
    print TILES (" " x 4) . "default: return -1;\n";
    print TILES (" " x 4) . "}\n";
    print TILES "}\n\n";
    close(TILES);
}

my %valid_func = (
    equip        => 1,
    unequip      => 1,
    world_reacts => 1,
    melee_effect => 1,
    evoke        => 1
);

sub read_funcs
{
    unless(open(INPUT, "<art-func.h"))
    {
        die "Couldn't open art-func.h for reading: $!\n";
    }

    while(<INPUT>)
    {
        if (/^static .* _([A-Z_]+)_(\S+)\s*\(/)
        {
            my $enum = $1;
            my $func = $2;

            if (!$valid_func{$func})
            {
                push(@warnings, "Unrecognized func '$func' for artefact " .
                                "'$enum'");
                next;
            }

            $found_funcs{$enum} ||= {};
            my $func_list = $found_funcs{$enum};

            my $key = $union_name{$func} || $func;
            $func_list->{$key} = $func;
        }
    }
    close(INPUT);
}

sub read_data
{
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
}

###############################################################3
###############################################################3
###############################################################3

chdir("..")     if (-e "../art-data.txt");
chdir("source") if (-e "source/art-data.txt");

die "Couldn't find art-data.txt\n" unless (-e "art-data.txt");
die "Couldn't find art-func.h\n"   unless (-e "art-func.h");
die "Couldn't find artefact.h\n"   unless (-e "artefact.h");
die "Can't read art-data.txt\n"    unless (-r "art-data.txt");
die "Can't read art-func.h\n"      unless (-r "art-func.h");
die "Can't read artefact.h\n"      unless (-r "artefact.h");
die "Can't write to artefact.h\n"  unless (-w "artefact.h");
die "Can't write to art-data.h\n"  if (-e "art-data.h" && !-w "art-data.h");

read_funcs();
read_data();

if (keys(%found_funcs) > 0)
{
    my $key;
    foreach $key (keys(%found_funcs))
    {
        push(@warnings, "Funcs for unknown artefact enum $key: " .
                        join(", ", keys(%{$found_funcs{$key}})));
    }
}

if (@warnings > 0)
{
    print STDERR "Warning(s):\n";

    my $warn;
    foreach $warn (@warnings)
    {
        print STDERR "$warn\n";
    }
    print STDERR "\n";
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

chdir("rltiles");
write_tiles();

exit (0);
