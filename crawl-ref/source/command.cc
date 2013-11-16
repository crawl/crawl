/**
 * @file
 * @brief Misc commands.
**/

#include "AppHdr.h"

#include "command.h"

#include <stdio.h>
#include <string.h>
#include <sstream>
#include <ctype.h>

#include "externs.h"
#include "options.h"

#include "abl-show.h"
#include "branch.h"
#include "chardump.h"
#include "cio.h"
#include "colour.h"
#include "database.h"
#include "decks.h"
#include "describe.h"
#include "directn.h"
#include "files.h"
#include "godmenu.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "showsymb.h"
#include "skills2.h"
#include "species.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "syscalls.h"
#include "tagstring.h"
#include "terrain.h"
#ifdef USE_TILE
#include "tilepick.h"
#endif
#include "transform.h"
#include "hints.h"
#include "version.h"
#include "view.h"
#include "viewchar.h"

static void _adjust_item(void);
static void _adjust_spell(void);
static void _adjust_ability(void);

static const char *features[] = {
#ifdef CLUA_BINDINGS
    "Lua user scripts",
#endif

#ifdef USE_TILE_LOCAL
    "Tile support",
#endif

#ifdef USE_TILE_WEB
    "Web Tile support",
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
};

static string _get_version_information(void)
{
    return string("This is <w>" CRAWL " ") + Version::Long + "</w>\n";
}

static string _get_version_features(void)
{
    string result  = "<w>Features</w>\n";
           result += "--------\n";

    for (unsigned int i = 0; i < ARRAYSZ(features); i++)
    {
        result += " * ";
        result += features[i];
        result += "\n";
    }

    return result;
}

static void _add_file_to_scroller(FILE* fp, formatted_scroller& m,
                                  int first_hotkey  = 0,
                                  bool auto_hotkeys = false);

static string _get_version_changes(void)
{
    // Attempts to print "Highlights" of the latest version.
    FILE* fp = fopen_u(datafile_path("changelog.txt", false).c_str(), "r");
    if (!fp)
        return "";

    string result = "";
    string help;
    char buf[200];
    bool start = false;
    while (fgets(buf, sizeof buf, fp))
    {
        // Remove trailing spaces.
        for (int i = strlen(buf) - 1; i >= 0; i++)
        {
            if (isspace(buf[i]))
                buf[i] = 0;
            else
                break;
        }
        help = buf;

        // Look for version headings
        if (help.find("Stone Soup ") == 0)
        {
            // Stop if this is for an older major version; otherwise, highlight
            if (help.find(string("Stone Soup ")+Version::Major) == string::npos)
                break;
            else
                goto highlight;
        }

        if (help.find("Highlights") != string::npos)
        {
        highlight:
            // Highlight the Highlights, so to speak.
            string text  = "<w>";
                   text += help;
                   text += "</w>";
                   text += "\n";
            result += text;
            // And start printing from now on.
            start = true;
        }
        else if (!start)
            continue;
        else
        {
            result += buf;
            result += "\n";
        }
    }
    fclose(fp);

    // Did we ever get to print the Highlights?
    if (start)
    {
        result.erase(1+result.find_last_not_of('\n'));
        result += "\n\n";
        result += "For earlier changes, see changelog.txt "
                  "in the docs/ directory.";
    }
    else
    {
        result += "For a list of changes, see changelog.txt in the docs/ "
                  "directory.";
    }

    result += "\n\n";

    return result;
}

//#define DEBUG_FILES
static void _print_version(void)
{
    formatted_scroller cmd_version;

    // Set flags.
    int flags = MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP | MF_EASY_EXIT;
    cmd_version.set_flags(flags, false);
    cmd_version.set_tag("version");
    cmd_version.set_more();

    cmd_version.add_text(_get_version_information(), true);
    cmd_version.add_text(_get_version_features(), true);
    cmd_version.add_text(_get_version_changes(), true);

    cmd_version.show();
}

void adjust(void)
{
    mpr("Adjust (i)tems, (s)pells, or (a)bilities? ", MSGCH_PROMPT);

    const int keyin = toalower(get_ch());

    if (keyin == 'i')
        _adjust_item();
    else if (keyin == 's')
        _adjust_spell();
    else if (keyin == 'a')
        _adjust_ability();
    else if (key_is_escape(keyin))
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
    you.inv[to_slot].slot  = index_to_letter(to_slot);//you.inv[from_slot].slot is 0 when 'from_slot' contains no item.
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
        mpr_nocap(you.inv[to_slot].name(DESC_INVENTORY_EQUIP).c_str());

        if (you.inv[from_slot].defined())
            mpr_nocap(you.inv[from_slot].name(DESC_INVENTORY_EQUIP).c_str());
    }

    if (to_slot == you.equip[EQ_WEAPON] || from_slot == you.equip[EQ_WEAPON])
    {
        you.wield_change = true;
        you.m_quiver->on_weapon_changed();
    }
    else // just to make sure
        you.redraw_quiver = true;

    // Remove the moved items from last_drop if they're there.
    you.last_pickup.erase(to_slot);
    you.last_pickup.erase(from_slot);
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

    mpr_nocap(you.inv[from_slot].name(DESC_INVENTORY_EQUIP).c_str());

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
    you.wield_change = true;
    you.redraw_quiver = true;
}

static void _adjust_spell(void)
{
    if (!you.spell_no)
    {
        canned_msg(MSG_NO_SPELLS);
        return;
    }

    // Select starting slot
    mpr("Adjust which spell? ", MSGCH_PROMPT);

    int keyin = 0;
    if (Options.auto_list)
        keyin = list_spells(false, false, false, "Adjust which spell?");
    else
    {
        keyin = get_ch();
        if (keyin == '?' || keyin == '*')
            keyin = list_spells(false, false, false, "Adjust which spell?");
    }

    if (!isaalpha(keyin))
    {
        canned_msg(MSG_OK);
        return;
    }

    const int input_1 = keyin;
    const int index_1 = letter_to_index(input_1);
    spell_type spell  = get_spell_by_letter(input_1);

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        return;
    }

    // Print targeted spell.
    mprf_nocap("%c - %s", keyin, spell_title(spell));

    // Select target slot.
    keyin = 0;
    while (!isaalpha(keyin))
    {
        mpr("Adjust to which letter? ", MSGCH_PROMPT);
        keyin = get_ch();
        if (key_is_escape(keyin))
        {
            canned_msg(MSG_OK);
            return;
        }
        // FIXME: It would be nice if the user really could select letters
        // without spells from this menu.
        if (keyin == '?' || keyin == '*')
            keyin = list_spells(true, false, false, "Adjust to which letter?");
    }

    const int input_2 = keyin;
    const int index_2 = letter_to_index(keyin);

    // swap references in the letter table:
    const int tmp = you.spell_letter_table[index_2];
    you.spell_letter_table[index_2] = you.spell_letter_table[index_1];
    you.spell_letter_table[index_1] = tmp;

    // print out spell in new slot
    mprf_nocap("%c - %s", input_2, spell_title(get_spell_by_letter(input_2)));

    // print out other spell if one was involved (now at input_1)
    spell = get_spell_by_letter(input_1);

    if (spell != SPELL_NO_SPELL)
        mprf_nocap("%c - %s", input_1, spell_title(spell));
}

static void _adjust_ability(void)
{
    const vector<talent> talents = your_talents(false);

    if (talents.empty())
    {
        mpr("You don't currently have any abilities.");
        return;
    }

    int selected = -1;
    mpr("Adjust which ability? ", MSGCH_PROMPT);

    if (Options.auto_list)
        selected = choose_ability_menu(talents);
    else
    {
        const int keyin = get_ch();

        if (keyin == '?' || keyin == '*')
            selected = choose_ability_menu(talents);
        else if (key_is_escape(keyin) || keyin == ' '
                 || keyin == '\r' || keyin == '\n')
        {
            canned_msg(MSG_OK);
            return;
        }
        else if (isaalpha(keyin))
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
        }
    }

    // If we couldn't find anything, cancel out.
    if (selected < 0)
    {
        mpr("No such ability.");
        return;
    }

    mprf_nocap("%c - %s", static_cast<char>(talents[selected].hotkey),
               ability_name(talents[selected].which));

    const int index1 = letter_to_index(talents[selected].hotkey);

    mpr("Adjust to which letter?", MSGCH_PROMPT);

    const int keyin = get_ch();

    if (!isaalpha(keyin))
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
            mprf("Swapping with: %c - %s", static_cast<char>(keyin),
                 ability_name(talents[i].which));
            printed_message = true;
            break;
        }
    }

    if (!printed_message)
    {
        mprf("Moving to: %c - %s", static_cast<char>(keyin),
             ability_name(talents[selected].which));
    }

    // Swap references in the letter table.
    ability_type tmp = you.ability_letter_table[index2];
    you.ability_letter_table[index2] = you.ability_letter_table[index1];
    you.ability_letter_table[index1] = tmp;
}

void list_armour()
{
    ostringstream estr;
    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_ARMOUR; i++)
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
            colour = menu_colour(estr.str(), item_prefix(you.inv[armour_id]),
                                 "equip");
        }
        else if (!you_can_wear(i))
            estr << "    (restricted)";
        else
            estr << "    none";

        if (colour == MSGCOL_BLACK)
            colour = menu_colour(estr.str(), "", "equip");

        mpr(estr.str().c_str(), MSGCH_EQUIPMENT, colour);
    }
}

void list_jewellery(void)
{
    string jstr;
    int cols = get_number_of_cols() - 1;
    bool split = you.species == SP_OCTOPODE && cols > 84;

    for (int i = EQ_LEFT_RING; i < NUM_EQUIP; i++)
    {
        if ((you.species != SP_OCTOPODE && i > EQ_AMULET)
            || (you.species == SP_OCTOPODE && i < EQ_AMULET))
        {
            continue;
        }

        const int jewellery_id = you.equip[i];
        int       colour       = MSGCOL_BLACK;

        const char *slot =
                 (i == EQ_LEFT_RING)  ? "Left ring" :
                 (i == EQ_RIGHT_RING) ? "Right ring" :
                 (i == EQ_AMULET)     ? "Amulet" :
                 (i == EQ_RING_ONE)   ? "1st ring" :
                 (i == EQ_RING_TWO)   ? "2nd ring" :
                 (i == EQ_RING_THREE) ? "3rd ring" :
                 (i == EQ_RING_FOUR)  ? "4th ring" :
                 (i == EQ_RING_FIVE)  ? "5th ring" :
                 (i == EQ_RING_SIX)   ? "6th ring" :
                 (i == EQ_RING_SEVEN) ? "7th ring" :
                 (i == EQ_RING_EIGHT) ? "8th ring"
                                      : "unknown";

        string item;
        if (jewellery_id != -1 && !you_tran_can_wear(you.inv[jewellery_id])
            || !you_tran_can_wear(i))
        {
            item = "    (currently unavailable)";
        }
        else if (jewellery_id != -1)
        {
            item = you.inv[jewellery_id].name(DESC_INVENTORY);
            string prefix = item_prefix(you.inv[jewellery_id]);
            colour = menu_colour(item, prefix, "equip");
        }
        else
            item = "    none";

        if (colour == MSGCOL_BLACK)
            colour = menu_colour(item, "", "equip");

        item = chop_string(make_stringf("%-*s: %s",
                                        split ? cols > 96 ? 9 : 8 : 11,
                                        slot, item.c_str()),
                           split && i > EQ_AMULET ? (cols - 1) / 2 : cols);
        item = colour_string(item, colour);

        if (split && i > EQ_AMULET && (i - EQ_AMULET) % 2)
            jstr = item + " ";
        else
            mpr(jstr + item, MSGCH_EQUIPMENT);
    }
}

void toggle_viewport_monster_hp()
{
    crawl_state.viewport_monster_hp = !crawl_state.viewport_monster_hp;
    viewwindow();
}

static bool _cmdhelp_textfilter(const string &tag)
{
#ifdef WIZARD
    if (tag == "wiz")
        return true;
#endif
    return false;
}

static const char *targetting_help_1 =
    "<h>Examine surroundings ('<w>x</w><h>' in main):\n"
    "<w>Esc</w> : cancel (also <w>Space</w>, <w>x</w>)\n"
    "<w>Dir.</w>: move cursor in that direction\n"
    "<w>.</w> : move to cursor (also <w>Enter</w>, <w>Del</w>)\n"
    "<w>g</w> : pick up item at cursor\n"
    "<w>v</w> : describe monster under cursor\n"
    "<w>+</w> : cycle monsters forward (also <w>=</w>)\n"
    "<w>-</w> : cycle monsters backward\n"
    "<w>*</w> : cycle objects forward\n"
    "<w>/</w> : cycle objects backward (also <w>;</w>)\n"
    "<w>^</w> : cycle through traps\n"
    "<w>_</w> : cycle through altars\n"
    "<w><<</w>/<w>></w> : cycle through up/down stairs\n"
    "<w>Tab</w> : cycle through shops and portals\n"
    "<w>r</w> : move cursor to you\n"
    "<w>e</w> : create/remove travel exclusion\n"
#ifndef USE_TILE_LOCAL
    "<w>Ctrl-L</w> : targetting via monster list\n"
#endif
    "<w>Ctrl-P</w> : repeat prompt\n"
;
#ifdef WIZARD
static const char *targetting_help_wiz =
    "<h>Wizard targetting commands:</h>\n"
    "<w>Ctrl-C</w> : cycle through beam paths\n"
    "<w>D</w>: get debugging information about the monster\n"
    "<w>o</w>: give item to monster\n"
    "<w>F</w>: cycle monster friendly/good neutral/neutral/hostile\n"
    "<w>Ctrl-H</w>: heal the monster to full hit points\n"
    "<w>P</w>: apply divine blessing to monster\n"
    "<w>m</w>: move monster or player\n"
    "<w>M</w>: cause spell miscast for monster or player\n"
    "<w>s</w>: force monster to shout or speak\n"
    "<w>S</w>: make monster a summoned monster\n"
    "<w>w</w>: calculate shortest path to any point on the map\n"
    "<w>\"</w>: get debugging information about a portal\n"
    "<w>~</w>: polymorph monster to specific type\n"
    "<w>,</w>: bring down the monster to 1 hp\n"
    "<w>Ctrl-B</w>: banish monster\n"
    "<w>Ctrl-K</w>: kill monster\n"
;
#endif

static const char *targetting_help_2 =
    "<h>Targetting (zap wands, cast spells, etc.):\n"
    "Most keys from examine surroundings work.\n"
    "Some keys fire at the target. By default,\n"
    "range is respected and beams don't stop.\n"
    "<w>Enter</w> : fire (<w>Space</w>, <w>Del</w>)\n"
    "<w>.</w> : fire, stop at target\n"
    "<w>@</w> : fire, stop at target, ignore range\n"
    "<w>!</w> : fire, don't stop, ignore range\n"
    "<w>p</w> : fire at Previous target (also <w>f</w>)\n"
    "<w>:</w> : show/hide beam path\n"
    "<w>Shift-Dir.</w> : fire straight-line beam\n"
    "\n"
    "<h>Firing or throwing a missile:\n"
    "<w>(</w> : cycle to next suitable missile.\n"
    "<w>)</w> : cycle to previous suitable missile.\n"
    "<w>i</w> : choose from Inventory.\n"
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
    m.add_entry(new MenuEntry(string(), MEL_TITLE));
    for (int i = 0; i < get_number_of_lines(); ++i)
        m.add_entry(new MenuEntry(string()));
    m.add_entry(new MenuEntry(string(), MEL_TITLE));

    while (fgets(buf, sizeof buf, fp))
    {
        MenuEntry* me = new MenuEntry(buf);
        if (next_is_hotkey && (isaupper(buf[0]) || isadigit(buf[0]))
            || is_first && first_hotkey)
        {
            int hotkey = (is_first ? first_hotkey : buf[0]);
            if (!is_first && buf[0] == 'X'
                && strlen(buf) >= 3 && isadigit(buf[2]))
            {
                // X.# is hotkeyed to the #
                hotkey = buf[2];
            }
            me->add_hotkey(hotkey);
            if (isaupper(hotkey))
                me->add_hotkey(toalower(hotkey));
            me->level  = MEL_SUBTITLE;
            me->colour = WHITE;
        }
        m.add_entry(me);
        // FIXME: There must be a better way to identify sections!
        next_is_hotkey =
            (auto_hotkeys
                && (strstr(buf, "------------------------------------------"
                                "------------------------------") == buf));
        is_first = false;
    }
}


struct help_file
{
    const char* name;
    int hotkey;
    bool auto_hotkey;
};

static help_file help_files[] = {
    { "crawl_manual.txt",  '*', true },
    { "../README.txt",     '!', false },
    { "aptitudes.txt",     '%', false },
    { "quickstart.txt",    '^', false },
    { "macros_guide.txt",  '~', false },
    { "options_guide.txt", '&', false },
#ifdef USE_TILE_LOCAL
    { "tiles_help.txt",    'T', false },
#endif
    { NULL, 0, false }
};

static bool _compare_mon_names(MenuEntry *entry_a, MenuEntry* entry_b)
{
    monster* a = static_cast<monster* >(entry_a->data);
    monster* b = static_cast<monster* >(entry_b->data);

    if (a->type == b->type)
        return false;

    string a_name = mons_type_name(a->type, DESC_PLAIN);
    string b_name = mons_type_name(b->type, DESC_PLAIN);
    return (lowercase(a_name) < lowercase(b_name));
}

// Compare monsters by location-independent level, or by hitdice if
// levels are equal, or by name if both level and hitdice are equal.
static bool _compare_mon_toughness(MenuEntry *entry_a, MenuEntry* entry_b)
{
    monster* a = static_cast<monster* >(entry_a->data);
    monster* b = static_cast<monster* >(entry_b->data);

    if (a->type == b->type)
        return false;

    int a_toughness = mons_avg_hp(a->type);
    int b_toughness = mons_avg_hp(b->type);

    if (a_toughness == b_toughness)
    {
        string a_name = mons_type_name(a->type, DESC_PLAIN);
        string b_name = mons_type_name(b->type, DESC_PLAIN);
        return (lowercase(a_name) < lowercase(b_name));
    }
    return (a_toughness > b_toughness);
}

class DescMenu : public Menu
{
public:
    DescMenu(int _flags, bool _show_mon, bool _text_only)
        : Menu(_flags, "", _text_only), sort_alpha(true),
          showing_monsters(_show_mon)
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
            string prompt = "Describe which? ";

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
                ::sort(items.begin(), items.end(), _compare_mon_names);
            else
                ::sort(items.begin(), items.end(), _compare_mon_toughness);

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

static vector<string> _get_desc_keys(string regex, db_find_filter filter)
{
    vector<string> key_matches = getLongDescKeysByRegex(regex, filter);

    if (key_matches.size() == 1)
        return key_matches;
    else if (key_matches.size() > 52)
        return key_matches;

    vector<string> body_matches = getLongDescBodiesByRegex(regex, filter);

    if (key_matches.empty() && body_matches.empty())
        return key_matches;
    else if (key_matches.empty() && body_matches.size() == 1)
        return body_matches;

    // Merge key_matches and body_matches, discarding duplicates.
    vector<string> tmp = key_matches;
    tmp.insert(tmp.end(), body_matches.begin(), body_matches.end());
    sort(tmp.begin(), tmp.end());
    vector<string> all_matches;
    for (unsigned int i = 0, size = tmp.size(); i < size; i++)
        if (i == 0 || all_matches[all_matches.size() - 1] != tmp[i])
            all_matches.push_back(tmp[i]);

    return all_matches;
}

static vector<string> _get_monster_keys(ucs_t showchar)
{
    vector<string> mon_keys;

    for (monster_type i = MONS_0; i < NUM_MONSTERS; ++i)
    {
        if (i == MONS_PROGRAM_BUG)
            continue;

        const monsterentry *me = get_monster_data(i);

        if (me == NULL || me->name == NULL || me->name[0] == '\0')
            continue;

        if (me->mc != i)
            continue;

        if (i == MONS_MARA_FAKE || i == MONS_RAKSHASA_FAKE)
            continue;

        if (getLongDescription(me->name).empty())
            continue;

        if ((ucs_t)me->basechar == showchar)
            mon_keys.push_back(me->name);
    }

    return mon_keys;
}

static vector<string> _get_god_keys()
{
    vector<string> names;

    for (int i = GOD_NO_GOD + 1; i < NUM_GODS; i++)
    {
        god_type which_god = static_cast<god_type>(i);
        names.push_back(god_name(which_god));
    }

    return names;
}

static vector<string> _get_branch_keys()
{
    vector<string> names;

    for (int i = BRANCH_MAIN_DUNGEON; i < NUM_BRANCHES; i++)
    {
        branch_type which_branch = static_cast<branch_type>(i);
        const Branch &branch     = branches[which_branch];

        // Skip unimplemented branches
        if (branch_is_unfinished(which_branch))
            continue;

        names.push_back(branch.shortname);
    }
    return names;
}

static bool _monster_filter(string key, string body)
{
    monster_type mon_num = get_monster_by_name(key.c_str());
    return mons_class_flag(mon_num, M_CANT_SPAWN);
}

static bool _spell_filter(string key, string body)
{
    if (!ends_with(key, " spell"))
        return true;
    key.erase(key.length() - 6);

    spell_type spell = spell_by_name(key);

    if (spell == SPELL_NO_SPELL)
        return true;

    if (get_spell_flags(spell) & (SPFLAG_MONSTER | SPFLAG_TESTING))
        return !you.wizard;

    return false;
}

static bool _item_filter(string key, string body)
{
    return (item_kind_by_name(key).base_type == OBJ_UNASSIGNED);
}

static bool _skill_filter(string key, string body)
{
    lowercase(key);
    string name;
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; i++)
    {
        skill_type sk = static_cast<skill_type>(i);
        // There are a couple of NULL entries in the skill set.
        if (!skill_name(sk))
            continue;

        name = lowercase_string(skill_name(sk));

        if (name.find(key) != string::npos)
            return false;
    }
    return true;
}

static bool _feature_filter(string key, string body)
{
    return (feat_by_desc(key) == DNGN_UNSEEN);
}

static bool _card_filter(string key, string body)
{
    lowercase(key);

    // Every card description contains the keyword "card".
    if (!ends_with(key, " card"))
        return true;
    key.erase(key.length() - 5);

    for (int i = 0; i < NUM_CARDS; ++i)
    {
        if (key == lowercase_string(card_name(static_cast<card_type>(i))))
            return false;
    }
    return true;
}

static bool _ability_filter(string key, string body)
{
    lowercase(key);
    if (!ends_with(key, " ability"))
        return true;
    key.erase(key.length() - 8);

    if (string_matches_ability_name(key))
        return false;

    return true;
}

typedef void (*db_keys_recap)(vector<string>&);

static void _recap_mon_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        monster_type type = get_monster_by_name(keys[i]);
        keys[i] = mons_type_name(type, DESC_PLAIN);
    }
}

static void _recap_feat_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        dungeon_feature_type type = feat_by_desc(keys[i]);
        if (type == DNGN_ENTER_SHOP)
            keys[i] = "A shop";
        else
        {
            keys[i] = feature_description(type, NUM_TRAPS, "", DESC_A,
                                          false);
        }
    }
}

static void _recap_card_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        lowercase(keys[i]);

        for (int j = 0; j < NUM_CARDS; ++j)
        {
            card_type card = static_cast<card_type>(j);
            if (keys[i] == lowercase_string(card_name(card)) + " card")
            {
                keys[i] = string(card_name(card)) + " card";
                break;
            }
        }
    }
}

// Extra info on this item wasn't found anywhere else.
static void _append_non_item(string &desc, string key)
{
    if (ends_with(key, " spell"))
        key.erase(key.length() - 6);

    spell_type type = spell_by_name(key);

    if (type == SPELL_NO_SPELL)
        return;

    unsigned int flags = get_spell_flags(type);

    if (flags & SPFLAG_TESTING)
    {
        desc += "\nThis is a testing spell, only available via the "
                "&Z wizard command.";
    }
    else if (flags & SPFLAG_MONSTER)
    {
        // We can get here only in wizmode, the spell isn't listed otherwise.
        // And only if it has a description -- no monster ones do.
        desc += "\nThis is a monster-only spell, only available via the "
                "&Z wizard command.";
    }
    else
    {
        desc += "\nOdd, this spell can't be found anywhere. Please "
                "file a bug report.";
    }

    if (!you.wizard && (flags & (SPFLAG_TESTING | SPFLAG_MONSTER)))
    {
        desc += "\n\nYou aren't in wizard mode, so you shouldn't be "
                "seeing this entry. Please file a bug report.";
    }
}

static bool _is_rod_spell(spell_type spell)
{
    if (spell == SPELL_NO_SPELL)
        return false;

    for (int i = 0; i < NUM_RODS; i++)
        for (int j = 0; j < 8; j++)
            if (which_spell_in_book(i + NUM_FIXED_BOOKS, j) == spell)
                return true;

    return false;
}

static bool _is_book_spell(spell_type spell)
{
    if (spell == SPELL_NO_SPELL)
        return false;

    for (int i = 0; i < NUM_FIXED_BOOKS; i++)
        for (int j = 0; j < 8; j++)
            if (which_spell_in_book(i, j) == spell)
                return true;

    return false;
}

// Adds a list of all books/rods that contain a given spell (by name)
// to a description string.
static bool _append_books(string &desc, item_def &item, string key)
{
    if (ends_with(key, " spell"))
        key.erase(key.length() - 6);

    spell_type type = spell_by_name(key, true);

    if (type == SPELL_NO_SPELL)
        return false;

    desc += "\nType:       ";
    bool already = false;

    for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
    {
        if (spell_typematch(type, 1 << i))
        {
            if (already)
                desc += "/" ;

            desc += spelltype_long_name(1 << i);
            already = true;
        }
    }
    if (!already)
        desc += "None";

    desc += make_stringf("\nLevel:      %d", spell_difficulty(type));

    bool form = false;
    if (you_cannot_memorise(type, form))
    {
        desc += "\n";
        desc += desc_cannot_memorise_reason(form);
    }

    set_ident_flags(item, ISFLAG_IDENT_MASK);
    vector<string> books;
    vector<string> rods;

    item.base_type = OBJ_BOOKS;
    for (int i = 0; i < NUM_FIXED_BOOKS; i++)
        for (int j = 0; j < 8; j++)
            if (which_spell_in_book(i, j) == type)
            {
                item.sub_type = i;
                books.push_back(item.name(DESC_PLAIN));
            }

    item.base_type = OBJ_RODS;
    int book;
    for (int i = 0; i < NUM_RODS; i++)
    {
        item.sub_type = i;
        book = item.book_number();

        for (int j = 0; j < 8; j++)
            if (which_spell_in_book(book, j) == type)
                rods.push_back(item.name(DESC_BASENAME));
    }

    if (!books.empty())
    {
        desc += "\n\nThis spell can be found in the following book";
        if (books.size() > 1)
            desc += "s";
        desc += ":\n";
        desc += comma_separated_line(books.begin(), books.end(), "\n", "\n");

        if (!rods.empty())
        {
            desc += "\n\n... and the following rod";
            if (rods.size() > 1)
                desc += "s";
            desc += ":\n";
            desc += comma_separated_line(rods.begin(), rods.end(), "\n", "\n");
        }
    }
    else if (!rods.empty()) // rods-only
    {
        desc += "\n\nThis spell can be found in the following rod";
        if (rods.size() > 1)
            desc += "s";
        desc += ":\n";
        desc += comma_separated_line(rods.begin(), rods.end(), "\n", "\n");
    }
    else
        desc += "\n\nThis spell is not found in any books or rods.";

    return true;
}

// Returns the result of the keypress.
static int _do_description(string key, string type, const string &suffix,
                           string footer = "")
{
    describe_info inf;
    inf.quote = getQuoteString(key);

    string desc = getLongDescription(key);
    int width = min(80, get_number_of_cols());

    god_type which_god = str_to_god(key);
    if (which_god != GOD_NO_GOD)
    {
        if (is_good_god(which_god))
        {
            inf.suffix = "\n\n" + god_name(which_god) +
                         " won't accept worship from undead or evil beings.";
        }
        string help = get_god_powers(which_god);
        if (!help.empty())
        {
            desc += "\n";
            desc += help;
        }
        desc += "\n";
        desc += get_god_likes(which_god);

        help = get_god_dislikes(which_god);
        if (!help.empty())
        {
            desc += "\n\n";
            desc += help;
        }
    }
    else
    {
        monster_type mon_num = get_monster_by_name(key);
        // Don't attempt to get more information on ghost demon
        // monsters, as the ghost struct has not been initialised, which
        // will cause a crash.  Similarly for zombified monsters, since
        // they require a base monster.
        if (mon_num != MONS_PROGRAM_BUG && !mons_is_ghost_demon(mon_num)
            && !mons_class_is_zombified(mon_num) && !mons_is_mimic(mon_num))
        {
            monster_info mi(mon_num);
            return describe_monsters(mi, true, footer);
        }
        else
        {
            int thing_created = get_mitm_slot();
            if (thing_created != NON_ITEM
                && (type == "item" || type == "spell"))
            {
                char name[80];
                strncpy(name, key.c_str(), sizeof(name));
                if (get_item_by_name(&mitm[thing_created], name, OBJ_WEAPONS))
                {
                    append_weapon_stats(desc, mitm[thing_created]);
                    desc += "\n";
                }
                else if (get_item_by_name(&mitm[thing_created], name, OBJ_ARMOUR))
                {
                    append_armour_stats(desc, mitm[thing_created]);
                    desc += "\n";
                }
                else if (get_item_by_name(&mitm[thing_created], name, OBJ_MISSILES)
                         && mitm[thing_created].sub_type != MI_THROWING_NET)
                {
                    append_missile_info(desc);
                    desc += "\n";
                }
                else if (type == "spell"
                         || get_item_by_name(&mitm[thing_created], name, OBJ_BOOKS)
                         || get_item_by_name(&mitm[thing_created], name, OBJ_RODS))
                {
                    if (!_append_books(desc, mitm[thing_created], key))
                    {
                        // FIXME: Duplicates messages from describe.cc.
                        if (!player_can_memorise_from_spellbook(mitm[thing_created]))
                        {
                            desc += "This book is beyond your current level "
                                    "of understanding.";
                        }
                        append_spells(desc, mitm[thing_created]);
                    }
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

    if (ends_with(key, suffix))
        key.erase(key.length() - suffix.length());
    key = uppercase_first(key);
    linebreak_string(footer, width - 1);

    inf.footer = footer;
    inf.title  = key;

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "description");
#endif

    print_description(inf);
    return getchm();
}

// Reads all questions from database/FAQ.txt, outputs them in the form of
// a selectable menu and prints the corresponding answer for a chosen question.
static bool _handle_FAQ()
{
    clrscr();
    viewwindow();

    vector<string> question_keys = getAllFAQKeys();
    if (question_keys.empty())
    {
        mpr("No questions found in FAQ! Please submit a bug report!");
        return false;
    }
    Menu FAQmenu(MF_SINGLESELECT | MF_ANYPRINTABLE | MF_ALLOW_FORMATTING);
    MenuEntry *title = new MenuEntry("Frequently Asked Questions");
    title->colour = YELLOW;
    FAQmenu.set_title(title);
    const int width = get_number_of_cols();

    for (unsigned int i = 0, size = question_keys.size(); i < size; i++)
    {
        const char letter = index_to_letter(i);

        string question = getFAQ_Question(question_keys[i]);
        // Wraparound if the question is longer than fits into a line.
        linebreak_string(question, width - 4);
        vector<formatted_string> fss;
        formatted_string::parse_string_to_multiple(question, fss);

        MenuEntry *me;
        for (unsigned int j = 0; j < fss.size(); j++)
        {
            if (j == 0)
            {
                me = new MenuEntry(question, MEL_ITEM, 1, letter);
                me->data = &question_keys[i];
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
        vector<MenuEntry*> sel = FAQmenu.show();
        redraw_screen();
        if (sel.empty())
            return false;
        else
        {
            ASSERT(sel.size() == 1);
            ASSERT(sel[0]->hotkeys.size() == 1);

            string key = *((string*) sel[0]->data);
            string answer = getFAQ_Answer(key);
            if (answer.empty())
            {
                answer = "No answer found in the FAQ! Please submit a "
                         "bug report!";
            }
            answer = "Q: " + getFAQ_Question(key) + "\n" + answer;
            linebreak_string(answer, width - 1, true);
            {
#ifdef USE_TILE_WEB
                tiles_crt_control show_as_menu(CRT_MENU, "faq_entry");
#endif
                print_description(answer);
                getchm();
            }
        }
    }

    return true;
}

static void _find_description(bool *again, string *error_inout)
{
    *again = true;

    redraw_screen();

    if (!error_inout->empty())
        mpr(error_inout->c_str(), MSGCH_PROMPT);
    mpr("Describe a (M)onster, (S)pell, s(K)ill, (I)tem, (F)eature, (G)od, "
        "(A)bility, (B)ranch, or (C)ard? ", MSGCH_PROMPT);
    int ch;
    {
        cursor_control con(true);
        ch = toupper(getchm());
    }
    string    type;
    string    extra;
    string    suffix;
    db_find_filter filter     = NULL;
    db_keys_recap  recap      = NULL;
    bool           want_regex = true;
    bool           want_sort  = true;

    bool doing_mons     = false;
    bool doing_items    = false;
    bool doing_gods     = false;
    bool doing_branches = false;
    bool doing_features = false;
    bool doing_spells   = false;

    switch (ch)
    {
    case 'M':
        type       = "monster";
        extra      = " Enter a single letter to list monsters displayed by "
                     "that symbol.";
        filter     = _monster_filter;
        recap      = _recap_mon_keys;
        doing_mons = true;
        break;
    case 'S':
        type         = "spell";
        filter       = _spell_filter;
        suffix       = " spell";
        doing_spells = true;
        break;
    case 'K':
        type   = "skill";
        filter = _skill_filter;
        break;
    case 'A':
        type   = "ability";
        filter = _ability_filter;
        suffix = " ability";
        break;
    case 'C':
        type   = "card";
        filter = _card_filter;
        suffix = " card";
        recap  = _recap_card_keys;
        break;
    case 'I':
        type        = "item";
        extra       = " Enter a single letter to list items displayed by "
                      "that symbol.";
        filter      = _item_filter;
        doing_items = true;
        break;
    case 'F':
        type   = "feature";
        filter = _feature_filter;
        recap  = _recap_feat_keys;

        doing_features = true;
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
        *error_inout = "Okay, then.";
        *again = false;
        return;
    }

    string regex = "";

    if (want_regex)
    {
        mprf(MSGCH_PROMPT,
             "Describe a %s; partial names and regexps are fine.%s",
             type.c_str(), extra.c_str());

        mpr("Describe what? ", MSGCH_PROMPT);
        char buf[80];
        if (cancellable_get_line(buf, sizeof(buf)) || buf[0] == '\0')
        {
            *error_inout = "Okay, then.";
            return;
        }

        if (strlen(buf) == 1)
            regex = buf;
        else
            regex = trimmed_string(buf);

        if (regex.empty())
        {
            *error_inout = "Description must contain at least "
                "one non-space.";
            return;
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
        string desc = getLongDescription(regex + suffix);

        if (!desc.empty())
            exact_match = true;
    }

    vector<string> key_list;

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

    if (key_list.empty())
    {
        if (by_mon_symbol)
        {
            *error_inout  = "No monsters with symbol '";
            *error_inout += regex;
            *error_inout += "'.";
        }
        else if (by_item_symbol)
        {
            *error_inout  = "No items with symbol '";
            *error_inout += regex;
            *error_inout += "'.";
        }
        else
        {
            *error_inout  = "No matching ";
            *error_inout += pluralise(type);
            *error_inout += ".";
        }
        return;
    }
    else if (key_list.size() > 52)
    {
        if (by_mon_symbol)
        {
            *error_inout  = "Too many monsters with symbol '";
            *error_inout += regex;
            *error_inout += "' to display";
        }
        else
        {
            ostringstream os;
            os << "Too many matching " << pluralise(type)
               << " (" << key_list.size() << ") to display.";
            *error_inout = os.str();
        }
        return;
    }
    else if (key_list.size() == 1)
    {
        _do_description(key_list[0], type, suffix);
        return;
    }

    if (exact_match)
    {
        string footer = "This entry is an exact match for '";
        footer += regex;
        footer += "'. To see non-exact matches, press space.";

        if (_do_description(regex, type, suffix, footer) != ' ')
            return;
    }

    if (want_sort)
        sort(key_list.begin(), key_list.end());

    // For tiles builds use a tiles menu to display monsters.
    const bool text_only =
#ifdef USE_TILE_LOCAL
        !(doing_mons || doing_features || doing_spells);
#else
        true;
#endif

    DescMenu desc_menu(MF_SINGLESELECT | MF_ANYPRINTABLE |
                       MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING,
                       doing_mons, text_only);
    desc_menu.set_tag("description");
    list<monster_info> monster_list;
    list<item_def> item_list;
    for (unsigned int i = 0, size = key_list.size(); i < size; i++)
    {
        const char letter = index_to_letter(i);
        string     str    = uppercase_first(key_list[i]);

        if (ends_with(str, suffix)) // perhaps we should assert this?
            str.erase(str.length() - suffix.length());

        MenuEntry *me = NULL;

        if (doing_mons)
        {
            // Create and store fake monsters, so the menu code will
            // have something valid to refer to.
            monster_type m_type = get_monster_by_name(str);

            // Not worth the effort handling the item; also, it would
            // puzzle the players.  Thus, only unique matches work.
            if (mons_is_mimic(m_type))
                continue;

            // No proper monster, and causes crashes in Tiles.
            if (mons_is_tentacle_segment(m_type))
                continue;

            monster_type base_type = MONS_NO_MONSTER;
            // HACK: Set an arbitrary humanoid monster as base type.
            if (mons_class_is_zombified(m_type))
                base_type = MONS_GOBLIN;
            monster_info fake_mon(m_type, base_type);
            fake_mon.props["fake"] = true;

            // FIXME: This doesn't generate proper draconian monsters.
            monster_list.push_back(fake_mon);

#ifndef USE_TILE_LOCAL
            int colour = mons_class_colour(m_type);
            if (colour == BLACK)
                colour = LIGHTGREY;

            string prefix = "(<";
            prefix += colour_to_str(colour);
            prefix += ">";
            prefix += stringize_glyph(mons_char(m_type));
            prefix += "</";
            prefix += colour_to_str(colour);
            prefix += ">) ";

            str = prefix + str;
#endif

            // NOTE: MonsterMenuEntry::get_tiles() takes care of setting
            // up a fake weapon when displaying a fake dancing weapon's
            // tile.
            me = new MonsterMenuEntry(str, &(monster_list.back()),
                                      letter);
        }
        else if (doing_features)
            me = new FeatureMenuEntry(str, feat_by_desc(str), letter);
        else if (doing_gods)
            me = new GodMenuEntry(str_to_god(key_list[i]));
        else
        {
            me = new MenuEntry(str, MEL_ITEM, 1, letter);

            if (doing_spells)
            {
                spell_type spell = spell_by_name(str);
#ifdef USE_TILE
                if (spell != SPELL_NO_SPELL)
                    me->add_tile(tile_def(tileidx_spell(spell), TEX_GUI));
#endif
                me->colour = _is_book_spell(spell) ? WHITE
                           : _is_rod_spell(spell)  ? LIGHTGREY
                                                   : DARKGREY; // monster-only
            }

            me->data = &key_list[i];
        }

        desc_menu.add_entry(me);
    }

    desc_menu.sort();

    while (true)
    {
        vector<MenuEntry*> sel = desc_menu.show();
        redraw_screen();
        if (sel.empty())
        {
            if (doing_mons && desc_menu.getkey() == CONTROL('S'))
                desc_menu.toggle_sorting();
            else
                return;
        }
        else
        {
            ASSERT(sel.size() == 1);
            ASSERT(sel[0]->hotkeys.size() >= 1);

            string key;

            if (doing_mons)
            {
                monster_info* mon = (monster_info*) sel[0]->data;
                key = mons_type_name(mon->type, DESC_PLAIN);
            }
            else if (doing_features)
                key = sel[0]->text;
            else
                key = *((string*) sel[0]->data);

            _do_description(key, type, suffix);
        }
    }
}

static void _keyhelp_query_descriptions()
{
    bool again = false;
    string error;
    do
    {
        // resets 'again'
        _find_description(&again, &error);

        if (again)
            mesclr();
    }
    while (again);

    viewwindow();
    if (!error.empty())
        mpr(error);
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
        _keyhelp_query_descriptions();
        return -1;

    case 'q':
    case 'Q':
    {
        bool again;
        do
        {
            // resets 'again'
            again = _handle_FAQ();
            if (again)
                mesclr();
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
    help_highlighter(string = "");
    int entry_colour(const MenuEntry *entry) const;
private:
    text_pattern pattern;
    string get_species_key() const;
};

help_highlighter::help_highlighter(string highlight_string) :
    pattern(highlight_string == "" ? get_species_key() : highlight_string)
{
}

int help_highlighter::entry_colour(const MenuEntry *entry) const
{
    return !pattern.empty() && pattern.matches(entry->text) ? WHITE : -1;
}

// To highlight species in aptitudes list. ('?%')
string help_highlighter::get_species_key() const
{
    string result = species_name(you.species);
    // The table doesn't repeat the word "Draconian".
    if (you.species != SP_BASE_DRACONIAN && player_genus(GENPC_DRACONIAN))
        strip_tag(result, "Draconian");

    result += "  ";
    return result;
}
////////////////////////////////////////////////////////////////////////////

static int _show_keyhelp_menu(const vector<formatted_string> &lines,
                              bool with_manual, bool easy_exit = false,
                              int hotkey = 0,
                              string highlight_string = "")
{
    formatted_scroller cmd_help;

    // Set flags, and use easy exit if necessary.
    int flags = MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP;
    if (easy_exit)
        flags |= MF_EASY_EXIT;
    cmd_help.set_flags(flags, false);
    cmd_help.set_tag("help");
    cmd_help.set_more();

    if (with_manual)
    {
        cmd_help.set_highlighter(new help_highlighter(highlight_string));
        cmd_help.f_keyfilter = _keyhelp_keyfilter;
        column_composer cols(2, 40);

        cols.add_formatted(
            0,
            "<h>Dungeon Crawl Help\n"
            "\n"
            "Press one of the following keys to\n"
            "obtain more information on a certain\n"
            "aspect of Dungeon Crawl.\n"

            "<w>?</w>: List of commands\n"
            "<w>!</w>: Read Me!\n"
            "<w>^</w>: Quickstart Guide\n"
            "<w>:</w>: Browse character notes\n"
            "<w>~</w>: Macros help\n"
            "<w>&</w>: Options help\n"
            "<w>%</w>: Table of aptitudes\n"
            "<w>/</w>: Lookup description\n"
            "<w>Q</w>: FAQ\n"
#ifdef USE_TILE_LOCAL
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
            "<w>D</w>.      Exploring the Dungeon\n"
            "<w>E</w>.      Experience and Skills\n"
            "<w>F</w>.      Monsters\n"
            "<w>G</w>.      Items\n"
            "<w>H</w>.      Spellcasting\n"
            "<w>I</w>.      Targetting\n"
            "<w>J</w>.      Religion\n"
            "<w>K</w>.      Mutations\n"
            "<w>L</w>.      Licence, Contact, History\n"
            "<w>M</w>.      Macros, Options, Performance\n"
            "<w>N</w>.      Philosophy\n"
            "<w>1</w>.      List of Character Species\n"
            "<w>2</w>.      List of Character Backgrounds\n"
            "<w>3</w>.      List of Skills\n"
            "<w>4</w>.      List of Keys and Commands\n"
            "<w>5</w>.      List of Enchantments\n"
            "<w>6</w>.      Inscriptions\n",
            true, true, _cmdhelp_textfilter);

        vector<formatted_string> blines = cols.formatted_lines();
        unsigned i;
        for (i = 0; i < blines.size(); ++i)
            cmd_help.add_item_formatted_string(blines[i]);

        while (static_cast<int>(++i) < get_number_of_lines())
            cmd_help.add_item_string("");

        // unscrollable
        cmd_help.add_entry(new MenuEntry(string(), MEL_TITLE));
    }

    for (unsigned i = 0; i < lines.size(); ++i)
        cmd_help.add_item_formatted_string(lines[i], (i == 0 ? '?' : 0));

    if (with_manual)
    {
        for (int i = 0; help_files[i].name != NULL; ++i)
        {
            // Attempt to open this file, skip it if unsuccessful.
            string fname = canonicalise_file_separator(help_files[i].name);
            FILE* fp = fopen_u(datafile_path(fname, false).c_str(), "r");

            if (!fp)
                continue;

            // Put in a separator...
            cmd_help.add_item_string("");
            cmd_help.add_item_string(string(get_number_of_cols()-1,'='));
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

static void _show_specific_help(const string &help)
{
    vector<string> lines = split_string("\n", help, false, true);
    vector<formatted_string> formatted_lines;
    for (int i = 0, size = lines.size(); i < size; ++i)
    {
        formatted_lines.push_back(
            formatted_string::parse_string(
                lines[i], true, _cmdhelp_textfilter));
    }
    _show_keyhelp_menu(formatted_lines, false, Options.easy_exit_menu);
}

void show_levelmap_help()
{
    _show_specific_help(getHelpString("level-map"));
}

void show_pickup_menu_help()
{
    _show_specific_help(getHelpString("pick-up"));
}

void show_known_menu_help()
{
    _show_specific_help(getHelpString("known-menu"));
}

void show_targetting_help()
{
    column_composer cols(2, 40);
    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines() - 1);

    cols.add_formatted(0, targetting_help_1, true, true);
#ifdef WIZARD
    if (you.wizard)
        cols.add_formatted(0, targetting_help_wiz, true, true);
#endif
    cols.add_formatted(1, targetting_help_2, true, true);
    _show_keyhelp_menu(cols.formatted_lines(), false, Options.easy_exit_menu);
}
void show_interlevel_travel_branch_help()
{
    _show_specific_help(getHelpString("interlevel-travel.branch.prompt"));
}

void show_interlevel_travel_depth_help()
{
    _show_specific_help(getHelpString("interlevel-travel.depth.prompt"));
}

void show_stash_search_help()
{
    _show_specific_help(getHelpString("stash-search.prompt"));
}

void show_butchering_help()
{
    _show_specific_help(getHelpString("butchering"));
}

void show_skill_menu_help()
{
    _show_specific_help(getHelpString("skill-menu"));
}

static void _add_command(column_composer &cols, const int column,
                         const command_type cmd,
                         const string desc,
                         const unsigned int space_to_colon = 7)
{
    string command_name = command_to_string(cmd);
    if (strcmp(command_name.c_str(), "<") == 0)
        command_name += "<";

    const int cmd_len = strwidth(command_name);
    string line = "<w>" + command_name + "</w>";
    for (unsigned int i = cmd_len; i < space_to_colon; ++i)
        line += " ";
    line += ": " + untag_tiles_console(desc) + "\n";

    cols.add_formatted(
            column,
            line.c_str(),
            false, true, _cmdhelp_textfilter);
}

static void _add_insert_commands(column_composer &cols, const int column,
                                 const unsigned int space_to_colon,
                                 const string &desc, const int first, ...)
{
    const command_type cmd = (command_type) first;

    va_list args;
    va_start(args, first);
    int nargs = 10;

    vector<command_type> cmd_vector;
    while (nargs-- > 0)
    {
        int value = va_arg(args, int);
        if (!value)
            break;

        cmd_vector.push_back((command_type) value);
    }
    va_end(args);

    string line = desc;
    insert_commands(line, cmd_vector);
    line += "\n";
    _add_command(cols, column, cmd, line, space_to_colon);
}

static void _add_insert_commands(column_composer &cols, const int column,
                                 const string desc, const int first, ...)
{
    vector<command_type> cmd_vector;
    cmd_vector.push_back((command_type) first);

    va_list args;
    va_start(args, first);
    int nargs = 10;

    while (nargs-- > 0)
    {
        int value = va_arg(args, int);
        if (!value)
            break;

        cmd_vector.push_back((command_type) value);
    }
    va_end(args);

    string line = desc;
    insert_commands(line, cmd_vector);
    line += "\n";
    cols.add_formatted(
            column,
            line.c_str(),
            false, true, _cmdhelp_textfilter);
}

static void _add_formatted_keyhelp(column_composer &cols)
{
    cols.add_formatted(
            0,
            "<h>Movement:\n"
            "To move in a direction or to attack, \n"
            "use the numpad (try Numlock off and \n"
            "on) or vi keys:\n",
            true, true, _cmdhelp_textfilter);

    _add_insert_commands(cols, 0, "                 <w>7 8 9      % % %",
                         CMD_MOVE_UP_LEFT, CMD_MOVE_UP, CMD_MOVE_UP_RIGHT, 0);
    _add_insert_commands(cols, 0, "                  \\|/        \\|/", 0);
    _add_insert_commands(cols, 0, "                 <w>4</w>-<w>5</w>-<w>6</w>      <w>%</w>-<w>%</w>-<w>%</w>",
                         CMD_MOVE_LEFT, CMD_MOVE_NOWHERE, CMD_MOVE_RIGHT, 0);
    _add_insert_commands(cols, 0, "                  /|\\        /|\\", 0);
    _add_insert_commands(cols, 0, "                 <w>1 2 3      % % %",
                         CMD_MOVE_DOWN_LEFT, CMD_MOVE_DOWN, CMD_MOVE_DOWN_RIGHT, 0);

    cols.add_formatted(
            0,
            "<h>Rest/Search:\n",
            true, true, _cmdhelp_textfilter);

    _add_command(cols, 0, CMD_WAIT, "wait a turn (also <w>.</w>, <w>Del</w>)", 2);
    _add_command(cols, 0, CMD_REST, "rest and long wait; stops when", 2);
    cols.add_formatted(
            0,
            "    Health or Magic become full,\n"
            "    something is detected, or after\n"
            "    100 turns over (<w>numpad-5</w>)\n",
            false, true, _cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Extended Movement:\n",
            true, true, _cmdhelp_textfilter);

    _add_command(cols, 0, CMD_EXPLORE, "auto-explore");
    _add_command(cols, 0, CMD_INTERLEVEL_TRAVEL, "interlevel travel");
    _add_command(cols, 0, CMD_SEARCH_STASHES, "Find items");
    _add_command(cols, 0, CMD_FIX_WAYPOINT, "set Waypoint");

    cols.add_formatted(
            0,
            "<w>/ Dir.</w>, <w>Shift-Dir.</w>: long walk\n"
            "<w>* Dir.</w>, <w>Ctrl-Dir.</w> : open/close door, \n"
            "         untrap, attack without move\n",
            false, true, _cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "\n"
            "<h>Item types (and common commands)\n",
            true, true, _cmdhelp_textfilter);

    _add_insert_commands(cols, 0, "<cyan>)</cyan> : hand weapons (<w>%</w>ield)",
                         CMD_WIELD_WEAPON, 0);
    _add_insert_commands(cols, 0, "<brown>(</brown> : missiles (<w>%</w>uiver, "
                                  "<w>%</w>ire, <w>%</w>/<w>%</w> cycle)",
                         CMD_QUIVER_ITEM, CMD_FIRE, CMD_CYCLE_QUIVER_FORWARD,
                         CMD_CYCLE_QUIVER_BACKWARD, 0);
    _add_insert_commands(cols, 0, "<cyan>[</cyan> : armour (<w>%</w>ear and <w>%</w>ake off)",
                         CMD_WEAR_ARMOUR, CMD_REMOVE_ARMOUR, 0);
    _add_insert_commands(cols, 0, "<brown>percent</brown> : corpses and food "
                                  "(<w>%</w>hop up and <w>%</w>at)",
                         CMD_BUTCHER, CMD_EAT, 0);
    _add_insert_commands(cols, 0, "<w>?</w> : scrolls (<w>%</w>ead)",
                         CMD_READ, 0);
    _add_insert_commands(cols, 0, "<magenta>!</magenta> : potions (<w>%</w>uaff)",
                         CMD_QUAFF, 0);
    _add_insert_commands(cols, 0, "<blue>=</blue> : rings (<w>%</w>ut on and <w>%</w>emove)",
                         CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY, 0);
    _add_insert_commands(cols, 0, "<red>\"</red> : amulets (<w>%</w>ut on and <w>%</w>emove)",
                         CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY, 0);
    _add_insert_commands(cols, 0, "<lightgrey>/</lightgrey> : wands (e<w>%</w>oke)",
                         CMD_EVOKE, 0);

    string item_types = "<lightcyan>";
    item_types += stringize_glyph(get_item_symbol(SHOW_ITEM_BOOK));
    item_types +=
        "</lightcyan> : books (<w>%</w>ead, <w>%</w>emorise, <w>%</w>ap, <w>%</w>ap)";
    _add_insert_commands(cols, 0, item_types,
                         CMD_READ, CMD_MEMORISE_SPELL, CMD_CAST_SPELL,
                         CMD_FORCE_CAST_SPELL, 0);
    _add_insert_commands(cols, 0, "<brown>\\</brown> : staves and rods (<w>%</w>ield and e<w>%</w>oke)",
                         CMD_WIELD_WEAPON, CMD_EVOKE_WIELDED, 0);
    _add_insert_commands(cols, 0, "<lightgreen>}</lightgreen> : miscellaneous items (e<w>%</w>oke)",
                         CMD_EVOKE, 0);
    _add_insert_commands(cols, 0, "<yellow>$</yellow> : gold (<w>%</w> counts gold)",
                         CMD_LIST_GOLD, 0);

    cols.add_formatted(
            0,
            "<lightmagenta>0</lightmagenta> : the Orb of Zot\n"
            "    Carry it to the surface and win!\n",
            false, true, _cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Other Gameplay Actions:\n",
            true, true, _cmdhelp_textfilter);

    _add_insert_commands(cols, 0, 2, "use special Ability (<w>%!</w> for help)",
                         CMD_USE_ABILITY, CMD_USE_ABILITY, 0);
    _add_insert_commands(cols, 0, 2, "Pray (<w>%</w> and <w>%!</w> for help)",
                         CMD_PRAY, CMD_DISPLAY_RELIGION, CMD_DISPLAY_RELIGION, 0);
    _add_command(cols, 0, CMD_CAST_SPELL, "cast spell, abort without targets", 2);
    _add_command(cols, 0, CMD_FORCE_CAST_SPELL, "cast spell, no matter what", 2);
    _add_command(cols, 0, CMD_DISPLAY_SPELLS, "list all spells", 2);

    _add_insert_commands(cols, 0, 2, "tell allies (<w>%t</w> to shout)",
                         CMD_SHOUT, CMD_SHOUT, 0);
    _add_command(cols, 0, CMD_PREV_CMD_AGAIN, "re-do previous command", 2);
    _add_command(cols, 0, CMD_REPEAT_CMD, "repeat next command # of times", 2);

    cols.add_formatted(
            0,
            "<h>Non-Gameplay Commands / Info\n",
            true, true, _cmdhelp_textfilter);

    _add_command(cols, 0, CMD_REPLAY_MESSAGES, "show Previous messages");
    _add_command(cols, 0, CMD_REDRAW_SCREEN, "Redraw screen");
    _add_command(cols, 0, CMD_CLEAR_MAP, "Clear main and level maps");
    _add_command(cols, 0, CMD_ANNOTATE_LEVEL, "annotate the dungeon level", 2);
    _add_command(cols, 0, CMD_CHARACTER_DUMP, "dump character to file", 2);
    _add_insert_commands(cols, 0, 2, "add note (use <w>%:</w> to read notes)",
                         CMD_MAKE_NOTE, CMD_DISPLAY_COMMANDS, 0);
    _add_command(cols, 0, CMD_MACRO_ADD, "add macro (also <w>Ctrl-D</w>)", 2);
    _add_command(cols, 0, CMD_ADJUST_INVENTORY, "reassign inventory/spell letters", 2);
#ifdef USE_TILE_LOCAL
    _add_command(cols, 0, CMD_EDIT_PLAYER_TILE, "edit player doll", 2);
#else
#ifdef USE_TILE_WEB
    if (tiles.is_controlled_from_web())
        cols.add_formatted(0, "<w>F12</w> : read messages (online play only)",
                           false);
    else
#endif
    _add_command(cols, 0, CMD_READ_MESSAGES, "read messages (online play only)", 2);
#endif

    cols.add_formatted(
            1,
            "<h>Game Saving and Quitting:\n",
            true, true, _cmdhelp_textfilter);

    _add_command(cols, 1, CMD_SAVE_GAME, "Save game and exit");
    _add_command(cols, 1, CMD_SAVE_GAME_NOW, "Save and exit without query");
    _add_command(cols, 1, CMD_QUIT, "Suicide the current character");
    cols.add_formatted(1, "         and quit the game\n",
                       false, true, _cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Player Character Information:\n",
            true, true, _cmdhelp_textfilter);

    _add_command(cols, 1, CMD_DISPLAY_CHARACTER_STATUS, "display character status", 2);
    _add_command(cols, 1, CMD_DISPLAY_SKILLS, "show skill screen", 2);
    _add_command(cols, 1, CMD_RESISTS_SCREEN, "show resistances", 2);
    _add_command(cols, 1, CMD_DISPLAY_RELIGION, "show religion screen", 2);
    _add_command(cols, 1, CMD_DISPLAY_MUTATIONS, "show Abilities/mutations", 2);
    _add_command(cols, 1, CMD_DISPLAY_KNOWN_OBJECTS, "show item knowledge", 2);
    _add_command(cols, 1, CMD_DISPLAY_RUNES, "show runes collected", 2);
    _add_command(cols, 1, CMD_LIST_ARMOUR, "display worn armour", 2);
    _add_command(cols, 1, CMD_LIST_JEWELLERY, "display worn jewellery", 2);
    _add_command(cols, 1, CMD_LIST_GOLD, "display gold in possession", 2);
    _add_command(cols, 1, CMD_EXPERIENCE_CHECK, "display experience info", 2);

    cols.add_formatted(
            1,
            "<h>Dungeon Interaction and Information:\n",
            true, true, _cmdhelp_textfilter);

    _add_insert_commands(cols, 1, "<w>%</w>/<w>%</w> : Open/Close door",
                         CMD_OPEN_DOOR, CMD_CLOSE_DOOR, 0);
    _add_insert_commands(cols, 1, "<w>%</w>/<w>%</w> : use staircase",
                         CMD_GO_UPSTAIRS, CMD_GO_DOWNSTAIRS, 0);


    _add_command(cols, 1, CMD_INSPECT_FLOOR, "examine occupied tile and");
    cols.add_formatted(1, "         pickup part of a single stack\n",
                       false, true, _cmdhelp_textfilter);

    _add_command(cols, 1, CMD_LOOK_AROUND, "eXamine surroundings/targets");
    _add_insert_commands(cols, 1, 7, "eXamine level map (<w>%?</w> for help)",
                         CMD_DISPLAY_MAP, CMD_DISPLAY_MAP, 0);
    _add_command(cols, 1, CMD_FULL_VIEW, "list monsters, items, features");
    cols.add_formatted(1, "         in view\n",
                       false, true, _cmdhelp_textfilter);
    _add_command(cols, 1, CMD_SHOW_TERRAIN, "toggle terrain-only view");
    if (!is_tiles())
        _add_command(cols, 1, CMD_TOGGLE_VIEWPORT_MONSTER_HP, "colour monsters in view by HP");
    _add_command(cols, 1, CMD_DISPLAY_OVERMAP, "show dungeon Overview");
    _add_command(cols, 1, CMD_TOGGLE_AUTOPICKUP, "toggle auto-pickup");
    _add_command(cols, 1, CMD_TOGGLE_FRIENDLY_PICKUP, "change ally pickup behaviour");
    _add_command(cols, 1, CMD_TOGGLE_TRAVEL_SPEED, "set your travel speed to your");
    cols.add_formatted(1, "         slowest ally\n",
                           false, true, _cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Item Interaction (inventory):\n",
            true, true, _cmdhelp_textfilter);

    _add_command(cols, 1, CMD_DISPLAY_INVENTORY, "show Inventory list", 2);
    _add_command(cols, 1, CMD_LIST_EQUIPMENT, "show inventory of equipped items", 2);
    _add_command(cols, 1, CMD_INSCRIBE_ITEM, "inscribe item", 2);
    _add_command(cols, 1, CMD_FIRE, "Fire next appropriate item", 2);
    _add_command(cols, 1, CMD_THROW_ITEM_NO_QUIVER, "select an item and Fire it", 2);
    _add_command(cols, 1, CMD_QUIVER_ITEM, "select item slot to be quivered", 2);

    {
        string interact = (you.species == SP_VAMPIRE ? "Drain corpses"
                                                     : "Eat food");
        interact += " (tries floor first)\n";
        _add_command(cols, 1, CMD_EAT, interact, 2);
    }

    _add_command(cols, 1, CMD_QUAFF, "Quaff a potion", 2);
    _add_command(cols, 1, CMD_READ, "Read a scroll or book", 2);
    _add_command(cols, 1, CMD_MEMORISE_SPELL, "Memorise a spell from a book", 2);
    _add_command(cols, 1, CMD_WIELD_WEAPON, "Wield an item (<w>-</w> for none)", 2);
    _add_command(cols, 1, CMD_WEAPON_SWAP, "wield item a, or switch to b", 2);

    _add_insert_commands(cols, 1, "    (use <w>%</w> to assign slots)",
                         CMD_ADJUST_INVENTORY, 0);

    _add_command(cols, 1, CMD_EVOKE_WIELDED, "eVoke power of wielded item", 2);
    _add_command(cols, 1, CMD_EVOKE, "eVoke wand and miscellaneous item", 2);

    _add_insert_commands(cols, 1, "<w>%</w>/<w>%</w> : Wear or Take off armour",
                         CMD_WEAR_ARMOUR, CMD_REMOVE_ARMOUR, 0);
    _add_insert_commands(cols, 1, "<w>%</w>/<w>%</w> : Put on or Remove jewellery",
                         CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY, 0);

    cols.add_formatted(
            1,
            "<h>Item Interaction (floor):\n",
            true, true, _cmdhelp_textfilter);

    _add_command(cols, 1, CMD_PICKUP, "pick up items (also <w>g</w>)", 2);
    cols.add_formatted(
            1,
            "    (press twice for pick up menu)\n",
            false, true, _cmdhelp_textfilter);

    _add_command(cols, 1, CMD_DROP, "Drop an item", 2);
    _add_insert_commands(cols, 1, "<w>%#</w>: Drop exact number of items",
                         CMD_DROP, 0);
    _add_command(cols, 1, CMD_DROP_LAST, "Drop the last item(s) you picked up", 2);
    _add_command(cols, 1, CMD_BUTCHER, "Chop up a corpse", 2);

    {
        string interact = (you.species == SP_VAMPIRE ? "Drain corpses on"
                                                     : "Eat food from");
        interact += " floor\n";
        _add_command(cols, 1, CMD_EAT, interact, 2);
    }

    cols.add_formatted(
            1,
            "<h>Additional help:\n",
            true, true, _cmdhelp_textfilter);

    string text =
            "Many commands have context sensitive "
            "help, among them <w>%</w>, <w>%</w>, <w>%</w> (or any "
            "form of targetting), <w>%</w>, and <w>%</w>.\n"
            "You can read descriptions of your "
            "current spells (<w>%</w>), skills (<w>%?</w>) and "
            "abilities (<w>%!</w>).";
    insert_commands(text, CMD_DISPLAY_MAP, CMD_LOOK_AROUND, CMD_FIRE,
                    CMD_SEARCH_STASHES, CMD_INTERLEVEL_TRAVEL,
                    CMD_DISPLAY_SPELLS, CMD_DISPLAY_SKILLS, CMD_USE_ABILITY,
                    0);
    linebreak_string(text, 40);

    cols.add_formatted(
            1, text,
            false, true, _cmdhelp_textfilter);
}

static void _add_formatted_hints_help(column_composer &cols)
{
    // First column.
    cols.add_formatted(
            0,
            "<h>Movement:\n"
            "To move in a direction or to attack, \n"
            "use the numpad (try Numlock off and \n"
            "on) or vi keys:\n",
            false, true, _cmdhelp_textfilter);

    _add_insert_commands(cols, 0, "                 <w>7 8 9      % % %",
                         CMD_MOVE_UP_LEFT, CMD_MOVE_UP, CMD_MOVE_UP_RIGHT, 0);
    _add_insert_commands(cols, 0, "                  \\|/        \\|/", 0);
    _add_insert_commands(cols, 0, "                 <w>4</w>-<w>5</w>-<w>6</w>      <w>%</w>-<w>%</w>-<w>%</w>",
                         CMD_MOVE_LEFT, CMD_MOVE_NOWHERE, CMD_MOVE_RIGHT, 0);
    _add_insert_commands(cols, 0, "                  /|\\        /|\\", 0);
    _add_insert_commands(cols, 0, "                 <w>1 2 3      % % %",
                         CMD_MOVE_DOWN_LEFT, CMD_MOVE_DOWN, CMD_MOVE_DOWN_RIGHT, 0);

    cols.add_formatted(0, " ", false, true, _cmdhelp_textfilter);
    cols.add_formatted(0, "<w>Shift-Dir.</w> runs into one direction",
                       false, true, _cmdhelp_textfilter);
    _add_insert_commands(cols, 0, "<w>%</w> or <w>%</w> : ascend/descend the stairs",
                         CMD_GO_UPSTAIRS, CMD_GO_DOWNSTAIRS, 0);
    _add_command(cols, 0, CMD_EXPLORE, "autoexplore", 2);

    cols.add_formatted(
            0,
            "<h>Rest/Search:\n",
            true, true, _cmdhelp_textfilter);

    _add_command(cols, 0, CMD_WAIT, "wait a turn (also <w>.</w>, <w>Del</w>)", 2);
    _add_command(cols, 0, CMD_REST, "rest and long wait; stops when", 2);
    cols.add_formatted(
            0,
            "    Health or Magic become full,\n"
            "    something is detected, or after\n"
            "    100 turns over (<w>numpad-5</w>)\n",
            false, true, _cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "\n<h>Attacking monsters\n"
            "Walking into a monster will attack it\n"
            "with the wielded weapon or barehanded.",
            false, true, _cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "\n<h>Ranged combat and magic\n",
            false, true, _cmdhelp_textfilter);

    _add_insert_commands(cols, 0, "<w>%</w> to throw/fire missiles",
                         CMD_FIRE, 0);
    _add_insert_commands(cols, 0, "<w>%</w>/<w>%</w> to cast spells "
                                  "(<w>%?/%</w> lists spells)",
                         CMD_CAST_SPELL, CMD_FORCE_CAST_SPELL, CMD_CAST_SPELL,
                         CMD_DISPLAY_SPELLS, 0);
    _add_command(cols, 0, CMD_MEMORISE_SPELL, "Memorise a new spell", 2);
    _add_command(cols, 0, CMD_READ, "read a book to forget a spell", 2);

    // Second column.
    cols.add_formatted(
            1, "<h>Item types (and common commands)\n",
            false, true, _cmdhelp_textfilter);

    _add_insert_commands(cols, 1,
                         "<console><cyan>)</cyan> : </console>"
                         "hand weapons (<w>%</w>ield)",
                         CMD_WIELD_WEAPON, 0);
    _add_insert_commands(cols, 1,
                         "<console><brown>(</brown> : </console>"
                         "missiles (<w>%</w>uiver, <w>%</w>ire, <w>%</w>/<w>%</w> cycle)",
                         CMD_QUIVER_ITEM, CMD_FIRE, CMD_CYCLE_QUIVER_FORWARD,
                         CMD_CYCLE_QUIVER_BACKWARD, 0);
    _add_insert_commands(cols, 1,
                         "<console><cyan>[</cyan> : </console>"
                         "armour (<w>%</w>ear and <w>%</w>ake off)",
                         CMD_WEAR_ARMOUR, CMD_REMOVE_ARMOUR, 0);
    _add_insert_commands(cols, 1,
                         "<console><brown>percent</brown> : </console>"
                         "corpses and food (<w>%</w>hop up and <w>%</w>at)",
                         CMD_BUTCHER, CMD_EAT, 0);
    _add_insert_commands(cols, 1,
                         "<console><w>?</w> : </console>"
                         "scrolls (<w>%</w>ead)",
                         CMD_READ, 0);
    _add_insert_commands(cols, 1,
                         "<console><magenta>!</magenta> : </console>"
                         "potions (<w>%</w>uaff)",
                         CMD_QUAFF, 0);
    _add_insert_commands(cols, 1,
                         "<console><blue>=</blue> : </console>"
                         "rings (<w>%</w>ut on and <w>%</w>emove)",
                         CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY, 0);
    _add_insert_commands(cols, 1,
                         "<console><red>\"</red> : </console>"
                         "amulets (<w>%</w>ut on and <w>%</w>emove)",
                         CMD_WEAR_JEWELLERY, CMD_REMOVE_JEWELLERY, 0);
    _add_insert_commands(cols, 1,
                         "<console><lightgrey>/</lightgrey> : </console>"
                         "wands (e<w>%</w>oke)",
                         CMD_EVOKE, 0);

    string item_types =
                  "<console><lightcyan>";
    item_types += stringize_glyph(get_item_symbol(SHOW_ITEM_BOOK));
    item_types +=
        "</lightcyan> : </console>"
        "books (<w>%</w>ead, <w>%</w>emorise, <w>%</w>ap, <w>%</w>ap)";
    _add_insert_commands(cols, 1, item_types,
                         CMD_READ, CMD_MEMORISE_SPELL, CMD_CAST_SPELL,
                         CMD_FORCE_CAST_SPELL, 0);

    item_types =
                  "<console><brown>";
    item_types += stringize_glyph(get_item_symbol(SHOW_ITEM_STAVE));
    item_types +=
        "</brown> : </console>"
        "staves and rods (<w>%</w>ield and e<w>%</w>oke)";
    _add_insert_commands(cols, 1, item_types,
                         CMD_WIELD_WEAPON, CMD_EVOKE_WIELDED, 0);

    cols.add_formatted(1, " ", false, true, _cmdhelp_textfilter);
    _add_command(cols, 1, CMD_DISPLAY_INVENTORY, "list inventory (select item to view it)", 2);
    _add_command(cols, 1, CMD_PICKUP, "pick up item from ground (also <w>g</w>)", 2);
    _add_command(cols, 1, CMD_DROP, "drop item", 2);
    _add_command(cols, 1, CMD_DROP_LAST, "drop the last item(s) you picked up", 2);

    cols.add_formatted(
            1,
            "<h>Additional important commands\n",
            true, true, _cmdhelp_textfilter);

    _add_command(cols, 1, CMD_SAVE_GAME_NOW, "Save the game and exit", 2);
    _add_command(cols, 1, CMD_REPLAY_MESSAGES, "show previous messages", 2);
    _add_command(cols, 1, CMD_USE_ABILITY, "use an ability", 2);
    _add_command(cols, 1, CMD_RESISTS_SCREEN, "show character overview", 2);
    _add_command(cols, 1, CMD_DISPLAY_RELIGION, "show religion overview", 2);
    _add_command(cols, 1, CMD_DISPLAY_MAP, "show map of the whole level", 2);
    _add_command(cols, 1, CMD_DISPLAY_OVERMAP, "show dungeon overview", 2);

    cols.add_formatted(
            1,
            "\n<h>Targetting\n"
            "<w>Enter</w> or <w>.</w> or <w>Del</w> : confirm target\n"
            "<w>+</w> and <w>-</w> : cycle between targets\n"
            "<w>f</w> or <w>p</w> : shoot at previous target\n"
            "         if still alive and in sight\n",
            false, true, _cmdhelp_textfilter);
}

void list_commands(int hotkey, bool do_redraw_screen, string highlight_string)
{
    // 2 columns, split at column 40.
    column_composer cols(2, 41);

    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines() - 1);

    const bool hint_tuto = crawl_state.game_is_hints_tutorial();
    if (hint_tuto)
        _add_formatted_hints_help(cols);
    else
        _add_formatted_keyhelp(cols);

    _show_keyhelp_menu(cols.formatted_lines(), !hint_tuto, Options.easy_exit_menu,
                       hotkey, highlight_string);

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
    column_composer cols(2, 44);
    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines());

    cols.add_formatted(0,
                       "<yellow>Player stats</yellow>\n"
                       "<w>A</w>      set all skills to level\n"
                       "<w>Ctrl-D</w> change enchantments/durations\n"
                       "<w>g</w>      exercise a skill\n"
                       "<w>Ctrl-L</w> change experience level\n"
                       "<w>r</w>      change character's species\n"
                       "<w>s</w>      gain 20000 skill points\n"
                       "<w>S</w>      set skill to level\n"
                       "<w>x</w>      gain an experience level\n"
                       "<w>$</w>      get 1000 gold\n"
                       "<w>]</w>      get a mutation\n"
                       "<w>_</w>      gain religion\n"
                       "<w>^</w>      set piety to a value\n"
                       "<w>@</w>      set Str Int Dex\n"
                       "<w>#</w>      load character from a dump file\n"
                       "<w>Z</w>      gain lots of Zot Points\n"
                       "<w>&</w>      list all divine followers\n"
                       "\n"
                       "<yellow>Create level features</yellow>\n"
                       "<w>L</w>      place a vault by name\n"
                       "<w>p</w>      make a portal\n"
                       "<w>T</w>      make a trap\n"
                       "<w><<</w>/<w>></w>    create up/down staircase\n"
                       "<w>(</w>      turn cell into feature\n"
                       "<w>\\</w>      make a shop\n"
                       "<w>Ctrl-K</w> mark all vaults as unused\n"
                       "\n"
                       "<yellow>Other level related commands</yellow>\n"
                       "<w>Ctrl-A</w> generate new Abyss area\n"
                       "<w>b</w>      controlled blink\n"
                       "<w>Ctrl-B</w> controlled teleport\n"
                       "<w>B</w>      banish yourself to the Abyss\n"
                       "<w>k</w>      shift section of a labyrinth\n"
                       "<w>R</w>      change monster spawn rate\n"
                       "<w>Ctrl-S</w> change Abyss speed\n"
                       "<w>u</w>/<w>d</w>    shift up/down one level\n"
                       "<w>~</w>      go to a specific level\n"
                       "<w>:</w>      find branches and overflow\n"
                       "       temples in the dungeon\n"
                       "<w>;</w>      list known levels and counters\n"
                       "<w>{</w>      magic mapping\n"
                       "<w>}</w>      detect all traps on level\n"
                       "<w>)</w>      change Shoals' tide speed\n"
                       "<w>Ctrl-E</w> dump level builder information\n"
                       "<w>Ctrl-R</w> regenerate current level\n"
                       "<w>P</w>      create a level based on a vault\n",
                       true, true);

    cols.add_formatted(1,
                       "<yellow>Other player related effects</yellow>\n"
                       "<w>c</w>      card effect\n"
#ifdef DEBUG_BONES
                       "<w>Ctrl-G</w> save/load ghost (bones file)\n"
#endif
                       "<w>h</w>/<w>H</w>    heal yourself (super-Heal)\n"
                       "<w>Ctrl-H</w> set hunger state\n"
                       "<w>X</w>      make Xom do something now\n"
                       "<w>z</w>      cast spell by number/name\n"
                       "<w>Ctrl-M</w> memorise spell\n"
                       "<w>W</w>      god wrath\n"
                       "<w>w</w>      god mollification\n"
                       "<w>Ctrl-P</w> polymorph into a form\n"
                       "<w>Ctrl-V</w> toggle xray vision\n"
                       "\n"
                       "<yellow>Monster related commands</yellow>\n"
                       "<w>D</w>      detect all monsters\n"
                       "<w>G</w>      dismiss all monsters\n"
                       "<w>m</w>/<w>M</w>    create monster by number/name\n"
                       "<w>\"</w>      list monsters\n"
                       "\n"
                       "<yellow>Item related commands</yellow>\n"
                       "<w>a</w>      acquirement\n"
                       "<w>C</w>      (un)curse item\n"
                       "<w>i</w>/<w>I</w>    identify/unidentify inventory\n"
                       "<w>o</w>/<w>%</w>    create an object\n"
                       "<w>t</w>      tweak object properties\n"
                       "<w>v</w>      show gold value of an item\n"
                       "<w>-</w>      get a god gift\n"
                       "<w>|</w>      create all predefined artefacts\n"
                       "<w>+</w>      make randart from item\n"
                       "<w>'</w>      list items\n"
                       "<w>J</w>      Jiyva off-level sacrifice\n"
                       "\n"
                       "<yellow>Debugging commands</yellow>\n"
                       "<w>f</w>      quick fight simulation\n"
                       "<w>F</w>      single scale fsim\n"
                       "<w>Ctrl-F</w> double scale fsim\n"
                       "<w>Ctrl-I</w> item generation stats\n"
                       "<w>O</w>      measure exploration time\n"
                       "<w>Ctrl-T</w> dungeon (D)Lua interpreter\n"
                       "<w>Ctrl-U</w> client (C)Lua interpreter\n"
                       "<w>Ctrl-X</w> Xom effect stats\n"
#ifdef DEBUG_DIAGNOSTICS
                       "<w>Ctrl-Q</w> make some debug messages quiet\n"
#endif
                       "<w>Ctrl-C</w> force a crash\n"
                       "\n"
                       "<yellow>Other wizard commands</yellow>\n"
                       "(not prefixed with <w>&</w>!)\n"
                       "<w>x?</w>     list targeted commands\n"
                       "<w>X?</w>     list map-mode commands\n",
                       true, true);

    int key = _show_keyhelp_menu(cols.formatted_lines(), false,
                                 Options.easy_exit_menu);
    if (do_redraw_screen)
        redraw_screen();
    return key;
}
#endif
