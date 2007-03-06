/*
 *  File:       chardump.cc
 *  Summary:    Dumps character info out to the morgue file.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
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
#include "clua.h"

#include <string>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#if !defined(__IBMCPP__)
#include <unistd.h>
#endif
#include <ctype.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "debug.h"
#include "describe.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "misc.h"
#include "mutation.h"
#include "notes.h"
#include "output.h"
#include "overmap.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "shopping.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stash.h"
#include "stuff.h"
#include "version.h"
#include "view.h"

static bool dump_show_prices = false;
static bool dump_full_id     = false;

static void sdump_header(const std::string &section, std::string &text);
static void sdump_stats(const std::string &section, std::string &text);
static void sdump_location(const std::string &section, std::string &text);
static void sdump_religion(const std::string &section, std::string &text);
static void sdump_burden(const std::string &section, std::string &text);
static void sdump_hunger(const std::string &section, std::string &text);
static void sdump_transform(const std::string &section, std::string &text);
static void sdump_misc(const std::string &section, std::string &text);
static void sdump_notes(const std::string &section, std::string &text);
static void sdump_inventory(const std::string &section, std::string &text);
static void sdump_skills(const std::string &section, std::string &text);
static void sdump_spells(const std::string &section, std::string &text);
static void sdump_mutations(const std::string &section, std::string &text);
static void sdump_messages(const std::string &section, std::string &text);
static void sdump_screenshot(const std::string &section, std::string &text);
static void sdump_kills(const std::string &section, std::string &text);
static void sdump_newline(const std::string &section, std::string &text);
static void sdump_overview(const std::string &section, std::string &text);
static void sdump_separator(const std::string &section, std::string &text);
#ifdef CLUA_BINDINGS
static void sdump_lua(const std::string &section, std::string &text);
#endif
static bool write_dump(const std::string &fname, const std::string &text, 
                       bool full_id);
static void dump_stats2( std::string & text, bool calc_unid);
static void dump_stats( std::string & text );

struct dump_section_handler
{
    const char *name;
    void (*handler)(const std::string &section, std::string &text);
};

static dump_section_handler dump_handlers[] = {
    { "header",     sdump_header        },
    { "stats",      sdump_stats         },
    { "location",   sdump_location      },
    { "religion",   sdump_religion      },
    { "burden",     sdump_burden        },
    { "hunger",     sdump_hunger        },
    { "transform",  sdump_transform     },
    { "misc",       sdump_misc          },
    { "notes",      sdump_notes         },
    { "inventory",  sdump_inventory     },
    { "skills",     sdump_skills        },
    { "spells",     sdump_spells        },
    { "mutations",  sdump_mutations     },
    { "messages",   sdump_messages      },
    { "screenshot", sdump_screenshot    },
    { "kills",      sdump_kills         },
    { "overview",   sdump_overview      },

    // Conveniences for the .crawlrc artist.
    { "",           sdump_newline       },
    { "-",          sdump_separator     },

#ifdef CLUA_BINDINGS
    { NULL,         sdump_lua           }
#else
    { NULL,         NULL                }
#endif
};

static void dump_section(const std::string &section, std::string &text)
{
    for (int i = 0; ; ++i)
    {
        if (!dump_handlers[i].name || section == dump_handlers[i].name)
        {
            if (dump_handlers[i].handler)
                (*dump_handlers[i].handler)(section, text);
            break;
        }
    }
}

bool dump_char(const std::string &fname, bool show_prices, bool full_id)
{
    // start with enough room for 100 80 character lines
    std::string text;
    text.reserve(100 * 80);

    dump_show_prices = show_prices;
    dump_full_id     = full_id;

    for (int i = 0, size = Options.dump_order.size(); i < size; ++i)
    {
        const std::string &section = Options.dump_order[i];
        dump_section(section, text);
    }

    return write_dump(fname, text, full_id);
}

static void sdump_header(const std::string &, std::string &text)
{
    text += " " CRAWL " version " VERSION " character file.\n\n";
}

static void sdump_stats(const std::string &, std::string &text)
{
    if (Options.detailed_stat_dump)
        dump_stats2(text, dump_full_id);
    else
        dump_stats(text);
}

static void sdump_burden(const std::string &, std::string &text)
{
    switch (you.burden_state)
    {
    case BS_OVERLOADED:
        text += "You are overloaded with stuff.\n";
        break;
    case BS_ENCUMBERED:
        text += "You are encumbered.\n";
        break;
    default:
        break;
    }
}

static void sdump_hunger(const std::string &, std::string &text)
{
    text += std::string("You are ") + hunger_level() + ".\n\n";
}

static void sdump_transform(const std::string &, std::string &text)
{
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

        text += "\n\n";
    }
}

static void sdump_misc(const std::string &s, std::string &text)
{
    sdump_location(s, text);
    sdump_religion(s, text);
    sdump_burden(s, text);
    sdump_hunger(s, text);
    sdump_transform(s, text);
}

static void sdump_newline(const std::string &s, std::string &text)
{
    text += "\n";
}

static void sdump_separator(const std::string &s, std::string &text)
{
    text += std::string(79, '-') + "\n";
}

#ifdef CLUA_BINDINGS
// Assume this is an arbitrary Lua function name, call the function and
// dump whatever it returns.
static void sdump_lua(const std::string &s, std::string &text)
{
    std::string luatext;
    clua.callfn(s.c_str(), ">s", &luatext);
    text += luatext;
}
#endif

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
std::string munge_description(const std::string & inStr)
{
    std::string outStr;
    std::string eol = "\n";

    outStr.reserve(inStr.length() + 32);

    const long kIndent = 3;
    long lineLen = kIndent;

    long i = 0;

    outStr += std::string(kIndent, ' ');

    while (i < (long) inStr.length())
    {
        char ch = inStr[i];

        if (ch == '$')
        {
            outStr += eol;

            outStr += std::string(kIndent, ' ');
            lineLen = kIndent;

            while (inStr[++i] == '$')
                ;
        }
        else if (isspace(ch))
        {
            if (lineLen >= 79)
            {
                outStr += eol;
                outStr += std::string(kIndent, ' ');
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
                outStr += eol;
                outStr += std::string(kIndent, ' ');
                lineLen = kIndent;
            }

            outStr += word;
            lineLen += word.length();
        }
    }

    outStr += eol;

    return (outStr);
}                               // end munge_description()

static void sdump_messages(const std::string &, std::string &text)
{
    // A little message history:
    if (Options.dump_message_count > 0)
    {
        text += "Message History\n\n";
        text += get_last_messages(Options.dump_message_count);
    }
}

static void sdump_screenshot(const std::string &, std::string &text)
{
    text += screenshot();
    text += "\n\n";
}

static void dump_stats( std::string & text )
{
    char st_prn[20];

    text += you.your_name;
    text += " the ";

    text += player_title();
    text += " (";
    text += species_name(you.species, you.experience_level);
    text += ")";
    text += "\n";

    text += "(Level ";
    itoa(you.experience_level, st_prn, 10);
    text += st_prn;
    text += " ";
    text += you.class_name;
    text += ")";
    text += "\n\n";

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
        text += "\n\n";
    }

    text += "Experience : ";
    itoa(you.experience_level, st_prn, 10);
    text += st_prn;
    text += "/";
    itoa(you.experience, st_prn, 10);
    text += st_prn;
    text += "\n\n";

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
    text += "\n";

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
    text += "\n";

    text += "AC : ";
    itoa(player_AC(), st_prn, 10);
    text += st_prn;

    text += "          Evasion : ";
    itoa(player_evasion(), st_prn, 10);
    text += st_prn;

    text += "          Shield : ";
    itoa(player_shield_class(), st_prn, 10);
    text += st_prn;
    text += "\n";

    text += "GP : ";
    itoa( you.gold, st_prn, 10 );
    text += st_prn;
    text += "\n";
    text += "\n";
}                               // end dump_stats()

 //---------------------------------------------------------------
 //
 // dump_stats2
 //
 //---------------------------------------------------------------
static void dump_stats2( std::string & text, bool calc_unid)
{
    std::vector<formatted_string> vfs = get_full_detail(calc_unid);

    for (unsigned int i = 0; i < vfs.size(); i++)
    {
        text += vfs[i];
        text += '\n';
    }
    text += "\n\n";
}

static void sdump_notes(const std::string &, std::string& text)
{
    if ( note_list.size() == 0 || Options.use_notes == false )
        return;

    text += "\nNotes\n| Turn  |Location | Note\n";
    text += "--------------------------------------------------------------\n";
    for ( unsigned i = 0; i < note_list.size(); ++i ) {
        text += note_list[i].describe();
        text += "\n";
    }
    text += "\n";
}

 //---------------------------------------------------------------
 //
 // dump_location
 //
 //---------------------------------------------------------------
static void sdump_location(const std::string &, std::string & text)
{
    if (you.your_level == -1 
            && you.where_are_you == BRANCH_MAIN_DUNGEON
            && you.level_type == LEVEL_DUNGEON)
        text += "You escaped";
    else
        text += "You are " + prep_branch_level_name();

    text += ".";
    text += "\n";
}                               // end dump_location()

static void sdump_religion(const std::string &, std::string & text)
{
    if (you.religion != GOD_NO_GOD)
    {
        text += "You worship ";
        text += god_name(you.religion);
        text += ".";
        text += "\n";

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
                text += "\n";
            }
        }
        else
        {
            text += god_name(you.religion);
            text += " is demanding penance.";
            text += "\n";
        }
    }
}

extern char id[4][50];  // itemname.cc
static bool dump_item_origin(const item_def &item, int value)
{
#define fs(x) (flags & (x))
    const int flags = Options.dump_item_origins;
    if (flags == IODS_EVERYTHING)
        return (true);

    if (fs(IODS_ARTIFACTS)
            && (is_random_artefact(item) || is_fixed_artefact(item))
            && item_ident(item, ISFLAG_KNOW_PROPERTIES))
        return (true);

    if (fs(IODS_EGO_ARMOUR) && item.base_type == OBJ_ARMOUR
        && item_type_known( item ))
    {
        const int spec_ench = get_armour_ego_type( item );
        return (spec_ench != SPARM_NORMAL);
    }

    if (fs(IODS_EGO_WEAPON) && item.base_type == OBJ_WEAPONS
        && item_type_known( item ))
    {
        return (get_weapon_brand(item) != SPWPN_NORMAL);
    }

    if (fs(IODS_JEWELLERY) && item.base_type == OBJ_JEWELLERY)
        return (true);

    if (fs(IODS_RUNES) && item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_RUNE_OF_ZOT)
        return (true);

    if (fs(IODS_RODS) && item.base_type == OBJ_STAVES
            && item_is_rod(item))
        return (true);

    if (fs(IODS_STAVES) && item.base_type == OBJ_STAVES
            && !item_is_rod(item))
        return (true);

    if (fs(IODS_BOOKS) && item.base_type == OBJ_BOOKS)
        return (true);

    const int refpr = Options.dump_item_origin_price;
    if (refpr == -1)
        return (false);
    if (value == -1)
        value = item_value( item, id, false );
    return (value >= refpr);
#undef fs
}

 //---------------------------------------------------------------
 //
 // dump_inventory
 //
 //---------------------------------------------------------------
static void sdump_inventory(const std::string &, std::string & text)
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
        text += "\n";
    }
    else
    {
        text += "  Inventory:";
        text += "\n";

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
                text += "\n";

                for (j = 0; j < ENDOFPACK; j++)
                {
                    if (is_valid_item(you.inv[j]) && you.inv[j].base_type == i)
                    {
                        text += " ";

                        in_name( j, DESC_INVENTORY_EQUIP, st_pass );
                        text += st_pass;

                        inv_count--;

                        int ival = -1;
                        if (dump_show_prices)
                        {
                            text += " (";

                            itoa( ival = 
                                      item_value( you.inv[j], temp_id, true ), 
                                  tmp_quant, 10 );

                            text += tmp_quant;
                            text += " gold)";
                        }

                        if (origin_describable(you.inv[j])
                                && dump_item_origin(you.inv[j], ival))
                        {
                            text += "\n" "   (" + origin_desc(you.inv[j]) + ")";
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
                            text += "\n";
                        }
                    }
                }
            }
        }
    }
    text += "\n\n";
}                               // end dump_inventory()

//---------------------------------------------------------------
//
// dump_skills
//
//---------------------------------------------------------------
static void sdump_skills(const std::string &, std::string & text)
{
    char tmp_quant[20];

    text += " You have ";
    itoa( you.exp_available, tmp_quant, 10 );
    text += tmp_quant;
    text += " experience left.";

    text += "\n";
    text += "\n";
    text += "   Skills:";
    text += "\n";

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
            text += "\n";
        }
    }

    text += "\n";
    text += "\n";
}                               // end dump_skills()

//---------------------------------------------------------------
//
// Return string of the i-th spell type, with slash if required
//
//---------------------------------------------------------------
static std::string spell_type_shortname(int spell_class, bool slash)
{
    std::string ret;

    if (slash)
        ret = "/";

    ret += spelltype_short_name(spell_class);

    return (ret);
}                               // end spell_type_shortname()

//---------------------------------------------------------------
//
// dump_spells
//
//---------------------------------------------------------------
static void sdump_spells(const std::string &, std::string & text)
{
    char tmp_quant[20];

// This array helps output the spell types in the traditional order.
// this can be tossed as soon as I reorder the enum to the traditional order {dlb}
    const int spell_type_index[] =
    {
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

    text += "\n";

    if (!you.spell_no)
    {
        text += "You don't know any spells.";
        text += "\n";

    }
    else
    {
        text += "You know the following spells:" "\n";
        text += "\n";

        text += " Your Spells              Type           Power          Success   Level" "\n";

        for (int j = 0; j < 52; j++)
        {
            const char letter = index_to_letter( j );
            const int  spell  = get_spell_by_letter( letter );

            if (spell != SPELL_NO_SPELL)
            {
                std::string spell_line;

                spell_line += letter;
                spell_line += " - ";
                spell_line += spell_title( spell );

                if ( spell_line.length() > 24 )
                    spell_line = spell_line.substr(0, 24);
                
                for (int i = spell_line.length(); i < 26; i++) 
                    spell_line += ' ';

                bool already = false;

                for (int i = 0; spell_type_index[i] != 0; i++)
                {
                    if (spell_typematch( spell, spell_type_index[i] ))
                    {
                        spell_line += spell_type_shortname(spell_type_index[i],
                                                           already);
                        already = true;
                    }
                }

                for (int i = spell_line.length(); i < 41; ++i )
                    spell_line += ' ';

                spell_line += spell_power_to_string(calc_spell_power(spell,true));

                for (int i = spell_line.length(); i < 56; ++i )
                    spell_line += ' ';

                spell_line += failure_rate_to_string(spell_fail(spell));

                for (int i = spell_line.length(); i < 68; i++)
                    spell_line += ' ';

                itoa((int) spell_difficulty( spell ), tmp_quant, 10 );
                spell_line += tmp_quant;
                spell_line += "\n";

                text += spell_line;
            }
        }
    }
}                               // end dump_spells()


static void sdump_kills(const std::string &, std::string & text)
{
    text += you.kills.kill_info();
}

static void sdump_overview(const std::string&, std::string& text)
{
    text += formatted_string::parse_string(overview_description_string());
}

static void sdump_mutations(const std::string &, std::string & text)
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
        text += "\n";
        text += describe_mutations();
        text += "\n\n";
    }
}                               // end dump_mutations()

// ========================================================================
//      Public Functions
// ========================================================================

const char *hunger_level(void)
{
    return ((you.hunger <= 1000) ? "starving" :
             (you.hunger <= 2600) ? "hungry" :
             (you.hunger < 7000) ? "not hungry" :
             (you.hunger < 11000) ? "full" : "completely stuffed");
}

static std::string morgue_directory()
{
    std::string dir =
        !Options.morgue_dir.empty()? Options.morgue_dir :
        SysEnv.crawl_dir           ? SysEnv.crawl_dir : "";

    if (!dir.empty() && dir[dir.length() - 1] != FILE_SEPARATOR)
        dir += FILE_SEPARATOR;

    return (dir);
}

static bool write_dump(
        const std::string &fname, 
        const std::string &text, 
        bool full_id)
{
    bool succeeded = false;

    std::string file_name = morgue_directory();

    file_name += strip_filename_unsafe_chars(fname);

    std::string stash_file_name;
    stash_file_name = file_name;
    stash_file_name += ".lst";
    stashes.dump(stash_file_name.c_str(), full_id);

    file_name += ".txt";
    FILE *handle = fopen(file_name.c_str(), "w");

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "File name: %s", file_name.c_str());
#endif

    if (handle != NULL)
    {
        fputs(text.c_str(), handle);
        fclose(handle);
        succeeded = true;
    }
    else
        mprf("Error opening file '%s'", file_name.c_str());
    
    return (succeeded);
}                               // end dump_char()

void display_notes()
{
    Menu scr;
    scr.set_title( new MenuEntry("| Turn  |Location | Note"));
    for ( unsigned int i = 0; i < note_list.size(); ++i )
    {
        std::string prefix = note_list[i].describe(true, true, false);
        std::string suffix = note_list[i].describe(false, false, true);
        if ( suffix.empty() )
            continue;
        int spaceleft = get_number_of_cols() - prefix.length() - 1;
        if ( spaceleft <= 0 )
            return;
        linebreak_string(suffix, spaceleft - 4, spaceleft);
        std::vector<std::string> parts = split_string("\n", suffix);
        scr.add_entry(new MenuEntry(prefix + parts[0]));
        for ( unsigned int j = 1; j < parts.size(); ++j )
            scr.add_entry(new MenuEntry(std::string(prefix.length()-2, ' ') +
                                        std::string("| ") + parts[j]));

    }
    scr.show();
    redraw_screen();
}

void resists_screen()
{
    std::vector<formatted_string> vfs = get_full_detail(false);
    clrscr();
    gotoxy(1,1);
    textcolor(LIGHTGREY);
    
    formatted_scroller scr;
    for ( unsigned i = 0; i < vfs.size(); ++i )
        scr.add_item_formatted_string(vfs[i]);

    scr.show();
    redraw_screen();
}
