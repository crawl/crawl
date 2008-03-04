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
#include "branch.h"
#include "chardump.h"
#include "cio.h"
#include "database.h"
#include "describe.h"
#include "files.h"
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
#include "player.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
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
    char buf[10];
    mpr("Are you sure you want to quit (enter \"yes\" to confirm)? ",
        MSGCH_PROMPT);
    if (!cancelable_get_line(buf, sizeof buf) && !strcasecmp(buf, "yes"))
        ouch(INSTANT_DEATH, 0, KILLED_BY_QUITTING);
    else
        canned_msg(MSG_OK);
}

static const char *features[] = {
    "Stash-tracking",

#ifdef CLUA_BINDINGS
    "Lua user scripts",
#endif

#ifdef WIZARD
    "Wizard mode",
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

#ifdef USE_TILE
    "Tile support",
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
    
    // slot switching
    tmp.slot = you.inv[to_slot].slot;
    you.inv[to_slot].slot = you.inv[from_slot].slot;
    you.inv[from_slot].slot = tmp.slot;

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
    {
        you.wield_change = true;
        you.quiver_change = true;
    }
    if (!you.quiver_change)
    {
        int quiver = you.quiver[get_quiver_type()];
        if (to_slot == quiver || from_slot == quiver)
            you.quiver_change = true;
    }

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

static void adjust_spells(void)
{
    if (!you.spell_no)
    {
        mpr("You don't know any spells.");
        return;
    }

    // Select starting slot
    mpr("Adjust which spell?", MSGCH_PROMPT);

    int keyin = 0;
    if ( Options.auto_list )
        keyin = list_spells();
    else
    {
        keyin = get_ch();
        if (keyin == '?' || keyin == '*')
            keyin = list_spells();
    }
    
    if ( !isalpha(keyin) )
    {
        canned_msg( MSG_OK );
        return;
    }

    const int input_1 = keyin;
    const int index_1 = letter_to_index( input_1 );
    spell_type spell = get_spell_by_letter( input_1 );

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        return;
    }

    // print out targeted spell:
    mprf( "%c - %s", keyin, spell_title( spell ) );

    // Select target slot
    keyin = 0;
    while ( !isalpha(keyin) )
    {
        mpr( "Adjust to which letter?", MSGCH_PROMPT );
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
    const int index_2 = letter_to_index( keyin );

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
        int       colour    = MSGCOL_BLACK;

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
        {
            estr << you.inv[armour_id].name(DESC_INVENTORY);
            colour = menu_colour(estr.str(),
                                 menu_colour_item_prefix(you.inv[armour_id]),
                                 "equip");
        }
        else if (!you_can_wear(i,true))
            estr << "    (unavailable)";
        else if (!you_tran_can_wear(i, true))
            estr << "    (currently unavailable)";
        else if (!you_can_wear(i))
            estr << "    (restricted)";
        else
            estr << "    none";

        if (colour == MSGCOL_BLACK)
            colour = menu_colour(estr.str(), "", "equip");

        mpr( estr.str().c_str(), MSGCH_EQUIPMENT, colour);
    }
}                               // end list_armour()

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

        if (jewellery_id != -1)
        {
            jstr << you.inv[jewellery_id].name(DESC_INVENTORY);
            std::string
                prefix = menu_colour_item_prefix(you.inv[jewellery_id]);
            colour = menu_colour(jstr.str(), prefix, "equip");
        }
        else if (!you_tran_can_wear(i))
            jstr << "    (currently unavailable)";
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

        colour = MSGCOL_BLACK;
        if (is_valid_item( you.inv[i]) &&
               (you.inv[i].base_type == OBJ_WEAPONS
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

    // Now we print out the current default fire weapon
    wstring = "Firing    : ";

    const int item = get_current_fire_item();

    colour = MSGCOL_BLACK;
    if (item == ENDOFPACK)
        wstring += "    nothing";
    else
    {
        wstring += you.inv[item].name(DESC_INVENTORY_EQUIP);
        colour = menu_colour(wstring,
                             menu_colour_item_prefix(you.inv[item]),
                             "equip");
    }

    if (colour == MSGCOL_BLACK)
        colour = menu_colour(wstring, "", "equip");

    mpr( wstring.c_str(), MSGCH_EQUIPMENT, colour );
}                               // end list_weapons()

static bool cmdhelp_textfilter(const std::string &tag)
{
#ifdef WIZARD
    if (tag == "wiz")
        return (true);
#endif
    return (false);
}

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
    "<w>Shift-Dir</w> : shoot straight-line beam\n"
#ifdef WIZARD
    " \n"
    "<h>Wizard targeting comands:</h>\n"
    "<w>F</w>: make target friendly\n"
    "<w>s</w>: force target to shout or speak\n"
    "<w>g</w>: give item to monster\n"
#endif
;

static const char *targeting_help_2 =
    "<h>Firing or throwing a missile:\n"
    "<w>Ctrl-P</w> : cycle to previous missile.\n"
    "<w>Ctrl-N</w> : cycle to next missile.\n"
    "<w>i</w>      : choose from inventory.\n"
;
    

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

static std::string list_commands_err = "";


static bool compare_mon_names(MenuEntry *entry_a, MenuEntry* entry_b)
{
    monster_type *a = static_cast<monster_type*>( entry_a->data );
    monster_type *b = static_cast<monster_type*>( entry_b->data );

    if (*a == *b)
        return false;

    std::string a_name = mons_type_name(*a, DESC_PLAIN);
    std::string b_name = mons_type_name(*b, DESC_PLAIN);
    return (lowercase(a_name) < lowercase(b_name));
}

// Compare monsters by location-independant level, or by hitdice if
// levels are equal, or by name if both level and hitdice are equal.
static bool compare_mon_toughness(MenuEntry *entry_a, MenuEntry* entry_b)
{
    monster_type *a = static_cast<monster_type*>( entry_a->data );
    monster_type *b = static_cast<monster_type*>( entry_b->data );

    if (*a == *b)
        return false;

    int a_toughness = mons_global_level(*a);
    int b_toughness = mons_global_level(*b);

    if (a_toughness == b_toughness)
    {
        a_toughness = mons_type_hit_dice(*a);
        b_toughness = mons_type_hit_dice(*b);
    }

    if (a_toughness == b_toughness)
    {
        std::string a_name = mons_type_name(*a, DESC_PLAIN);
        std::string b_name = mons_type_name(*b, DESC_PLAIN);
        return ( lowercase(a_name) < lowercase(b_name));
    }

    return (a_toughness < b_toughness);
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
                std::sort(items.begin(), items.end(), compare_mon_names);
            else
                std::sort(items.begin(), items.end(), compare_mon_toughness);

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

static std::vector<std::string> get_desc_keys(std::string regex,
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
        {
            all_matches.push_back(tmp[i]);
        }

    return (all_matches);
}

static std::vector<std::string> get_monster_keys(unsigned char showchar)
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

        if (getLongDescription(me->name) == "")
            continue;

        if (me->showchar == showchar)
            mon_keys.push_back(me->name);
    }

    return (mon_keys);
}

static std::vector<std::string> get_god_keys()
{
    std::vector<std::string> names;

    for (int i = ((int) GOD_NO_GOD) + 1; i < NUM_GODS; i++)
    {
        god_type which_god = static_cast<god_type>(i);

        names.push_back(god_name(which_god));
    }

    return names;
}

static std::vector<std::string> get_branch_keys()
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

    return names;
}

static bool monster_filter(std::string key, std::string body)
{
    int mon_num = get_monster_by_name(key.c_str(), true);
    return (mon_num == MONS_PROGRAM_BUG || mons_global_level(mon_num) == 0);
}

static bool spell_filter(std::string key, std::string body)
{
    return (spell_by_name(key) == SPELL_NO_SPELL);
}

static bool item_filter(std::string key, std::string body)
{
    return (item_types_by_name(key).base_type == OBJ_UNASSIGNED);
}

static bool feature_filter(std::string key, std::string body)
{
    return (feat_by_desc(key) == DNGN_UNSEEN);
}

typedef void (*db_keys_recap)(std::vector<std::string>&);

static void recap_mon_keys(std::vector<std::string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        monster_type type = get_monster_by_name(keys[i], true);
        keys[i] = mons_type_name(type, DESC_PLAIN);
    }
}

static void recap_feat_keys(std::vector<std::string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        dungeon_feature_type type = feat_by_desc(keys[i]);
        keys[i] = feature_description(type);
        //fprintf(stderr, "%s\n", keys[i].c_str());
    }
}

static bool do_description(std::string key, std::string footer = "")
{
    std::string desc = getLongDescription(key);

    monster_type mon_num = get_monster_by_name(key, true);
    if (mon_num != MONS_PROGRAM_BUG)
    {
        if (mons_genus(mon_num) == MONS_DRACONIAN)
        {
            monsters mon;

            mon.type = mon_num;

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
                mon.number = mon_num;
                break;
            default:
                mon.number = 0;
                break;
            }

            describe_monsters(mon);
            return (false);
        }

        std::string symbol = "";
        symbol += get_monster_data(mon_num)->showchar;
        if (isupper(symbol[0]))
            symbol = "cap-" + symbol;

        std::string symbol_prefix = "__";
        symbol_prefix += symbol;
        symbol_prefix += "_prefix";
        desc = getLongDescription(symbol_prefix) + desc;

        std::string symbol_suffix = "__";
        symbol_suffix += symbol;
        symbol_suffix += "_suffix";
        desc += getLongDescription(symbol_suffix);
    }

    key = uppercase_first(key);
    key += "$$";

    clrscr();
    print_description(key + desc);

    if (footer != "")
    {
        const int numcols = get_number_of_cols();
        int num_lines = linebreak_string2(footer, numcols);
        num_lines++;

        cgotoxy(1, get_number_of_lines() - num_lines);

        cprintf(footer.c_str());
    }

    return (true);
}

static bool find_description(bool &again)
{
    again = true;

    clrscr();
    viewwindow(true, false);

    mpr("Describe a (M)onster, (S)pell, (I)tem, (F)eature, (G)od "
        "or (B)ranch?", MSGCH_PROMPT);

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

    switch(ch)
    {
    case 'M':
        type       = "monster";
        extra      = "  Enter a single letter to list monsters displayed by "
            "that symbol.";
        filter     = monster_filter;
        recap      = recap_mon_keys;
        doing_mons = true;
        break;
    case 'S':
        type   = "spell";
        filter = spell_filter;
        break;
    case 'I':
        type        = "item";
        extra      = "  Enter a single letter to list items displayed by "
            "that symbol.";
        filter      = item_filter;
        doing_items = true;
        break;
    case 'F':
        type   = "feature";
        filter = feature_filter;
        recap  = recap_feat_keys;
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
        list_commands_err = "Okay, then.";
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
            list_commands_err = "Okay, then.";
            return (false);
        }

        regex = trimmed_string(buf);

        if (regex == "")
        {
            list_commands_err = "Description must contain at least "
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

        if (desc != "")
            exact_match = true;
    }

    std::vector<std::string> key_list;

    if (by_mon_symbol)
        key_list = get_monster_keys(regex[0]);
    else if (by_item_symbol)
        key_list = item_name_list_for_glyph(regex[0]);
    else if (doing_gods)
        key_list = get_god_keys();
    else if (doing_branches)
        key_list = get_branch_keys();
    else
        key_list = get_desc_keys(regex, filter);

    if (recap != NULL)
        (*recap)(key_list);

    if (key_list.size() == 0)
    {
        if (by_mon_symbol)
        {
            list_commands_err  = "No monsters with symbol '";
            list_commands_err += regex;
            list_commands_err += "'";
        }
        else if (by_item_symbol)
        {
            list_commands_err  = "No items with symbol '";
            list_commands_err += regex;
            list_commands_err += "'";
        }
        else
        {
            list_commands_err  = "No matching ";
            list_commands_err += type;
            list_commands_err += "s";
        }
        return (false);
    }
    else if (key_list.size() > 52)
    {
        if (by_mon_symbol)
        {
            list_commands_err  = "Too many monsters with symbol '";
            list_commands_err += regex;
            list_commands_err += "' to display";
        }
        else
        {
            std::ostringstream os;
            os << "Too many matching " << type << "s (" << key_list.size()
               << ") to display.";
            list_commands_err = os.str();
        }
        return (false);
    }
    else if (key_list.size() == 1)
    {
        return do_description(key_list[0]);
    }

    if (exact_match)
    {
        std::string footer = "This entry is an exact match for '";
        footer += regex;
        footer += "'.  To see non-exact matches, press space.";

        if (!do_description(regex, footer))
        {
            DEBUGSTR("do_description() returned false for exact_match");
            return (false);
        }

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

            if (do_description(key))
                if ( getch() == 0 )
                    getch();
        }
    }

    return (false);
}

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
        break;

    case '/':
    {
        bool again = false;
        do {
            if (find_description(again))
                if ( getch() == 0 )
                    getch();
            if (again)
                mesclr(true);
        } while(again);

        viewwindow(true, false);

        return -1;
    }
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

// to highlight species in aptitudes list ('?%')
std::string help_highlighter::get_species_key() const
{
    if (player_genus(GENPC_DRACONIAN) && you.experience_level < 7)
        return "";

    std::string result = species_name(you.species, you.experience_level);
    if (player_genus(GENPC_DRACONIAN))
        strip_tag(result,
                  species_name(you.species, you.experience_level, true));
    
    result += "  ";
    return (result);
}
////////////////////////////////////////////////////////////////////////////

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
    cmd_help.set_tag("help");

    // FIXME: Allow for hiding Page down when at the end of the listing, ditto
    // for page up at start of listing.
    cmd_help.set_more( formatted_string::parse_string(
                           "<cyan>[ + : Page down.   - : Page up."
                           "                           Esc exits.]"));

    if ( with_manual )
    {
        cmd_help.set_highlighter(new help_highlighter);
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
            "<w>/</w>: Lookup description\n"
            "<w>Home</w>: This screen\n",
            true, true, cmdhelp_textfilter);

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

void show_specific_help( const std::string &help )
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
    show_specific_help( getHelpString("level-map") );
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

void list_commands(bool wizard, int hotkey, bool do_redraw_screen)
{
    if (wizard)
    {
        list_wizard_commands();

        if (do_redraw_screen)
            redraw_screen();

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
            "              <w>4</w>-<w>5</w>-<w>6</w>"
                    "      <w>h</w>-<w>.</w>-<w>l</w>\n"
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
            "<w>]</w> : show inventory of equipped items\n"
            "<w>v</w> : View item description\n"
            "<w>{</w> : inscribe item\n"
            "<w>f</w> : Fire or throw an item\n"
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
            "<w>R</w> : Remove jewellery"
            "\n ",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>In-game Toggles:\n"
            "<w>Ctrl-A</w> : toggle Autopickup\n"
            "<w>Ctrl-V</w> : toggle auto-prayer\n"
            "\n ",
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
            "<lightmagenta>0</lightmagenta> : the Orb of Zot (Carry the Orb \n"
            "    to the surface and win!)\n"
            "<yellow>$</yellow> : gold\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(            
            0,
            "<h>Stash Management Commands:\n"
            "<w>Ctrl-F</w> : Find (in stashes and shops)\n"
            "\n"
            "Searching in stashes allows regular\n"
            "expressions, and terms like 'altar'\n"
            "or 'artefact' or 'long blades'.\n"
            "\n"
            "For more help on searching, you can\n"
            "hit ? at the search prompt.\n",
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
            "<w>(</w> : cycle current ammunition\n"
            "<w>)</w> : display current weapons\n"
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
            "<w>O</w>   : show dungeon Overview\n",
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
            "<w>!</w> : shout or command allies\n"
            "<w>`</w> : re-do previous command\n"
            "<w>0</w> : repeat next command # of times\n"
            " \n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Non-Gameplay Commands / Info\n"
            "<w>V</w> : display Version information\n"
            "<w>Ctrl-P</w> : show Previous messages\n"
            "<w>Ctrl-R</w> : Redraw screen\n"
            "<w>Ctrl-C</w> : Clear main and level maps\n"
            "<w>Ctrl-I</w> : annotate the dungeon level\n"
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
            "<w>.</w> : selects next item\n"
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
    
    list_commands_err = "";
    show_keyhelp_menu(cols.formatted_lines(), true, false, hotkey);

    if (do_redraw_screen)
    {
        clrscr();
        redraw_screen();
    }

    if (list_commands_err != "")
        mpr(list_commands_err.c_str());
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
                       "Ctrl-A : generate new Abyss area\n"
                       "b      : controlled blink\n"
                       "B      : banish yourself to the Abyss\n"
                       "c      : card effect\n"
                       "C      : (un)curse item\n"
                       "g      : add a skill\n"
                       "G      : banish all monsters\n"
                       "Ctrl-G : save ghost (bones file)\n"
                       "f      : player combat damage stats\n"
                       "F      : combat stats with fsim_kit\n"
                       "Ctrl-F : combat stats (monster vs PC)\n"
                       "h/H    : heal yourself (super-Heal)\n"
                       "i/I    : identify/unidentify inventory\n"
                       "Ctrl-I : item generation stats\n"
                       "l      : make entrance to labyrinth\n"
                       "L      : place a vault by name\n"
                       "m/M    : create monster by number/name\n"
                       "o/%    : create an object\n"
                       "p      : make entrance to pandemonium\n"
                       "P      : make a portal (i.e., bazaars)\n"
                       "r      : change character's species\n"
                       "s      : gain 20000 skill points\n"
                       "S      : set skill to level\n",
                       true, true);

    cols.add_formatted(1, 
                       "t      : tweak object properties\n"
                       "T      : make a trap\n"
                       "v      : show gold value of an item\n"
                       "x      : gain an experience level\n"
                       "Ctrl-X : change experience level\n"
                       "X      : make Xom do something now\n"
                       "z/Z    : cast spell by number/name\n"
                       "$      : get 1000 gold\n"
                       "</>    : create up/down staircase\n"
                       "u/d    : shift up/down one level\n"
                       "~      : go to a specific level\n"
                       "(/)    : make feature by number/name\n"
                       "]      : get a mutation\n"
                       "[      : get a demonspawn mutation\n"
                       ":      : find branches in the dungeon\n"
                       "{      : magic mapping\n"
                       "^      : gain piety\n"
                       "_      : gain religion\n"
                       "'      : list items\n"
                       "\"      : list monsters\n"
                       "?      : list wizard commands\n"
                       "|      : make unrand/fixed artefacts\n"
                       "+      : make randart from item\n"
                       "@      : set Str Int Dex\n"
                       "\\      : make a shop\n",
                       true, true);
    show_keyhelp_menu(cols.formatted_lines(), false, true);
#endif
}
