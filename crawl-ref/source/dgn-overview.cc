/**
 * @file
 * @brief Records location of stairs etc
**/

#include "AppHdr.h"

#include "dgn-overview.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "branch.h"
#include "cio.h"
#include "colour.h"
#include "coord.h"
#include "dgnevent.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "feature.h"
#include "files.h"
#include "map_knowledge.h"
#include "menu.h"
#include "message.h"
#include "religion.h"
#include "shopping.h"
#include "state.h"
#include "tagstring.h"
#include "terrain.h"
#include "travel.h"

typedef std::map<branch_type, std::set<level_id> > stair_map_type;
typedef std::map<level_pos, shop_type> shop_map_type;
typedef std::map<level_pos, god_type> altar_map_type;
typedef std::map<level_pos, portal_type> portal_map_type;
typedef std::map<level_pos, std::string> portal_vault_map_type;
typedef std::map<level_pos, std::string> portal_note_map_type;
typedef std::map<level_pos, uint8_t> portal_vault_colour_map_type;
typedef std::map<level_id, std::string> annotation_map_type;
typedef std::pair<std::string, level_id> monster_annotation;

stair_map_type stair_level;
shop_map_type shops_present;
altar_map_type altars_present;
portal_map_type portals_present;
portal_vault_map_type portal_vaults_present;
portal_note_map_type portal_vault_notes;
portal_vault_colour_map_type portal_vault_colours;
annotation_map_type level_annotations;
annotation_map_type level_exclusions;
annotation_map_type level_uniques;
std::set<monster_annotation> auto_unique_annotations;

static void _seen_altar(god_type god, const coord_def& pos);
static void _seen_staircase(const coord_def& pos);
static void _seen_other_thing(dungeon_feature_type which_thing,
                              const coord_def& pos);

static std::string _get_branches(bool display);
static std::string _get_altars(bool display);
static std::string _get_shops(bool display);
static std::string _get_portals();
static std::string _get_notes();
static std::string _print_altars_for_gods(const std::vector<god_type>& gods,
                                          bool print_unseen, bool display);
static const std::string _get_coloured_level_annotation(level_id li);

void overview_clear()
{
    stair_level.clear();
    shops_present.clear();
    altars_present.clear();
    portals_present.clear();
    portal_vaults_present.clear();
    portal_vault_notes.clear();
    portal_vault_colours.clear();
    level_annotations.clear();
    level_exclusions.clear();
    level_uniques.clear();
    auto_unique_annotations.clear();
}

void seen_notable_thing(dungeon_feature_type which_thing, const coord_def& pos)
{
    // Don't record in temporary terrain
    if (you.level_type != LEVEL_DUNGEON)
        return;

    const god_type god = feat_altar_god(which_thing);
    if (god != GOD_NO_GOD)
        _seen_altar(god, pos);
    else if (feat_is_branch_stairs(which_thing))
        _seen_staircase(pos);
    else
        _seen_other_thing(which_thing, pos);
}

bool move_notable_thing(const coord_def& orig, const coord_def& dest)
{
    ASSERT(in_bounds(orig) && in_bounds(dest));
    ASSERT(orig != dest);
    ASSERT(!is_notable_terrain(grd(dest)));

    if (!is_notable_terrain(grd(orig)))
        return (false);

    level_pos pos1(level_id::current(), orig);
    level_pos pos2(level_id::current(), dest);

    shops_present[pos2]         = shops_present[pos1];
    altars_present[pos2]        = altars_present[pos1];
    portals_present[pos2]       = portals_present[pos1];
    portal_vaults_present[pos2] = portal_vaults_present[pos1];
    portal_vault_notes[pos2]    = portal_vault_notes[pos1];
    portal_vault_colours[pos2]  = portal_vault_colours[pos1];

    unnotice_feature(pos1);

    return (true);
}

static dungeon_feature_type portal_to_feature(portal_type p)
{
    switch (p)
    {
    case PORTAL_LABYRINTH:   return DNGN_ENTER_LABYRINTH;
    case PORTAL_HELL:        return DNGN_ENTER_HELL;
    case PORTAL_ABYSS:       return DNGN_ENTER_ABYSS;
    case PORTAL_PANDEMONIUM: return DNGN_ENTER_PANDEMONIUM;
    default:                 return DNGN_FLOOR;
    }
}

static const char* portaltype_to_string(portal_type p)
{
    switch (p)
    {
    case PORTAL_LABYRINTH:   return "<cyan>Labyrinth:</cyan>";
    case PORTAL_HELL:        return "<red>Hell:</red>";
    case PORTAL_ABYSS:       return "<magenta>Abyss:</magenta>";
    case PORTAL_PANDEMONIUM: return "<blue>Pan:</blue>";
    default:                 return "<lightred>Buggy:</lightred>";
    }
}

static std::string shoptype_to_string(shop_type s)
{
    switch (s)
    {
    case SHOP_WEAPON:          return "(";
    case SHOP_WEAPON_ANTIQUE:  return "<yellow>(</yellow>";
    case SHOP_ARMOUR:          return "[";
    case SHOP_ARMOUR_ANTIQUE:  return "<yellow>[</yellow>";
    case SHOP_GENERAL:         return "*";
    case SHOP_GENERAL_ANTIQUE: return "<yellow>*</yellow>";
    case SHOP_JEWELLERY:       return "=";
    case SHOP_WAND:            return "/";
    case SHOP_BOOK:            return "+";
    case SHOP_FOOD:            return "%";
    case SHOP_DISTILLERY:      return "!";
    case SHOP_SCROLL:          return "?";
    default:                   return "x";
    }
}

inline static std::string place_desc(const level_pos &pos)
{
    return "[" + pos.id.describe(false, true) + "] ";
}

inline static std::string altar_description(god_type god)
{
    return feature_description(altar_for_god(god));
}

inline static std::string portal_description(portal_type portal)
{
    return feature_description(portal_to_feature(portal));
}

bool overview_knows_portal(dungeon_feature_type portal)
{
    for (portal_map_type::const_iterator pl_iter = portals_present.begin();
          pl_iter != portals_present.end(); ++pl_iter)
    {
        if (portal_to_feature(pl_iter->second) == portal)
            return (true);
    }
    return (false);
}

int overview_knows_num_portals(dungeon_feature_type portal)
{
    int num = 0;
    for (portal_map_type::const_iterator pl_iter = portals_present.begin();
          pl_iter != portals_present.end(); ++pl_iter)
    {
        if (portal_to_feature(pl_iter->second) == portal)
            num++;
    }

    return (num);
}

static std::string _portals_description_string()
{
    std::string disp;
    level_id    last_id;
    for (int cur_portal = PORTAL_NONE; cur_portal < NUM_PORTALS; ++cur_portal)
    {
        last_id.depth = 10000;
        portal_map_type::const_iterator ci_portals;
        for (ci_portals = portals_present.begin();
              ci_portals != portals_present.end();
              ++ci_portals)
        {
            // one line per region should be enough, they're all of
            // the form D:XX, except for labyrinth portals, of which
            // you would need 11 (at least) to have a problem.
            if (ci_portals->second == cur_portal)
            {
                if (last_id.depth == 10000)
                    disp += portaltype_to_string(ci_portals->second);

                if (ci_portals->first.id == last_id)
                    disp += '*';
                else
                {
                    disp += ' ';
                    disp += ci_portals->first.id.describe(false, true);
                }
                last_id = ci_portals->first.id;
            }
        }
        if (last_id.depth != 10000)
            disp += "\n";
    }
    return disp;
}

static std::string _portal_vaults_description_string()
{
    // Collect all the different portal vault entrance names and then
    // display them in alphabetical order.
    std::set<std::string>    vault_names_set;
    std::vector<std::string> vault_names_vec;

    portal_vault_map_type::const_iterator ci_portals;
    for (ci_portals = portal_vaults_present.begin();
         ci_portals != portal_vaults_present.end(); ++ci_portals)
    {
        vault_names_set.insert(ci_portals->second);
    }

    for (std::set<std::string>::iterator i = vault_names_set.begin();
         i != vault_names_set.end(); ++i)
    {
        vault_names_vec.push_back(*i);
    }
    std::sort(vault_names_vec.begin(), vault_names_vec.end());

    std::string disp;
    level_id    last_id;
    for (unsigned int i = 0; i < vault_names_vec.size(); i++)
    {
        last_id.depth = 10000;
        for (ci_portals = portal_vaults_present.begin();
             ci_portals != portal_vaults_present.end(); ++ci_portals)
        {
            // one line per region should be enough, they're all of
            // the form D:XX, except for labyrinth portals, of which
            // you would need 11 (at least) to have a problem.
            if (ci_portals->second == vault_names_vec[i])
            {
                if (last_id.depth == 10000)
                {
                    uint8_t col = portal_vault_colours[ci_portals->first];
                    disp += '<';
                    disp += colour_to_str(col) + '>';
                    disp += vault_names_vec[i];
                    disp += "</";
                    disp += colour_to_str(col) + '>';
                    disp += ':';
                }

                const level_id  lid   = ci_portals->first.id;
                const level_pos where = ci_portals->first;

                if (lid == last_id)
                {
                    if (!portal_vault_notes[where].empty())
                    {
                        disp += " (";
                        disp += portal_vault_notes[where];
                        disp += ")";
                    }
                    else
                        disp += '*';
                }
                else
                {
                    disp += ' ';
                    disp += lid.describe(false, true);

                    if (!portal_vault_notes[where].empty())
                    {
                        disp += " (";
                        disp += portal_vault_notes[where];
                        disp += ") ";
                    }
                }
                last_id = lid;
            }
        }
        if (last_id.depth != 10000)
            disp += "\n";
    }
    return disp;
}

// display: format for in-game display; !display: format for dump
std::string overview_description_string(bool display)
{
    std::string disp;

    disp += "                    <white>Dungeon Overview and Level Annotations</white>\n" ;
    disp += _get_branches(display);
    disp += _get_altars(display);
    disp += _get_shops(display);
    disp += _get_portals();
    disp += _get_notes();

    return disp.substr(0, disp.find_last_not_of('\n')+1);
}

// iterate through every dungeon branch, listing the ones which have been found
static std::string _get_seen_branches(bool display)
{
    int num_printed_branches = 1;
    char buffer[100];
    std::string disp;

    disp += "\n<green>Branches:</green>";
    if (display)
    {
        disp += " (use <white>G</white> to reach them and "
                "<white>?/B</white> for more information)";
    }
    disp += "\n";

    level_id dungeon_lid(branches[0].id, 0);
    dungeon_lid = find_deepest_explored(dungeon_lid);
    if (crawl_state.game_is_sprint())
    {
        snprintf(buffer, sizeof(buffer),
                        "<yellow>Dungeon</yellow> <darkgrey>(1/1)</darkgrey>");
    }
    else if (crawl_state.game_is_zotdef())
    {
        snprintf(buffer, sizeof(buffer),
                        "<yellow>Zot</yellow>     <darkgrey>(1/1)</darkgrey>");
    }
    else
    {
        snprintf(buffer, sizeof(buffer),
                dungeon_lid.depth < 10 ?
                        "<yellow>Dungeon</yellow> <darkgrey>(%d/27)</darkgrey>            " :
                        "<yellow>Dungeon</yellow> <darkgrey>(%d/27)</darkgrey>           ",
                dungeon_lid.depth);
    }
    disp += buffer;

    for (int i = BRANCH_FIRST_NON_DUNGEON; i < NUM_BRANCHES; i++)
    {
        const branch_type branch = branches[i].id;

        if (stair_level.find(branch) != stair_level.end())
        {
            level_id lid(branch, 0);
            lid = find_deepest_explored(lid);

            std::string entry_desc;
            for (std::set<level_id>::iterator it = stair_level[branch].begin();
                 it != stair_level[branch].end(); ++it)
            {
                entry_desc += " " + it->describe(false, true);
            }

            snprintf(buffer, sizeof buffer,
                "<yellow>%7s</yellow> <darkgrey>(%d/%d)</darkgrey>%s",
                     branches[branch].abbrevname,
                     lid.depth,
                     branches[branch].depth,
                     entry_desc.c_str());

            disp += buffer;
            num_printed_branches++;

            disp += (num_printed_branches % 3) == 0
                    ? "\n"
                    // Each branch entry takes up 26 spaces + 38 for tags.
                    : std::string(std::max<int>(64 - strlen(buffer), 0), ' ');
        }
    }

    if (num_printed_branches % 3 != 0)
        disp += "\n";
    return disp;
}

static std::string _get_unseen_branches()
{
    int num_printed_branches = 0;
    char buffer[100];
    std::string disp;

    /* see if we need to hide lair branches that don't exist */
    int seen_lair_branches = 0;
    for (int i = BRANCH_FIRST_NON_DUNGEON; i < NUM_BRANCHES; i++)
    {
        const branch_type branch = branches[i].id;

        if (!is_random_lair_subbranch(branch))
            continue;

        if (stair_level.find(branch) != stair_level.end())
            seen_lair_branches++;
    }

    for (int i = BRANCH_FIRST_NON_DUNGEON; i < NUM_BRANCHES; i++)
    {
        const branch_type branch = branches[i].id;

        if (seen_lair_branches >= 2 && is_random_lair_subbranch(branch))
            continue;

        if (i == BRANCH_VESTIBULE_OF_HELL)
            continue;

        if (branch_is_unfinished(branch))
            continue;

        if (stair_level.find(branch) == stair_level.end())
        {
            const branch_type parent_branch = branches[i].parent_branch;
            level_id lid(parent_branch, 0);
            lid = find_deepest_explored(lid);
            if (lid.depth >= branches[branch].mindepth)
            {
                if (branches[branch].mindepth != branches[branch].maxdepth)
                {
                    snprintf(buffer, sizeof buffer,
                        "<darkgrey>%6s: %s:%d-%d</darkgrey>",
                            branches[branch].abbrevname,
                            branches[parent_branch].abbrevname,
                            branches[branch].mindepth,
                            branches[branch].maxdepth);
                }
                else
                {
                    snprintf(buffer, sizeof buffer,
                        "<darkgrey>%6s: %s:%d</darkgrey>",
                            branches[branch].abbrevname,
                            branches[parent_branch].abbrevname,
                            branches[branch].mindepth);
                }

                disp += buffer;
                num_printed_branches++;

                disp += (num_printed_branches % 4) == 0
                        ? "\n"
                        // Each branch entry takes up 20 spaces
                        : std::string(20 + 21 - strlen(buffer), ' ');
            }
        }
    }

    if (num_printed_branches % 4 != 0)
        disp += "\n";
    return disp;
}

static std::string _get_branches(bool display)
{
    return _get_seen_branches(display) + _get_unseen_branches();
}

// iterate through every god and display their altar's discovery state by color
static std::string _get_altars(bool display)
{
    // Just wastes space for demigods.
    if (you.species == SP_DEMIGOD)
        return "";

    std::string disp;

    disp += "\n<green>Altars:</green>";
    if (display)
    {
        disp += " (use <white>Ctrl-F \"altar\"</white> to reach them and "
                "<white>?/G</white> for information about gods)";
    }
    disp += "\n";
    disp += _print_altars_for_gods(temple_god_list(), true, display);
    disp += _print_altars_for_gods(nontemple_god_list(), false, display);

    return disp;
}

// Loops through gods, printing their altar status by color.
static std::string _print_altars_for_gods(const std::vector<god_type>& gods,
                                          bool print_unseen, bool display)
{
    std::string disp;
    char buffer[100];
    int num_printed = 0;
    char const *colour;

    for (unsigned int cur_god = 0; cur_god < gods.size(); cur_god++)
    {
        const god_type god = gods[cur_god];

        // for each god, look through the notable altars list for a match
        bool has_altar_been_seen = false;
        for (altar_map_type::const_iterator na_iter = altars_present.begin();
             na_iter != altars_present.end(); ++na_iter)
        {
            if (na_iter->second == god)
            {
                has_altar_been_seen = true;
                break;
            }
        }

        // If dumping, only laundry list the seen gods
        if (!display)
        {
            if (has_altar_been_seen)
                disp += god_name(god, false) + "\n";
            continue;
        }

        colour = "darkgrey";
        if (has_altar_been_seen)
            colour = "white";
        // Good gods don't inflict penance unless they hate your god.
        if (you.penance[god]
            && (!is_good_god(god) || god_hates_your_god(god)))
            colour = (you.penance[god] > 10) ? "red" : "lightred";
        // Indicate good gods that you've abandoned, though.
        else if (you.penance[god])
            colour = "magenta";
        else if (you.religion == god)
            colour = "yellow";
        else if (god_likes_your_god(god) && has_altar_been_seen)
            colour = "brown";

        if (!print_unseen && !strcmp(colour, "darkgrey"))
            continue;

        if (is_unavailable_god(god))
            colour = "darkgrey";

        snprintf(buffer, sizeof buffer, "<%s>%s</%s>",
                 colour, god_name(god, false).c_str(), colour);
        disp += buffer;
        num_printed++;

        if (num_printed % 5 == 0)
            disp += "\n";
        else
            // manually aligning the god columns: five whitespaces between columns
            switch (num_printed % 5)
            {
            case 1: disp += std::string(14 - strwidth(god_name(god, false)), ' ');
                    break;
            case 2: disp += std::string(18 - strwidth(god_name(god, false)), ' ');
                    break;
            case 3: disp += std::string(13 - strwidth(god_name(god, false)), ' ');
                    break;
            case 4: disp += std::string(16 - strwidth(god_name(god, false)), ' ');
            }
    }

    if (num_printed > 0 && num_printed % 5 != 0)
        disp += "\n";
    return disp;
}

// iterate through all discovered shops, printing what level they are on.
static std::string _get_shops(bool display)
{
    std::string disp;
    level_id last_id;

    if (!shops_present.empty())
    {
        disp +="\n<green>Shops:</green>";
        if (display)
            disp += " (use <white>Ctrl-F \"shop\"</white> to reach them - yellow denotes antique shop)";
        disp += "\n";
    }
    last_id.depth = 10000;
    std::map<level_pos, shop_type>::const_iterator ci_shops;

    // There are at most 5 shops per level, plus 7 chars for the level
    // name, plus 4 for the spacing; that makes a total of 17
    // characters per shop.
    const int maxcolumn = get_number_of_cols() - 17;
    int column_count = 0;

    for (ci_shops = shops_present.begin();
         ci_shops != shops_present.end(); ++ci_shops)
    {
        if (ci_shops->first.id != last_id)
        {
            if (column_count > maxcolumn)
            {
                disp += "\n";
                column_count = 0;
            }
            else if (column_count != 0)
            {
                disp += "  ";
                column_count += 2;
            }
            disp += "<brown>";

            const std::string loc = ci_shops->first.id.describe(false, true);
            disp += loc;
            column_count += strwidth(loc);

            disp += ": ";
            disp += "</brown>";

            column_count += 2;

            last_id = ci_shops->first.id;
        }
        disp += shoptype_to_string(ci_shops->second);
        ++column_count;
    }

    if (!shops_present.empty())
        disp += "\n";

    return disp;
}

// Loop through found portals and display them
static std::string _get_portals()
{
    std::string disp;

    if (!portals_present.empty() || !portal_vaults_present.empty())
        disp += "\n<green>Portals:</green>\n";
    disp += _portals_description_string();
    disp += _portal_vaults_description_string();

    return disp;
}

// Loop through each branch, printing stored notes.
static std::string _get_notes()
{
    std::string disp;

    for (level_id_iterator i; i; ++i)
        if (!get_level_annotation(*i).empty())
            disp += _get_coloured_level_annotation(*i) + "\n";

    if (disp.empty())
        return disp;
    return "\n<green>Annotations</green>\n" + disp;
}

template <typename Z, typename Key>
inline static bool _find_erase(Z &map, const Key &k)
{
    if (map.find(k) != map.end())
    {
        map.erase(k);
        return (true);
    }
    return (false);
}

static bool _unnotice_portal(const level_pos &pos)
{
    return _find_erase(portals_present, pos);
}

static bool _unnotice_portal_vault(const level_pos &pos)
{
    (void) _find_erase(portal_vault_colours, pos);
    (void) _find_erase(portal_vault_notes, pos);
    return _find_erase(portal_vaults_present, pos);
}

static bool _unnotice_altar(const level_pos &pos)
{
    return _find_erase(altars_present, pos);
}

static bool _unnotice_shop(const level_pos &pos)
{
    return _find_erase(shops_present, pos);
}

static bool _unnotice_stair(const level_pos &pos)
{
    const dungeon_feature_type feat = grd(pos.pos);
    if (!feat_is_branch_stairs(feat))
        return false;

    for (int i = 0; i < NUM_BRANCHES; ++i)
        if (branches[i].entry_stairs == feat)
        {
            const branch_type br = static_cast<branch_type>(i);
            if (stair_level.find(br) != stair_level.end())
            {
                stair_level[br].erase(level_id::current());
                if (stair_level[br].empty())
                    stair_level.erase(br);
                return true;
            }
        }

    return false;
}

bool unnotice_feature(const level_pos &pos)
{
    StashTrack.remove_shop(pos);
    shopping_list.forget_pos(pos);
    return (_unnotice_portal(pos)
            || _unnotice_portal_vault(pos)
            || _unnotice_altar(pos)
            || _unnotice_shop(pos)
            || _unnotice_stair(pos));
}

void display_overview()
{
    clrscr();
    std::string disp = overview_description_string(true);
    linebreak_string(disp, get_number_of_cols());
    formatted_scroller(MF_EASY_EXIT | MF_ANYPRINTABLE | MF_NOSELECT,
                       disp).show();
    redraw_screen();
}

static void _seen_staircase(const coord_def& pos)
{
    // Only handles stairs, not gates or arches
    // Don't worry about:
    //   - stairs returning to dungeon - predictable
    //   - entrances to the hells - always in vestibule

    // If the branch has already been entered, then the new entry is obviously
    // a mimic, don't add it.
    const branch_type branch = get_branch_at(pos);
    if (!branch_entered(branch))
        stair_level[branch].insert(level_id::current());
}

// If player has seen an altar; record it.
static void _seen_altar(god_type god, const coord_def& pos)
{
    // Can't record in Abyss or Pan.
    if (you.level_type != LEVEL_DUNGEON)
        return;

    level_pos where(level_id::current(), pos);
    altars_present[where] = god;
}

static portal_type _feature_to_portal(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_ENTER_LABYRINTH:   return PORTAL_LABYRINTH;
    case DNGN_ENTER_HELL:        return PORTAL_HELL;
    case DNGN_ENTER_ABYSS:       return PORTAL_ABYSS;
    case DNGN_ENTER_PANDEMONIUM: return PORTAL_PANDEMONIUM;
    default:                     return PORTAL_NONE;
    }
}

// If player has seen any other thing; record it.
static void _seen_other_thing(dungeon_feature_type which_thing,
                              const coord_def& pos)
{
    level_pos where(level_id::current(), pos);

    switch (which_thing)
    {
    case DNGN_ENTER_SHOP:
        shops_present[where] = static_cast<shop_type>(get_shop(pos)->type);
        break;

    case DNGN_ENTER_PORTAL_VAULT:
    {
        std::string portal_name;

        portal_name = env.markers.property_at(pos, MAT_ANY, "overview");

        if (portal_name.empty())
            portal_name = env.markers.property_at(pos, MAT_ANY, "dstname");
        if (portal_name.empty())
            portal_name = env.markers.property_at(pos, MAT_ANY, "dst");
        if (portal_name.empty())
            portal_name = "buggy vault portal";

        portal_name = replace_all(portal_name, "_", " ");
        portal_vaults_present[where] = uppercase_first(portal_name);

        uint8_t col;
        if (env.grid_colours(pos) != BLACK)
            col = env.grid_colours(pos);
        else
            col = get_feature_def(which_thing).colour;

        portal_vault_colours[where] = element_colour(col, true);
        portal_vault_notes[where] =
            env.markers.property_at(pos, MAT_ANY, "overview_note");

        break;
    }

    default:
        const portal_type portal = _feature_to_portal(which_thing);
        // hell upstairs are never interesting
        if (portal == PORTAL_HELL && player_in_hell())
            break;
        if (portal != PORTAL_NONE)
            portals_present[where] = portal;
        break;
    }
}

void enter_branch(branch_type branch, level_id from)
{
    if (stair_level[branch].size() > 1)
    {
        stair_level[branch].clear();
        stair_level[branch].insert(from);
    }
}

////////////////////////////////////////////////////////////////////////

static void _update_unique_annotation(level_id level)
{
    std::string note = "";
    std::string sep = ", ";
    for (std::set<monster_annotation>::iterator i = auto_unique_annotations.begin();
         i != auto_unique_annotations.end(); ++i)
    {
        if (i->first.find(',') != std::string::npos)
            sep = "; ";
    }
    for (std::set<monster_annotation>::iterator i = auto_unique_annotations.begin();
         i != auto_unique_annotations.end(); ++i)
    {
        if (i->second == level)
        {
            if (note.length() > 0)
                note += sep;
            note += i->first;
        }
    }
    set_level_unique_annotation(note, level);
}

static std::string unique_name(monster* mons)
{
    std::string name = mons->name(DESC_PLAIN, true);
    if (mons->type == MONS_PLAYER_GHOST)
        name += ", " + short_ghost_description(mons, true);
    else
    {
        if (strstr(name.c_str(), "royal jelly")
            || strstr(name.c_str(), "Royal Jelly"))
            name = "Royal Jelly";
        if (strstr(name.c_str(), "Lernaean hydra"))
            name = "Lernaean hydra";
        if (strstr(name.c_str(), "Serpent of Hell"))
            name = "Serpent of Hell";
        if (strstr(name.c_str(), "Blork"))
            name = "Blork the orc";
    }
    return name;
}

void set_unique_annotation(monster* mons, const level_id level)
{
    // Abyss persists its denizens.
    if (level.level_type != LEVEL_DUNGEON && level.level_type != LEVEL_ABYSS)
        return;
    if (!mons_is_unique(mons->type)
        && !(mons->props.exists("original_was_unique")
            && mons->props["original_was_unique"].get_bool())
        && mons->type != MONS_PLAYER_GHOST)
    {
        return;
    }

    remove_unique_annotation(mons);
    auto_unique_annotations.insert(std::make_pair(
                unique_name(mons), level));
    _update_unique_annotation(level);
}

void remove_unique_annotation(monster* mons)
{
    std::set<level_id> affected_levels;
    std::string name = unique_name(mons);
    for (std::set<monster_annotation>::iterator i = auto_unique_annotations.begin();
         i != auto_unique_annotations.end();)
    {
        if (i->first == name)
        {
            affected_levels.insert(i->second);
            auto_unique_annotations.erase(i++);
        }
        else
            ++i;
    }
    for (std::set<level_id>::iterator i = affected_levels.begin();
         i != affected_levels.end(); ++i)
        _update_unique_annotation(*i);
}

void set_level_annotation(std::string str, level_id li)
{
    if (str.empty())
        level_annotations.erase(li);
    else
        level_annotations[li] = str;
}

void set_level_exclusion_annotation(std::string str, level_id li)
{
    if (str.empty())
    {
        clear_level_exclusion_annotation(li);
        return;
    }

    level_exclusions[li] = str;
}

void set_level_unique_annotation(std::string str, level_id li)
{
    if (str.empty())
        level_uniques.erase(li);
    else
        level_uniques[li] = str;
}

void clear_level_exclusion_annotation(level_id li)
{
    level_exclusions.erase(li);
}

std::string get_level_annotation(level_id li, bool skip_excl,
                                 bool skip_uniq, bool use_colour, int colour)
{
    annotation_map_type::const_iterator i = level_annotations.find(li);
    annotation_map_type::const_iterator j = level_exclusions.find(li);
    annotation_map_type::const_iterator k = level_uniques.find(li);

    std::string note = "";

    if (i != level_annotations.end())
    {
        if (use_colour)
            note += colour_string(i->second, colour);
        else
            note += i->second;
    }

    if (!skip_excl && j != level_exclusions.end())
    {
        if (note.length() > 0)
            note += ", ";
        note += j->second;
    }

    if (!skip_uniq && k != level_uniques.end())
    {
        if (note.length() > 0)
            note += ", ";
        note += k->second;
    }

    return note;
}

static const std::string _get_coloured_level_annotation(level_id li)
{
    std::string place = "<yellow>" + li.describe();
    place = replace_all(place, ":", "</yellow>:");
    if (place.find("</yellow>") == std::string::npos)
        place += "</yellow>";
    int col = level_annotation_has("!", li) ? LIGHTRED : MAGENTA;
    return place + " " + get_level_annotation(li, false, false, true, col);
}

bool level_annotation_has(std::string find, level_id li)
{
    std::string str = get_level_annotation(li);

    return (str.find(find) != std::string::npos);
}

void annotate_level()
{
    level_id li  = level_id::current();
    level_id li2 = level_id::current();

    if (feat_is_stair(grd(you.pos())))
    {
        li2 = level_id::get_next_level_id(you.pos());

        if (li2.level_type != LEVEL_DUNGEON || li2.depth <= 0)
            li2 = level_id::current();
    }

    if (you.level_type != LEVEL_DUNGEON && li2.level_type != LEVEL_DUNGEON)
    {
        mpr("You can't annotate this level.");
        return;
    }

    if (you.level_type != LEVEL_DUNGEON)
        li = li2;
    else if (li2 != level_id::current())
    {
        if (yesno("Annotate level on other end of current stairs?", true, 'n'))
            li = li2;
    }

    do_annotate(li);
}

void do_annotate(level_id& li)
{
    if (!get_level_annotation(li).empty())
    {
        mpr("Current level annotation: " +
            colour_string(get_level_annotation(li, true, true), LIGHTGREY),
            MSGCH_PROMPT);
    }

    const std::string prompt = "New annotation for " + li.describe()
                               + " (include '!' for warning): ";

    char buf[77];
    if (msgwin_get_line_autohist(prompt, buf, sizeof(buf)))
        return;

    if (*buf)
        level_annotations[li] = buf;
    else if (get_level_annotation(li, true).empty())
        canned_msg(MSG_OK);
    else if (yesno("Really clear the annotation?", true, 'n'))
    {
        mpr("Cleared.");
        level_annotations.erase(li);
    }
}

void marshallUniqueAnnotations(writer& outf)
{
    marshallShort(outf, auto_unique_annotations.size());
    for (std::set<monster_annotation>::iterator i = auto_unique_annotations.begin();
         i != auto_unique_annotations.end(); ++i)
    {
        marshallString(outf, i->first);
        i->second.save(outf);
    }
}

void unmarshallUniqueAnnotations(reader& inf)
{
    auto_unique_annotations.clear();
    int num_notes = unmarshallShort(inf);
    for (int i = 0; i < num_notes; ++i)
    {
        std::string name = unmarshallString(inf);
        level_id level;
        level.load(inf);
        auto_unique_annotations.insert(std::make_pair(name, level));
    }
}
