/*
 *  File:       command.cc
 *  Summary:    Misc commands.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <4>     10/12/99        BCR             BUILD_DATE is now used in version()
 *      <3>     6/13/99         BWR             New equipment listing commands
 *      <2>     5/20/99         BWR             Swapping inventory letters.
 *      <1>     -/--/--         LRH             Created
 */

#include "AppHdr.h"
#include "command.h"

#include <stdio.h>
#include <string.h>
#include <sstream>
#include <ctype.h>

#include "externs.h"

#include "abl-show.h"
#include "chardump.h"
#include "files.h"
#include "invent.h"
#include "itemname.h"
#include "item_use.h"
#include "items.h"
#include "libutil.h"
#include "menu.h"
#include "ouch.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "version.h"
#include "view.h"

static void adjust_item(void);
static void adjust_spells(void);
static void adjust_ability(void);
static void list_wizard_commands();
#ifdef OBSOLETE_COMMAND_HELP
static const char *command_string( int i );
#endif

void quit_game(void)
{
    if (yesno("Really quit?", false, 'n'))
        ouch(INSTANT_DEATH, 0, KILLED_BY_QUITTING);
}                               // end quit_game()

static const char *features[] = {
    "Stash-tracking",

#ifdef CLUA_BINDINGS
    "Lua user scripts",
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

#ifdef UNICODE_GLYPHS
    "Unicode glyphs",
#endif
};

void version(void)
{
    mpr( "This is " CRAWL " " VERSION " (" VERSION_DETAIL ")." );
    mprf("Features: %s",
         comma_separated_line(features, features + ARRAYSIZE(features))
             .c_str());
}                               // end version()

void adjust(void)
{
    mpr( "Adjust (i)tems, (s)pells, or (a)bilities?", MSGCH_PROMPT );

    const int keyin = tolower( get_ch() );

    if (keyin == 'i')
        adjust_item();
    else if (keyin == 's')
        adjust_spells();
    else if (keyin == 'a')
        adjust_ability();
    else if (keyin == ESCAPE)
        canned_msg( MSG_OK );
    else
        canned_msg( MSG_HUH );
}                               // end adjust()

void swap_inv_slots(int from_slot, int to_slot, bool verbose)
{
    // swap items
    item_def tmp = you.inv[to_slot];
    you.inv[to_slot] = you.inv[from_slot];
    you.inv[from_slot] = tmp;

    you.inv[from_slot].link = from_slot;
    you.inv[to_slot].link = to_slot;

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
        you.wield_change = true;
}

static void adjust_item(void)
{
    int from_slot, to_slot;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    from_slot = prompt_invent_item( "Adjust which item?", MT_INVLIST, -1 );
    if (from_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    mpr(you.inv[from_slot].name(DESC_INVENTORY_EQUIP).c_str());

    to_slot = prompt_invent_item(
                    "Adjust to which letter?", 
                    MT_INVLIST, 
                    -1, 
                    false, 
                    false );
    if (to_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    swap_inv_slots(from_slot, to_slot, true);
}                               // end adjust_item()

static void adjust_spells_cleanup(bool needs_redraw)
{
    if (needs_redraw)
        redraw_screen();
}

static void adjust_spells(void)
{
    unsigned char index_1, index_2;
    unsigned char nthing = 0;

    bool needs_redraw = false;

    if (!you.spell_no)
    {
        mpr("You don't know any spells.");
        return;
    }

  query:
    mpr("Adjust which spell?", MSGCH_PROMPT);

    unsigned char keyin = get_ch();

    if (keyin == '?' || keyin == '*')
    {
        if (keyin == '*' || keyin == '?')
        {
            nthing = list_spells();
            needs_redraw = true;
        }

        if (isalpha( nthing ) || nthing == ESCAPE)
            keyin = nthing;
        else
        {
            mesclr( true );
            goto query;
        }
    }

    if (keyin == ESCAPE)
    {
        adjust_spells_cleanup(needs_redraw);
        canned_msg( MSG_OK );
        return;
    }

    int input_1 = keyin;

    if (!isalpha( input_1 ))
    {
        adjust_spells_cleanup(needs_redraw);
        mpr("You don't know that spell.");
        return;
    }

    index_1 = letter_to_index( input_1 );
    spell_type spell = get_spell_by_letter( input_1 ); 

    if (spell == SPELL_NO_SPELL)
    {
        adjust_spells_cleanup(needs_redraw);
        mpr("You don't know that spell.");
        return;
    }

    // print out targeted spell:
    mprf( "%c - %s", input_1, spell_title( spell ) );

    mpr( "Adjust to which letter?", MSGCH_PROMPT );

    keyin = get_ch();

    if (keyin == '?' || keyin == '*')
    {
        if (keyin == '*' || keyin == '?')
        {
            nthing = list_spells();
            needs_redraw = true;
        }

        if (isalpha( nthing ) || nthing == ESCAPE)
            keyin = nthing;
        else
        {
            mesclr( true );
            goto query;
        }
    }

    if (keyin == ESCAPE)
    {
        adjust_spells_cleanup(needs_redraw);
        canned_msg( MSG_OK );
        return;
    }

    int input_2 = keyin;

    if (!isalpha( input_2 ))
    {
        adjust_spells_cleanup(needs_redraw);
        mpr("What?");
        return;
    }

    adjust_spells_cleanup(needs_redraw);

    index_2 = letter_to_index( input_2 );

    // swap references in the letter table:
    int tmp = you.spell_letter_table[index_2];
    you.spell_letter_table[index_2] = you.spell_letter_table[index_1];
    you.spell_letter_table[index_1] = tmp;

    // print out spell in new slot (now at input_2)
    mprf("%c - %s", input_2, spell_title( get_spell_by_letter(input_2) ) );

    // print out other spell if one was involved (now at input_1)
    spell = get_spell_by_letter( input_1 );

    if (spell != SPELL_NO_SPELL)
        mprf("%c - %s", input_1, spell_title(spell) );
}                               // end adjust_spells()

static void adjust_ability(void)
{
    const std::vector<talent> talents = your_talents(false);

    if ( talents.empty() )
    {
        mpr("You don't currently have any abilities.");
        return;
    }

    int selected = -1;
    while ( selected < 0 )
    {
        msg::streams(MSGCH_PROMPT) << "Adjust which ability? (? or * to list)"
                                   << std::endl;

        const int keyin = get_ch();

        if ( keyin == '?' || keyin == '*' )
        {
            selected = choose_ability_menu(talents);
        }
        else if (keyin == ESCAPE || keyin == ' ' ||
                 keyin == '\r' || keyin == '\n')
        {
            canned_msg( MSG_OK );
            return;
        }
        else if ( isalpha(keyin) )
        {
            // try to find the hotkey
            for (unsigned int i = 0; i < talents.size(); ++i)
            {
                if ( talents[i].hotkey == keyin )
                {
                    selected = static_cast<int>(i);
                    break;
                }
            }

            // if we can't, cancel out
            if ( selected < 0 )
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
    
    msg::streams(MSGCH_PROMPT) << "Adjust to which letter?" << std::endl;
    
    const int keyin = get_ch();

    if ( !isalpha(keyin) )
    {
        canned_msg(MSG_HUH);
        return;
    }

    const int index2 = letter_to_index(keyin);
    if ( index1 == index2 )
    {
        mpr("That would be singularly pointless.");
        return;
    }

    // see if we moved something out
    bool printed_message = false;
    for ( unsigned int i = 0; i < talents.size(); ++i )
    {
        if ( talents[i].hotkey == keyin )
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

    // swap references in the letter table
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

        estr.str("");
        estr.clear();

        estr << ((i == EQ_CLOAK)       ? "Cloak  " :
                 (i == EQ_HELMET)      ? "Helmet " :
                 (i == EQ_GLOVES)      ? "Gloves " :
                 (i == EQ_SHIELD)      ? "Shield " :
                 (i == EQ_BODY_ARMOUR) ? "Armour " :
                 (i == EQ_BOOTS) ?
                 ((you.species == SP_CENTAUR || you.species == SP_NAGA) ?
                  "Barding" : "Boots  ") : "unknown")
             << " : ";

        if (armour_id != -1)
            estr << you.inv[armour_id].name(DESC_INVENTORY);
        else
            estr << "    none";

        mpr( estr.str().c_str(), MSGCH_EQUIPMENT, menu_colour(estr.str()) );
    }
}                               // end list_armour()

void list_jewellery(void)
{
    std::ostringstream jstr;

    for (int i = EQ_LEFT_RING; i <= EQ_AMULET; i++)
    {
        const int jewellery_id = you.equip[i];

        jstr.str("");
        jstr.clear();

        jstr << ((i == EQ_LEFT_RING)  ? "Left ring " :
                 (i == EQ_RIGHT_RING) ? "Right ring" :
                 (i == EQ_AMULET)     ? "Amulet    "
                                      : "unknown   ")
             << " : ";

        if (jewellery_id != -1)
            jstr << you.inv[jewellery_id].name(DESC_INVENTORY);
        else
            jstr << "    none";

        mpr( jstr.str().c_str(), MSGCH_EQUIPMENT, menu_colour(jstr.str()) );
    }
}                               // end list_jewellery()

void list_weapons(void)
{
    const int weapon_id = you.equip[EQ_WEAPON];

    // Output the current weapon
    //
    // Yes, this is already on the screen... I'm outputing it
    // for completeness and to avoid confusion.
    std::string wstring = "Current   : ";

    if (weapon_id != -1)
    {
        wstring += you.inv[weapon_id].name(DESC_INVENTORY_EQUIP);
    }
    else
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            wstring += "    blade hands";
        else
            wstring += "    empty hands";
    }

    mpr(wstring.c_str(), MSGCH_EQUIPMENT, menu_colour(wstring));

    // Print out the swap slots
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

        if (is_valid_item( you.inv[i] ))
            wstring += you.inv[i].name(DESC_INVENTORY_EQUIP);
        else
            wstring += "    none";

        mpr(wstring.c_str(), MSGCH_EQUIPMENT, menu_colour(wstring));
    }

    // Now we print out the current default fire weapon
    wstring = "Firing    : ";

    const int item = get_fire_item_index();

    if (item == ENDOFPACK)
        wstring += "    nothing";
    else
        wstring += you.inv[item].name(DESC_INVENTORY_EQUIP);

    mpr( wstring.c_str(), MSGCH_EQUIPMENT, menu_colour(wstring) );
}                               // end list_weapons()

static bool cmdhelp_textfilter(const std::string &tag)
{
#ifdef WIZARD
    if (tag == "wiz")
        return (true);
#endif
    return (false);
}

static const char *level_map_help = 
    "<h>Level Map ('<w>X</w><h>' in main screen):\n"
    "<w>Esc</w> : leave level map (also Space)\n"
    "<w>Dir.</w>: move cursor\n"
    "<w>/ Dir.</w>, <w>Shift-Dir.</w>: move cursor far\n"
    "<w>-</w>/<w>+</w> : scroll level map up/down\n"
    "<w>.</w>   : travel (also <w>Enter</w> and <w>,</w> and <w>;</w>)\n"
    "       (moves cursor to last travel\n"
    "       destination if still on @)\n"
    "<w><<</w>/<w>></w> : cycle through up/down stairs\n"
    "<w>^</w>   : cycle through traps\n"
    "<w>Tab</w> : cycle through shops and portals\n"
    "<w>X</w>   : cycle through travel eXclusions\n"
    "<w>x</w>   : change the radius of a travel exclusion\n"
    "<w>W</w>   : cycle through waypoints\n"
    "<w>*</w>   : cycle forward through stashes\n"
    "<w>/</w>   : cycle backward through stashes\n"
    "<w>_</w>   : cycle through altars\n"
    "<w>Ctrl-X</w> : set travel eXclusion\n"
    "<w>Ctrl-E</w> : Erase all travel exclusions\n"
    "<w>Ctrl-W</w> : set Waypoint\n"
    "<w>Ctrl-C</w> : Clear level and main maps\n";

static const char *targeting_help_1 =
    "<h>Examine surroundings ('<w>x</w><h>' in main):\n"
    "<w>Esc</w> : cancel (also <w>Space</w>)\n"
    "<w>Dir.</w>: move cursor in that direction\n"
    "<w>.</w> : move to cursor (also <w>Enter</w>, <w>Del</w>)\n"
    "<w>v</w> : describe monster under cursor\n"
    "<w>+</w> : cycle monsters forward (also <w>=</w>)\n"
    "<w>-</w> : cycle monsters backward\n"
    "<w>*</w> : cycle objects forward\n"
    "<w>/</w> : cycle objects backward (also <w>;</w>)\n"
    "<w>^</w>   : cycle through traps\n"
    "<w>_</w>   : cycle through altars\n"
    "<w><<</w>/<w>></w> : cycle through up/down stairs\n"
    "<w>Tab</w> : cycle through shops and portals\n"
    "<w>Ctrl-F</w> : change monster targeting mode\n"
    " \n"
    "<h>Targeting (zapping wands, casting spells, etc.):\n"
    "The keys from examine surroundings also work here.\n"
    "In addition, you can use:\n"
    "<w>.</w> : fire at target (<w>Enter</w>, <w>Del</w>, <w>Space</w>)\n" 
    "<w>!</w> : fire at target and stop there (may hit submerged creatures)\n"
    "<w>p</w> : fire at Previous target (also <w>t</w>, <w>f</w>)\n"
    "<w>:</w> : show/hide beam path\n"
    "<w>Shift-Dir</w> : shoot straight-line beam\n";

static const char *targeting_help_2 =
    "<h>Firing or throwing a missile:\n"
    "<w>Ctrl-P</w> : cycle to previous missile.\n"
    "<w>Ctrl-N</w> : cycle to next missile.\n";
    

static const char *interlevel_travel_branch_help =
    "<h>Interlevel Travel (choose a branch):\n"
    " Use the shortcut letter for a branch to select the branch for travel.\n"
    "\n"
    " Once you select a branch, you will be prompted for a depth in that\n"
    " branch (more help is available there).\n"
    "\n"
   " <w>Enter</w>  : Repeat last interlevel travel.\n"
    " <w>.</w>      : Travel to a level in the current branch.\n"
    " <w><<</w>      : Go up the nearest stairs.\n"
    " <w>></w>      : Go down the nearest stairs.\n"
    " <w>Ctrl-P</w> : Travel to a level in the branch above this one.\n"
    " <w>*</w>      : Show available waypoints (if any are set).\n"
    " <w>0</w>-<w>9</w>    : Go to the numbered waypoint.\n";

static const char *interlevel_travel_depth_help =
    "<h>Interlevel Travel (go to a specific level in the selected branch)\n"
    " Type in the level number you want to go to and hit Enter, or use:\n"
    " <w>Enter</w>  : Go to the default level.\n"
    " <w><<</w>      : Change the default to one level above the current.\n"
    " <w>></w>      : Change default to one level below the current.\n"
    " <w>-</w>/<w>p</w>    : Change default to the branch above this one.\n"
    " <w>$</w>      : Change default to deepest visited level in this branch.\n"
    " <w>^</w>      : Change default to the entrance to the current level.\n"
    "\n";

// Add the contents of the file fp to the scroller menu m.
// If first_hotkey is nonzero, that will be the hotkey for the
// start of the contents of the file.
// If auto_hotkeys is true, the function will try to identify
// sections and add appropriate hotkeys.
static void add_file_to_scroller(FILE* fp, formatted_scroller& m,
                                 int first_hotkey, bool auto_hotkeys )
{
    bool next_is_hotkey = false;
    bool is_first = true;
    char buf[200];

    // bracket with MEL_TITLES, so that you won't scroll
    // into it or above it
    m.add_entry(new MenuEntry(std::string(), MEL_TITLE));
    for ( int i = 0; i < get_number_of_lines(); ++i )
        m.add_entry(new MenuEntry(std::string()));
    m.add_entry(new MenuEntry(std::string(), MEL_TITLE));
    
    while (fgets(buf, sizeof buf, fp))
    {
        MenuEntry* me = new MenuEntry(buf);
        if ((next_is_hotkey && (isupper(buf[0]) || isdigit(buf[0]))) ||
            (is_first && first_hotkey))
        {
            int hotkey = is_first ? first_hotkey : buf[0];
            if ( !is_first && buf[0] == 'X' &&
                 strlen(buf) >= 3 && isdigit(buf[2]) )
            {
                // X.# is hotkeyed to the #
                hotkey = buf[2];
            }
            me->add_hotkey(hotkey);
            if ( isupper(hotkey) )
                me->add_hotkey(tolower(hotkey));
            me->level = MEL_SUBTITLE;
            me->colour = WHITE;
        }
        m.add_entry(me);
        // XXX FIXME: there must be a better way to identify sections
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
    { "crawl_manual.txt", '*', true },
    { "tables.txt", '%', false },
    { "readme.txt", '^', false },
    { "crawl_macros.txt", '~', false },
    { "crawl_options.txt", '!', false },
    { NULL, 0, false }
};

static int keyhelp_keyfilter(int ch)
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
        // fall through
    default:
        return ch;
    }
}

static void show_keyhelp_menu(const std::vector<formatted_string> &lines,
                              bool with_manual, bool easy_exit = false,
                              int hotkey = 0)
{
    formatted_scroller cmd_help;
    
    // Set flags, and use easy exit if necessary.
    int flags = MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP;
    if (easy_exit)
        flags |= MF_EASY_EXIT;
    cmd_help.set_flags(flags, false);
    
    // FIXME: Allow for hiding Page down when at the end of the listing, ditto
    // for page up at start of listing.
    cmd_help.set_more( formatted_string::parse_string(
                           "<cyan>[ + : Page down.   - : Page up."
                           "                           Esc exits.]"));

    if ( with_manual )
    {
        cmd_help.f_keyfilter = keyhelp_keyfilter;
        column_composer cols(2, 40);

        cols.add_formatted(
            0,
            "<h>Dungeon Crawl Help\n"
            "\n"
            "Press one of the following keys to\n"
            "obtain more information on a certain\n"
            "aspect of Dungeon Crawl.\n"

            "<w>?</w>: List of keys\n"
            "<w>^</w>: Quickstart Guide\n"
            "<w>:</w>: Browse character notes\n"
            "<w>~</w>: Macros help\n"
            "<w>!</w>: Options help\n"
            "<w>%</w>: Table of aptitudes\n"
            "<w>Home</w>: This screen\n",
            true, true, cmdhelp_textfilter);

        cols.add_formatted(
            1,
            "Manual Contents\n\n"
            "<w>*</w>       Table of contents\n"
            "<w>A</w>.      Overview\n"
            "<w>B</w>.      Starting Screen\n"
            "<w>C</w>.      Abilities and Stats\n"
            "<w>D</w>.      Dungeon Exploration\n"
            "<w>E</w>.      Experience and Skills\n"
            "<w>F</w>.      Monsters\n"
            "<w>G</w>.      Items\n"
            "<w>H</w>.      Spellcasting\n"
            "<w>I</w>.      Religion\n"
            "<w>J</w>.      Mutations\n"
            "<w>K</w>.      Keymaps, Macros, Options\n"
            "<w>L</w>.      Licence, Contact, History\n"
            "<w>M</w>.      Philosophy\n"
            "<w>1</w>.      List of Species\n"
            "<w>2</w>.      List of Classes\n"
            "<w>3</w>.      List of Skills\n"
            "<w>4</w>.      Keys and Commands\n"
            "<w>5</w>.      List of Enchantments\n",
            true, true, cmdhelp_textfilter);
        
        std::vector<formatted_string> blines = cols.formatted_lines();
        unsigned i;
        for (i = 0; i < blines.size(); ++i )
            cmd_help.add_item_formatted_string(blines[i]);

        while ( static_cast<int>(++i) < get_number_of_lines() )
            cmd_help.add_item_string("");
        // unscrollable
        cmd_help.add_entry(new MenuEntry(std::string(), MEL_TITLE));
    }

    for (unsigned i = 0; i < lines.size(); ++i )
        cmd_help.add_item_formatted_string(lines[i], (i == 0 ? '?' : 0) );

    if ( with_manual )
    {
        for ( int i = 0; help_files[i].name != NULL; ++i )
        {
            // attempt to open this file, skip it if unsuccessful
            FILE* fp =
                fopen(datafile_path(help_files[i].name, false).c_str(), "r");
            if ( !fp )
                continue;

            // put in a separator
            cmd_help.add_item_string("");
            cmd_help.add_item_string(std::string(get_number_of_cols()-1,'='));
            cmd_help.add_item_string("");

            // and the file itself
            add_file_to_scroller(fp, cmd_help, help_files[i].hotkey,
                                 help_files[i].auto_hotkey);

            // done with this file
            fclose(fp);
        }
    }

    if ( hotkey )
        cmd_help.jump_to_hotkey(hotkey);

    cmd_help.show();
}

void show_specific_help( const char* help )
{
    std::vector<std::string> lines = split_string("\n", help, false, true);
    std::vector<formatted_string> formatted_lines;
    for (int i = 0, size = lines.size(); i < size; ++i)
        formatted_lines.push_back(
            formatted_string::parse_string(
                lines[i], true, cmdhelp_textfilter));
    show_keyhelp_menu(formatted_lines, false, true);
}

void show_levelmap_help()
{
    show_specific_help( level_map_help );
}

void show_targeting_help()
{
    column_composer cols(2, 41);
    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines() - 1);

    cols.add_formatted(0, targeting_help_1, true, true);
    cols.add_formatted(1, targeting_help_2, true, true);
    show_keyhelp_menu(cols.formatted_lines(), false, true);
}

void show_interlevel_travel_branch_help()
{
    show_specific_help( interlevel_travel_branch_help );
}

void show_interlevel_travel_depth_help()
{
    show_specific_help( interlevel_travel_depth_help );
}

void list_commands(bool wizard, int hotkey)
{
    if (wizard)
    {
        list_wizard_commands();
        return;
    }

    // 2 columns, split at column 40.
    column_composer cols(2, 41);

    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines() - 1);

    cols.add_formatted(
            0,
            "<h>Movement:\n"
            "To move in a direction or to attack, use\n"
            "the numpad (try Numlock both off and on)\n"
            "or vi keys:\n"
            "              <w>1 2 3      y k u\n"
            "               \\|/        \\|/\n"
            "              <w>4</w>-5-<w>6</w>"
                              "      <w>h</w>-.-<w>l</w>\n"
            "               /|\\        /|\\\n"
            "              <w>7 8 9      b j n\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Rest/Search:\n"
            "<w>s</w> : rest one turn and search adjacent\n"
            "    squares (also <w>numpad-5</w>, <w>.</w>, <w>Del</w>)\n"
            "<w>5</w> : fully rest HP/MP and long search\n"
            "    (max 100 turns, also <w>Shift-numpad-5</w>)\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Extended Movement:\n"
            "<w>/ Dir.</w>, <w>Shift-Dir.</w>: long walk\n"
            "<w>* Dir.</w>, <w>Ctrl-Dir.</w> : untrap, open door,\n"
            "         attack without move\n"
            "<w>Ctrl-G</w> : interlevel travel\n"
            "<w>Ctrl-O</w> : auto-explore\n"
            "<w>Ctrl-W</w> : set Waypoint\n"
            "<w>Ctrl-F</w> : Find items\n"
            " \n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Item Interaction (inventory):\n"
            "<w>i</w> : show Inventory list\n"
            "<w>v</w> : View item description\n"
            "<w>{</w> : inscribe item\n"
            "<w>t</w> : Throw/shoot an item\n"
            "<w>f</w> : Fire first available missile\n"
            "<w>e</w> : Eat food (but tries floor first)\n"
            "<w>q</w> : Quaff a potion\n"
            "<w>z</w> : Zap a wand\n"
            "<w>r</w> : Read a scroll or book\n"
            "<w>M</w> : Memorise a spell from a book\n"
            "<w>w</w> : Wield an item ( <w>-</w> for none)\n"
            "<w>'</w> : wield item a, or switch to b\n"
            "<w>E</w> : Evoke power of wielded item\n"
            "<w>W</w> : Wear armour\n"
            "<w>T</w> : Take off armour\n"
            "<w>P</w> : Put on jewellery\n"
            "<w>R</w> : Remove jewellery\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>In-game Toggles:\n"
            "<w>Ctrl-A</w> : toggle Autopickup\n"
            "<w>Ctrl-V</w> : toggle auto-prayer"
            " \n"
            " \n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Item types (and common commands)\n"
            "<cyan>)</cyan> : hand weapons (<w>w</w>ield)\n"
            "<brown>(</brown> : missiles (<w>t</w>hrow or <w>f</w>ire)\n"
            "<cyan>[</cyan> : armour (<w>W</w>ear and <w>T</w>ake off)\n"
            "<brown>%</brown> : food and corpses (<w>e</w>at and <w>D</w>issect)\n"
            "<w>?</w> : scrolls (<w>r</w>ead)\n"
            "<magenta>!</magenta> : potions (<w>q</w>uaff)\n"
            "<blue>=</blue> : rings (<w>P</w>ut on and <w>R</w>emove)\n"
            "<red>\"</red> : amulets (<w>P</w>ut on and <w>R</w>emove)\n"
            "<lightgrey>/</lightgrey> : wands (<w>z</w>ap)\n"            
            "<lightcyan>+</lightcyan> : books (<w>r</w>ead, <w>M</w>emorise and <w>Z</w>ap)\n"   
            "<brown>\\</brown> : staves and rods (<w>w</w>ield and <w>E</w>voke)\n"
            "<lightgreen>}</lightgreen> : miscellaneous items (<w>E</w>voke)\n"
            "<lightmagenta>0</lightmagenta> : the Orb of Zot\n"
            "\n"
            "<yellow>$</yellow> : gold\n"
            " \n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(            
            0,
            "<h>Stash Management Commands:\n"
            "<w>Ctrl-F</w> : Find (in stashes and shops)\n"
            "\n"
            "Searching in stashes allows regular\n"
            "expressions, and terms like 'altar'\n"
            "or 'artefact' or 'long blades'.\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Game Saving and Quitting:\n"
            "<w>S</w> : Save game and exit \n"
            "<w>Q</w> : Quit without saving\n"
            "<w>Ctrl-X</w> : save game without query\n",
            true, true, cmdhelp_textfilter, 45);

    cols.add_formatted(
            1,
            "<h>Player Character Information:\n"
            "<w>@</w> : display character status\n"
            "<w>[</w> : display worn armour\n"
            "<w>(</w> : display current weapons (also <w>)</w>)\n"
            "<w>\"</w> : display worn jewellery\n"
            "<w>C</w> : display experience info\n"
            "<w>^</w> : show religion screen\n"
            "<w>A</w> : show Abilities/mutations\n"
            "<w>\\</w> : show item knowledge\n"
            "<w>m</w> : show skill screen\n"
            "<w>%</w> : show resistances\n",
            true, true, cmdhelp_textfilter,45);

    cols.add_formatted(
            1,
            "<h>Dungeon Interaction and Information:\n"
            "<w>o</w>/<w>c</w> : Open/Close door\n"
            "<w><<</w>/<w>></w> : use staircase (<w><<</w> enter shop)\n"
            "<w>;</w>   : examine occupied tile\n"
            "<w>x</w>   : eXamine surroundings/targets\n"
            "<w>X</w>   : eXamine level map\n"
            "<w>O</w>   : show dungeon Overview\n"
            " \n",
            true, true, cmdhelp_textfilter,45);

    cols.add_formatted(
            1,
            "<h>Item Interaction (floor):\n"
            "<w>,</w> : pick up items (also <w>g</w>) \n"
            "    (press twice for pick up menu) \n"
            "<w>d</w> : Drop an item\n"
            "<w>d#</w>: Drop exact number of items \n"
            "<w>D</w> : Dissect a corpse \n"
            "<w>e</w> : Eat food from floor \n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Other Gameplay Actions:\n"
            "<w>a</w> : use special Ability\n"
            "<w>p</w> : Pray\n"
            "<w>Z</w> : cast a spell\n"
            "<w>!</w> : shout or command allies\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Non-Gameplay Commands / Info\n"
            "<w>V</w> : display Version information\n"
            "<w>Ctrl-P</w> : show Previous messages\n"
            "<w>Ctrl-R</w> : Redraw screen\n"
            "<w>Ctrl-C</w> : Clear main and level maps\n"
            "<w>#</w> : dump character to file\n"
            "<w>:</w> : add note (use <w>?:</w> to read notes)\n"
            "<w>~</w> : add macro\n"
            "<w>=</w> : reassign inventory/spell letters"
            " \n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(            
            1,
            "<h>Shortcuts in Lists (like multidrop):\n"
            "<w>)</w> : selects hand weapons\n"
            "<w>(</w> : selects missiles\n"
            "<w>[</w> : selects armour\n"
            "<w>%</w> : selects food, <w>&</w> : selects carrion\n"
            "<w>?</w> : selects scrolls\n"
            "<w>!</w> : selects potions\n"
            "<w>=</w> : selects rings\n"
            "<w>\"</w> : selects amulets\n"
            "<w>/</w> : selects wands\n"
            "<w>+</w> : selects books\n"
            "<w>\\</w> : selects staves\n"
            "<w>}</w> : selects miscellaneous items\n"
            "Carry the Orb to the surface and win!\n"
            "<w>,</w> : global selection\n"
            "<w>-</w> : global deselection\n"
            "<w>*</w> : invert selection\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(            
            1, 
            " \n"
            "Crawl usually considers every item it\n"
            "sees as a stash. When using a value\n"
            "different from <green>stash_tracking = all</green>, you\n"
            "can use <w>Ctrl-S</w> to manually declare\n"
            "stashes, and <w>Ctrl-E</w> to erase them.\n",
            true, true, cmdhelp_textfilter);
    
    show_keyhelp_menu(cols.formatted_lines(), true, false, hotkey);
}

void list_tutorial_help()
{
    // 2 columns, split at column 40.
    column_composer cols(2, 41);
    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines());

    unsigned ch;
    unsigned short colour;

    std::ostringstream text;
    text <<
        "<h>Item types (and common commands)\n"
        "<cyan>)</cyan> : hand weapons (<w>w</w>ield)\n"
        "<brown>(</brown> : missiles (<w>t</w>hrow or <w>f</w>ire)\n"
        "<cyan>[</cyan> : armour (<w>W</w>ear and <w>T</w>ake off)\n"
        "<brown>%</brown> : food and corpses (<w>e</w>at and <w>D</w>issect)\n"
        "<w>?</w> : scrolls (<w>r</w>ead)\n"
        "<magenta>!</magenta> : potions (<w>q</w>uaff)\n"
        "<blue>=</blue> : rings (<w>P</w>ut on and <w>R</w>emove)\n"
        "<red>\"</red> : amulets (<w>P</w>ut on and <w>R</w>emove)\n"
        "<darkgrey>/</darkgrey> : wands (<w>z</w>ap)\n"
        "<lightcyan>";
    get_item_symbol(DNGN_ITEM_BOOK, &ch, &colour);
    text << static_cast<char>(ch);
    text << "</lightcyan> : books (<w>r</w>ead, <w>M</w>emorise and "
        "<w>Z</w>ap)\n"
        "<brown>";
    get_item_symbol(DNGN_ITEM_STAVE, &ch, &colour);
    text << static_cast<char>(ch);
    text << "</brown> : staves, rods (<w>w</w>ield and <w>E</w>voke)\n"
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
            "<w>t</w> to throw items by hand (like darts)\n"
            "<w>Z</w> to cast spells (<w>Z?</w> lists spells).\n",

    cols.add_formatted(
            0, text.str(),
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Additional important commands\n"
            "<w>S</w> : Save the game and exit\n"
            "<w>s</w> : search for one turn (also <w>.</w> and <w>Del</w>)\n"
            "<w>5</w> : rest full/search longer (<w>Shift-Num 5</w>)\n"
            "<w>x</w> : examine surroundings\n"
            "<w>v</w> : examine object in inventory\n"
            "<w>i</w> : list inventory\n"
            "<w>g</w> : pick up item from ground (also <w>,</w>)\n"
            "<w>d</w> : drop item\n"
            "<w>X</w> : show map of the whole level\n"
            "<w><<</w> or <w>></w> : ascend/descend the stairs\n"
            "<w>Ctrl-P</w> : show previous messages\n"
            "\n"
            "<h>Targeting (for spells and missiles)\n"
            "Use <w>+</w> (or <w>=</w>) and <w>-</w> to cycle between\n"
            "hostile monsters. <w>Enter</w> or <w>.</w> or <w>Del</w>\n"
            "all fire at the selected target.\n"
            "If the previous target is still alive\n"
            "and in sight, one of <w>f</w>, <w>p</w>, <w>t</w> fires\n"
            "at it again (without selecting anything).\n",
            true, true, cmdhelp_textfilter, 40);
            
    show_keyhelp_menu(cols.formatted_lines(), false);
}
 
static void list_wizard_commands()
{
#ifdef WIZARD
    // 2 columns
    column_composer cols(2, 43);
    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines());

    cols.add_formatted(0,
                       "a      : acquirement\n"
                       "A      : set all skills to level\n"
                       "b      : controlled blink\n"
                       "B      : banish yourself to the Abyss\n"
                       "c      : card effect\n"
                       "g      : add a skill\n"
                       "G      : banish all monsters\n"
                       "Ctrl-G : save ghost (bones file)\n"
                       "f      : player combat damage stats\n"
                       "F      : combat stats with fsim_kit\n"
                       "Ctrl-F : combat stats (monster vs PC)\n"
                       "h/H    : heal yourself (super-Heal)\n"
                       "i/I    : identify/unidentify inventory\n"
                       "l      : make entrance to labyrinth\n"
                       "m/M    : create monster by number/name\n"
                       "o/%    : create an object\n"
                       "p      : make entrance to pandemonium\n"
                       "x      : gain an experience level\n"
                       "r      : change character's species\n"
                       "s      : gain 20000 skill points\n"
                       "S      : set skill to level\n"
                       "t      : tweak object properties\n",
                       true, true);

    cols.add_formatted(1, 
                       "T      : make a trap\n"
                       "v      : show gold value of an item\n"
                       "X      : make Xom do something now\n"
                       "z/Z    : cast spell by number/name\n"
                       "$      : get 1000 gold\n"
                       "</>    : create up/down staircase\n"
                       "u/d    : shift up/down one level\n"
                       "~/\"    : go to a specific level\n"
                       "(/)    : make feature by number/name\n"
                       "]      : get a mutation\n"
                       "[      : get a demonspawn mutation\n"
                       ":      : find branches in the dungeon\n"
                       "{      : magic mapping\n"
                       "^      : gain piety\n"
                       "_      : gain religion\n"
                       "'      : list items\n"
                       "?      : list wizard commands\n"
                       "|      : make unrand/fixed artefacts\n"
                       "+      : make randart from item\n"
                       "=      : sum skill points\n"
                       "@      : set Str Int Dex\n"
                       "\\      : make a shop\n",
                       true, true);
    show_keyhelp_menu(cols.formatted_lines(), false, true);
#endif
}
