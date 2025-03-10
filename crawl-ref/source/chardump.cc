/**
 * @file
 * @brief Dumps character info out to the morgue file.
**/

#include "AppHdr.h"

#include "chardump.h"

#include <string>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#if defined(UNIX) || defined(TARGET_COMPILER_MINGW)
#include <unistd.h>
#endif

#include "ability.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "describe.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "fight.h"
#include "files.h"
#include "god-companions.h"
#include "god-prayer.h"
#include "hiscores.h"
#include "initfile.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "kills.h"
#include "libutil.h"
#include "melee-attack.h"
#include "message.h"
#include "mutation.h"
#include "notes.h"
#include "output.h"
#include "place.h"
#include "prompt.h"
#include "religion.h"
#include "scroller.h"
#include "showsymb.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "transform.h"
#include "travel.h"
#include "unicode.h"
#include "version.h"
#include "viewchar.h"
#include "view.h"
#include "xom.h"

struct dump_params;

static void _sdump_header(dump_params &);
static void _sdump_stats(dump_params &);
static void _sdump_location(dump_params &);
static void _sdump_religion(dump_params &);
static void _sdump_transform(dump_params &);
static void _sdump_visits(dump_params &);
static void _sdump_gold(dump_params &);
static void _sdump_misc(dump_params &);
static void _sdump_turns_by_place(dump_params &);
static void _sdump_notes(dump_params &);
static void _sdump_screenshots(dump_params &);
static void _sdump_inventory(dump_params &);
static void _sdump_skills(dump_params &);
static void _sdump_spells(dump_params &);
static void _sdump_mutations(dump_params &);
static void _sdump_messages(dump_params &);
static void _sdump_screenshot(dump_params &);
static void _sdump_kills_by_place(dump_params &);
static void _sdump_kills(dump_params &);
static void _sdump_xp_by_level(dump_params &);
static void _sdump_newline(dump_params &);
static void _sdump_overview(dump_params &);
static void _sdump_hiscore(dump_params &);
static void _sdump_monster_list(dump_params &);
static void _sdump_vault_list(dump_params &);
static void _sdump_skill_gains(dump_params &);
static void _sdump_action_counts(dump_params &);
static void _sdump_apostles(dump_params &);
static void _sdump_separator(dump_params &);
static void _sdump_lua(dump_params &);
static bool _write_dump(const string &fname, const dump_params &,
                        bool print_dump_path = false);

struct dump_section_handler
{
    const char *name;
    void (*handler)(dump_params &);
};

struct dump_params
{
    string text;
    string section;
    bool full_id;
    const scorefile_entry *se;

    dump_params(const string &sec = "", bool id = false,
                const scorefile_entry *s = nullptr)
        : section(sec), full_id(id), se(s)
    {
        // Start with enough room for 100 80 character lines.
        text.reserve(100 * 80);
    }
};

static dump_section_handler dump_handlers[] =
{
    { "header",         _sdump_header        },
    { "stats",          _sdump_stats         },
    { "location",       _sdump_location      },
    { "religion",       _sdump_religion      },
    { "transform",      _sdump_transform     },
    { "visits",         _sdump_visits        },
    { "gold",           _sdump_gold          },
    { "misc",           _sdump_misc          },
    { "turns_by_place", _sdump_turns_by_place},
    { "notes",          _sdump_notes         },
    { "screenshots",    _sdump_screenshots   },
    { "inventory",      _sdump_inventory     },
    { "skills",         _sdump_skills        },
    { "spells",         _sdump_spells        },
    { "mutations",      _sdump_mutations     },
    { "messages",       _sdump_messages      },
    { "screenshot",     _sdump_screenshot    },
    { "kills_by_place", _sdump_kills_by_place},
    { "kills",          _sdump_kills         },
    { "xp_by_level",    _sdump_xp_by_level   },
    { "overview",       _sdump_overview      },
    { "hiscore",        _sdump_hiscore       },
    { "monlist",        _sdump_monster_list  },
    { "vaults",         _sdump_vault_list    },
    { "spell_usage",    _sdump_action_counts }, // compat
    { "action_counts",  _sdump_action_counts },
    { "skill_gains",    _sdump_skill_gains   },
    { "apostles",       _sdump_apostles      },

    // Conveniences for the .crawlrc artist.
    { "",               _sdump_newline       },
    { "-",              _sdump_separator     },

    { nullptr,          _sdump_lua           }
};

static void dump_section(dump_params &par)
{
    for (int i = 0; ; ++i)
    {
        if (!dump_handlers[i].name || par.section == dump_handlers[i].name)
        {
            if (dump_handlers[i].handler)
                (*dump_handlers[i].handler)(par);
            break;
        }
    }
}

static dump_params _get_dump(bool full_id = false,
                             const scorefile_entry *se = nullptr)
{
    dump_params par("", full_id, se);

    for (const string &section : Options.dump_order)
    {
        par.section = section;
        dump_section(par);
    }

    // Hopefully we get RVO so we don't have to copy the text.
    return par;
}

bool dump_char(const string &fname, bool quiet, bool full_id,
               const scorefile_entry *se)
{
    return _write_dump(fname, _get_dump(full_id, se), quiet);
}

string seed_description()
{
    return make_stringf(
        "Game seed: %" PRIu64 "%s", crawl_state.seed,
            crawl_state.type == GAME_TYPE_CUSTOM_SEED
            ? " (custom seed)"
            : you.deterministic_levelgen ? "" : " (classic levelgen)");
}

static void _sdump_header(dump_params &par)
{
    string type = crawl_state.game_type_name();
    if (type.empty())
        type = CRAWL;
    else
        type += " DCSS";

    par.text += " " + type + " version " + Version::Long;
#ifdef USE_TILE_LOCAL
    par.text += " (tiles)";
#elif defined(USE_TILE_WEB)
    if (::tiles.is_controlled_from_web())
        par.text += " (webtiles)";
    else
        par.text += " (console)";
#else
    par.text += " (console)";
#endif
    par.text += " character file.\n\n";

    if (you.fully_seeded && crawl_state.seed_is_known())
        par.text += seed_description() + "\n\n";
}

static void _sdump_stats(dump_params &par)
{
    par.text += dump_overview_screen();
    par.text += "\n\n";
}

static void _sdump_transform(dump_params &par)
{
    string &text(par.text);
    if (you.form != transformation::none)
        text += get_form()->get_description(par.se) + "\n\n";}

static branch_type single_portals[] =
{
    BRANCH_TROVE,
    BRANCH_SEWER,
    BRANCH_OSSUARY,
    BRANCH_BAILEY,
    BRANCH_GAUNTLET,
    BRANCH_ICE_CAVE,
    BRANCH_VOLCANO,
    BRANCH_WIZLAB,
    BRANCH_DESOLATION,
#if TAG_MAJOR_VERSION == 34
    BRANCH_LABYRINTH,
#endif
};

static void _sdump_visits(dump_params &par)
{
    string &text(par.text);

    string have = "have ";
    string seen = "seen";
    if (par.se) // you died -> past tense
    {
        have = "";
        seen = "saw";
    }

    const vector<PlaceInfo> branches_visited = you.get_all_place_info(true, true);

    PlaceInfo branches_total;
    for (const PlaceInfo &branch : branches_visited)
        branches_total += branch;

    text += make_stringf("You %svisited %d branch",
                         have.c_str(), (int)branches_visited.size());
    if (branches_visited.size() != 1)
        text += "es";
    if (brdepth[root_branch] > 1 || branches_visited.size() != 1)
    {
        text += make_stringf(" of the dungeon, and %s %d of its levels.\n",
                             seen.c_str(), branches_total.levels_seen);
    }

    {
        const PlaceInfo place_info = you.get_place_info(BRANCH_PANDEMONIUM);
        if (place_info.num_visits > 0)
        {
            text += make_stringf("You %svisited Pandemonium %d time",
                                 have.c_str(), place_info.num_visits);
            if (place_info.num_visits > 1)
                text += "s";
            text += make_stringf(", and %s %d of its levels.\n",
                                 seen.c_str(), place_info.levels_seen);
        }
    }

    {
        const PlaceInfo place_info = you.get_place_info(BRANCH_ABYSS);
        if (place_info.num_visits > 0)
        {
            text += make_stringf("You %svisited the Abyss %d time",
                                 have.c_str(), place_info.num_visits);
            if (place_info.num_visits > 1)
                text += "s";
            text += ".\n";
        }
    }

    {
        const PlaceInfo place_info = you.get_place_info(BRANCH_BAZAAR);
        if (place_info.num_visits > 0)
        {
            text += make_stringf("You %svisited %d bazaar",
                                 have.c_str(), place_info.num_visits);
            if (place_info.num_visits > 1)
                text += "s";
            text += ".\n";
        }
    }

    {
        const PlaceInfo place_info = you.get_place_info(BRANCH_ZIGGURAT);
        if (place_info.num_visits > 0)
        {
            int num_zigs = place_info.num_visits;
            text += make_stringf("You %s%s %d ziggurat",
                                 have.c_str(),
                                 (num_zigs == you.zigs_completed) ? "completed"
                                                                  : "visited",
                                 num_zigs);
            if (num_zigs > 1)
                text += "s";
            if (num_zigs != you.zigs_completed && you.zigs_completed)
                text += make_stringf(" (completing %d)", you.zigs_completed);
            text += make_stringf(", and %s %d of %s levels",
                                 seen.c_str(), place_info.levels_seen,
                                 num_zigs > 1 ? "their" : "its");
            if (num_zigs != 1 && !you.zigs_completed)
                text += make_stringf(" (deepest: %d)", you.zig_max);
            text += ".\n";
        }
    }

    vector<string> misc_portals;
    for (branch_type br : single_portals)
    {
        const PlaceInfo place_info = you.get_place_info(br);
        if (!place_info.num_visits)
            continue;
        string name = branches[br].shortname;
        if (place_info.num_visits > 1)
            name += make_stringf(" (%d times)", place_info.num_visits);
        misc_portals.push_back(name);
    }
    if (!misc_portals.empty())
    {
        text += "You " + have + "also visited: "
                + comma_separated_line(misc_portals.begin(),
                                       misc_portals.end())
                + ".\n";
    }

    text += "\n";
}

static void _sdump_gold(dump_params &par)
{
    string &text(par.text);

    int lines = 0;

    const char* have = "have ";
    if (par.se) // you died -> past tense
        have = "";

    if (you.attribute[ATTR_GOLD_FOUND] > 0)
    {
        lines++;
        text += make_stringf("You %scollected %d gold pieces.\n", have,
                             you.attribute[ATTR_GOLD_FOUND]);
    }

    if (you.attribute[ATTR_PURCHASES] > 0)
    {
        lines++;
        text += make_stringf("You %sspent %d gold pieces at shops.\n", have,
                             you.attribute[ATTR_PURCHASES]);
    }

    if (you.attribute[ATTR_DONATIONS] > 0)
    {
        lines++;
        text += make_stringf("You %sdonated %d gold pieces to Zin.\n", have,
                             you.attribute[ATTR_DONATIONS]);
    }

    if (you.attribute[ATTR_GOZAG_GOLD_USED] > 0)
    {
        lines++;
        text += make_stringf("You %spaid %d gold pieces to Gozag.\n", have,
                             you.attribute[ATTR_GOZAG_GOLD_USED]);
    }

    if (you.attribute[ATTR_MISC_SPENDING] > 0)
    {
        lines++;
        text += make_stringf("You %sused %d gold pieces for miscellaneous "
                             "purposes.\n", have,
                             you.attribute[ATTR_MISC_SPENDING]);
    }

    if (lines > 0)
        text += "\n";
}

static void _sdump_misc(dump_params &par)
{
    _sdump_location(par);
    _sdump_religion(par);
    _sdump_transform(par);
    _sdump_visits(par);
    _sdump_gold(par);
}

#define TO_PERCENT(x, y) (100.0f * (static_cast<float>(x)) / (static_cast<float>(y)))

static string _denanify(const string &s)
{
    string out = replace_all(s, " nan ", " N/A ");
    out = replace_all(out, " -nan ", " N/A  ");
    out = replace_all(out, " 1#IND ", "  N/A  ");
    out = replace_all(out, " -1#IND ", "  N/A   ");
    return out;
}

static string _sdump_level_xp_info(LevelXPInfo xp_info, string name = "")
{
    string out;

    if (name.empty())
        name = xp_info.level.describe();

    float c, f;
    unsigned int total_xp = xp_info.vault_xp + xp_info.non_vault_xp;
    unsigned int total_count = xp_info.vault_count + xp_info.non_vault_count;

    c = TO_PERCENT(xp_info.vault_xp, total_xp);
    f = TO_PERCENT(xp_info.vault_count, total_count);

    out =
        make_stringf("%11s | %7d | %7d | %5.1f | %7d | %7d | %5.1f\n",
                     name.c_str(), xp_info.non_vault_xp, xp_info.vault_xp,
                     c, xp_info.non_vault_count, xp_info.vault_count, f);

    return _denanify(out);
}

static string _sdump_turns_place_info(const PlaceInfo place_info, string name = "")
{
    string out;

    if (name.empty())
        name = place_info.short_name();

    unsigned int non_interlevel =
        place_info.elapsed_total / 10 - place_info.elapsed_interlevel / 10;

    const float g = static_cast<float>(place_info.elapsed_total / 10)
                            / static_cast<float>(place_info.levels_seen);

    out =
        make_stringf("%14s | %6d | %6d | %6d | %6d | %6d | %3d | %6.1f |\n",
                     name.c_str(),
                     place_info.elapsed_total / 10,
                     non_interlevel,
                     place_info.elapsed_interlevel / 10,
                     place_info.elapsed_resting / 10,
                     place_info.elapsed_explore / 10,
                     place_info.levels_seen,
                     g);

    return _denanify(out);
}

static void _sdump_turns_by_place(dump_params &par)
{
    string &text(par.text);

    const vector<PlaceInfo> all_visited = you.get_all_place_info(true);

    text +=
"Table legend: (Time is in decaauts)\n"
" A = Elapsed time spent in this place.\n"
" B = Non-inter-level travel time spent in this place.\n"
" C = Inter-level travel time spent in this place.\n"
" D = Time resting spent in this place.\n"
" E = Time spent auto-exploring this place.\n"
" F = Levels seen in this place.\n"
" G = Mean time per level.\n";

    text += "               ";
    text += "    A        B        C        D        E      F       G\n";
    text += "               ";
    text += "+--------+--------+--------+--------+--------+-----+--------+\n";

    text += _sdump_turns_place_info(you.global_info, "Total");

    for (const PlaceInfo &pi : all_visited)
        text += _sdump_turns_place_info(pi);

    text += "               ";
    text += "+--------+--------+--------+--------+--------+-----+--------+\n";

    text += "\n";

    CrawlHashTable &time_tracking = you.props[TIME_PER_LEVEL_KEY].get_table();
    vector<pair<int, string>> to_sort;
    for (auto &l : time_tracking)
        if (l.first != "upgrade" && l.first != "Pan")
            to_sort.emplace_back(l.second.get_int(), l.first);
    if (to_sort.size() == 0)
        return; // turn 0 game
    sort(to_sort.begin(), to_sort.end());
    reverse(to_sort.begin(), to_sort.end());

    text += "Top non-repeatable levels by time:\n";
    for (unsigned int i = 0; i < 15 && i < to_sort.size(); i++)
        text += make_stringf("%8s: %d daAuts\n", to_sort[i].second.c_str(), to_sort[i].first / 10);
    if (time_tracking.exists("upgrade"))
        text += "Note: time per level data comes from an upgraded game and may be incomplete.\n";
    text += "\n";
}

static void _sdump_xp_by_level(dump_params &par)
{
    string &text(par.text);

    vector<LevelXPInfo> all_info = you.get_all_xp_info(true);

    text +=
"Table legend:\n"
" A = Non-vault XP\n"
" B = Vault XP\n"
" C = Vault XP percentage of total XP\n"
" D = Non-vault monster count\n"
" E = Vault monster count\n"
" F = Vault count percentage of total count\n\n";

    text += "            ";
    text += "     A         B        C        D         E        F   \n";
    text += "            ";
    text += "+---------+---------+-------+---------+---------+-------\n";

    text += _sdump_level_xp_info(you.global_xp_info, "Total");

    text += "            ";
    text += "+---------+---------+-------+---------+---------+-------\n";

    for (const LevelXPInfo &mi : all_info)
        text += _sdump_level_xp_info(mi);

    text += "            ";
    text += "+---------+---------+-------+---------+---------+-------\n";

    text += "\n";
}

static void _sdump_newline(dump_params &par)
{
    par.text += "\n";
}

static void _sdump_separator(dump_params &par)
{
    par.text += string(79, '-') + "\n";
}

// Assume this is an arbitrary Lua function name, call the function and
// dump whatever it returns.
static void _sdump_lua(dump_params &par)
{
    string luatext;
    if (!clua.callfn(par.section.c_str(), ">s", &luatext)
        && !clua.error.empty())
    {
        par.text += "Lua dump error: " + clua.error + "\n";
    }
    else
        par.text += luatext;
}

string chardump_desc(const item_def& item)
{
    string desc = get_item_description(item, IDM_DUMP);
    string outs;

    outs.reserve(desc.length() + 32);

    const int indent = 3;

    if (desc.empty()) // always at least an empty line
        return "\n";

    while (!desc.empty())
    {
        outs += string(indent, ' ')
                  + wordwrap_line(desc, 79 - indent)
                  + "\n";
    }

    return outs;
}

static void _sdump_messages(dump_params &par)
{
    // A little message history:
    if (Options.dump_message_count > 0)
    {
        par.text += "Message History\n\n";
        par.text += get_last_messages(Options.dump_message_count);
    }
}

static void _sdump_screenshot(dump_params &par)
{
    par.text += screenshot();
    par.text += "\n\n";
}

static void _sdump_screenshots(dump_params &par)
{
    string &text(par.text);
    if (note_list.empty())
        return;

    text += "Illustrated notes\n\n";

    for (const Note &note : note_list)
    {
        if (note.hidden() || note.type != NOTE_USER_NOTE || note.screen.length() == 0)
            continue;

        text += note.screen;
        text += "\n";
        text += make_stringf("Turn %d on ", note.turn);
        text += note.place.describe() + ": ";
        text += note.name;
        text += "\n\n";
    }
}

static void _sdump_notes(dump_params &par)
{
    string &text(par.text);
    if (note_list.empty())
        return;

    text += "Notes\n";
    text += "Turn   | Place    | Note\n";
    text += "-------+----------+-------------------------------------------\n";
    for (const Note &note : note_list)
    {
        if (note.hidden())
            continue;

        string prefix = note.describe(true, true, false);
        string suffix = note.describe(false, false, true);
        if (suffix.empty())
            continue;
        int spaceleft = 80 - prefix.length() - 1; // Use 100 cols
        if (spaceleft <= 0)
            return;

        linebreak_string(suffix, spaceleft);
        vector<string> parts = split_string("\n", suffix);
        if (parts.empty()) // Disregard pure-whitespace notes.
            continue;

        text += prefix + parts[0] + "\n";
        for (unsigned int j = 1; j < parts.size(); ++j)
            text += string(prefix.length()-2, ' ') + string("| ") + parts[j] + "\n";
    }
    text += "\n";
}

static void _sdump_location(dump_params &par)
{
    if (you.depth == 0 && player_in_branch(BRANCH_DUNGEON))
        par.text += "You escaped";
    else if (par.se)
        par.text += "You were " + prep_branch_level_name();
    else
        par.text += "You are " + prep_branch_level_name();

    par.text += ".";
    par.text += "\n";
}

static void _sdump_religion(dump_params &par)
{
    string &text(par.text);
    if (!you_worship(GOD_NO_GOD))
    {
        if (par.se)
            text += "You worshipped ";
        else
            text += "You worship ";
        text += god_name(you.religion);
        text += ".\n";

        if (!you_worship(GOD_XOM))
        {
            if (!player_under_penance())
            {
                text += god_prayer_reaction();
                text += "\n";
            }
            else
            {
                string verb = par.se ? "was" : "is";

                text += uppercase_first(god_name(you.religion));
                text += " " + verb + " demanding penance.\n";
            }
        }
        else
        {
            if (par.se)
                text += "You were ";
            else
                text += "You are ";
            text += describe_xom_favour();
            text += "\n";
        }
    }
}

static bool _dump_item_origin(const item_def &item)
{
#define fs(x) (flags & (x))
    const int flags = Options.dump_item_origins;
    if (flags == IODS_EVERYTHING)
        return true;

    if (fs(IODS_ARTEFACTS)
        && is_artefact(item) && item.is_identified())
    {
        return true;
    }
    if (fs(IODS_EGO_ARMOUR) && item.base_type == OBJ_ARMOUR
        && item.is_identified())
    {
        const int spec_ench = get_armour_ego_type(item);
        return spec_ench != SPARM_NORMAL;
    }

    if (fs(IODS_EGO_WEAPON) && item.base_type == OBJ_WEAPONS
        && item.is_identified())
    {
        return get_weapon_brand(item) != SPWPN_NORMAL;
    }

    if (fs(IODS_JEWELLERY) && item.base_type == OBJ_JEWELLERY)
        return true;

    if (fs(IODS_STAVES) && item.base_type == OBJ_STAVES)
        return true;

    const int refpr = Options.dump_item_origin_price;
    if (refpr == -1)
        return false;
    return (int)item_value(item, false) >= refpr;
#undef fs
}

static void _sdump_inventory(dump_params &par)
{
    int i;

    string &text(par.text);

    int inv_class2[NUM_OBJECT_CLASSES] = { 0, };
    int inv_count = 0;

    for (const auto &item : you.inv)
    {
        if (item.defined())
        {
            // adds up number of each class in invent.
            inv_class2[item.base_type]++;
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
        text += "Inventory:\n\n";

        for (int obj = 0; obj < NUM_OBJECT_CLASSES; obj++)
        {
            i = inv_order[obj];

            if (inv_class2[i] == 0)
                continue;

            text += item_class_name(i);
            text += "\n";

            for (const auto &item : you.inv)
            {
                if (!item.defined() || item.base_type != i)
                    continue;

                text += " ";
                text += item.name(DESC_INVENTORY_EQUIP);

                inv_count--;

                if (origin_describable(item) && _dump_item_origin(item))
                    text += "\n" "   (" + origin_desc(item) + ")";

                if (is_dumpable_artefact(item))
                    text += chardump_desc(item);
                else
                    text += "\n";
            }
        }
    }
    text += "\n\n";
}

static void _sdump_skills(dump_params &par)
{
    string &text(par.text);

    text += "   Skills:\n";

    dump_skills(text);
    text += "\n";
    text += "\n";
}

static string spell_type_shortname(spschool spell_class, bool slash)
{
    string ret;

    if (slash)
        ret = "/";

    ret += spelltype_short_name(spell_class);

    return ret;
}

static void _sdump_spells(dump_params &par)
{
    string &text(par.text);

    string verb = par.se? "had" : "have";

    if (!you.has_mutation(MUT_INNATE_CASTER))
    {
        int spell_levels = player_spell_levels();

        if (spell_levels == 1)
            text += "You " + verb + " one spell level left.";
        else if (spell_levels == 0)
        {
            verb = par.se? "couldn't" : "cannot";

            text += "You " + verb + " memorise any spells.";
        }
        else
        {
            if (par.se)
                text += "You had ";
            else
                text += "You have ";
            text += make_stringf("%d spell levels left.", spell_levels);
        }

        text += "\n";
    }

    if (!you.spell_no)
    {
        verb = par.se? "didn't" : "don't";

        text += "You " + verb + " know any spells.\n";
    }
    else
    {
        verb = par.se? "knew" : "know";

        text += "You " + verb + " the following spells:\n\n";

        text += " Your Spells              Type           Power      Damage    Failure   Level" "\n";

        for (int j = 0; j < 52; j++)
        {
            const char letter = index_to_letter(j);
            const spell_type spell  = get_spell_by_letter(letter);

            if (spell != SPELL_NO_SPELL)
            {
                string spell_line;

                spell_line += letter;
                spell_line += " - ";
                spell_line += spell_title(spell);

                spell_line = chop_string(spell_line, 24);
                spell_line += "  ";

                bool already = false;

                for (const auto bit : spschools_type::range())
                {
                    if (spell_typematch(spell, bit))
                    {
                        spell_line += spell_type_shortname(bit, already);
                        already = true;
                    }
                }

                spell_line = chop_string(spell_line, 41);

                spell_line += spell_power_string(spell);

                spell_line = chop_string(spell_line, 52);

                const string spell_damage = spell_damage_string(spell);
                spell_line += spell_damage.length() ? spell_damage : "N/A";

                spell_line = chop_string(spell_line, 62);

                spell_line += failure_rate_to_string(raw_spell_fail(spell));

                spell_line = chop_string(spell_line, 74);

                spell_line += make_stringf("%d", spell_difficulty(spell));

                spell_line += "\n";

                text += spell_line;
            }
        }
        text += "\n";
    }

    if (!you.spell_library.count())
    {
        verb = par.se ? "was" : "is";
        text += "Your spell library " + verb + " empty.\n\n";
    }
    else
    {
        verb = par.se? "contained" : "contains";
        text += "Your spell library " + verb + " the following spells:\n\n";
        text += " Spells                   Type           Power      Damage    Failure   Level" "\n";

        auto const library = get_sorted_spell_list(true, false);

        for (const spell_type spell : library)
        {
            const bool memorisable = you_can_memorise(spell);

            string spell_line;

            spell_line += ' ';
            spell_line += spell_title(spell);

            spell_line = chop_string(spell_line, 24);
            spell_line += "  ";

            bool already = false;

            for (const auto bit : spschools_type::range())
            {
                if (spell_typematch(spell, bit))
                {
                    spell_line += spell_type_shortname(bit, already);
                    already = true;
                }
            }

            spell_line = chop_string(spell_line, 41);

            if (memorisable)
                spell_line += spell_power_string(spell);
            else
                spell_line += "Unusable";

            spell_line = chop_string(spell_line, 52);

            const string spell_damage = spell_damage_string(spell);
            spell_line += spell_damage.length() ? spell_damage : "N/A";

            spell_line = chop_string(spell_line, 62);

            if (memorisable)
                spell_line += failure_rate_to_string(raw_spell_fail(spell));
            else
                spell_line += "N/A";

            spell_line = chop_string(spell_line, 74);

            spell_line += make_stringf("%d", spell_difficulty(spell));

            spell_line += "\n";

            text += spell_line;
        }
        text += "\n\n";
    }
}

static void _sdump_kills(dump_params &par)
{
    par.text += you.kills.kill_info();
    par.text += "\n";
}

static string _sdump_kills_place_info(const PlaceInfo place_info, string name = "")
{
    string out;

    if (name.empty())
        name = place_info.short_name();

    unsigned int global_total_kills = 0;
    for (int i = 0; i < KC_NCATEGORIES; i++)
        global_total_kills += you.global_info.mon_kill_num[i];

    unsigned int total_kills = 0;
    for (int i = 0; i < KC_NCATEGORIES; i++)
        total_kills += place_info.mon_kill_num[i];

    // Skip places where nothing was killed.
    if (total_kills == 0)
        return "";

    float a, b, c, d, e, f;

    a = TO_PERCENT(total_kills, global_total_kills);
    b = TO_PERCENT(place_info.mon_kill_num[KC_YOU],
                   you.global_info.mon_kill_num[KC_YOU]);
    c = TO_PERCENT(place_info.mon_kill_num[KC_FRIENDLY],
                   you.global_info.mon_kill_num[KC_FRIENDLY]);
    d = TO_PERCENT(place_info.mon_kill_num[KC_OTHER],
                   you.global_info.mon_kill_num[KC_OTHER]);
    e = TO_PERCENT(place_info.mon_kill_exp,
                   you.global_info.mon_kill_exp);

    f = float(place_info.mon_kill_exp) / place_info.levels_seen;

    out =
        make_stringf("%14s | %5.1f | %5.1f | %5.1f | %5.1f | %5.1f |"
                     " %13.1f\n",
                     name.c_str(), a, b, c , d, e, f);

    return _denanify(out);
}

static void _sdump_kills_by_place(dump_params &par)
{
    string &text(par.text);

    const vector<PlaceInfo> all_visited = you.get_all_place_info(true);

    string result = "";

    string header =
    "Table legend:\n"
    " A = Kills in this place as a percentage of kills in the entire game.\n"
    " B = Kills by you in this place as a percentage of kills by you in\n"
    "     the entire game.\n"
    " C = Kills by friends in this place as a percentage of kills by\n"
    "     friends in the entire game.\n"
    " D = Other kills in this place as a percentage of other kills in the\n"
    "     entire game.\n"
    " E = Experience gained in this place as a percentage of experience\n"
    "     gained in the entire game.\n"
    " F = Experience gained in this place divided by the number of levels of\n"
    "     this place that you have seen.\n\n";

    header += "               ";
    header += "    A       B       C       D       E               F\n";
    header += "               ";
    header += "+-------+-------+-------+-------+-------+----------------------\n";

    string footer = "               ";
    footer += "+-------+-------+-------+-------+-------+----------------------\n";

    result += _sdump_kills_place_info(you.global_info, "Total");

    for (const PlaceInfo &pi : all_visited)
        result += _sdump_kills_place_info(pi);

    if (!result.empty())
        text += header + result + footer + "\n";
}

static void _sdump_overview(dump_params &par)
{
    string overview =
        formatted_string::parse_string(overview_description_string(false));
    trim_string(overview);
    linebreak_string(overview, 80);
    par.text += overview;
    par.text += "\n\n";
}

static void _sdump_hiscore(dump_params &par)
{
    if (!par.se)
        return;

    string hiscore = hiscores_format_single_long(*(par.se), true);
    trim_string(hiscore);
    par.text += hiscore;
    par.text += "\n\n";
}

static void _sdump_monster_list(dump_params &par)
{
    string monlist = mpr_monster_list(par.se);
    trim_string(monlist);
    while (!monlist.empty())
        par.text += wordwrap_line(monlist, 80) + "\n";
    par.text += "\n";
}

static void _sdump_vault_list(dump_params &par)
{
    if (par.full_id || par.se
#ifdef WIZARD
        || you.wizard || you.suppress_wizard
#endif
     )
    {
        par.text += "Levels and vault maps discovered:\n";
        par.text += dump_vault_maps();
        par.text += "\n";
    }
}

static bool _sort_by_first(pair<int, FixedVector<int, 28> > a,
                           pair<int, FixedVector<int, 28> > b)
{
    for (int i = 0; i < 27; i++)
    {
        if (a.second[i] > b.second[i])
            return true;
        else if (a.second[i] < b.second[i])
            return false;
    }
    return false;
}

static void _count_action(caction_type type, int subtype)
{
    pair<caction_type, int> pair(type, subtype);
    if (!you.action_count.count(pair))
        you.action_count[pair].init(0);
    you.action_count[pair][you.experience_level - 1]++;
}

/**
 * The alternate type is stored in the higher bytes.
 **/
void count_action(caction_type type, int subtype, int auxtype)
{
    ASSERT_RANGE(subtype, -32768, 32768);
    ASSERT_RANGE(auxtype, -32768, 32768);
    _count_action(type, caction_compound(subtype, auxtype));
}

int caction_compound(int subtype, int auxtype)
{
    ASSERT_RANGE(subtype, -32768, 32768);
    ASSERT_RANGE(auxtype, -32768, 32768);
    return (auxtype << 16) | (subtype & 0xFFFF);
}

/**
 * .first is the subtype; .second is the auxtype (-1 if none).
 **/
pair<int, int> caction_extract_types(int compound_subtype)
{
    return make_pair(int16_t(compound_subtype),
                     int16_t(compound_subtype >> 16));
}

static string _describe_action(caction_type type)
{
    switch (type)
    {
    case CACT_MELEE:
        return "Melee";
    case CACT_FIRE:
        return "Fire";
    case CACT_THROW:
        return "Throw";
    case CACT_ARMOUR:
        return "Armour";
    case CACT_BLOCK:
        return "Block";
    case CACT_DODGE:
        return "Dodge";
    case CACT_CAST:
        return "Cast";
    case CACT_INVOKE:
        return "Invoke";
    case CACT_ABIL:
        return "Ability";
    case CACT_EVOKE:
        return "Evoke";
    case CACT_USE:
        return "Use";
    case CACT_STAB:
        return "Stab";
#if TAG_MAJOR_VERSION == 34
    case CACT_EAT:
        return "Eat";
#endif
    case CACT_RIPOSTE:
        return "Riposte";
    case CACT_FORM:
        return "Form";
    default:
        return "Error";
    }
}

static const char* _stab_names[] =
{
    "Normal",
    "Distracted",
    "Confused",
    "Fleeing",
    "Invisible",
    "Held in net/web",
    "Petrifying", // could be nice to combine the two
    "Petrified",
    "Paralysed",
    "Sleeping",
    "Betrayed ally",
    "Blind",
};

static const char* _aux_attack_names[] =
{
    "No attack",
    "Constrict",
    "Kick",
    "Headbutt",
    "Peck",
    "Tailslap",
    "Touch",
    "Punch",
    "Bite",
    "Pseudopods",
    "Tentacles",
    "Maw",
    "Executioner Blades",
    "Fungal Fists",
};
COMPILE_CHECK(ARRAYSZ(_aux_attack_names) == NUM_UNARMED_ATTACKS);

static string _describe_action_subtype(caction_type type, int compound_subtype)
{
    pair<int, int> types = caction_extract_types(compound_subtype);
    int subtype = types.first;
    int auxtype = types.second;

    switch (type)
    {
    case CACT_THROW:
    {
        if (auxtype == OBJ_MISSILES)
            return uppercase_first(item_base_name(OBJ_MISSILES, subtype));
        else
            return "Other";
    }
    case CACT_MELEE:
    case CACT_FIRE:
    case CACT_RIPOSTE:
        if (subtype == -1)
        {
            if (auxtype == -1)
                return "Unarmed";
            else
            {
                ASSERT_RANGE(auxtype, 0, NUM_UNARMED_ATTACKS);
                return _aux_attack_names[auxtype];
            }
        }
        else if (subtype >= UNRAND_START)
        {
            // Paranoia: an artefact may lose its specialness.
            const char *tn = get_unrand_entry(subtype)->type_name;
            if (tn)
                return uppercase_first(tn);
            subtype = get_unrand_entry(subtype)->sub_type;
        }
        return uppercase_first(item_base_name(OBJ_WEAPONS, subtype));
    case CACT_ARMOUR:
        return (subtype == -1) ? "Skin"
               : uppercase_first(item_base_name(OBJ_ARMOUR, subtype));
    case CACT_BLOCK:
    {
        if (subtype > -1)
            return uppercase_first(item_base_name(OBJ_ARMOUR, subtype));
        switch (auxtype)
        {
        case BLOCK_OTHER:
            return "Other"; // non-shield block
        case BLOCK_REFLECT:
            return "Reflection";
        default:
            return "Error";
        }
    }
    case CACT_DODGE:
    {
        switch ((dodge_type)subtype)
        {
        case DODGE_EVASION:
            return "Dodged";
        case DODGE_REPEL:
            return "Repelled";
        default:
            return "Error";
        }
    }
    case CACT_CAST:
        return spell_title((spell_type)subtype);
    case CACT_INVOKE:
    case CACT_ABIL:
        return ability_name((ability_type)subtype, true);
    case CACT_EVOKE:
        if (subtype >= UNRAND_START && subtype <= UNRAND_LAST)
            return uppercase_first(get_unrand_entry(subtype)->name);

        if (auxtype > -1)
        {
            item_def dummy;
            dummy.base_type = (object_class_type)(auxtype);
            dummy.sub_type  = subtype;
            dummy.quantity  = 1;
            return uppercase_first(dummy.name(DESC_DBNAME, true));
        }

        switch ((evoc_type)subtype)
        {
        case EVOC_WAND:
            return "Wand";
#if TAG_MAJOR_VERSION == 34
        case EVOC_ROD:
            return "Rod";
        case EVOC_DECK:
            return "Deck";
        case EVOC_MISC:
            return "Miscellaneous";
        case EVOC_BUGGY_TOME:
            return "tome";
#endif
        default:
            return "Error";
        }
    case CACT_USE:
        return uppercase_first(base_type_string((object_class_type)subtype));
    case CACT_STAB:
        COMPILE_CHECK(ARRAYSZ(_stab_names) == NUM_STABS);
        ASSERT_RANGE(subtype, 1, NUM_STABS);
        return _stab_names[subtype];
    case CACT_FORM:
        return get_form((transformation)subtype)->short_name;
#if TAG_MAJOR_VERSION == 34
    case CACT_EAT:
        return "Removed food";
#endif
    default:
        return "Error";
    }
}

static void _sdump_action_counts(dump_params &par)
{
    if (you.action_count.empty())
        return;
    int max_lt = (min<int>(you.max_level, 27) - 1) / 3;

    // Don't show both a total and 1..3 when there's only one tier.
    if (max_lt)
        max_lt++;

    par.text += make_stringf("%-26s", "Action");
    for (int lt = 0; lt < max_lt; lt++)
        par.text += make_stringf(" | %2d-%2d", lt * 3 + 1, lt * 3 + 3);
    par.text += " || total\n" + string(27, '-');
    for (int lt = 0; lt < max_lt; lt++)
        par.text += "+-------";
    par.text += "++-------\n";

    for (int cact = 0; cact < NUM_CACTIONS; cact++)
    {
        vector<pair<int, FixedVector<int, 28> > > action_vec;
        for (const auto &entry : you.action_count)
        {
            if (entry.first.first != cact)
                continue;
            FixedVector<int, 28> v;
            v[27] = 0;
            for (int i = 0; i < 27; i++)
            {
                v[i] = entry.second[i];
                v[27] += v[i];
            }
            action_vec.emplace_back(entry.first.second, v);
        }
        sort(action_vec.begin(), action_vec.end(), _sort_by_first);

        for (auto ac = action_vec.begin(); ac != action_vec.end(); ++ac)
        {
            if (ac == action_vec.begin())
            {
                const string act = _describe_action(caction_type(cact));
                par.text += make_stringf("%7.7s: ", act.c_str());
            }
            else
                par.text.append(9, ' ');
            par.text += chop_string(_describe_action_subtype(caction_type(cact), ac->first), 17);
            for (int lt = 0; lt < max_lt; lt++)
            {
                int ltotal = 0;
                for (int i = lt * 3; i < lt * 3 + 3; i++)
                    ltotal += ac->second[i];
                if (ltotal)
                    par.text += make_stringf(" |%6d", ltotal);
                else
                    par.text += " |      ";
            }
            par.text += make_stringf(" ||%6d", ac->second[27]);
            par.text += "\n";
        }
    }
    par.text += "\n";
}

static void _sdump_skill_gains(dump_params &par)
{
    typedef map<int, int> XlToSkillLevelMap;
    map<skill_type, XlToSkillLevelMap> skill_gains;
    vector<skill_type> skill_order;
    int xl = 0;
    int max_xl = 0;
    for (const Note &note : note_list)
    {
        if (note.type == NOTE_XP_LEVEL_CHANGE)
            xl = note.first;
        else if (note.type == NOTE_GAIN_SKILL || note.type == NOTE_LOSE_SKILL)
        {
            skill_type skill = static_cast<skill_type>(note.first);
            int skill_level = note.second;
            if (skill_gains.find(skill) == skill_gains.end())
                skill_order.push_back(skill);
            skill_gains[skill][xl] = skill_level;
            max_xl = max(max_xl, xl);
        }
    }

    if (skill_order.empty())
        return;

    for (int i = 0; i < NUM_SKILLS; i++)
    {
        skill_type skill = static_cast<skill_type>(i);
        if (you.skill(skill, 10, true) > 0
            && skill_gains.find(skill) == skill_gains.end())
        {
            skill_order.push_back(skill);
        }
    }

    par.text += "Skill      XL: |";
    for (xl = 1; xl <= max_xl; xl++)
        par.text += make_stringf(" %2d", xl);
    par.text += " |\n";
    par.text += "---------------+";
    for (xl = 1; xl <= max_xl; xl++)
        par.text += "---";
    par.text += "-+-----\n";

    for (skill_type skill : skill_order)
    {
        par.text += make_stringf("%-14s |", skill_name(skill));
        const XlToSkillLevelMap &gains = skill_gains[skill];
        for (xl = 1; xl <= max_xl; xl++)
        {
            auto it = gains.find(xl);
            if (it != gains.end())
                par.text += make_stringf(" %2d", it->second);
            else
                par.text += "   ";
        }
        par.text += make_stringf(" | %4.1f\n",
                                 you.skill(skill, 10, true) * 0.1);
    }
    par.text += "\n";
}

static void _sdump_mutations(dump_params &par)
{
    string &text(par.text);

    if (you.how_mutated(true, false))
    {
        text += "\n";
        text += (formatted_string::parse_string(describe_mutations(false)));
        text += "\n\n";
    }
}

static void _sdump_apostles(dump_params &par)
{
    if (get_num_apostles() == 0)
        return;

    par.text += "Apostles: \n\n";

    for (int i = 1; i <= get_num_apostles(); ++i)
        par.text += formatted_string::parse_string(trimmed_string(apostle_short_description(i)) + "\n\n");

    par.text += "\n";
}

string morgue_directory()
{
    string dir = (!Options.morgue_dir.empty() ? Options.morgue_dir :
                  !SysEnv.crawl_dir.empty()   ? SysEnv.crawl_dir
                                              : "");

    if (!dir.empty() && dir[dir.length() - 1] != FILE_SEPARATOR)
        dir += FILE_SEPARATOR;

    return dir;
}

void dump_map(FILE *fp, bool debug, bool dist, bool log)
{
    if (debug)
    {
#ifdef COLOURED_DUMPS
        // Usage: make EXTERNAL_DEFINES="-DCOLOURED_DUMPS"
        // To read the dumps, cat them or use less -R.
        // ansi2html can be used to make html.

        fprintf(fp, "Vaults used:\n");
        for (size_t i = 0; i < env.level_vaults.size(); ++i)
        {
            const vault_placement &vp(*env.level_vaults[i]);
            fprintf(fp, "  \e[3%dm%s\e[0m at (%d,%d) size (%d,%d)\n",
                    6 - (int)i % 6, vp.map.name.c_str(),
                    vp.pos.x, vp.pos.y, vp.size.x, vp.size.y);
        }
        fprintf(fp, "  (bright = stacked, \e[37;1mwhite\e[0m = not in level_map_ids)\n");
        size_t last_nv = 0;
        int    last_v = 0;
#endif
        // Write the whole map out without checking for mappedness. Handy
        // for debugging level-generation issues.
        for (int y = 0; y < GYM; ++y)
        {
            for (int x = 0; x < GXM; ++x)
            {
#ifdef COLOURED_DUMPS
                size_t nv = 0;
                for (auto &vault : env.level_vaults)
                    if (vault->map.in_map(coord_def(x, y) - vault->pos))
                        nv++;

                int v = env.level_map_ids[x][y];
                if (v == INVALID_MAP_INDEX)
                    v = -1;
                if (nv != last_nv || v != last_v)
                {
                    if (nv)
                        fprintf(fp, "\e[%d;3%dm", nv != 1, 6 - v % 6);
                    else
                        fprintf(fp, "\e[0m");
                    last_nv = nv;
                    last_v = v;
                }
#endif
                if (dist && you.pos() == coord_def(x, y))
                    fputc('@', fp);
                else if (testbits(env.pgrid[x][y], FPROP_HIGHLIGHT))
                    fputc('?', fp);
                else if (dist && env.grid[x][y] == DNGN_FLOOR
                         && travel_point_distance[x][y] > 0
                         && travel_point_distance[x][y] < 10)
                {
                    fputc('0' + travel_point_distance[x][y], fp);
                }
                else if (env.grid[x][y] >= NUM_FEATURES)
                    fputc('!', fp);
                else
                {
                    fputs(OUTS(stringize_glyph(
                               get_feature_def(env.grid[x][y]).symbol())), fp);
                }
            }
            fputc('\n', fp);
#ifdef COLOURED_DUMPS
            last_v = 0; // force a colour code, because of less+libvte
#endif
        }
#ifdef COLOURED_DUMPS
        fprintf(fp, "\e[0m");
#endif
    }
    else
    {
        int min_x = GXM-1, max_x = 0, min_y = GYM-1, max_y = 0;

        for (int i = X_BOUND_1; i <= X_BOUND_2; i++)
            for (int j = Y_BOUND_1; j <= Y_BOUND_2; j++)
                if (env.map_knowledge[i][j].known())
                {
                    if (i > max_x)
                        max_x = i;
                    if (i < min_x)
                        min_x = i;
                    if (j > max_y)
                        max_y = j;
                    if (j < min_y)
                        min_y = j;
                }

        for (int y = min_y; y <= max_y; ++y)
        {
            for (int x = min_x; x <= max_x; ++x)
            {
                fputs(OUTS(stringize_glyph(
                           get_cell_glyph(coord_def(x, y)).ch)), fp);
            }

            fputc('\n', fp);
        }
    }

    // for debug use in scripts, e.g. placement.lua
    if (log)
    {
        string the_log = get_last_messages(NUM_STORED_MESSAGES, true);
        fprintf(fp, "\n%s", the_log.c_str());
    }
}

void dump_map(const char* fname, bool debug, bool dist, bool log)
{
    FILE* fp = fopen_replace(fname);
    if (!fp)
        return;

    dump_map(fp, debug, dist, log);

    fclose(fp);
}

static bool _write_dump(const string &fname, const dump_params &par, bool quiet)
{
    bool succeeded = false;

    string file_name = morgue_directory();

    file_name += strip_filename_unsafe_chars(fname);

    StashTrack.update_corpses();

    string stash_file_name;
    stash_file_name = file_name;
    stash_file_name += ".lst";
    StashTrack.dump(stash_file_name.c_str(), par.full_id);

    file_name += ".txt";
    FILE *handle = fopen_replace(file_name.c_str());

    dprf("File name: %s", file_name.c_str());

    if (handle != nullptr)
    {
        fputs(OUTS(par.text), handle);
        fclose(handle);
        succeeded = true;
        if (!quiet)
#ifdef DGAMELAUNCH
            mpr("Char dumped successfully.");
#else
            mprf("Char dumped to '%s'.", file_name.c_str());
#endif
    }
    else
        mprf(MSGCH_ERROR, "Error opening file '%s'", file_name.c_str());

    return succeeded;
}

static int _add_text(formatted_scroller &scr, const string &tag,
                     const string &text)
{
    int colour = menu_colour(text, "", tag);
    scr.add_formatted_string(formatted_string(text, colour));
    return colour;
}

void display_notes()
{
    formatted_scroller scr(FS_START_AT_END | FS_PREWRAPPED_TEXT);
    scr.set_more();
    const string tag = "notes";
    scr.set_tag(tag);
    _add_text(scr, tag, "Turn   | Place    | Note\n");
    for (const Note &note : note_list)
    {
        if (note.hidden())
            continue;
        string prefix = note.describe(true, true, false);
        string suffix = note.describe(false, false, true);
        if (suffix.empty())
            continue;
        int spaceleft = 80 - prefix.length() - 1; // Use 100 cols
        if (spaceleft <= 0)
            return;

        linebreak_string(suffix, spaceleft);
        vector<string> parts = split_string("\n", suffix);
        if (parts.empty()) // Disregard pure-whitespace notes.
            continue;

        auto colour = _add_text(scr, tag, prefix + parts[0] + "\n");
        for (unsigned int j = 1; j < parts.size(); ++j)
        {
            auto str = string(prefix.length()-2, ' ') + "| " + parts[j] + "\n";
            scr.add_formatted_string(formatted_string(str, colour));
        }
    }
    scr.show();
}

void display_char_dump()
{
    formatted_scroller scr(FS_PREWRAPPED_TEXT);
    scr.add_raw_text(_get_dump().text, false);
    scr.set_more();
    scr.set_tag("dump");
    scr.show();
}

#ifdef DGL_WHEREIS
///////////////////////////////////////////////////////////////////////////
// whereis player
void whereis_record(const xlog_fields &xl)
{
    const string file_name = morgue_directory()
                             + strip_filename_unsafe_chars(you.your_name)
                             + string(".where");

    if (FILE *handle = fopen_replace(file_name.c_str()))
    {
        // no need to bother with supporting ancient charsets for DGL
        fprintf(handle, "%s\n", xl.xlog_line().c_str());
        fclose(handle);
    }
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Turn timestamps
//
// For DGL installs, write a timestamp at regular intervals into a file in
// the morgue directory. The timestamp file is named
// "timestamp-<player>-<starttime>.ts". All timestamps are standard Unix
// time_t, but currently only the low 4 bytes are saved even on systems
// with 64-bit time_t.
//
// Timestamp files are append only, and Crawl will check and handle cases
// where a previous Crawl process crashed at a higher turn count on the same
// game.
//
// Having timestamps associated with the game allows for much easier seeking
// within Crawl ttyrecs by external tools such as FooTV.

#ifdef DGL_TURN_TIMESTAMPS

#include "syscalls.h"
#include <sys/stat.h>

// File-format version for timestamp files. Crawl will never append to a
const uint32_t DGL_TIMESTAMP_VERSION = 1;
const int VERSION_SIZE = sizeof(DGL_TIMESTAMP_VERSION);
const int TIMESTAMP_SIZE = sizeof(uint32_t);

// Returns the name of the timestamp file based on the morgue_dir,
// character name and the game start time.
static string _dgl_timestamp_filename()
{
    const string filename = "timestamp-" + you.your_name + "-"
                            + make_file_time(you.birth_time);
    return morgue_directory() + strip_filename_unsafe_chars(filename) + ".ts";
}

// Returns true if the given file exists and is not a timestamp file
// of a known version or truncated timestamp file.
static bool _dgl_unknown_timestamp_file(const string &filename)
{
    if (FILE *inh = fopen_u(filename.c_str(), "rb"))
    {
        reader r(inh);
        r.set_safe_read(true);
        try
        {
            const uint32_t file_version = unmarshallInt(r);
            fclose(inh);
            return file_version != DGL_TIMESTAMP_VERSION;
        }
        catch (short_read_exception &e)
        {
            // Empty file, or <4 bytes: remove file and use it.
            fclose(inh);
            // True (don't use) if we couldn't remove it, false if we could.
            return unlink_u(filename.c_str()) != 0;
        }
    }
    return false;
}

// Returns a filehandle to use to write turn timestamps, nullptr if
// timestamps should not be written.
static FILE *_dgl_timestamp_filehandle()
{
    static FILE *timestamp_file = nullptr;
    static bool opened_file = false;
    if (!opened_file)
    {
        opened_file = true;

        const string filename = _dgl_timestamp_filename();
        // First check if there's already a timestamp file. If it exists
        // but has a different version, we cannot safely modify it, so bail.
        if (!_dgl_unknown_timestamp_file(filename))
            timestamp_file = fopen_u(filename.c_str(), "ab");
    }
    return timestamp_file;
}

// Records a timestamp in the .ts file at the given offset. If no timestamp
// file exists, a new file will be created.
static void _dgl_record_timestamp(unsigned long file_offset, time_t time)
{
    static bool timestamp_first_write = true;
    if (FILE *ftimestamp = _dgl_timestamp_filehandle())
    {
        writer w(_dgl_timestamp_filename(), ftimestamp, true);
        if (timestamp_first_write)
        {
            unsigned long ts_size = file_size(ftimestamp);
            if (!ts_size)
            {
                marshallInt(w, DGL_TIMESTAMP_VERSION);
                ts_size += sizeof(DGL_TIMESTAMP_VERSION);
            }

            // It's possible that the file we want to write is already
            // larger than the offset we expect if the game previously
            // crashed. When the game crashes, turn count is
            // effectively rewound to the point of the last save. In
            // such cases, we should not add timestamps until we reach
            // the correct turn count again.
            if (ts_size && ts_size > file_offset)
                return;

            if (file_offset > ts_size)
            {
                const int backlog =
                    (file_offset - ts_size) / TIMESTAMP_SIZE;
                for (int i = 0; i < backlog; ++i)
                    marshallInt(w, 0);
            }

            timestamp_first_write = false;
        }
        fseek(ftimestamp, 0, SEEK_END);
        // [ds] FIXME: Eventually switch to 8 byte timestamps.
        marshallInt(w, static_cast<uint32_t>(time));
        fflush(ftimestamp);
    }
}

// Record timestamps every so many turns:
const int TIMESTAMP_TURN_INTERVAL = 100;
// Stop recording timestamps after this turncount.
const int TIMESTAMP_TURN_MAX = 500000;
static void _dgl_record_timestamp(int turn)
{
    if (turn && turn < TIMESTAMP_TURN_MAX && !(turn % TIMESTAMP_TURN_INTERVAL))
    {
        const time_t now = time(nullptr);
        const unsigned long offset =
            (VERSION_SIZE +
             (turn / TIMESTAMP_TURN_INTERVAL - 1) * TIMESTAMP_SIZE);
        _dgl_record_timestamp(offset, now);
    }
}

#endif

// Records a timestamp for the current player turn if appropriate.
void record_turn_timestamp()
{
#ifdef DGL_TURN_TIMESTAMPS
    if (crawl_state.need_save)
        _dgl_record_timestamp(you.num_turns);
#endif
}
