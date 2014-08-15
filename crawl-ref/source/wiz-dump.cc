/**
 * @file
 * @brief Wizmode character dump loading
**/

#include "AppHdr.h"

#include "wiz-dump.h"

#include <algorithm>
#include <functional>

#include "chardump.h"
#include "dbg-util.h"
#include "defines.h"
#include "enum.h"
#include "env.h"
#include "externs.h"
#include "items.h"
#include "itemname.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "player.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "strings.h"
#include "unicode.h"
#include "wiz-you.h"

static item_def _item_from_string(string s)
{
    item_def ret;
    if (s.length() == 0)
        return ret;

    if (s[0] == '+' || s[0] == '-')
    {
        size_t space = s.find(' ');
        if (space == string::npos)
            return ret;

        ret.plus = atoi(s.substr(1).c_str());
        if (s[0] == '-')
            ret.plus = -ret.plus;

        s = s.substr(space + 1);
        if (s.length() == 0)
            return ret;
    }

    size_t end = s.find_first_of("({");
    if (end == string::npos)
        end = s.length();
    else
        end--;

    string base_name = s.substr(0, end);
    item_kind parsed = item_kind_by_name(base_name);
    if (parsed.base_type != OBJ_UNASSIGNED)
    {
        ret.base_type = parsed.base_type;
        ret.sub_type  = parsed.sub_type;
        /* pluses for item_kinds are only valid for manuals and runes
         * so we can skip them here for now */
    }

    ret.quantity = 1;
    item_colour(ret);

    while (end < s.length())
    {
        size_t begin_brand = s.find_first_not_of("{(, ", end);
        if (begin_brand == string::npos)
            break;
        size_t end_brand = s.find_first_of("}), ", begin_brand);
        if (end_brand == begin_brand)
            break;
        else if (end_brand == string::npos)
            end_brand = s.length();
        string brand_name = s.substr(begin_brand, end_brand - begin_brand);

        bool found = false;
        switch (ret.base_type)
        {
        case OBJ_WEAPONS:
            for (int i = SPWPN_NORMAL; i < NUM_SPECIAL_WEAPONS; ++i)
            {
                ret.special = i;
                if (brand_name == weapon_brand_name(ret, true))
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                ret.special = SPWPN_NORMAL;
            break;
        case OBJ_ARMOUR:
            for (int i = SPARM_NORMAL; i < NUM_SPECIAL_ARMOURS; ++i)
            {
                ret.special = i;
                if (brand_name == armour_ego_name(ret, true))
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                ret.special = SPARM_NORMAL;
        break;
        // XXX
        default:
            break;
        }

        end = end_brand;
    }

    return ret;
}

bool chardump_parser::_check_skill(const vector<string> &tokens)
{
    size_t size = tokens.size();
    // * Level 25.0(24.4) Dodging
    if (size <= 3 || tokens[1] != "Level")
        return false;

    if (!seen_skills)
    {
        you.init_skills();
        seen_skills = true;
    }

    skill_type skill = skill_from_name(lowercase_string(tokens[3]).c_str());
    double amount = atof(tokens[2].c_str());
    set_skill_level(skill, amount);
    if (tokens[0] == "+")
        you.train[skill] = 1;
    else if (tokens[0] == "*")
        you.train[skill] = 2;
    else
        you.train[skill] = 0;

    redraw_skill(skill);

    return true;
}

bool chardump_parser::_check_stats1(const vector<string> &tokens)
{
    size_t size = tokens.size();
    // HP 121/199   AC 75   Str 35   XL: 27
    if (size <= 7 || tokens[0] != "HP")
        return false;

    bool found = false;
    for (size_t k = 1; k < size; k++)
    {
        if (tokens[k] == "Str")
        {
            you.base_stats[STAT_STR] = debug_cap_stat(atoi(tokens[k+1].c_str()));
            you.redraw_stats.init(true);
            you.redraw_evasion = true;
            found = true;
        }
        else if (tokens[k] == "XL:")
        {
            set_xl(atoi(tokens[k+1].c_str()), false);
            found = true;
        }
    }

    return found;
}

bool chardump_parser::_check_stats2(const vector<string> &tokens)
{
    size_t size = tokens.size();
    // MP  45/45   EV 13   Int 12   God: Makhleb [******]
    if (size <= 8 || tokens[0] != "MP")
        return false;

    bool found = false;
    for (size_t k = 1; k < size; k++)
    {
        if (tokens[k] == "Int")
        {
            you.base_stats[STAT_INT] = debug_cap_stat(atoi(tokens[k+1].c_str()));
            you.redraw_stats.init(true);
            you.redraw_evasion = true;
            found = true;
        }
        else if (tokens[k] == "God:")
        {
            god_type god = find_earliest_match(lowercase_string(tokens[k+1]),
                                               (god_type) 1, NUM_GODS,
                                               _always_true<god_type>,
                                               bind2nd(ptr_fun(god_name),
                                                       false));
            join_religion(god, true);

            string piety = tokens[k+2];
            int piety_levels = std::count(piety.begin(), piety.end(), '*');
            wizard_set_piety_to(piety_levels > 0
                                ? piety_breakpoint(piety_levels - 1)
                                : 15);
        }
    }

    return found;
}

bool chardump_parser::_check_stats3(const vector<string> &tokens)
{
    size_t size = tokens.size();
    // Gold 15872   SH 59   Dex  9   Spells:  0 memorised, 26 levels left
    if (size <= 5 || tokens[0] != "Gold")
        return false;

    bool found;
    for (size_t k = 0; k < size; k++)
    {
        if (tokens[k] == "Dex")
        {
            you.base_stats[STAT_DEX] = debug_cap_stat(atoi(tokens[k+1].c_str()));
            you.redraw_stats.init(true);
            you.redraw_evasion = true;
            found = true;
        }
        else if (tokens[k] == "Gold")
            you.set_gold(atoi(tokens[k+1].c_str()));
    }

    return found;
}

bool chardump_parser::_check_char(const vector<string> &tokens)
{
    size_t size = tokens.size();

    if (size <= 8)
        return false;

    for (size_t k = 1; k < size; k++)
    {
        if (tokens[k] == "Turns:")
        {
            string race = tokens[k-2].substr(1);
            string role = tokens[k-1].substr(0, tokens[k-1].length() - 1);

            wizard_change_species_to(find_species_from_string(race));
            wizard_change_job_to(find_job_from_string(role));
            return true;
        }
    }

    return false;
}

bool chardump_parser::_check_equipment(const vector<string> &tokens)
{
    size_t size = tokens.size();

    if (size <= 0)
        return false;

    size_t offset;
    if (tokens[0] == "rFire")
        offset = 9;
    else if (tokens[0] == "rCold")
        offset = 9;
    else if (tokens[0] == "rNeg")
        offset = 9;
    else if (tokens[0] == "rPois")
        offset = 7;
    else if (tokens[0] == "rElec")
        offset = 7;
    else if (tokens[0] == "SustAb")
        offset = 8;
    else if (tokens[0] == "rMut")
        offset = 7;
    else if (tokens[0] == "Saprov")
        offset = 9;
    else if (tokens[0] == "MR")
        offset = 5;
    else if (in_equipment)
        offset = 3;
    else
        return false;

    if (size < offset)
        return false;

    string item_desc = tokens[offset - 1];
    for (vector<string>::const_iterator it = tokens.begin() + offset;
         it != tokens.end();
         ++it)
    {
        item_desc.append(" ");
        item_desc.append(*it);
    }

    item_def item = _item_from_string(item_desc);
    if (item.base_type == OBJ_UNASSIGNED)
        mprf("unknown item: %s", item_desc.c_str());
    else
    {
        int mitm_slot = get_mitm_slot();
        if (mitm_slot != NON_ITEM)
        {
            mitm[mitm_slot] = item;
            move_item_to_grid(&mitm_slot, you.pos());
        }
    }

    return true;
}

void chardump_parser::_modify_character(const string &inputdata)
{
    vector<string> tokens = split_string(" ", inputdata);

    if (_check_skill(tokens))
        return;
    if (_check_stats1(tokens))
        return;
    if (_check_stats2(tokens))
        return;
    if (_check_stats3(tokens))
        return;
    if (_check_char(tokens))
        return;

    if (_check_equipment(tokens))
    {
        in_equipment = true;
        return;
    }
    else
        in_equipment = false;
}

/**
 * Load a character from a dump file.
 *
 * @param filename The name of the file to open.
 * @pre The file either does not exist, or is a complete
 *      dump or morgue file.
 * @returns True if the file existed and could be opened.
 * @post The player's stats, level, skills, training, and gold are
 *       those listed in the dump file.
 */
bool chardump_parser::_parse_from_file(const string &full_filename)
{
    FileLineInput f(full_filename.c_str());
    if (f.eof())
        return false;

    string first_line = f.get_line();
    if (first_line.substr(0, 34) != " Dungeon Crawl Stone Soup version ")
        return false;

    while (!f.eof())
        _modify_character(f.get_line());

    if (seen_skills)
    {
        init_skill_order();
        init_can_train();
        init_train();
        init_training();
    }

    return true;
}

bool chardump_parser::parse()
{
    return _parse_from_file(filename)
        || _parse_from_file(morgue_directory() + filename);
}

void wizard_load_dump_file()
{
    char filename[80];
    msgwin_get_line_autohist("Which dump file? ", filename, sizeof(filename));

    if (filename[0] == '\0')
        canned_msg(MSG_OK);
    else
    {
        chardump_parser parser(filename);
        if (!parser.parse())
            canned_msg(MSG_NOTHING_THERE);
    }
}
