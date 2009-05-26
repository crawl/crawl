/*
 *  File:       command.cc
 *  Summary:    Misc commands.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "command.h"

#include <stdio.h>
#include <string.h>
#include <sstream>
#include <ctype.h>

#include "externs.h"

#include "abl-show.h"
#include "branch.h"
#include "chardump.h"
#include "cio.h"
#include "database.h"
#include "debug.h"
#include "decks.h"
#include "describe.h"
#include "files.h"
#include "ghost.h"
#include "initfile.h"
#include "invent.h"
#include "itemname.h"
#include "item_use.h"
#include "items.h"
#include "libutil.h"
#include "menu.h"
#include "message.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "ouch.h"
#include "place.h"
#include "player.h"
#include "quiver.h"
#include "religion.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
#include "version.h"
#include "view.h"

static void _adjust_item(void);
static void _adjust_spells(void);
static void _adjust_ability(void);

static const char *features[] = {
#ifdef CLUA_BINDINGS
    "Lua user scripts",
#endif

#ifdef USE_TILE
    "Tile support",
#endif

#ifdef WIZARD
    "Wizard mode",
#endif

#ifdef DEBUG
    "Debug mode",
#endif

#if defined(REGEX_POSIX)
    "POSIX regexps",
#elif defined(REGEX_PCRE)
    "PCRE regexps",
#else
    "Glob patterns",
#endif

#if defined(SOUND_PLAY_COMMAND) || defined(WINMM_PLAY_SOUNDS)
    "Sound support",
#endif

#ifdef DGL_MILESTONES
    "Milestones",
#endif

#ifdef UNICODE_GLYPHS
    "Unicode glyphs",
#endif
};

static std::string _get_version_information(void)
{
    std::string result  = "This is <w>" CRAWL " " VERSION "</w> (";
#ifdef DISPLAY_BUILD_REVISION
                result += "r" + number_to_string(svn_revision()) + ", ";
#endif
                result += VERSION_DETAIL ").";

    result += "\n";

    return (result);
}

static std::string _get_version_features(void)
{
    std::string result  = "<w>Features</w>" EOL;
                result += "--------" EOL;

    for (unsigned int i = 0; i < ARRAYSZ(features); i++)
    {
        result += " * ";
        result += features[i];
        result += EOL;
    }

    return (result);
}

static void _add_file_to_scroller(FILE* fp, formatted_scroller& m,
                                  int first_hotkey  = 0,
                                  bool auto_hotkeys = false);

static std::string _get_version_changes(void)
{
    // Attempts to print "Highlights" of the latest version.
    FILE* fp = fopen(datafile_path("changes.stone_soup", false).c_str(), "r");
    if (!fp)
        return "";

    std::string result = "";
    std::string help;
    char buf[200];
    bool start = false;
    while (fgets(buf, sizeof buf, fp))
    {
        // Remove trailing spaces.
        for (int i = strlen(buf) - 1; i >= 0; i++)
        {
            if (isspace( buf[i] ))
                buf[i] = 0;
            else
                break;
        }
        help = buf;

        // Give up if you encounter an older version.
        if (help.find("Stone Soup 0.4") != std::string::npos)
            break;

        if (help.find("Highlights") != std::string::npos)
        {
            // Highlight the Highlights, so to speak.
            std::string text  = "<w>";
                        text += buf;
                        text += "</w>";
                        text += EOL;
            result += text;
            // And start printing from now on.
            start = true;
        }
        else if (!start)
            continue;
        else if (buf[0] == 0)
        {
            // Stop reading and copying text with the first empty line
            // following the Highlights section.
            break;
        }
        else
        {
            result += buf;
            result += EOL;
        }
    }
    fclose(fp);

    // Did we ever get to print the Highlights?
    if (start)
    {
        result += EOL;
        result += "For a more complete list of changes, see changes.stone_soup "
                  "in the " EOL
                  "/docs folder.";
    }
    else
    {
        result += "For a list of changes, see changes.stone_soup in the /docs "
                  "folder.";
    }

    result += "\n\n";

    return (result);
}

//#define DEBUG_FILES
static void _print_version(void)
{
    formatted_scroller cmd_version;

    // Set flags.
    int flags = MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP | MF_EASY_EXIT;
    cmd_version.set_flags(flags, false);
    cmd_version.set_tag("version");

    // FIXME: Allow for hiding Page down when at the end of the listing, ditto
    // for page up at start of listing.
    cmd_version.set_more( formatted_string::parse_string(
#ifdef USE_TILE
                              "<cyan>[ +/L-click : Page down.   - : Page up."
                              "           Esc/R-click exits.]"));
#else
                              "<cyan>[ + : Page down.   - : Page up."
                              "                           Esc exits.]"));
#endif

    cmd_version.add_text(_get_version_information());
    cmd_version.add_text(_get_version_features());
    cmd_version.add_text(_get_version_changes());

    std::string fname = "key_changes.txt";
    // Read in information about changes in comparison to the latest version.
    FILE* fp = fopen(datafile_path(fname, false).c_str(), "r");

#if defined(DOS)
    if (!fp)
    {
 #ifdef DEBUG_FILES
        mprf(MSGCH_DIAGNOSTICS, "File '%s' could not be opened.",
             fname.c_str());
 #endif
        if (get_dos_compatible_file_name(&fname))
        {
 #ifdef DEBUG_FILES
            mprf(MSGCH_DIAGNOSTICS,
                 "Attempting to open file '%s'", fname.c_str());
 #endif
            fp = fopen(datafile_path(fname, false).c_str(), "r");
        }
    }
#endif

    if (fp)
    {
        char buf[200];
        bool first = true;
        while (fgets(buf, sizeof buf, fp))
        {
            // Remove trailing spaces.
            for (int i = strlen(buf) - 1; i >= 0; i++)
            {
                if (isspace( buf[i] ))
                    buf[i] = 0;
                else
                    break;
            }
            if (first)
            {
                // Highlight the first line (title).
                std::string text  = "<w>";
                            text += buf;
                            text += "</w>";
                cmd_version.add_text(text);
                first = false;
            }
            else
                cmd_version.add_text(buf);
        }
        fclose(fp);
    }

    cmd_version.show();
}

void adjust(void)
{
    mpr("Adjust (i)tems, (s)pells, or (a)bilities? ", MSGCH_PROMPT);

    const int keyin = tolower(get_ch());

    if (keyin == 'i')
        _adjust_item();
    else if (keyin == 's')
        _adjust_spells();
    else if (keyin == 'a')
        _adjust_ability();
    else if (keyin == ESCAPE)
        canned_msg(MSG_OK);
    else
        canned_msg(MSG_HUH);
}

void swap_inv_slots(int from_slot, int to_slot, bool verbose)
{
    // Swap items.
    item_def tmp = you.inv[to_slot];
    you.inv[to_slot]   = you.inv[from_slot];
    you.inv[from_slot] = tmp;

    // Slot switching.
    tmp.slot = you.inv[to_slot].slot;
    you.inv[to_slot].slot  = you.inv[from_slot].slot;
    you.inv[from_slot].slot = tmp.slot;

    you.inv[from_slot].link = from_slot;
    you.inv[to_slot].link  = to_slot;

    for (int i = 0; i < NUM_EQUIP; i++)
    {
        if (you.equip[i] == from_slot)
            you.equip[i] = to_slot;
        else if (you.equip[i] == to_slot)
            you.equip[i] = from_slot;
    }

    if (verbose)
    {
        mpr( you.inv[to_slot].name(DESC_INVENTORY_EQUIP).c_str() );

        if (is_valid_item( you.inv[from_slot] ))
            mpr( you.inv[from_slot].name(DESC_INVENTORY_EQUIP).c_str() );
    }

    if (to_slot == you.equip[EQ_WEAPON] || from_slot == you.equip[EQ_WEAPON])
    {
        you.wield_change = true;
        you.m_quiver->on_weapon_changed();
    }
    else // just to make sure
        you.redraw_quiver = true;
}

static void _adjust_item(void)
{
    int from_slot, to_slot;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    from_slot = prompt_invent_item("Adjust which item?", MT_INVLIST, -1);
    if (prompt_failed(from_slot))
        return;

    mpr(you.inv[from_slot].name(DESC_INVENTORY_EQUIP).c_str());

    to_slot = prompt_invent_item("Adjust to which letter? ",
                                 MT_INVLIST,
                                 -1,
                                 false,
                                 false);
    if (to_slot == PROMPT_ABORT)
    {
        canned_msg(MSG_OK);
        return;
    }

    swap_inv_slots(from_slot, to_slot, true);
}

static void _adjust_spells(void)
{
    if (!you.spell_no)
    {
        mpr("You don't know any spells.");
        return;
    }

    // Select starting slot
    mpr("Adjust which spell? ", MSGCH_PROMPT);

    int keyin = 0;
    if (Options.auto_list)
        keyin = list_spells(false);
    else
    {
        keyin = get_ch();
        if (keyin == '?' || keyin == '*')
            keyin = list_spells(false);
    }

    if (!isalpha(keyin))
    {
        canned_msg(MSG_OK);
        return;
    }

    const int input_1 = keyin;
    const int index_1 = letter_to_index( input_1 );
    spell_type spell  = get_spell_by_letter( input_1 );

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        return;
    }

    // Print targeted spell.
    mprf( "%c - %s", keyin, spell_title( spell ) );

    // Select target slot.
    keyin = 0;
    while (!isalpha(keyin))
    {
        mpr("Adjust to which letter? ", MSGCH_PROMPT);
        keyin = get_ch();
        if (keyin == ESCAPE)
        {
            canned_msg( MSG_OK );
            return;
        }
        if (keyin == '?' || keyin == '*')
            keyin = list_spells();
    }

    const int input_2 = keyin;
    const int index_2 = letter_to_index(keyin);

    // swap references in the letter table:
    const int tmp = you.spell_letter_table[index_2];
    you.spell_letter_table[index_2] = you.spell_letter_table[index_1];
    you.spell_letter_table[index_1] = tmp;

    // print out spell in new slot
    mprf("%c - %s", input_2, spell_title(get_spell_by_letter(input_2)));

    // print out other spell if one was involved (now at input_1)
    spell = get_spell_by_letter( input_1 );

    if (spell != SPELL_NO_SPELL)
        mprf("%c - %s", input_1, spell_title(spell) );
}

static void _adjust_ability(void)
{
    const std::vector<talent> talents = your_talents(false);

    if (talents.empty())
    {
        mpr("You don't currently have any abilities.");
        return;
    }

    int selected = -1;
    while (selected < 0)
    {
        msg::streams(MSGCH_PROMPT) << "Adjust which ability? (? or * to list) "
                                   << std::endl;

        const int keyin = get_ch();

        if (keyin == '?' || keyin == '*')
            selected = choose_ability_menu(talents);
        else if (keyin == ESCAPE || keyin == ' ' ||
                 keyin == '\r' || keyin == '\n')
        {
            canned_msg(MSG_OK);
            return;
        }
        else if (isalpha(keyin))
        {
            // Try to find the hotkey.
            for (unsigned int i = 0; i < talents.size(); ++i)
            {
                if (talents[i].hotkey == keyin)
                {
                    selected = static_cast<int>(i);
                    break;
                }
            }

            // If we can't, cancel out.
            if (selected < 0)
            {
                mpr("No such ability.");
                return;
            }
        }
    }

    msg::stream << static_cast<char>(talents[selected].hotkey)
                << " - "
                << ability_name(talents[selected].which)
                << std::endl;

    const int index1 = letter_to_index(talents[selected].hotkey);

    msg::streams(MSGCH_PROMPT) << "Adjust to which letter? " << std::endl;

    const int keyin = get_ch();

    if (!isalpha(keyin))
    {
        canned_msg(MSG_HUH);
        return;
    }

    const int index2 = letter_to_index(keyin);
    if (index1 == index2)
    {
        mpr("That would be singularly pointless.");
        return;
    }

    // See if we moved something out.
    bool printed_message = false;
    for (unsigned int i = 0; i < talents.size(); ++i)
    {
        if (talents[i].hotkey == keyin)
        {
            msg::stream << "Swapping with: "
                        << static_cast<char>(keyin) << " - "
                        << ability_name(talents[i].which)
                        << std::endl;
            printed_message = true;
            break;
        }
    }

    if (!printed_message)
        msg::stream << "Moving to: "
                    << static_cast<char>(keyin) << " - "
                    << ability_name(talents[selected].which)
                    << std::endl;

    // Swap references in the letter table.
    ability_type tmp = you.ability_letter_table[index2];
    you.ability_letter_table[index2] = you.ability_letter_table[index1];
    you.ability_letter_table[index1] = tmp;
}

void list_armour()
{
    std::ostringstream estr;
    for (int i = EQ_CLOAK; i <= EQ_BODY_ARMOUR; i++)
    {
        const int armour_id = you.equip[i];
        int       colour    = MSGCOL_BLACK;

        estr.str("");
        estr.clear();

        estr << ((i == EQ_CLOAK)       ? "Cloak  " :
                 (i == EQ_HELMET)      ? "Helmet " :
                 (i == EQ_GLOVES)      ? "Gloves " :
                 (i == EQ_SHIELD)      ? "Shield " :
                 (i == EQ_BODY_ARMOUR) ? "Armour " :
                 (i == EQ_BOOTS) ?
                 ((you.species == SP_CENTAUR
                   || you.species == SP_NAGA) ? "Barding"
                                              : "Boots  ")
                                 : "unknown")
             << " : ";

        if (!you_can_wear(i, true))
            estr << "    (unavailable)";
        else if (armour_id != -1 && !you_tran_can_wear(you.inv[armour_id])
                 || !you_tran_can_wear(i))
        {
            estr << "    (currently unavailable)";
        }
        else if (armour_id != -1)
        {
            estr << you.inv[armour_id].name(DESC_INVENTORY);
            colour = menu_colour(estr.str(),
                                 menu_colour_item_prefix(you.inv[armour_id]),
                                 "equip");
        }
        else if (!you_can_wear(i))
            estr << "    (restricted)";
        else
            estr << "    none";

        if (colour == MSGCOL_BLACK)
            colour = menu_colour(estr.str(), "", "equip");

        mpr( estr.str().c_str(), MSGCH_EQUIPMENT, colour);
    }
}

void list_jewellery(void)
{
    std::ostringstream jstr;

    for (int i = EQ_LEFT_RING; i <= EQ_AMULET; i++)
    {
        const int jewellery_id = you.equip[i];
        int       colour       = MSGCOL_BLACK;

        jstr.str("");
        jstr.clear();

        jstr << ((i == EQ_LEFT_RING)  ? "Left ring " :
                 (i == EQ_RIGHT_RING) ? "Right ring" :
                 (i == EQ_AMULET)     ? "Amulet    "
                                      : "unknown   ")
             << " : ";

        if (jewellery_id != -1 && !you_tran_can_wear(you.inv[jewellery_id])
            || !you_tran_can_wear(i))
        {
            jstr << "    (currently unavailable)";
        }
        else if (jewellery_id != -1)
        {
            jstr << you.inv[jewellery_id].name(DESC_INVENTORY);
            std::string
                prefix = menu_colour_item_prefix(you.inv[jewellery_id]);
            colour = menu_colour(jstr.str(), prefix, "equip");
        }
        else
            jstr << "    none";

        if (colour == MSGCOL_BLACK)
            colour = menu_colour(jstr.str(), "", "equip");

        mpr( jstr.str().c_str(), MSGCH_EQUIPMENT, colour);
    }
}

void list_weapons(void)
{
    const int weapon_id = you.equip[EQ_WEAPON];

    // Output the current weapon
    //
    // Yes, this is already on the screen... I'm outputing it
    // for completeness and to avoid confusion.
    std::string wstring = "Current   : ";
    int         colour;

    if (weapon_id != -1)
    {
        wstring += you.inv[weapon_id].name(DESC_INVENTORY_EQUIP);
        colour = menu_colour(wstring,
                             menu_colour_item_prefix(you.inv[weapon_id]),
                             "equip");
    }
    else
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            wstring += "    blade hands";
        else if (!you_tran_can_wear(EQ_WEAPON))
            wstring += "    (currently unavailable)";
        else
            wstring += "    empty hands";
        colour = menu_colour(wstring, "", "equip");
    }

    mpr(wstring.c_str(), MSGCH_EQUIPMENT, colour);

    // Print out the swap slots.
    for (int i = 0; i <= 1; i++)
    {
        // We'll avoid repeating the current weapon for these slots,
        // in order to keep things clean.
        if (weapon_id == i)
            continue;

        if ( i == 0 )
            wstring = "Primary   : ";
        else
            wstring = "Secondary : ";

        colour = MSGCOL_BLACK;
        if (is_valid_item( you.inv[i])
            && (you.inv[i].base_type == OBJ_WEAPONS
                || you.inv[i].base_type == OBJ_STAVES
                || you.inv[i].base_type == OBJ_MISCELLANY))
        {
            wstring += you.inv[i].name(DESC_INVENTORY_EQUIP);
            colour = menu_colour(wstring,
                                 menu_colour_item_prefix(you.inv[i]),
                                 "equip");
        }
        else
            wstring += "    none";

        if (colour == MSGCOL_BLACK)
            colour = menu_colour(wstring, "", "equip");

        mpr(wstring.c_str(), MSGCH_EQUIPMENT, colour);
    }

    // Now we print out the current default fire weapon.
    wstring = "Firing    : ";

    int slot = you.m_quiver->get_fire_item();

    colour = MSGCOL_BLACK;
    if (slot == -1)
    {
        const item_def* item;
        you.m_quiver->get_desired_item(&item, &slot);
        if (!is_valid_item(*item))
        {
            wstring += "    nothing";
        }
        else
        {
            wstring += "  - ";
            wstring += item->name(DESC_NOCAP_A);
            wstring += " (empty)";
        }
    }
    else
    {
        wstring += you.inv[slot].name(DESC_INVENTORY_EQUIP);
        colour = menu_colour(wstring,
                             menu_colour_item_prefix(you.inv[slot]),
                             "equip");
    }

    if (colour == MSGCOL_BLACK)
        colour = menu_colour(wstring, "", "equip");

    mpr( wstring.c_str(), MSGCH_EQUIPMENT, colour );
}                               // end list_weapons()

static bool _cmdhelp_textfilter(const std::string &tag)
{
#ifdef WIZARD
    if (tag == "wiz")
        return (true);
#endif
    return (false);
}

static const char *targeting_help_1 =
    "<h>Examine surroundings ('<w>x</w><h>' in main):\n"
    "<w>Esc</w> : cancel (also <w>Space</w>, <w>x</w>)\n"
    "<w>Dir.</w>: move cursor in that direction\n"
    "<w>.</w> : move to cursor (also <w>Enter</w>, <w>Del</w>)\n"
    "<w>v</w> : describe monster under cursor\n"
    "<w>+</w> : cycle monsters forward (also <w>=</w>)\n"
    "<w>-</w> : cycle monsters backward\n"
    "<w>*</w> : cycle objects forward\n"
    "<w>/</w> : cycle objects backward (also <w>;</w>)\n"
    "<w>^</w> : cycle through traps\n"
    "<w>_</w> : cycle through altars\n"
    "<w><<</w>/<w>></w> : cycle through up/down stairs\n"
    "<w>Tab</w> : cycle through shops and portals\n"
    "<w>Ctrl-F</w> : change monster targeting mode\n"
#ifndef USE_TILE
    "<w>Ctrl-L</w> : toggle targeting via monster list\n"
#endif
    "<w>Ctrl-P</w> : repeat prompt\n"
    " \n"
    "<h>Targeting (zapping wands, casting spells, etc.):\n"
    "The keys from examine surroundings also work here.\n"
    "In addition, you can use:\n"
    "<w>Enter</w> : fire at target (<w>Space</w>, <w>Del</w>)\n"
    "<w>.</w> : fire at target and stop there (may hit submerged creatures)\n"
    "<w>!</w> : fire at target, ignoring range\n"
    "<w>@</w> : fire at target and stop there, ignoring range\n"
    "<w>p</w> : fire at Previous target (also <w>f</w>)\n"
    "<w>:</w> : show/hide beam path\n"
    "<w>Shift-Dir</w> : shoot straight-line beam\n"
#ifdef WIZARD
    " \n"
    "<h>Wizard targeting commands:</h>\n"
    "<w>g</w>: give item to monster\n"
    "<w>s</w>: force monster to shout or speak\n"
    "<w>S</w>: make monster a summoned monster\n"
    "<w>F</w>: cycle monster friendly/good neutral/neutral/hostile\n"
    "<w>P</w>: apply divine blessing to monster\n"
    "<w>m</w>: move monster or player\n"
    "<w>M</w>: cause spell miscast for monster or player\n"
    "<w>w</w>: calculate shortest path to any point on the map\n"
    "<w>D</w>: get debugging information about the monster\n"
    "<w>~</w>: polymorph monster to specific type\n"
#endif
;

static const char *targeting_help_2 =
    "<h>Firing or throwing a missile:\n"
    "<w>(</w> : cycle to next suitable missile.\n"
    "<w>)</w> : cycle to previous suitable missile.\n"
    "<w>i</w> : choose from inventory.\n"
;


// Add the contents of the file fp to the scroller menu m.
// If first_hotkey is nonzero, that will be the hotkey for the
// start of the contents of the file.
// If auto_hotkeys is true, the function will try to identify
// sections and add appropriate hotkeys.
static void _add_file_to_scroller(FILE* fp, formatted_scroller& m,
                                  int first_hotkey, bool auto_hotkeys)
{
    bool next_is_hotkey = false;
    bool is_first = true;
    char buf[200];

    // Bracket with MEL_TITLES, so that you won't scroll into it or above it.
    m.add_entry(new MenuEntry(std::string(), MEL_TITLE));
    for (int i = 0; i < get_number_of_lines(); ++i)
        m.add_entry(new MenuEntry(std::string()));
    m.add_entry(new MenuEntry(std::string(), MEL_TITLE));

    while (fgets(buf, sizeof buf, fp))
    {
        MenuEntry* me = new MenuEntry(buf);
        if (next_is_hotkey && (isupper(buf[0]) || isdigit(buf[0]))
            || is_first && first_hotkey)
        {
            int hotkey = (is_first ? first_hotkey : buf[0]);
            if (!is_first && buf[0] == 'X'
                && strlen(buf) >= 3 && isdigit(buf[2]))
            {
                // X.# is hotkeyed to the #
                hotkey = buf[2];
            }
            me->add_hotkey(hotkey);
            if (isupper(hotkey))
                me->add_hotkey(tolower(hotkey));
            me->level  = MEL_SUBTITLE;
            me->colour = WHITE;
        }
        m.add_entry(me);
        // FIXME: There must be a better way to identify sections!
        next_is_hotkey = auto_hotkeys &&
            (strstr(buf, "------------------------------------------"
                         "------------------------------") == buf);
        is_first = false;
    }
}


struct help_file
{
    const char* name;
    int hotkey;
    bool auto_hotkey;
};

help_file help_files[] = {
    { "crawl_manual.txt",  '*', true },
    { "../README.txt",     '!', false },
    { "aptitudes.txt",     '%', false },
    { "quickstart.txt",    '^', false },
    { "macros_guide.txt",  '~', false },
    { "options_guide.txt", '&', false },
#ifdef USE_TILE
    { "tiles_help.txt",    'T', false },
#endif
    { NULL, 0, false }
};

static bool _compare_mon_names(MenuEntry *entry_a, MenuEntry* entry_b)
{
    monster_type *a = static_cast<monster_type*>( entry_a->data );
    monster_type *b = static_cast<monster_type*>( entry_b->data );

    if (*a == *b)
        return (false);

    std::string a_name = mons_type_name(*a, DESC_PLAIN);
    std::string b_name = mons_type_name(*b, DESC_PLAIN);
    return (lowercase(a_name) < lowercase(b_name));
}

// Compare monsters by location-independant level, or by hitdice if
// levels are equal, or by name if both level and hitdice are equal.
static bool _compare_mon_toughness(MenuEntry *entry_a, MenuEntry* entry_b)
{
    monster_type *a = static_cast<monster_type*>( entry_a->data );
    monster_type *b = static_cast<monster_type*>( entry_b->data );

    if (*a == *b)
        return (false);

    int a_toughness = mons_difficulty(*a);
    int b_toughness = mons_difficulty(*b);

    if (a_toughness == b_toughness)
    {
        std::string a_name = mons_type_name(*a, DESC_PLAIN);
        std::string b_name = mons_type_name(*b, DESC_PLAIN);
        return (lowercase(a_name) < lowercase(b_name));
    }
    return (a_toughness > b_toughness);
}

class DescMenu : public Menu
{
public:
    DescMenu( int _flags, bool _show_mon )
        : Menu(_flags), sort_alpha(true), showing_monsters(_show_mon)
        {
            set_highlighter(NULL);

            if (_show_mon)
                toggle_sorting();

            set_prompt();
        }

    bool sort_alpha;
    bool showing_monsters;

    void set_prompt()
        {
            std::string prompt = "Describe which? ";

            if (showing_monsters)
            {
                if (sort_alpha)
                    prompt += "(CTRL-S to sort by monster toughness)";
                else
                    prompt += "(CTRL-S to sort by name)";
            }
            set_title(new MenuEntry(prompt, MEL_TITLE));
        }

    void sort()
        {
            if (!showing_monsters)
                return;

            if (sort_alpha)
                std::sort(items.begin(), items.end(), _compare_mon_names);
            else
                std::sort(items.begin(), items.end(), _compare_mon_toughness);

            for (unsigned int i = 0, size = items.size(); i < size; i++)
            {
                const char letter = index_to_letter(i);

                items[i]->hotkeys.clear();
                items[i]->add_hotkey(letter);
            }
        }

    void toggle_sorting()
        {
            if (!showing_monsters)
                return;

            sort_alpha = !sort_alpha;

            sort();
            set_prompt();
        }
};

static std::vector<std::string> _get_desc_keys(std::string regex,
                                               db_find_filter filter)
{
    std::vector<std::string> key_matches = getLongDescKeysByRegex(regex,
                                                                  filter);

    if (key_matches.size() == 1)
        return (key_matches);
    else if (key_matches.size() > 52)
        return (key_matches);

    std::vector<std::string> body_matches = getLongDescBodiesByRegex(regex,
                                                                     filter);

    if (key_matches.size() == 0 && body_matches.size() == 0)
        return (key_matches);
    else if (key_matches.size() == 0 && body_matches.size() == 1)
        return (body_matches);

    // Merge key_matches and body_matches, discarding duplicates.
    std::vector<std::string> tmp = key_matches;
    tmp.insert(tmp.end(), body_matches.begin(), body_matches.end());
    std::sort(tmp.begin(), tmp.end());
    std::vector<std::string> all_matches;
    for (unsigned int i = 0, size = tmp.size(); i < size; i++)
        if (i == 0 || all_matches[all_matches.size() - 1] != tmp[i])
            all_matches.push_back(tmp[i]);

    return (all_matches);
}

static std::vector<std::string> _get_monster_keys(unsigned char showchar)
{
    std::vector<std::string> mon_keys;

    for (int i = 0; i < NUM_MONSTERS; i++)
    {
        if (i == MONS_PROGRAM_BUG || mons_global_level(i) == 0)
            continue;

        monsterentry *me = get_monster_data(i);

        if (me == NULL || me->name == NULL || me->name[0] == '\0')
            continue;

        if (me->mc != i)
            continue;

        if (getLongDescription(me->name).empty())
            continue;

        if (me->showchar == showchar)
            mon_keys.push_back(me->name);
    }

    return (mon_keys);
}

static std::vector<std::string> _get_god_keys()
{
    std::vector<std::string> names;

    for (int i = GOD_NO_GOD + 1; i < NUM_GODS; i++)
    {
        god_type which_god = static_cast<god_type>(i);
        names.push_back(god_name(which_god));
    }

    return names;
}

static std::vector<std::string> _get_branch_keys()
{
    std::vector<std::string> names;

    for (int i = BRANCH_MAIN_DUNGEON; i < NUM_BRANCHES; i++)
    {
        branch_type which_branch = static_cast<branch_type>(i);
        Branch     &branch       = branches[which_branch];

        // Skip unimplemented branches
        if (branch.depth < 1 || branch.shortname == NULL)
            continue;

        names.push_back(branch.shortname);
    }
/*
    // Maybe include other level areas, as well.
    for (int i = LEVEL_LABYRINTH; i < NUM_LEVEL_AREA_TYPES; i++)
    {
        names.push_back(place_name(
                            get_packed_place(BRANCH_MAIN_DUNGEON, 1,
                                static_cast<level_area_type>(i)), true));
    }
*/
    return (names);
}

static bool _monster_filter(std::string key, std::string body)
{
    int mon_num = get_monster_by_name(key.c_str(), true);
    return (mon_num == MONS_PROGRAM_BUG || mons_global_level(mon_num) == 0);
}

static bool _spell_filter(std::string key, std::string body)
{
    spell_type spell = spell_by_name(key);

    if (spell == SPELL_NO_SPELL)
        return (true);

    if (get_spell_flags(spell) & (SPFLAG_MONSTER | SPFLAG_TESTING
                                  | SPFLAG_DEVEL))
    {
#ifdef WIZARD
        return (!you.wizard);
#else
        return (true);
#endif
    }

    return (false);
}

static bool _item_filter(std::string key, std::string body)
{
    return (item_types_by_name(key).base_type == OBJ_UNASSIGNED);
}

static bool _skill_filter(std::string key, std::string body)
{
    key = lowercase_string(key);
    std::string name;
    for (int i = 0; i < NUM_SKILLS; i++)
    {
        // There are a couple of NULL entries in the skill set.
        if (!skill_name(i))
            continue;

        name = lowercase_string(skill_name(i));

        if (name.find(key) != std::string::npos)
            return (false);
    }
    return (true);
}

static bool _feature_filter(std::string key, std::string body)
{
    return (feat_by_desc(key) == DNGN_UNSEEN);
}

static bool _card_filter(std::string key, std::string body)
{
    key = lowercase_string(key);
    std::string name;

    // Every card description contains the keyword "card".
    if (key.find("card") != std::string::npos)
        return (false);

    for (int i = 0; i < NUM_CARDS; ++i)
    {
        name = lowercase_string(card_name((card_type) i));

        if (name.find(key) != std::string::npos)
            return (false);
    }
    return (true);
}

static bool _ability_filter(std::string key, std::string body)
{
    key = lowercase_string(key);
    if (string_matches_ability_name(key))
        return (false);

    return (true);
}

typedef void (*db_keys_recap)(std::vector<std::string>&);

static void _recap_mon_keys(std::vector<std::string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        monster_type type = get_monster_by_name(keys[i], true);
        keys[i] = mons_type_name(type, DESC_PLAIN);
    }
}

static void _recap_feat_keys(std::vector<std::string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        dungeon_feature_type type = feat_by_desc(keys[i]);
        if (type == DNGN_ENTER_SHOP)
            keys[i] = "A shop";
        else
        {
            keys[i] = feature_description(type, NUM_TRAPS, false, DESC_CAP_A,
                                          false);
        }
    }
}

// Extra info on this item wasn't found anywhere else.
static void _append_non_item(std::string &desc, std::string key)
{
    spell_type type = spell_by_name(key);

    if (type == SPELL_NO_SPELL)
        return;

    unsigned int flags = get_spell_flags(type);

    if (flags & SPFLAG_DEVEL)
    {
        desc += "$This spell is still being developed, and is only available "
                "via the &Z wizard command.";
    }
    else if (flags & SPFLAG_TESTING)
    {
        desc += "$This is a testing spell, only available via the "
                "&Z wizard command.";
    }
    else if (flags & SPFLAG_MONSTER)
    {
        desc += "$This is a monster-only spell, only available via the "
                "&Z wizard command.";
    }
    else if (flags & SPFLAG_CARD)
    {
        desc += "$This is a card-effect spell, unavailable in ordinary "
                "spellbooks.";
    }
    else
    {
        desc += "$Odd, this spell can't be found anywhere.  Please "
                "file a bug report.";
    }

#ifdef WIZARD
    if (!you.wizard)
#else
    if (true)
#endif
    {
        if (flags & (SPFLAG_TESTING | SPFLAG_MONSTER | SPFLAG_DEVEL))
        {
            desc += "$$You aren't in wizard mode, so you shouldn't be "
                    "seeing this entry.  Please file a bug report.";
        }
    }
}

// Adds a list of all books/rods that contain a given spell (by name)
// to a description string.
static bool _append_books(std::string &desc, item_def &item, std::string key)
{
    spell_type type = spell_by_name(key, true);

    if (type == SPELL_NO_SPELL)
        return (false);

    desc += "$Type:       ";
    bool already = false;

    for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
    {
        if (spell_typematch( type, 1 << i ))
        {
            if (already)
                desc += "/" ;

            desc += spelltype_name( 1 << i );
            already = true;
        }
    }
    if (!already)
        desc += "None";

    desc += "$Level:      ";
    char sval[3];
    itoa( spell_difficulty( type ), sval, 10 );
    desc += sval;

    set_ident_flags(item, ISFLAG_IDENT_MASK);
    std::vector<std::string> books;
    std::vector<std::string> rods;

    item.base_type = OBJ_BOOKS;
    for (int i = 0; i < NUM_FIXED_BOOKS; i++)
        for (int j = 0; j < 8; j++)
            if (which_spell_in_book(i, j) == type)
            {
                item.sub_type = i;
                books.push_back(item.name(DESC_PLAIN));
            }

    item.base_type = OBJ_STAVES;
    int book;
    for (int i = STAFF_FIRST_ROD; i < NUM_STAVES; i++)
    {
        item.sub_type = i;
        book = item.book_number();

        for (int j = 0; j < 8; j++)
            if (which_spell_in_book(book, j) == type)
                rods.push_back(item.name(DESC_PLAIN));
    }

    if (!books.empty())
    {
        desc += "$$This spell can be found in the following book";
        if (books.size() > 1)
            desc += "s";
        desc += ":$";
        desc += comma_separated_line(books.begin(), books.end(), "$", "$");

        if (!rods.empty())
        {
            desc += "$$... and the following rod";
            if (rods.size() > 1)
                desc += "s";
            desc += ":$";
            desc += comma_separated_line(rods.begin(), rods.end(), "$", "$");
        }
    }
    else // rods-only
    {
        desc += "$$This spell can be found in the following rod";
        if (rods.size() > 1)
            desc += "s";
        desc += ":$";
        desc += comma_separated_line(rods.begin(), rods.end(), "$", "$");
    }

    return (true);
}

static bool _do_description(std::string key, std::string type,
                            std::string footer = "")
{
    describe_info inf;
    inf.quote = getQuoteString(key);

    std::string desc = getLongDescription(key);

    int width = std::min(80, get_number_of_cols());

    god_type which_god = string_to_god(key.c_str());
    if (which_god != GOD_NO_GOD)
    {
        if (is_good_god(which_god))
        {
            inf.suffix = "$$" + god_name(which_god) +
                         " won't accept worship from undead or evil beings.";
        }
        std::string help = get_god_powers(which_god);
        if (!help.empty())
        {
            desc += EOL;
            desc += help;
        }
        desc += EOL;
        desc += get_god_likes(which_god);

        help = get_god_dislikes(which_god);
        if (!help.empty())
        {
            desc += EOL EOL;
            desc += help;
        }
    }
    else
    {
        monster_type mon_num = get_monster_by_name(key, true);
        // Don't attempt to get more information on ghosts or
        // pandemonium demons as the ghost struct has not been initialized
        // which will cause a crash.
        if (mon_num != MONS_PROGRAM_BUG && mon_num != MONS_PLAYER_GHOST
            && mon_num != MONS_PANDEMONIUM_DEMON)
        {
            monsters mon;
            mon.type = mon_num;

            if (mons_genus(mon_num) == MONS_DRACONIAN)
            {
                switch (mon_num)
                {
                case MONS_BLACK_DRACONIAN:
                case MONS_MOTTLED_DRACONIAN:
                case MONS_YELLOW_DRACONIAN:
                case MONS_GREEN_DRACONIAN:
                case MONS_PURPLE_DRACONIAN:
                case MONS_RED_DRACONIAN:
                case MONS_WHITE_DRACONIAN:
                case MONS_PALE_DRACONIAN:
                    mon.base_monster = mon_num;
                    break;
                default:
                    mon.base_monster = MONS_PROGRAM_BUG;
                    break;
                }
            }
            else
                mon.base_monster = MONS_PROGRAM_BUG;

            describe_monsters(mon, true);
            return (false);
        }
        else
        {
            int thing_created = get_item_slot();
            if (thing_created != NON_ITEM
                && (type == "item" || type == "spell"))
            {
                char name[80];
                strncpy(name, key.c_str(), sizeof(name));
                if (get_item_by_name(&mitm[thing_created], name, OBJ_WEAPONS))
                {
                    append_weapon_stats(desc, mitm[thing_created]);
                    desc += "$";
                }
                else if (get_item_by_name(&mitm[thing_created], name, OBJ_ARMOUR))
                {
                    append_armour_stats(desc, mitm[thing_created]);
                    desc += "$";
                }
                else if (get_item_by_name(&mitm[thing_created], name, OBJ_MISSILES))
                {
                    append_missile_info(desc);
                    desc += "$";
                }
                else if (type == "spell"
                         || get_item_by_name(&mitm[thing_created], name, OBJ_BOOKS)
                         || get_item_by_name(&mitm[thing_created], name, OBJ_STAVES))
                {
                    if (!_append_books(desc, mitm[thing_created], key))
                        append_spells(desc, mitm[thing_created]);
                }
                else
                    _append_non_item(desc, key);
            }

            // Now we don't need the item anymore.
            if (thing_created != NON_ITEM)
                destroy_item(thing_created);
        }
    }

    inf.body << desc;

    key = uppercase_first(key);
    linebreak_string2(footer, width - 1);

    inf.footer = footer;
    inf.title  = key;

    print_description(inf);
    return (true);
}

// Reads all questions from database/FAQ.txt, outputs them in the form of
// a selectable menu and prints the corresponding answer for a chosen question.
static bool _handle_FAQ()
{
    clrscr();
    viewwindow(true, false);

    std::vector<std::string> question_keys = getAllFAQKeys();
    if (question_keys.empty())
    {
        mpr("No questions found in FAQ! Please submit a bug report!");
        return (false);
    }
    Menu FAQmenu(MF_SINGLESELECT | MF_ANYPRINTABLE | MF_ALLOW_FORMATTING);
    MenuEntry *title = new MenuEntry("Frequently Asked Questions");
    title->colour = YELLOW;
    FAQmenu.set_title(title);
    const int width = std::min(80, get_number_of_cols());

    for (unsigned int i = 0, size = question_keys.size(); i < size; i++)
    {
        const char letter = index_to_letter(i);

        std::string question = getFAQ_Question(question_keys[i]);
        // Wraparound if the question is longer than fits into a line.
        linebreak_string2(question, width - 4);
        std::vector<formatted_string> fss;
        formatted_string::parse_string_to_multiple(question, fss);

        MenuEntry *me;
        for (unsigned int j = 0; j < fss.size(); j++)
        {
            if (j == 0)
            {
                me = new MenuEntry(question, MEL_ITEM, 1, letter);
                me->data = (void*) &question_keys[i];
            }
            else
            {
                question = "    " + fss[j].tostring();
                me = new MenuEntry(question, MEL_ITEM, 1);
            }
            FAQmenu.add_entry(me);
        }
    }

    while (true)
    {
        std::vector<MenuEntry*> sel = FAQmenu.show();
        redraw_screen();
        if (sel.empty())
            return (false);
        else
        {
            ASSERT(sel.size() == 1);
            ASSERT(sel[0]->hotkeys.size() == 1);

            std::string key = *((std::string*) sel[0]->data);
            std::string answer = getFAQ_Answer(key);
            if (answer.empty())
            {
                answer = "No answer found in the FAQ! Please submit a "
                         "bug report!";
            }
            answer = "Q: " + getFAQ_Question(key) + EOL + answer;
            linebreak_string2(answer, width - 1);
            print_description(answer);
            if (getch() == 0)
                getch();
        }
    }

    return (true);
}

static bool _find_description(bool &again, std::string& error_inout)
{
    again = true;

    clrscr();
    viewwindow(true, false);

    if (!error_inout.empty())
        mpr(error_inout.c_str(), MSGCH_PROMPT);
    mpr("Describe a (M)onster, (S)pell, s(K)ill, (I)tem, (F)eature, (G)od, "
        "(A)bility, (B)ranch, or (C)ard? ", MSGCH_PROMPT);

    int ch = toupper(getch());
    std::string    type;
    std::string    extra;
    db_find_filter filter     = NULL;
    db_keys_recap  recap      = NULL;
    bool           want_regex = true;
    bool           want_sort  = true;

    bool doing_mons     = false;
    bool doing_items    = false;
    bool doing_gods     = false;
    bool doing_branches = false;

    switch (ch)
    {
    case 'M':
        type       = "monster";
        extra      = "  Enter a single letter to list monsters displayed by "
                     "that symbol.";
        filter     = _monster_filter;
        recap      = _recap_mon_keys;
        doing_mons = true;
        break;
    case 'S':
        type   = "spell";
        filter = _spell_filter;
        break;
    case 'K':
        type   = "skill";
        filter = _skill_filter;
        break;
    case 'A':
        type   = "ability";
        filter = _ability_filter;
        break;
    case 'C':
        type   = "card";
        filter = _card_filter;
        break;
    case 'I':
        type        = "item";
        extra       = "  Enter a single letter to list items displayed by "
                      "that symbol.";
        filter      = _item_filter;
        doing_items = true;
        break;
    case 'F':
        type   = "feature";
        filter = _feature_filter;
        recap  = _recap_feat_keys;
        break;
    case 'G':
        type       = "god";
        filter     = NULL;
        want_regex = false;
        doing_gods = true;
        break;
    case 'B':
        type           = "branch";
        filter         = NULL;
        want_regex     = false;
        want_sort      = false;
        doing_branches = true;
        break;

    default:
        error_inout = "Okay, then.";
        again = false;
        return (false);
    }

    std::string regex = "";

    if (want_regex)
    {
        mprf(MSGCH_PROMPT,
             "Describe a %s; partial names and regexps are fine.%s",
             type.c_str(), extra.c_str());

        mpr("Describe what? ", MSGCH_PROMPT);
        char buf[80];
        if (cancelable_get_line(buf, sizeof(buf)) || buf[0] == '\0')
        {
            error_inout = "Okay, then.";
            return (false);
        }

        if (strlen(buf) == 1)
            regex = buf;
        else
            regex = trimmed_string(buf);

        if (regex.empty())
        {
            error_inout = "Description must contain at least "
                          "one non-space.";
            return (false);
        }
    }

    bool by_mon_symbol  = (doing_mons  && regex.size() == 1);
    bool by_item_symbol = (doing_items && regex.size() == 1);

    if (by_mon_symbol)
        want_regex = false;

    bool exact_match = false;
    if (want_regex && !(*filter)(regex, ""))
    {
        // Try to get an exact match first.
        std::string desc = getLongDescription(regex);

        if (!desc.empty())
            exact_match = true;
    }

    std::vector<std::string> key_list;

    if (by_mon_symbol)
        key_list = _get_monster_keys(regex[0]);
    else if (by_item_symbol)
        key_list = item_name_list_for_glyph(regex[0]);
    else if (doing_gods)
        key_list = _get_god_keys();
    else if (doing_branches)
        key_list = _get_branch_keys();
    else
        key_list = _get_desc_keys(regex, filter);

    if (recap != NULL)
        (*recap)(key_list);

    if (key_list.size() == 0)
    {
        if (by_mon_symbol)
        {
            error_inout  = "No monsters with symbol '";
            error_inout += regex;
            error_inout += "'.";
        }
        else if (by_item_symbol)
        {
            error_inout  = "No items with symbol '";
            error_inout += regex;
            error_inout += "'.";
        }
        else
        {
            error_inout  = "No matching ";
            error_inout += type;
            error_inout += "s.";
        }
        return (false);
    }
    else if (key_list.size() > 52)
    {
        if (by_mon_symbol)
        {
            error_inout  = "Too many monsters with symbol '";
            error_inout += regex;
            error_inout += "' to display";
        }
        else
        {
            std::ostringstream os;
            os << "Too many matching " << type << "s (" << key_list.size()
               << ") to display.";
            error_inout = os.str();
        }
        return (false);
    }
    else if (key_list.size() == 1)
        return _do_description(key_list[0], type);

    if (exact_match)
    {
        std::string footer = "This entry is an exact match for '";
        footer += regex;
        footer += "'.  To see non-exact matches, press space.";

        _do_description(regex, type, footer);

        if (getch() != ' ')
            return (false);
    }

    if (want_sort)
        std::sort(key_list.begin(), key_list.end());

    DescMenu desc_menu(MF_SINGLESELECT | MF_ANYPRINTABLE |
                       MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING,
                       doing_mons);
    desc_menu.set_tag("description");
    std::list<monster_type> monster_types;
    for (unsigned int i = 0, size = key_list.size(); i < size; i++)
    {
        const char  letter = index_to_letter(i);
        std::string str    = uppercase_first(key_list[i]);

        MenuEntry *me = new MenuEntry(uppercase_first(key_list[i]),
                                      MEL_ITEM, 1, letter);

        if (doing_mons)
        {
            monster_type  mon    = get_monster_by_name(str, true);
            unsigned char colour = mons_class_colour(mon);

            monster_types.push_back(mon);

            if (colour == BLACK)
                colour = LIGHTGREY;

            std::string prefix = "(<";
            prefix += colour_to_str(colour);
            prefix += ">";
            prefix += stringize_glyph(mons_char(mon));
            prefix += "</";
            prefix += colour_to_str(colour);
            prefix += ">) ";

            me->text = prefix + str;
            me->data = &*monster_types.rbegin();
        }
        else
            me->data = (void*) &key_list[i];

        desc_menu.add_entry(me);
    }

    desc_menu.sort();

    while (true)
    {
        std::vector<MenuEntry*> sel = desc_menu.show();
        redraw_screen();
        if ( sel.empty() )
        {
            if (doing_mons && desc_menu.getkey() == CONTROL('S'))
                desc_menu.toggle_sorting();
            else
                return (false);
        }
        else
        {
            ASSERT(sel.size() == 1);
            ASSERT(sel[0]->hotkeys.size() == 1);

            std::string key;

            if (doing_mons)
                key = mons_type_name(*(monster_type*) sel[0]->data, DESC_PLAIN);
            else
                key = *((std::string*) sel[0]->data);

            if (_do_description(key, type))
            {
                if (getch() == 0)
                    getch();
            }
        }
    }

    return (false);
}

static int _keyhelp_keyfilter(int ch)
{
    switch (ch)
    {
    case ':':
        // If the game has begun, show notes.
        if (crawl_state.need_save)
        {
            display_notes();
            return -1;
        }
        break;

    case '/':
    {
        bool again = false;
        std::string error;
        do
        {
            // resets 'again'
            if (_find_description(again, error) && getch() == 0)
                getch();

            if (again)
                mesclr(true);
        }
        while (again);

        viewwindow(true, false);

        return -1;
    }

    case 'q':
    case 'Q':
    {
        bool again;
        do
        {
            // resets 'again'
            again = _handle_FAQ();
            if (again)
                mesclr(true);
        }
        while (again);

        return -1;
    }

    case 'v':
    case 'V':
        _print_version();
        return -1;
    }
    return ch;
}

///////////////////////////////////////////////////////////////////////////
// Manual menu highlighter.

class help_highlighter : public MenuHighlighter
{
public:
    help_highlighter();
    int entry_colour(const MenuEntry *entry) const;
private:
    text_pattern pattern;
    std::string get_species_key() const;
};

help_highlighter::help_highlighter()
    : pattern(get_species_key())
{
}

int help_highlighter::entry_colour(const MenuEntry *entry) const
{
    return !pattern.empty() && pattern.matches(entry->text)? WHITE : -1;
}

// To highlight species in aptitudes list. ('?%')
std::string help_highlighter::get_species_key() const
{
    if (player_genus(GENPC_DRACONIAN) && you.experience_level < 7)
        return "";

    std::string result = species_name(you.species, you.experience_level);
    if (player_genus(GENPC_DRACONIAN))
    {
        strip_tag(result,
                  species_name(you.species, you.experience_level, true));
    }

    result += "  ";
    return (result);
}
////////////////////////////////////////////////////////////////////////////

static int _show_keyhelp_menu(const std::vector<formatted_string> &lines,
                              bool with_manual, bool easy_exit = false,
                              int hotkey = 0)
{
    formatted_scroller cmd_help;

    // Set flags, and use easy exit if necessary.
    int flags = MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP;
    if (easy_exit)
        flags |= MF_EASY_EXIT;
    cmd_help.set_flags(flags, false);
    cmd_help.set_tag("help");

    // FIXME: Allow for hiding Page down when at the end of the listing, ditto
    // for page up at start of listing.
    cmd_help.set_more( formatted_string::parse_string(
#ifdef USE_TILE
                            "<cyan>[ +/L-click : Page down.   - : Page up."
                            "           Esc/R-click exits.]"));
#else
                            "<cyan>[ + : Page down.   - : Page up."
                            "                           Esc exits.]"));
#endif

    if (with_manual)
    {
        cmd_help.set_highlighter(new help_highlighter);
        cmd_help.f_keyfilter = _keyhelp_keyfilter;
        column_composer cols(2, 40);

        cols.add_formatted(
            0,
            "<h>Dungeon Crawl Help\n"
            "\n"
            "Press one of the following keys to\n"
            "obtain more information on a certain\n"
            "aspect of Dungeon Crawl.\n"

            "<w>?</w>: List of keys\n"
            "<w>!</w>: Read Me!\n"
            "<w>^</w>: Quickstart Guide\n"
            "<w>:</w>: Browse character notes\n"
            "<w>~</w>: Macros help\n"
            "<w>&</w>: Options help\n"
            "<w>%</w>: Table of aptitudes\n"
            "<w>/</w>: Lookup description\n"
            "<w>Q</w>: FAQ\n"
#ifdef USE_TILE
            "<w>T</w>: Tiles key help\n"
#endif
            "<w>V</w>: Version information\n"
            "<w>Home</w>: This screen\n",
            true, true, _cmdhelp_textfilter);

        cols.add_formatted(
            1,
            "<h>Manual Contents\n\n"
            "<w>*</w>       Table of contents\n"
            "<w>A</w>.      Overview\n"
            "<w>B</w>.      Starting Screen\n"
            "<w>C</w>.      Attributes and Stats\n"
            "<w>D</w>.      Dungeon Exploration\n"
            "<w>E</w>.      Experience and Skills\n"
            "<w>F</w>.      Monsters\n"
            "<w>G</w>.      Items\n"
            "<w>H</w>.      Spellcasting\n"
            "<w>I</w>.      Targeting\n"
            "<w>J</w>.      Religion\n"
            "<w>K</w>.      Mutations\n"
            "<w>L</w>.      Licence, Contact, History\n"
            "<w>M</w>.      Keymaps, Macros, Options\n"
            "<w>N</w>.      Philosophy\n"
            "<w>1</w>.      List of Species\n"
            "<w>2</w>.      List of Classes\n"
            "<w>3</w>.      List of Skills\n"
            "<w>4</w>.      Keys and Commands\n"
            "<w>5</w>.      List of Enchantments\n"
            "<w>6</w>.      Inscriptions\n",
            true, true, _cmdhelp_textfilter);

        std::vector<formatted_string> blines = cols.formatted_lines();
        unsigned i;
        for (i = 0; i < blines.size(); ++i)
            cmd_help.add_item_formatted_string(blines[i]);

        while (static_cast<int>(++i) < get_number_of_lines())
            cmd_help.add_item_string("");

        // unscrollable
        cmd_help.add_entry(new MenuEntry(std::string(), MEL_TITLE));
    }

    for (unsigned i = 0; i < lines.size(); ++i)
        cmd_help.add_item_formatted_string(lines[i], (i == 0 ? '?' : 0) );

    if (with_manual)
    {
        for (int i = 0; help_files[i].name != NULL; ++i)
        {
            // Attempt to open this file, skip it if unsuccessful.
            std::string fname = canonicalise_file_separator(help_files[i].name);
            FILE* fp = fopen(datafile_path(fname, false).c_str(), "r");

#if defined(DOS)
            if (!fp)
            {
 #ifdef DEBUG_FILES
                mprf(MSGCH_DIAGNOSTICS, "File '%s' could not be opened.",
                     help_files[i].name);
 #endif
                if (get_dos_compatible_file_name(&fname))
                {
 #ifdef DEBUG_FILES
                    mprf(MSGCH_DIAGNOSTICS,
                         "Attempting to open file '%s'", fname.c_str());
 #endif
                    fp = fopen(datafile_path(fname, false).c_str(), "r");
                }
            }
#endif

            if (!fp)
                continue;

            // Put in a separator...
            cmd_help.add_item_string("");
            cmd_help.add_item_string(std::string(get_number_of_cols()-1,'='));
            cmd_help.add_item_string("");

            // ...and the file itself.
            _add_file_to_scroller(fp, cmd_help, help_files[i].hotkey,
                                  help_files[i].auto_hotkey);

            // Done with this file.
            fclose(fp);
        }
    }

    if (hotkey)
        cmd_help.jump_to_hotkey(hotkey);

    cmd_help.show();

    return cmd_help.getkey();
}

void show_specific_help( const std::string &help )
{
    std::vector<std::string> lines = split_string("\n", help, false, true);
    std::vector<formatted_string> formatted_lines;
    for (int i = 0, size = lines.size(); i < size; ++i)
    {
        formatted_lines.push_back(
            formatted_string::parse_string(
                lines[i], true, _cmdhelp_textfilter));
    }
    _show_keyhelp_menu(formatted_lines, false, true);
}

void show_levelmap_help()
{
    show_specific_help( getHelpString("level-map") );
}

void show_targeting_help()
{
    column_composer cols(2, 41);
    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines() - 1);

    cols.add_formatted(0, targeting_help_1, true, true);
    cols.add_formatted(1, targeting_help_2, true, true);
    _show_keyhelp_menu(cols.formatted_lines(), false, true);
}
void show_interlevel_travel_branch_help()
{
    show_specific_help( getHelpString("interlevel-travel.branch.prompt") );
}

void show_interlevel_travel_depth_help()
{
    show_specific_help( getHelpString("interlevel-travel.depth.prompt") );
}

void show_stash_search_help()
{
    show_specific_help( getHelpString("stash-search.prompt") );
}

void show_butchering_help()
{
    show_specific_help( getHelpString("butchering") );
}


static void _add_formatted_keyhelp(column_composer &cols)
{
    cols.add_formatted(
            0,
            "<h>Movement:\n"
            "To move in a direction or to attack, \n"
            "use the numpad (try Numlock off and \n"
            "on) or vi keys:\n"
            "                 <w>1 2 3      y k u\n"
            "                  \\|/        \\|/\n"
            "                 <w>4</w>-<w>5</w>-<w>6</w>"
                     "      <w>h</w>-<w>.</w>-<w>l</w>\n"
            "                  /|\\        /|\\\n"
            "                 <w>7 8 9      b j n\n",
            true, true, _cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Rest/Search:\n"
            "<w>s</w> : wait a turn; searches adjacent\n"
            "    squares (also <w>numpad-5</w>, <w>.</w>, <w>Del</w>)\n"
            "<w>5</w> : rest and long search; stops when\n"
            "    Health or Magic become full,\n"
            "    something is detected, or after\n"
            "    100 turns over (<w>Shift-numpad-5</w>)\n",
            true, true, _cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Extended Movement:\n"
            "<w>o</w> : auto-explore\n"
            "<w>G</w> : interlevel travel (also <w>Ctrl-G</w>)\n"
            "<w>Ctrl-F</w> : Find items\n"
            "<w>Ctrl-W</w> : set Waypoint\n"
            "<w>Ctrl-E</w> : Exclude square from searches\n"
            "<w>/ Dir.</w>, <w>Shift-Dir.</w>: long walk\n"
            "<w>* Dir.</w>, <w>Ctrl-Dir.</w> : open/close door, \n"
            "         untrap, attack without move\n",
            true, true, _cmdhelp_textfilter);

    unsigned ch;
    // Initialize colour to quiet some Valgrind warnings
    unsigned short colour = BLACK;
    std::string item_types =
        "\n"
        "<h>Item types (and common commands)\n"
        "<cyan>)</cyan> : hand weapons (<w>w</w>ield)\n"
        "<brown>(</brown> : missiles (<w>Q</w>uiver, <w>f</w>ire, <w>()</w> cycle)\n"
        "<cyan>[</cyan> : armour (<w>W</w>ear and <w>T</w>ake off)\n"
        "<brown>%</brown> : corpses and food (<w>c</w>hop up and <w>e</w>at)\n"
        "<w>?</w> : scrolls (<w>r</w>ead)\n"
        "<magenta>!</magenta> : potions (<w>q</w>uaff)\n"
        "<blue>=</blue> : rings (<w>P</w>ut on and <w>R</w>emove)\n"
        "<red>\"</red> : amulets (<w>P</w>ut on and <w>R</w>emove)\n"
        "<lightgrey>/</lightgrey> : wands (e<w>V</w>oke)\n"
        "<lightcyan>";

    get_item_symbol(DNGN_ITEM_BOOK, &ch, &colour);
    item_types += static_cast<char>(ch);
    item_types +=
        "</lightcyan> : books (<w>r</w>ead, <w>M</w>emorise, <w>z</w>ap, <w>Z</w>ap)\n"
        "<brown>\\</brown> : staves and rods (<w>w</w>ield and e<w>v</w>oke)\n"
        "<lightgreen>}</lightgreen> : miscellaneous items (e<w>v</w>oke)\n"
        "<yellow>$</yellow> : gold (<w>$</w> counts gold)\n"
        "<lightmagenta>0</lightmagenta> : the Orb of Zot\n"
        "    Carry it to the surface and win!\n",



    cols.add_formatted(
            0, item_types,
            true, true, _cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Other Gameplay Actions:\n"
            "<w>a</w> : use special Ability (<w>a!</w> for help)\n"
            "<w>p</w> : Pray (<w>^</w> and <w>^!</w> for help)\n"
            "<w>z</w> : cast spell, abort without targets\n"
            "<w>Z</w> : cast spell, no matter what\n"
            "<w>I</w> : list all spells\n"
            "<w>t</w> : tell allies (<w>tt</w> to shout)\n"
            "<w>`</w> : re-do previous command\n"
            "<w>0</w> : repeat next command # of times\n",
            true, true, _cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Non-Gameplay Commands / Info\n"
            "<w>Ctrl-P</w> : show Previous messages\n"
            "<w>Ctrl-R</w> : Redraw screen\n"
            "<w>Ctrl-C</w> : Clear main and level maps\n"
            "<w>!</w> : annotate the dungeon level\n"
            "<w>#</w> : dump character to file\n"
            "<w>:</w> : add note (use <w>?:</w> to read notes)\n"
            "<w>~</w> : add macro (also <w>Ctrl-D</w>)\n"
            "<w>=</w> : reassign inventory/spell letters\n"
            "<w>_</w> : read messages (online play only)"
            " \n",
            true, true, _cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Game Saving and Quitting:\n"
            "<w>S</w> : Save game and exit\n"
            "<w>Ctrl-S</w> : Save and exit without query\n"
            "<w>Ctrl-Q</w> : Quit without saving\n",
            true, true, _cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Player Character Information:\n"
            "<w>@</w> : display character status\n"
            "<w>m</w> : show skill screen\n"
            "<w>%</w> : show resistances\n"
            "<w>^</w> : show religion screen\n"
            "<w>A</w> : show Abilities/mutations\n"
            "<w>\\</w> : show item knowledge\n"
            "<w>[</w> : display worn armour\n"
            "<w>}</w> : display current weapons\n"
            "<w>\"</w> : display worn jewellery\n"
            "<w>$</w> : display gold in possession\n"
            "<w>E</w> : display experience info\n",
            true, true, _cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Dungeon Interaction and Information:\n"
            "<w>O</w>/<w>C</w> : Open/Close door\n"
            "<w><<</w>/<w>></w> : use staircase (<w><<</w> enter shop)\n"
            "<w>;</w>   : examine occupied tile\n"
            "<w>x</w>   : eXamine surroundings/targets\n"
            "<w>X</w>   : eXamine level map (<w>X?</w> for help)\n"
            "<w>Ctrl-X</w> : list monsters, items, features in view\n"
            "<w>Ctrl-O</w> : show dungeon Overview\n"
            "<w>Ctrl-A</w> : toggle auto-pickup\n"
            "<w>Ctrl-T</w> : change ally pickup behaviour\n",
            true, true, _cmdhelp_textfilter);

    std::string interact =
            "<h>Item Interaction (inventory):\n"
            "<w>i</w> : show Inventory list\n"
            "<w>]</w> : show inventory of equipped items\n"
            "<w>{</w> : inscribe item\n"
            "<w>f</w> : Fire next appropriate item\n"
            "<w>F</w> : select an item and Fire it\n"
            "<w>Q</w> : select item slot to be quivered\n"
            "<w>(</w>, <w>)</w> : cycle current ammunition\n"
            "<w>e</w> : ";

    interact += (you.species == SP_VAMPIRE ? "Drain corpses" : "Eat food");
    interact +=
            " (tries floor first)\n"
            "<w>q</w> : Quaff a potion\n"
            "<w>r</w> : Read a scroll or book\n"
            "<w>M</w> : Memorise a spell from a book\n"
            "<w>w</w> : Wield an item ( <w>-</w> for none)\n"
            "<w>'</w> : wield item a, or switch to b \n"
            "    (use <w>=</w> to assign slots)\n"
            "<w>v</w> : eVoke power of wielded item\n"
            "<w>V</w> : eVoke wand\n"
            "<w>W</w>/<w>T</w> : Wear or Take off armour\n"
            "<w>P</w>/<w>R</w> : Put on or Remove jewellery\n";

            cols.add_formatted(
                  1, interact,
                  true, true, _cmdhelp_textfilter);

    interact =
            "<h>Item Interaction (floor):\n"
            "<w>,</w> : pick up items (also <w>g</w>) \n"
            "    (press twice for pick up menu)\n"
            "<w>d</w> : Drop an item\n"
            "<w>d#</w>: Drop exact number of items\n"
            "<w>c</w> : Chop up a corpse";

    if (you.species == SP_VAMPIRE && you.experience >= 6)
        interact += " or bottle its blood";

    interact +=
            "\n"
            "<w>e</w> : Eat food from floor\n";

    cols.add_formatted(
            1, interact,
            true, true, _cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Additional help:\n"
            "Many commands have context sensitive \n"
            "help, among them <w>X</w>, <w>x</w>, <w>f</w> (or any \n"
            "form of targeting), <w>Ctrl-G</w> or <w>G</w>, and \n"
            "<w>Ctrl-F</w>.\n"
            "You can read descriptions of your \n"
            "current spells (<w>I</w>), skills (<w>m?</w>) and \n"
            "abilities (<w>a!</w>).",
            true, true, _cmdhelp_textfilter);
}

static void _add_formatted_tutorial_help(column_composer &cols)
{
    unsigned ch;
    unsigned short colour;

    std::ostringstream text;
    text <<
        "<h>Item types (and common commands)\n"
        "<cyan>)</cyan> : hand weapons (<w>w</w>ield)\n"
        "<brown>(</brown> : missiles (<w>Q</w>uiver, <w>f</w>ire, <w>()</w> to cycle ammo)\n"
        "<cyan>[</cyan> : armour (<w>W</w>ear and <w>T</w>ake off)\n"
        "<brown>%</brown> : corpses and food (<w>c</w>hop up and <w>e</w>at)\n"
        "<w>?</w> : scrolls (<w>r</w>ead)\n"
        "<magenta>!</magenta> : potions (<w>q</w>uaff)\n"
        "<blue>=</blue> : rings (<w>P</w>ut on and <w>R</w>emove)\n"
        "<red>\"</red> : amulets (<w>P</w>ut on and <w>R</w>emove)\n"
        "<darkgrey>/</darkgrey> : wands (<w>Z</w>ap)\n"
        "<lightcyan>";
    get_item_symbol(DNGN_ITEM_BOOK, &ch, &colour);
    text << static_cast<char>(ch);
    text << "</lightcyan> : books (<w>r</w>ead, <w>M</w>emorise and "
        "<w>z</w>ap)\n"
        "<brown>";
    get_item_symbol(DNGN_ITEM_STAVE, &ch, &colour);
    text << static_cast<char>(ch);
    text << "</brown> : staves, rods (<w>w</w>ield and e<w>v</w>oke)\n"
            "\n"
            "<h>Movement and attacking\n"
            "Use the <w>numpad</w> for movement (try both\n"
            "Numlock on and off). You can also use\n"
            "     <w>hjkl</w> : left, down, up, right and\n"
            "     <w>yubn</w> : diagonal movement.\n"
            "Walking into a monster will attack it\n"
            "with the wielded weapon or barehanded.\n"
            "For ranged attacks use either\n"
            "<w>f</w> to launch missiles (like arrows)\n"
            "<w>z</w>/<w>Z</w> to cast spells (<w>z?</w> lists spells).\n",

    cols.add_formatted(
            0, text.str(),
            true, true, _cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Additional important commands\n"
            "<w>S</w> : Save the game and exit\n"
            "\n"
            "<w>s</w> : search for one turn (also <w>.</w> and <w>Del</w>)\n"
            "<w>5</w> : rest full/search longer (<w>Shift-Num 5</w>)\n"
            "<w>x</w> : examine surroundings\n"
            "<w>i</w> : list inventory (select item to view it)\n"
            "<w>g</w> : pick up item from ground (also <w>,</w>)\n"
            "<w>d</w> : drop item\n"
            "<w><<</w> or <w>></w> : ascend/descend the stairs\n"
            "<w>Ctrl-P</w> : show previous messages\n"
            "<w>X</w> : show map of the whole level\n"
            "<w>Ctrl-X</w> : list monsters, items, features in sight\n"
            "\n"
            "<h>Targeting (for spells and missiles)\n"
            "Use <w>+</w> (or <w>=</w>) and <w>-</w> to cycle between\n"
            "hostile monsters. <w>Enter</w> or <w>.</w> or <w>Del</w>\n"
            "all fire at the selected target.\n"
            "If the previous target is still alive\n"
            "and in sight, one of <w>f</w> or <w>p</w> fires at it\n"
            "again (without selecting anything).\n",
            true, true, _cmdhelp_textfilter, 40);
}

void list_commands(int hotkey, bool do_redraw_screen)
{
    // 2 columns, split at column 40.
    column_composer cols(2, 41);

    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines() - 1);

    if (Options.tutorial_left)
        _add_formatted_tutorial_help(cols);
    else
        _add_formatted_keyhelp(cols);

    _show_keyhelp_menu(cols.formatted_lines(), true, false, hotkey);

    if (do_redraw_screen)
    {
        clrscr();
        redraw_screen();
    }
}

#ifdef WIZARD
int list_wizard_commands(bool do_redraw_screen)
{
    // 2 columns
    column_composer cols(2, 43);
    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines());

    cols.add_formatted(0,
                       "<yellow>Player stats</yellow>\n"
                       "<w>A</w>      : set all skills to level\n"
                       "<w>g</w>      : exercise a skill\n"
                       "<w>r</w>      : change character's species\n"
                       "<w>s</w>      : gain 20000 skill points\n"
                       "<w>S</w>      : set skill to level\n"
                       "<w>x</w>      : gain an experience level\n"
                       "<w>Ctrl-X</w> : change experience level\n"
                       "<w>$</w>      : get 1000 gold\n"
                       "<w>]</w>      : get a mutation\n"
                       "<w>[</w>      : get a demonspawn mutation\n"
                       "<w>^</w>      : gain piety\n"
                       "<w>_</w>      : gain religion\n"
                       "<w>@</w>      : set Str Int Dex\n"
                       "<w>Ctrl-D</w> : change enchantments/durations\n"
                       "\n"
                       "<yellow>Other player related effects</yellow>\n"
                       "<w>c</w>      : card effect\n"
                       "<w>Ctrl-G</w> : save ghost (bones file)\n"
                       "<w>h</w>/<w>H</w>    : heal yourself (super-Heal)\n"
                       "<w>Ctrl-H</w> : set hunger state\n"
                       "<w>X</w>      : make Xom do something now\n"
                       "<w>z</w>/<w>Z</w>    : cast spell by number/name\n"
                       "\n"
                       "<yellow>Item related commands</yellow>\n"
                       "<w>a</w>      : acquirement\n"
                       "<w>C</w>      : (un)curse item\n"
                       "<w>i</w>/<w>I</w>    : identify/unidentify inventory\n"
                       "<w>o</w>/<w>%</w>    : create an object\n"
                       "<w>t</w>      : tweak object properties\n"
                       "<w>v</w>      : show gold value of an item\n"
                       "<w>|</w>      : create all unrand/fixed artefacts\n"
                       "<w>+</w>      : make randart from item\n"
                       "<w>'</w>      : list items\n",
                       true, true);

    cols.add_formatted(1,
                       "<yellow>Monster related commands</yellow>\n"
                       "<w>G</w>      : banish all monsters\n"
                       "<w>m</w>/<w>M</w>    : create monster by number/name\n"
                       "<w>\"</w>      : list monsters\n"
                       "\n"
                       "<yellow>Create level features</yellow>\n"
                       "<w>l</w>      : make entrance to labyrinth\n"
                       "<w>L</w>      : place a vault by name\n"
                       "<w>p</w>      : make entrance to pandemonium\n"
                       "<w>P</w>      : make a portal\n"
                       "<w>T</w>      : make a trap\n"
                       "<w><<</w>/<w>></w>    : create up/down staircase\n"
                       "<w>(</w>/<w>)</w>    : make feature by number/name\n"
                       "<w>\\</w>      : make a shop\n"
                       "\n"
                       "<yellow>Other level related commands</yellow>\n"
                       "<w>Ctrl-A</w> : generate new Abyss area\n"
                       "<w>b</w>      : controlled blink\n"
                       "<w>B</w>      : banish yourself to the Abyss\n"
                       "<w>R</w>      : change monster spawn rate\n"
                       "<w>k</w>      : shift section of a labyrinth\n"
                       "<w>u</w>/<w>d</w>    : shift up/down one level\n"
                       "<w>~</w>      : go to a specific level\n"
                       "<w>:</w>      : find branches in the dungeon\n"
                       "<w>{</w>      : magic mapping\n"
                       "\n"
                       "<yellow>Debugging commands</yellow>\n"
                       "<w>f</w>      : player combat damage stats\n"
                       "<w>F</w>      : combat stats with fsim_kit\n"
                       "<w>Ctrl-F</w> : combat stats (monster vs PC)\n"
                       "<w>Ctrl-I</w> : item generation stats\n"
                       "<w>O</w>      : measure exploration time\n"
                       "\n"
                       "<w>?</w>      : list wizard commands\n",
                       true, true);

    int key = _show_keyhelp_menu(cols.formatted_lines(), false, true);
    if (do_redraw_screen)
        redraw_screen();
    return key;
}
#endif
