/*
 *  File:       chardump.cc
 *  Summary:    Dumps character info out to the morgue file.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *
 * <4> 19 June 2000    GDL Changed handles to FILE *
 * <3> 6/13/99         BWR Improved spell listing
 * <2> 5/30/99         JDJ dump_spells dumps failure rates (from Brent).
 * <1> 4/20/99         JDJ Reformatted, uses string objects, split out 7
 *                         functions from dump_char, dumps artifact info.
 */

#include "AppHdr.h"
#include "chardump.h"

#include <string>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#if !(defined(__IBMCPP__) || defined(__BCPLUSPLUS__))
#include <unistd.h>
#endif
#include <ctype.h>

#ifdef USE_EMX
#include <sys/types.h>
#endif

#ifdef OS9
#include <stat.h>
#else
#include <sys/stat.h>
#endif

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "debug.h"
#include "describe.h"
#include "itemname.h"
#include "items.h"
#include "macro.h"
#include "mutation.h"
#include "player.h"
#include "religion.h"
#include "shopping.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "version.h"


 // ========================================================================
 //      Internal Functions
 // ========================================================================

 // fillstring() is a hack to get around a missing constructor in
 // Borland C++ implementation of the STD basic_string.   Argh!!!
static std::string fillstring(size_t strlen, char filler)
{
    std::string s;

    for (size_t i=0; i<strlen; i++)
        s += filler;

    return s;
}

 //---------------------------------------------------------------
 //
 // munge_description
 //
 // Convert dollar signs to EOL and word wrap to 80 characters.
 // (for some obscure reason get_item_description uses dollar
 // signs instead of EOL).
 //  - It uses $ signs because they're easier to manipulate than the EOL
 //  macro, which is of uncertain length (well, that and I didn't know how
 //  to do it any better at the time) (LH)
 //---------------------------------------------------------------
static std::string munge_description(const std::string & inStr)
{
    std::string outStr;

    outStr.reserve(inStr.length() + 32);

    const long kIndent = 3;
    long lineLen = kIndent;

    long i = 0;

    outStr += fillstring(kIndent, ' ');

    while (i < (long) inStr.length())
    {
        char ch = inStr[i];

        if (ch == '$')
        {
            outStr += EOL;

            outStr += fillstring(kIndent, ' ');
            lineLen = kIndent;

            while (inStr[++i] == '$')
                ;
        }
        else if (isspace(ch))
        {
            if (lineLen >= 79)
            {
                outStr += EOL;
                outStr += fillstring(kIndent, ' ');
                lineLen = kIndent;

            }
            else if (lineLen > 0)
            {
                outStr += ch;
                ++lineLen;
            }
            ++i;
        }
        else
        {
            std::string word;

            while (i < (long) inStr.length()
                   && lineLen + (long) word.length() < 79
                   && !isspace(inStr[i]) && inStr[i] != '$')
            {
                word += inStr[i++];
            }

            if (lineLen + word.length() >= 79)
            {
                outStr += EOL;
                outStr += fillstring(kIndent, ' ');
                lineLen = kIndent;
            }

            outStr += word;
            lineLen += word.length();
        }
    }

    outStr += EOL;

    return (outStr);
}                               // end munge_description()

 //---------------------------------------------------------------
 //
 // dump_stats
 //
 //---------------------------------------------------------------
static void dump_stats( std::string & text )
{
    char st_prn[20];

    text += you.your_name;
    text += " the ";

    text += player_title();
    text += " (";
    text += species_name(you.species, you.experience_level);
    text += ")";
    text += EOL;

    text += "(Level ";
    itoa(you.experience_level, st_prn, 10);
    text += st_prn;
    text += " ";
    text += you.class_name;
    text += ")";
    text += EOL EOL;

    if (you.real_time != -1)
    {
        const time_t curr = you.real_time + (time(NULL) - you.start_time);
        char buff[200];

        make_time_string( curr, buff, sizeof(buff) );

        text += "Play time: ";
        text += buff;

        text += "       Number of turns: ";
        itoa( you.num_turns, st_prn, 10 );
        text += st_prn;
        text += EOL EOL;
    }

    text += "Experience : ";
    itoa(you.experience_level, st_prn, 10);
    text += st_prn;
    text += "/";
    itoa(you.experience, st_prn, 10);
    text += st_prn;
    text += EOL;

    text += "Strength ";
    itoa(you.strength, st_prn, 10);
    text += st_prn;
    if (you.strength < you.max_strength)
    {
        text += "/";
        itoa(you.max_strength, st_prn, 10);
        text += st_prn;
    }

    text += "     Dexterity ";
    itoa(you.dex, st_prn, 10);
    text += st_prn;
    if (you.dex < you.max_dex)
    {
        text += "/";
        itoa(you.max_dex, st_prn, 10);
        text += st_prn;
    }

    text += "     Intelligence ";
    itoa(you.intel, st_prn, 10);
    text += st_prn;
    if (you.intel < you.max_intel)
    {
        text += "/";
        itoa(you.max_intel, st_prn, 10);
        text += st_prn;
    }
    text += EOL;

    text += "Hit Points : ";
    itoa(you.hp, st_prn, 10);
    text += st_prn;

    int max_max_hp = you.hp_max + player_rotted();

    if (you.hp < you.hp_max || max_max_hp != you.hp_max)
    {
        text += "/";
        itoa(you.hp_max, st_prn, 10);
        text += st_prn;

        if (max_max_hp != you.hp_max)
        {
            text += " (";
            itoa(max_max_hp, st_prn, 10);
            text += st_prn;
            text += ")";
        }

        if (you.hp < 1)
        {
            text += " ";
            text += ((!you.deaths_door) ? "(dead)" : "(almost dead)");
        }
    }

    text += "          Magic Points : ";
    itoa(you.magic_points, st_prn, 10);
    text += st_prn;
    if (you.magic_points < you.max_magic_points)
    {
        text += "/";
        itoa(you.max_magic_points, st_prn, 10);
        text += st_prn;
    }
    text += EOL;

    text += "AC : ";
    itoa(player_AC(), st_prn, 10);
    text += st_prn;

    text += "          Evasion : ";
    itoa(player_evasion(), st_prn, 10);
    text += st_prn;

    text += "          Shield : ";
    itoa(player_shield_class(), st_prn, 10);
    text += st_prn;
    text += EOL;

    text += "GP : ";
    itoa( you.gold, st_prn, 10 );
    text += st_prn;
    text += EOL;
    text += EOL;
}                               // end dump_stats()

 //---------------------------------------------------------------
 //
 // dump_location
 //
 //---------------------------------------------------------------
static void dump_location( std::string & text )
{
    if (you.level_type != LEVEL_DUNGEON || you.your_level != -1)
        text += "You are ";

    if (you.level_type == LEVEL_PANDEMONIUM)
        text += "in Pandemonium";
    else if (you.level_type == LEVEL_ABYSS)
        text += "in the Abyss";
    else if (you.level_type == LEVEL_LABYRINTH)
        text += "in a labyrinth";
    else if (you.where_are_you == BRANCH_DIS)
        text += "in Dis";
    else if (you.where_are_you == BRANCH_GEHENNA)
        text += "in Gehenna";
    else if (you.where_are_you == BRANCH_VESTIBULE_OF_HELL)
        text += "in the Vestibule of Hell";
    else if (you.where_are_you == BRANCH_COCYTUS)
        text += "in Cocytus";
    else if (you.where_are_you == BRANCH_TARTARUS)
        text += "in Tartarus";
    else if (you.where_are_you == BRANCH_INFERNO)
        text += "in the Inferno";
    else if (you.where_are_you == BRANCH_THE_PIT)
        text += "in the Pit";
    else if (you.where_are_you == BRANCH_ORCISH_MINES)
        text += "in the Mines";
    else if (you.where_are_you == BRANCH_HIVE)
        text += "in the Hive";
    else if (you.where_are_you == BRANCH_LAIR)
        text += "in the Lair";
    else if (you.where_are_you == BRANCH_SLIME_PITS)
        text += "in the Slime Pits";
    else if (you.where_are_you == BRANCH_VAULTS)
        text += "in the Vaults";
    else if (you.where_are_you == BRANCH_CRYPT)
        text += "in the Crypt";
    else if (you.where_are_you == BRANCH_HALL_OF_BLADES)
        text += "in the Hall of Blades";
    else if (you.where_are_you == BRANCH_HALL_OF_ZOT)
        text += "in the Hall of Zot";
    else if (you.where_are_you == BRANCH_ECUMENICAL_TEMPLE)
        text += "in the Ecumenical Temple";
    else if (you.where_are_you == BRANCH_SNAKE_PIT)
        text += "in the Snake Pit";
    else if (you.where_are_you == BRANCH_ELVEN_HALLS)
        text += "in the Elven Halls";
    else if (you.where_are_you == BRANCH_TOMB)
        text += "in the Tomb";
    else if (you.where_are_you == BRANCH_SWAMP)
        text += "in the Swamp";
    else
    {
        if (you.your_level == -1)
            text += "You escaped";
        else
        {
            text += "on level ";

            char st_prn[20];
            itoa(you.your_level + 1, st_prn, 10);
            text += st_prn;
        }
    }

    text += ".";
    text += EOL;
}                               // end dump_location()

 //---------------------------------------------------------------
 //
 // dump_religion
 //
 //---------------------------------------------------------------
static void dump_religion( std::string & text )
{
    if (you.religion != GOD_NO_GOD)
    {
        text += "You worship ";
        text += god_name(you.religion);
        text += ".";
        text += EOL;

        if (!player_under_penance())
        {
            if (you.religion != GOD_XOM)
            {                   // Xom doesn't care
                text += god_name(you.religion);
                text += " is ";
                text += ((you.piety <= 5) ? "displeased" :
                         (you.piety <= 20) ? "noncommittal" :
                         (you.piety <= 40) ? "pleased with you" :
                         (you.piety <= 70) ? "most pleased with you" :
                         (you.piety <= 100) ? "greatly pleased with you" :
                         (you.piety <= 130) ? "extremely pleased with you"
                         : "exalted by your worship");
                text += ".";
                text += EOL;
            }
        }
        else
        {
            text += god_name(you.religion);
            text += " is demanding penance.";
            text += EOL;
        }
    }
}                               // end dump_religion()

 //---------------------------------------------------------------
 //
 // dump_inventory
 //
 //---------------------------------------------------------------
static void dump_inventory( std::string & text, bool show_prices )
{
    int i, j;
    char temp_id[4][50];

    std::string text2;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 50; j++)
        {
            temp_id[i][j] = 1;
        }
    }

    char st_pass[ ITEMNAME_SIZE ] = "";
    int inv_class2[OBJ_GOLD];
    int inv_count = 0;
    char tmp_quant[20];

    for (i = 0; i < OBJ_GOLD; i++)
    {
        inv_class2[i] = 0;
    }

    for (i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item( you.inv[i] ))
        {
            // adds up number of each class in invent.
            inv_class2[you.inv[i].base_type]++;     
            inv_count++;
        }
    }

    if (!inv_count)
    {
        text += "You aren't carrying anything.";
        text += EOL;
    }
    else
    {
        text += "  Inventory:";
        text += EOL;

        for (i = 0; i < OBJ_GOLD; i++)
        {
            if (inv_class2[i] != 0)
            {
                switch (i)
                {
                case OBJ_WEAPONS:    text += "Hand weapons";    break;
                case OBJ_MISSILES:   text += "Missiles";        break;
                case OBJ_ARMOUR:     text += "Armour";          break;
                case OBJ_WANDS:      text += "Magical devices"; break;
                case OBJ_FOOD:       text += "Comestibles";     break;
                case OBJ_SCROLLS:    text += "Scrolls";         break;
                case OBJ_JEWELLERY:  text += "Jewellery";       break;
                case OBJ_POTIONS:    text += "Potions";         break;
                case OBJ_BOOKS:      text += "Books";           break;
                case OBJ_STAVES:     text += "Magical staves";  break;
                case OBJ_ORBS:       text += "Orbs of Power";   break;
                case OBJ_MISCELLANY: text += "Miscellaneous";   break;
                case OBJ_CORPSES:    text += "Carrion";         break;

                default:
                    DEBUGSTR("Bad item class");
                }
                text += EOL;

                for (j = 0; j < ENDOFPACK; j++)
                {
                    if (is_valid_item(you.inv[j]) && you.inv[j].base_type == i)
                    {
                        text += " ";

                        in_name( j, DESC_INVENTORY_EQUIP, st_pass );
                        text += st_pass;

                        inv_count--;

                        if (show_prices)
                        {
                            text += " (";

                            itoa( item_value( you.inv[j], temp_id, true ), 
                                  tmp_quant, 10 );

                            text += tmp_quant;
                            text += " gold)";
                        }

                        if (is_dumpable_artifact( you.inv[j], 
                                                  Options.verbose_dump ))
                        {
                            text2 = get_item_description( you.inv[j], 
                                                          Options.verbose_dump,
                                                          true );

                            text += munge_description(text2);
                        }
                        else
                        {
                            text += EOL;
                        }
                    }
                }
            }
        }
    }
}                               // end dump_inventory()

//---------------------------------------------------------------
//
// dump_skills
//
//---------------------------------------------------------------
static void dump_skills( std::string & text )
{
    char tmp_quant[20];

    text += EOL;
    text += EOL;
    text += "   Skills:";
    text += EOL;

    for (unsigned char i = 0; i < 50; i++)
    {
        if (you.skills[i] > 0)
        {
            text += ( (you.skills[i] == 27)   ? " * " :
                      (you.practise_skill[i]) ? " + " 
                                              : " - " );

            text += "Level ";
            itoa( you.skills[i], tmp_quant, 10 );
            text += tmp_quant;
            text += " ";
            text += skill_name(i);
            text += EOL;
        }
    }

    text += EOL;
    text += EOL;
}                               // end dump_skills()

//---------------------------------------------------------------
//
// Return string of the i-th spell type, with slash if required
//
//---------------------------------------------------------------
static std::string spell_type_name(int spell_class, bool slash)
{
    std::string ret;

    if (slash)
        ret = "/";

    ret += spelltype_name(spell_class);

    return (ret);
}                               // end spell_type_name()

//---------------------------------------------------------------
//
// dump_spells
//
//---------------------------------------------------------------
static void dump_spells( std::string & text )
{
    char tmp_quant[20];

// This array helps output the spell types in the traditional order.
// this can be tossed as soon as I reorder the enum to the traditional order {dlb}
    const int spell_type_index[] = {
        SPTYP_HOLY,
        SPTYP_POISON,
        SPTYP_FIRE,
        SPTYP_ICE,
        SPTYP_EARTH,
        SPTYP_AIR,
        SPTYP_CONJURATION,
        SPTYP_ENCHANTMENT,
        SPTYP_DIVINATION,
        SPTYP_TRANSLOCATION,
        SPTYP_SUMMONING,
        SPTYP_TRANSMIGRATION,
        SPTYP_NECROMANCY,
        0
    };

    int spell_levels = player_spell_levels();

    if (spell_levels == 1)
        text += "You have one spell level left.";
    else if (spell_levels == 0)
        text += "You cannot memorise any spells.";
    else
    {
        text += "You have ";
        itoa( spell_levels, tmp_quant, 10 );
        text += tmp_quant;
        text += " spell levels left.";
    }

    text += EOL;

    if (!you.spell_no)
    {
        text += "You don't know any spells.";
        text += EOL;

    }
    else
    {
        text += "You know the following spells:" EOL;
        text += EOL;

        text += "  Your Spells                       Type                  Success   Level" EOL;

        for (int j = 0; j < 52; j++)
        {
            const char letter = index_to_letter( j );
            const int  spell  = get_spell_by_letter( letter );

            if (spell != SPELL_NO_SPELL)
            {
                std::string spell_line = " ";

                char strng[2];
                strng[0] = letter;
                strng[1] = '\0';

                spell_line += strng;
                spell_line += " - ";
                spell_line += spell_title( spell );

                for (int i = spell_line.length(); i < 34; i++)
                {
                    spell_line += ' ';
                }

                bool already = false;

                for (int i = 0; spell_type_index[i] != 0; i++)
                {
                    if (spell_typematch( spell, spell_type_index[i] ))
                    {
                        spell_line += 
                                spell_type_name(spell_type_index[i], already);
                        already = true;
                    }
                }

                for (int i = spell_line.length(); i < 58; i++)
                {
                    spell_line += ' ';
                }

                int fail_rate = spell_fail( spell );

                spell_line += (fail_rate == 100) ? "Useless"   :
                              (fail_rate >   90) ? "Terrible"  :
                              (fail_rate >   80) ? "Cruddy"    :
                              (fail_rate >   70) ? "Bad"       :
                              (fail_rate >   60) ? "Very Poor" :
                              (fail_rate >   50) ? "Poor"      :
                              (fail_rate >   40) ? "Fair"      :
                              (fail_rate >   30) ? "Good"      :
                              (fail_rate >   20) ? "Very Good" :
                              (fail_rate >   10) ? "Great"     :
                              (fail_rate >   0)  ? "Excellent" 
                                                 : "Perfect";

                for (int i = spell_line.length(); i < 70; i++)
                    spell_line += ' ';

                itoa((int) spell_difficulty( spell ), tmp_quant, 10 );
                spell_line += tmp_quant;
                spell_line += EOL;

                text += spell_line;
            }
        }
    }
}                               // end dump_spells()

//---------------------------------------------------------------
//
// dump_mutations
//
//---------------------------------------------------------------
static void dump_mutations( std::string & text )
{
    // Can't use how_mutated() here, as it doesn't count demonic powers
    int xz = 0;

    for (int xy = 0; xy < 100; xy++)
    {
        if (you.mutation[xy] > 0)
            xz++;
    }

    if (xz > 0)
    {
        text += "";
        text += EOL;
        text += "           Mutations & Other Weirdness";
        text += EOL;

        for (int j = 0; j < 100; j++)
        {
            if (you.mutation[j])
            {
                if (you.demon_pow[j] > 0)
                    text += "* ";

                text += mutation_name(j);
                text += EOL;
            }
        }
    }
}                               // end dump_mutations()

#if MAC
#pragma mark -
#endif

// ========================================================================
//      Public Functions
// ========================================================================

//---------------------------------------------------------------
//
// dump_char
//
// Creates a disk record of a character. Returns true if the
// character was successfully saved.
//
//---------------------------------------------------------------
bool dump_char( const char fname[30], bool show_prices )  // $$$ a try block?
{
    bool succeeded = false;

    std::string text;

    // start with enough room for 100 80 character lines
    text.reserve(100 * 80);

    text += " Dungeon Crawl version " VERSION " character file.";
    text += EOL;
    text += EOL;

    dump_stats(text);
    dump_location(text);
    dump_religion(text);

    switch (you.burden_state)
    {
    case BS_OVERLOADED:
        text += "You are overloaded with stuff.";
        text += EOL;
        break;
    case BS_ENCUMBERED:
        text += "You are encumbered.";
        text += EOL;
        break;
    }

    text += "You are ";

    text += ((you.hunger <= 1000) ? "starving" :
             (you.hunger <= 2600) ? "hungry" :
             (you.hunger < 7000) ? "not hungry" :
             (you.hunger < 11000) ? "full" : "completely stuffed");

    text += ".";
    text += EOL;
    text += EOL;

    if (you.attribute[ATTR_TRANSFORMATION])
    {
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_SPIDER:
            text += "You are in spider-form.";
            break;
        case TRAN_BLADE_HANDS:
            text += "Your hands are blades.";
            break;
        case TRAN_STATUE:
            text += "You are a stone statue.";
            break;
        case TRAN_ICE_BEAST:
            text += "You are a creature of crystalline ice.";
            break;
        case TRAN_DRAGON:
            text += "You are a fearsome dragon!";
            break;
        case TRAN_LICH:
            text += "You are in lich-form.";
            break;
        case TRAN_SERPENT_OF_HELL:
            text += "You are a huge, demonic serpent!";
            break;
        case TRAN_AIR:
            text += "You are a cloud of diffuse gas.";
            break;
        }

        text += EOL;
        text += EOL;
    }

    dump_inventory(text, show_prices);

    char tmp_quant[20];

    text += EOL;
    text += EOL;
    text += " You have ";
    itoa( you.exp_available, tmp_quant, 10 );
    text += tmp_quant;
    text += " experience left.";

    dump_skills(text);
    dump_spells(text);
    dump_mutations(text);

    char file_name[kPathLen] = "\0";

    if (SysEnv.crawl_dir)
        strncpy(file_name, SysEnv.crawl_dir, kPathLen);

    strncat(file_name, fname, kPathLen);

    if (strcmp(fname, "morgue.txt") != 0)
        strncat(file_name, ".txt", kPathLen);

    FILE *handle = fopen(file_name, "wb");

#if DEBUG_DIAGNOSTICS
    strcpy( info, "File name: " );
    strcat( info, file_name );
    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (handle != NULL)
    {
        size_t begin = 0;
        size_t end = text.find(EOL);

        while (end != std::string::npos)
        {
            end += strlen(EOL);

            size_t len = end - begin;

            if (len > 80)
                len = 80;

            fwrite(text.c_str() + begin, len, 1, handle);

            begin = end;
            end = text.find(EOL, end);
        }

        fclose(handle);
        succeeded = true;
    }
    else
        mpr("Error opening file.");
    
    return (succeeded);
}                               // end dump_char()
