/**
 * @file
 * @brief Wizmode character dump loading
**/

#include "AppHdr.h"

#include "wiz-dump.h"

#include <algorithm>
#include <functional>

#include "artefact.h"
#include "chardump.h"
#include "dbg-util.h"
#include "describe.h"
#include "env.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "religion.h"
#include "skills.h"
#include "stringutil.h"
#include "unicode.h"
#include "wiz-you.h"

static uint8_t _jewellery_type_from_artefact_prop(const string &s
#if TAG_MAJOR_VERSION == 34
                                                  , bool is_amulet
#endif
                                                  )
{
    if (s == "Regen")
#if TAG_MAJOR_VERSION == 34
        return is_amulet ? AMU_REGENERATION : RING_REGENERATION;
#else
        return AMU_REGENERATION;
#endif

    if (s == "+Rage")
        return AMU_RAGE;
    if (s == "Clar")
        return AMU_CLARITY;
    if (s == "Ward")
        return AMU_WARDING;
    if (s == "rCorr")
        return AMU_RESIST_CORROSION;
    if (s == "Gourm")
        return AMU_THE_GOURMAND;
    if (s == "Inacc")
        return AMU_INACCURACY;
    if (s == "rMut")
        return AMU_RESIST_MUTATION;
    if (s == "Spirit")
        return AMU_GUARDIAN_SPIRIT;
    if (s == "Faith")
        return AMU_FAITH;
    if (s == "Stasis")
        return AMU_STASIS;

    if (s == "Fire")
        return RING_FIRE;
    if (s == "Ice")
        return RING_ICE;
    if (s == "+/*Tele")
        return RING_TELEPORTATION;
    if (s == "+cTele")
        return RING_TELEPORT_CONTROL;
    if (s == "SustAb")
        return RING_SUSTAIN_ABILITIES;
    if (s == "Wiz")
        return RING_WIZARDRY;
    if (s == "SInv")
        return RING_SEE_INVISIBLE;
    if (s == "+Inv")
        return RING_INVISIBILITY;
    if (s == "Noisy")
        return RING_LOUDNESS;
    if (s == "+Fly")
        return RING_FLIGHT;
    if (s == "rPois")
        return RING_POISON_RESISTANCE;

    if (s.substr(0, 2) == "AC")
        return RING_PROTECTION;
    if (s.substr(0, 2) == "MP")
        return RING_MAGICAL_POWER;
    if (s.substr(0, 4) == "Slay")
        return RING_SLAYING;
    if (s.substr(0, 3) == "Str")
        return RING_STRENGTH;
    if (s.substr(0, 3) == "Dex")
        return RING_DEXTERITY;
    if (s.substr(0, 3) == "Int")
        return RING_INTELLIGENCE;
    if (s.substr(0, 2) == "EV")
        return RING_EVASION;
    if (s.substr(0, 5) == "Stlth")
        return RING_STEALTH;
    if (s.substr(0, 2) == "MR")
        return RING_PROTECTION_FROM_MAGIC;

    if (s.substr(0, 2) == "rF")
        return RING_PROTECTION_FROM_FIRE;
    if (s.substr(0, 2) == "rC")
        return RING_PROTECTION_FROM_COLD;
    if (s.substr(0, 2) == "rN")
        return RING_LIFE_PROTECTION;

    return NUM_JEWELLERY;
}

static item_kind _find_real_item_kind(string &s)
{
    if (s.substr(0, 7) == "amulet ")
    {
        item_kind amulet = {OBJ_JEWELLERY, NUM_JEWELLERY, 0, 0};
        return amulet;
    }
    else if (s.substr(0, 5) == "ring " && s.substr(0, 9) != "ring mail")
    {
        item_kind ring = {OBJ_JEWELLERY, NUM_JEWELLERY, 0, 0};
        return ring;
    }

    for (size_t end = s.length();
         end > 0 && end != string::npos;
         end = s.rfind(' ', end - 1))
    {
        item_kind kind = item_kind_by_name(s.substr(0, end));
        if (kind.base_type != OBJ_UNASSIGNED)
        {
            if (s.substr(0, 8) == "pair of ")
                s = s.substr(8);
            return kind;
        }
    }

    item_kind err = {OBJ_UNASSIGNED, 0, 0, 0};
    return err;
}

static void _apply_randart_properties(item_def &item,
                                      const string &name,
                                      const string &props)
{
    CrawlVector &rap = item.props[ARTEFACT_PROPS_KEY].get_vector();
    for (vec_size i = 0; i < ART_PROPERTIES; i++)
        rap[i] = static_cast<short>(0);

    set_artefact_name(item, name);

    size_t idx = 0;
    while (idx < props.length())
    {
        size_t begin_brand = props.find_first_not_of("{(, ", idx);
        if (begin_brand == string::npos)
            break;
        size_t end_brand = props.find_first_of("}), ", begin_brand);
        if (end_brand == begin_brand)
            break;
        else if (end_brand == string::npos)
            end_brand = props.length();
        string brand_name = props.substr(begin_brand, end_brand - begin_brand);

        if (item.is_type(OBJ_JEWELLERY, NUM_JEWELLERY))
        {
            item.sub_type = _jewellery_type_from_artefact_prop(
                brand_name
#if TAG_MAJOR_VERSION == 34
                , name.find("amulet") != string::npos
#endif
            );
        }

        string ins = artefact_inscription(item);
        for (vec_size i = 0; i < ART_PROPERTIES; i++)
        {
            for (short j = 1; j < 9; j++)
            {
                item_def copy = item;
                copy.props[ARTEFACT_PROPS_KEY].get_vector()[i] = j;
                string ins_with_prop = ins.length()
                    ? ins + " " + brand_name
                    : brand_name;
                if (artefact_inscription(copy) == ins_with_prop)
                {
                    rap[i] = j;
                    break;
                }
            }
            for (short j = -1; j > -8; j--)
            {
                item_def copy = item;
                copy.props[ARTEFACT_PROPS_KEY].get_vector()[i] = j;
                string ins_with_prop = ins.length()
                    ? ins + " " + brand_name
                    : brand_name;
                if (artefact_inscription(copy) == ins_with_prop)
                {
                    rap[i] = j;
                    break;
                }
            }
        }

        idx = end_brand;
    }

    if (item.base_type == OBJ_JEWELLERY)
        ASSERT(item.sub_type != NUM_JEWELLERY);
}

static int _find_ego_type(object_class_type type, const string &s)
{
    size_t begin_brand = s.find_first_not_of("{(, ");
    if (begin_brand == string::npos)
        return 0;
    size_t end_brand = s.find_first_of("}), ", begin_brand);
    if (end_brand == begin_brand)
        return 0;
    else if (end_brand == string::npos)
        end_brand = s.length();
    string brand_name = s.substr(begin_brand, end_brand - begin_brand);

    item_def item;
    item.base_type = type;

    switch (type)
    {
    case OBJ_WEAPONS:
        for (int i = SPWPN_NORMAL; i < NUM_SPECIAL_WEAPONS; ++i)
        {
            item.special = i;
            if (brand_name == weapon_brand_name(item, true))
                return i;
        }
        break;
    case OBJ_ARMOUR:
        for (int i = SPARM_NORMAL; i < NUM_SPECIAL_ARMOURS; ++i)
        {
            item.special = i;
            if (brand_name == armour_ego_name(item, true))
                return i;
        }
    break;
    default:
        break;
    }

    return 0;
}

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

    set_ident_type(ret, ID_KNOWN_TYPE);
    set_ident_flags(ret, ISFLAG_IDENT_MASK);

    string base_name = s.substr(0, end);
    item_kind parsed = item_kind_by_name(base_name);
    if (parsed.base_type == OBJ_UNASSIGNED)
    {
        // maybe a randart?
        parsed = _find_real_item_kind(base_name);
        if (parsed.base_type != OBJ_UNASSIGNED)
        {
            ret.base_type = parsed.base_type;
            ret.sub_type  = parsed.sub_type;

            make_item_randart(ret, true);

            _apply_randart_properties(ret, base_name, s.substr(end));
        }
    }
    else
    {
        ret.base_type = parsed.base_type;
        ret.sub_type  = parsed.sub_type;
        /* pluses for item_kinds are only valid for manuals and runes
         * so we can skip them here for now */

        ret.special = _find_ego_type(ret.base_type, s.substr(end));
    }

    ret.quantity = 1;
    item_colour(ret);

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
    // Health: 248/248    AC: 44    Str: 35    XL:     26   Next: 58%
    if (size <= 7 || (tokens[0] != "HP" && tokens[0] != "Health:"))
        return false;

    bool found = false;
    for (size_t k = 1; k < size; k++)
    {
        if (tokens[k] == "Str:" || tokens[k] == "Str")
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
    // Magic:  36/36      EV: 31    Int: 17    God:    Cheibriados [****..]
    if (size <= 8 || (tokens[0] != "MP" && tokens[0] != "Magic:"))
        return false;

    bool found = false;
    for (size_t k = 1; k < size; k++)
    {
        if (tokens[k] == "Int:" || tokens[k] == "Int")
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
                                               always_true<god_type>,
                                               bind(god_name, placeholders::_1,
                                                    false));
            if (!you_worship(god))
            {
                if (god == GOD_GOZAG)
                    you.gold += gozag_service_fee();
                join_religion(god, true);
            }

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
    // Gold: 2013  SH: 0  Dex: 26  Spells: 5 memorised, 36 levels left
    if (size <= 5 || tokens[0] != "Gold:")
        return false;

    bool found;
    for (size_t k = 0; k < size; k++)
    {
        if (tokens[k] == "Dex:" || tokens[k] == "Dex")
        {
            you.base_stats[STAT_DEX] = debug_cap_stat(atoi(tokens[k+1].c_str()));
            you.redraw_stats.init(true);
            you.redraw_evasion = true;
            found = true;
        }
        else if (tokens[k] == "Gold:" || tokens[k] == "Gold")
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
            string race;
            if (tokens[k-2][0] == '(')
                race = tokens[k-2].substr(1);
            else
                race = tokens[k-3].substr(1) + " " + tokens[k-2];
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
    else if (tokens[0] == "rCorr")
        offset = 7;
    else if (tokens[0] == "SustAb") // older dump files
        offset = 8;
    else if (tokens[0] == "rMut")
        offset = 7;
    else if (tokens[0] == "Saprov") // older dump files
        offset = 9;
    else if (tokens[0] == "Gourm") // older dump files
        offset = 5;
    else if (tokens[0] == "MR")
        offset = 5;
    else if (tokens[0] == "Stlth")
        offset = 5;
    else if (in_equipment)
        offset = 3;
    else
        return false;

    if (size < offset)
        return false;

    string item_desc = tokens[offset - 1];
    for (auto it = tokens.begin() + offset; it != tokens.end(); ++it)
        item_desc += " " + *it;

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
