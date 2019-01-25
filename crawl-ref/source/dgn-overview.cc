/**
 * @file
 * @brief Records location of stairs etc
**/

#include "AppHdr.h"

#include "dgn-overview.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "branch.h"
#include "describe.h"
#include "env.h"
#include "feature.h"
#include "files.h"
#include "libutil.h"
#include "menu.h"
#include "message.h"
#include "mon-poly.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "scroller.h"
#include "stairs.h"
#include "stringutil.h"
#include "terrain.h"
#include "travel.h"
#include "unicode.h"

typedef map<branch_type, set<level_id> > stair_map_type;
typedef map<level_pos, shop_type> shop_map_type;
typedef map<level_pos, god_type> altar_map_type;
typedef map<level_pos, branch_type> portal_map_type;
typedef map<level_pos, string> portal_note_map_type;
typedef map<level_id, string> annotation_map_type;
typedef pair<string, level_id> monster_annotation;

stair_map_type stair_level;
shop_map_type shops_present;
altar_map_type altars_present;
portal_map_type portals_present;
portal_note_map_type portal_notes;
annotation_map_type level_annotations;
annotation_map_type level_exclusions;
annotation_map_type level_uniques;
// FIXME: this should really be a multiset, in case you get multiple
// ghosts with the same name, combo, and XL on the same level.
set<monster_annotation> auto_unique_annotations;

static void _seen_altar(god_type god, const coord_def& pos);
static void _seen_staircase(const coord_def& pos);
static void _seen_shop(const coord_def& pos);
static void _seen_portal(dungeon_feature_type feat, const coord_def& pos);

static string _get_branches(bool display);
static string _get_altars(bool display);
static string _get_shops(bool display);
static string _get_portals();
static string _get_notes();
static string _print_altars_for_gods(const vector<god_type>& gods,
                                     bool print_unseen, bool display);
static const string _get_coloured_level_annotation(level_id li);

void overview_clear()
{
    stair_level.clear();
    shops_present.clear();
    altars_present.clear();
    portals_present.clear();
    portal_notes.clear();
    level_annotations.clear();
    level_exclusions.clear();
    level_uniques.clear();
    auto_unique_annotations.clear();
}

void seen_notable_thing(dungeon_feature_type which_thing, const coord_def& pos)
{
    // Don't record in temporary terrain
    if (!player_in_connected_branch())
        return;

    const god_type god = feat_altar_god(which_thing);
    if (god != GOD_NO_GOD)
        _seen_altar(god, pos);
    else if (feat_is_branch_entrance(which_thing))
        _seen_staircase(pos);
    else if (which_thing == DNGN_ENTER_SHOP)
        _seen_shop(pos);
    else if (feat_is_gate(which_thing)) // overinclusive
        _seen_portal(which_thing, pos);
}

bool move_notable_thing(const coord_def& orig, const coord_def& dest)
{
    ASSERT_IN_BOUNDS(orig);
    ASSERT_IN_BOUNDS(dest);
    ASSERT(orig != dest);
    ASSERT(!is_notable_terrain(grd(dest)));

    if (!is_notable_terrain(grd(orig)))
        return false;

    level_pos pos1(level_id::current(), orig);
    level_pos pos2(level_id::current(), dest);

    if (shops_present.count(pos1))
        shops_present[pos2]         = shops_present[pos1];
    if (altars_present.count(pos1))
        altars_present[pos2]        = altars_present[pos1];
    if (portals_present.count(pos1))
        portals_present[pos2]       = portals_present[pos1];
    if (portal_notes.count(pos1))
        portal_notes[pos2]          = portal_notes[pos1];

    unnotice_feature(pos1);

    return true;
}

static string coloured_branch(branch_type br)
{
    if (br < 0 || br >= NUM_BRANCHES)
        return "<lightred>Buggy buglands</lightred>";

    return make_stringf("<yellow>%s</yellow>", branches[br].shortname);
}

static string shoptype_to_string(shop_type s)
{
    switch (s)
    {
    case SHOP_WEAPON:          return "<w>(</w>";
    case SHOP_WEAPON_ANTIQUE:  return "<yellow>(</yellow>";
    case SHOP_ARMOUR:          return "<w>[</w>";
    case SHOP_ARMOUR_ANTIQUE:  return "<yellow>[</yellow>";
    case SHOP_GENERAL:         return "<w>*</w>";
    case SHOP_GENERAL_ANTIQUE: return "<yellow>*</yellow>";
    case SHOP_JEWELLERY:       return "<w>=</w>";
    case SHOP_EVOKABLES:       return "<w>}</w>";
    case SHOP_BOOK:            return "<w>:</w>";
    case SHOP_FOOD:            return "<w>%</w>";
    case SHOP_DISTILLERY:      return "<w>!</w>";
    case SHOP_SCROLL:          return "<w>?</w>";
    default:                   return "<w>x</w>";
    }
}

bool overview_knows_portal(branch_type portal)
{
    for (const auto &entry : portals_present)
        if (entry.second == portal)
            return true;

    return false;
}

// Ever used only for Pan, Abyss and Hell.
int overview_knows_num_portals(dungeon_feature_type portal)
{
    int num = 0;
    for (const auto &entry : portals_present)
    {
        if (branches[entry.second].entry_stairs == portal)
            num++;
    }

    return num;
}

static string _portals_description_string()
{
    string disp;
    level_id    last_id;
    for (branch_iterator it; it; ++it)
    {
        last_id.depth = 10000;
        for (const auto &entry : portals_present)
        {
            // one line per region should be enough, they're all of the form
            // Branch:XX.
            if (entry.second == it->id)
            {
                if (last_id.depth == 10000)
                    disp += coloured_branch(entry.second)+ ":";

                if (entry.first.id == last_id)
                    disp += '*';
                else
                {
                    disp += ' ';
                    disp += entry.first.id.describe(false, true);
                }
                last_id = entry.first.id;

                // Portals notes (Trove price).
                const string note = portal_notes[entry.first];
                if (!note.empty())
                    disp += " (" + note + ")";
            }
        }
        if (last_id.depth != 10000)
            disp += "\n";
    }
    return disp;
}

// display: format for in-game display; !display: format for dump
string overview_description_string(bool display)
{
    string disp;

    disp += "                    <white>Dungeon Overview and Level Annotations</white>\n" ;
    disp += _get_branches(display);
    disp += _get_altars(display);
    disp += _get_shops(display);
    disp += _get_portals();
    disp += _get_notes();

    return disp.substr(0, disp.find_last_not_of('\n')+1);
}

// iterate through every dungeon branch, listing the ones which have been found
static string _get_seen_branches(bool display)
{
    // Each branch entry takes up 26 spaces + 38 for tags.
    const int width = 64;

    int num_printed_branches = 0;
    char buffer[100];
    string disp;

    disp += "\n<green>Branches:</green>";
    if (display)
    {
        disp += " (use <white>G</white> to reach them and "
                "<white>?/B</white> for more information)";
    }
    disp += "\n";

    for (branch_iterator it; it; ++it)
    {
        const branch_type branch = it->id;

        if (branch == BRANCH_ZIGGURAT)
            continue;

        if (branch == root_branch
            || stair_level.count(branch))
        {
            level_id lid(branch, 0);
            lid = find_deepest_explored(lid);

            string entry_desc;
            for (auto lvl : stair_level[branch])
                entry_desc += " " + lvl.describe(false, true);

            // "D" is a little too short here.
            const char *brname = (branch == BRANCH_DUNGEON
                                  ? it->shortname
                                  : it->abbrevname);

            snprintf(buffer, sizeof buffer,
                "<yellow>%*s</yellow> <darkgrey>(%d/%d)</darkgrey>%s",
                branch == root_branch ? -7 : 7,
                brname, lid.depth, brdepth[branch], entry_desc.c_str());

            disp += buffer;
            num_printed_branches++;

            disp += (num_printed_branches % 3) == 0
                    ? "\n"
                    : string(max<int>(width - strlen(buffer), 0), ' ');
        }
    }

    if (num_printed_branches % 3 != 0)
        disp += "\n";
    return disp;
}

static string _get_unseen_branches()
{
    int num_printed_branches = 0;
    char buffer[100];
    string disp;

    for (branch_iterator it; it; ++it)
    {
        if (it->id < BRANCH_FIRST_NON_DUNGEON)
            continue;

        const branch_type branch = it->id;
        if (!connected_branch_can_exist(branch))
            continue;

        if (branch == BRANCH_VESTIBULE || !is_connected_branch(branch))
            continue;

        if (branch_is_unfinished(branch))
            continue;

        if (!stair_level.count(branch))
        {
            const branch_type parent = it->parent_branch;
            // Root branches.
            if (parent == NUM_BRANCHES)
                continue;
            level_id lid(parent, 0);
            lid = find_deepest_explored(lid);
            if (lid.depth >= it->mindepth)
            {
                if (it->mindepth != it->maxdepth)
                {
                    snprintf(buffer, sizeof buffer,
                        "<darkgrey>%6s: %s:%d-%d</darkgrey>",
                            it->abbrevname,
                            branches[parent].abbrevname,
                            it->mindepth,
                            it->maxdepth);
                }
                else
                {
                    snprintf(buffer, sizeof buffer,
                        "<darkgrey>%6s: %s:%d</darkgrey>",
                            it->abbrevname,
                            branches[parent].abbrevname,
                            it->mindepth);
                }

                disp += buffer;
                num_printed_branches++;

                disp += (num_printed_branches % 4) == 0
                        ? "\n"
                        // Each branch entry takes up 20 spaces
                        : string(20 + 21 - strlen(buffer), ' ');
            }
        }
    }

    if (num_printed_branches % 4 != 0)
        disp += "\n";
    return disp;
}

static string _get_branches(bool display)
{
    return _get_seen_branches(display) + _get_unseen_branches();
}

// iterate through every god and display their altar's discovery state by colour
static string _get_altars(bool display)
{
    // Just wastes space for demigods.
    if (you.species == SP_DEMIGOD)
        return "";

    string disp;

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

// Loops through gods, printing their altar status by colour.
static string _print_altars_for_gods(const vector<god_type>& gods,
                                     bool print_unseen, bool display)
{
    string disp;
    char buffer[100];
    int num_printed = 0;
    char const *colour;
    const int columns = 4;

    for (const god_type god : gods)
    {
        // for each god, look through the notable altars list for a match
        bool has_altar_been_seen = false;
        for (const auto &entry : altars_present)
        {
            if (entry.second == god)
            {
                has_altar_been_seen = true;
                break;
            }
        }

        // If dumping, only laundry list the seen gods
        if (!display)
        {
            if (has_altar_been_seen)
                disp += uppercase_first(god_name(god, false)) + "\n";
            continue;
        }

        colour = "darkgrey";
        if (has_altar_been_seen)
            colour = "white";
        // Good gods don't inflict penance unless they hate your god.
        if (player_under_penance(god)
            && (xp_penance(god) || active_penance(god)))
        {
            colour = (you.penance[god] > 10) ? "red" : "lightred";
        }
        // Indicate good gods that you've abandoned, though.
        else if (player_under_penance(god))
            colour = "magenta";
        else if (you_worship(god))
            colour = "yellow";
        else if (god_likes_your_god(god) && has_altar_been_seen)
            colour = "brown";

        if (!print_unseen && !strcmp(colour, "darkgrey"))
            continue;

        if (is_unavailable_god(god))
            colour = "darkgrey";

        string disp_name = uppercase_first(god_name(god, false));
        if (god == GOD_GOZAG && !you_worship(GOD_GOZAG))
            disp_name += make_stringf(" ($%d)", gozag_service_fee());

        snprintf(buffer, sizeof buffer, "<%s>%s</%s>",
                 colour, disp_name.c_str(), colour);
        disp += buffer;
        num_printed++;

        if (num_printed % columns == 0)
            disp += "\n";
        else
            // manually aligning the god columns: ten spaces between columns
            switch (num_printed % columns)
            {
            case 1: disp += string(19 - strwidth(disp_name), ' ');
                    break;
            case 2: disp += string(23 - strwidth(disp_name), ' ');
                    break;
            case 3: disp += string(20 - strwidth(disp_name), ' ');
                    break;
            }
    }

    if (num_printed > 0 && num_printed % columns != 0)
        disp += "\n";
    return disp;
}

// iterate through all discovered shops, printing what level they are on.
static string _get_shops(bool display)
{
    string disp;
    level_id last_id;

    if (!shops_present.empty())
    {
        disp +="\n<green>Shops:</green>";
        if (display)
            disp += " (use <white>Ctrl-F \"shop\"</white> to reach them - yellow denotes antique shop)";
        disp += "\n";
    }
    last_id.depth = 10000;

    // There are at most 5 shops per level, plus up to 8 chars for the
    // level name, plus 4 for the spacing (3 as padding + 1 separating
    // items from level). That makes a total of 17 characters per shop:
    //       1...5....0....5..
    // "D:8 *   Vaults:2 **([+   D:24 +";
    const int maxcolumn = 79 - 17;
    int column_count = 0;

    for (const auto &entry : shops_present)
    {
        if (entry.first.id != last_id)
        {
            const bool existing = is_existing_level(entry.first.id);
            if (column_count > maxcolumn)
            {
                disp += "\n";
                column_count = 0;
            }
            else if (column_count != 0)
            {
                disp += "   ";
                column_count += 3;
            }
            disp += existing ? "<lightgrey>" : "<darkgrey>";

            const string loc = entry.first.id.describe(false, true);
            disp += loc;
            column_count += strwidth(loc);

            disp += " ";
            disp += existing ? "</lightgrey>" : "</darkgrey>";
            column_count += 1;

            last_id = entry.first.id;
        }
        disp += shoptype_to_string(entry.second);
        ++column_count;
    }

    if (!shops_present.empty())
        disp += "\n";

    return disp;
}

// Loop through found portals and display them
static string _get_portals()
{
    string disp;

    if (!portals_present.empty())
        disp += "\n<green>Portals:</green>\n";
    disp += _portals_description_string();

    return disp;
}

// Loop through each branch, printing stored notes.
static string _get_notes()
{
    string disp;

    for (branch_iterator it; it; ++it)
        for (int d = 1; d <= brdepth[it->id]; ++d)
        {
            level_id i(it->id, d);
            if (!get_level_annotation(i).empty())
                disp += _get_coloured_level_annotation(i) + "\n";
        }

    if (disp.empty())
        return disp;
    return "\n<green>Annotations:</green>\n" + disp;
}

template <typename Z, typename Key>
static inline bool _find_erase(Z &map, const Key &k)
{
    if (map.count(k))
    {
        map.erase(k);
        return true;
    }
    return false;
}

static bool _unnotice_portal(const level_pos &pos)
{
    (void) _find_erase(portal_notes, pos);
    return _find_erase(portals_present, pos);
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
    if (!feat_is_branch_entrance(feat))
        return false;

    for (branch_iterator it; it; ++it)
        if (it->entry_stairs == feat)
        {
            const branch_type br = it->id;
            if (stair_level.count(br))
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
    return _unnotice_portal(pos)
        || _unnotice_altar(pos)
        || _unnotice_shop(pos)
        || _unnotice_stair(pos);
}

void display_overview()
{
    string disp = overview_description_string(true);
    linebreak_string(disp, 80);
    int flags = FS_PREWRAPPED_TEXT; // TODO: add ANYPRINTABLE
    formatted_scroller(flags, disp).show();
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
    if (!player_in_connected_branch())
        return;

    level_pos where(level_id::current(), pos);
    altars_present[where] = god;
}

static void _seen_shop(const coord_def& pos)
{
    shops_present[level_pos(level_id::current(), pos)] = shop_at(pos)->type;
}

static void _seen_portal(dungeon_feature_type which_thing, const coord_def& pos)
{
    if (feat_is_portal_entrance(which_thing)
        || which_thing == DNGN_ENTER_ABYSS
        || which_thing == DNGN_ENTER_PANDEMONIUM
        || which_thing == DNGN_ENTER_HELL && !player_in_hell())
    {
        level_pos where(level_id::current(), pos);
        portals_present[where] = stair_destination(pos).branch;
        portal_notes[where] =
            env.markers.property_at(pos, MAT_ANY, "overview_note");
    }
}

#define SEEN_RUNED_DOOR_KEY "num_runed_doors"
#define SEEN_TRANSPORTER_KEY "num_transporters"

// Get the env prop key we use to track discoveries of this feature.
static const char *_get_tracked_feature_key(dungeon_feature_type feat)
{
    switch (feat)
    {
        case DNGN_RUNED_DOOR:
        case DNGN_RUNED_CLEAR_DOOR:
            return SEEN_RUNED_DOOR_KEY;
            break;
        case DNGN_TRANSPORTER:
            return SEEN_TRANSPORTER_KEY;
            break;
        default:
            die("Unknown tracked feature: %s", get_feature_def(feat).name);
            return nullptr;
    }
}

/**
 * Update the level annotations for a feature we track in travel cache.
 *
 * @param feat       The type of feature we're updating.
 * @param old_num    The previous count of the feature, so we can determine
 *                   whether we need to remove the annotation entirely.
 **/
static void _update_tracked_feature_annot(dungeon_feature_type feat,
                                          int old_num)
{
    const level_id li = level_id::current();
    const char *feat_key = _get_tracked_feature_key(feat);
    const int new_num = env.properties[feat_key];
    const char *feat_desc = get_feature_def(feat).name;
    const string new_string = make_stringf("%d %s%s", new_num, feat_desc,
                                           new_num == 1 ? "" : "s");
    const string old_string = make_stringf("%d %s%s", old_num, feat_desc,
                                           old_num == 1 ? "" : "s");

    //TODO: regexes
    if (old_num > 0 && new_num > 0)
    {
       level_annotations[li] = replace_all(level_annotations[li],
                                           old_string, new_string);
    }
    else if (old_num == 0)
    {
        if (!level_annotations[li].empty())
            level_annotations[li] += ", ";
        level_annotations[li] += new_string;
    }
    else if (new_num == 0)
    {
        level_annotations[li] = replace_all(level_annotations[li],
                                            ", " + old_string, "");
        level_annotations[li] = replace_all(level_annotations[li],
                                            old_string, "");
    }
}

/**
 * Update the count for a newly discovered feature whose discovery/exploration
 * we track for travel cache purposes.
 *
 * @param feat    The type of feature we've just seen.
 **/
void seen_tracked_feature(dungeon_feature_type feat)
{
    const char *feat_key = _get_tracked_feature_key(feat);

    if (!env.properties.exists(feat_key))
        env.properties[feat_key] = 0;

    _update_tracked_feature_annot(feat, env.properties[feat_key].get_int()++);
}

/**
 * Update the count for an explored feature whose discovery/exploration we track
 * for travel cache purposes.
 *
 * @param feat    The type of feature we've just explored.
 **/
void explored_tracked_feature(dungeon_feature_type feat)
{
    const char *feat_key = _get_tracked_feature_key(feat);
#if TAG_MAJOR_VERSION > 34
    ASSERT(env.properties.exists(feat_key));
#endif

    // Opening a runed door we haven't seen (because of door_vault, probably).
    if (env.properties[feat_key].get_int() == 0)
        return;

    _update_tracked_feature_annot(feat, env.properties[feat_key].get_int()--);
}

void enter_branch(branch_type branch, level_id from)
{
    if (stair_level[branch].size() > 1)
    {
        stair_level[branch].clear();
        stair_level[branch].insert(from);
    }
}

// Add an annotation on a level if we corrupt with Lugonu's ability
void mark_corrupted_level(level_id li)
{
    if (!level_annotations[li].empty())
        level_annotations[li] += ", ";
    level_annotations[li] += "corrupted";
}

////////////////////////////////////////////////////////////////////////

static void _update_unique_annotation(level_id level)
{
    string note = "";
    string sep = ", ";

    for (const auto &annot : auto_unique_annotations)
        if (annot.first.find(',') != string::npos)
            sep = "; ";

    for (const auto &annot : auto_unique_annotations)
    {
        if (annot.second == level)
        {
            if (note.length() > 0)
                note += sep;
            note += annot.first;
        }
    }
    if (note.empty())
        level_uniques.erase(level);
    else
        level_uniques[level] = note;
}

static string unique_name(monster* mons)
{
    string name = mons->name(DESC_PLAIN, true);
    if (mons->type == MONS_PLAYER_GHOST)
        name += ", " + short_ghost_description(mons, true);
    else
    {
        if (strstr(name.c_str(), "Royal Jelly"))
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
    if (!mons_is_or_was_unique(*mons)
        && mons->type != MONS_PLAYER_GHOST
        || testbits(mons->flags, MF_SPECTRALISED)
        || mons->is_illusion()
        || mons->props.exists("no_annotate")
            && mons->props["no_annotate"].get_bool())

    {
        return;
    }

    remove_unique_annotation(mons);
    auto_unique_annotations.insert(make_pair(unique_name(mons), level));
    _update_unique_annotation(level);
}

void remove_unique_annotation(monster* mons)
{
    if (mons->is_illusion()) // Fake monsters don't clear real annotations
        return;
    set<level_id> affected_levels;
    string name = unique_name(mons);
    for (auto i = auto_unique_annotations.begin();
         i != auto_unique_annotations.end();)
    {
        // Only remove player ghosts from the current level or that you can see
        // (e.g. following you on stairs): there may be a different ghost with
        // the same unique_name elsewhere.
        if ((mons->type != MONS_PLAYER_GHOST
             || i->second == level_id::current()
             || you.can_see(*mons) && testbits(mons->flags, MF_TAKING_STAIRS))
            && i->first == name)
        {
            affected_levels.insert(i->second);
            auto_unique_annotations.erase(i++);
        }
        else
            ++i;
    }

    for (auto lvl : affected_levels)
        _update_unique_annotation(lvl);
}

void set_level_exclusion_annotation(string str, level_id li)
{
    if (str.empty())
    {
        clear_level_exclusion_annotation(li);
        return;
    }

    level_exclusions[li] = str;
}

void clear_level_exclusion_annotation(level_id li)
{
    level_exclusions.erase(li);
}

string get_level_annotation(level_id li, bool skip_excl, bool skip_uniq,
                            bool use_colour, int colour)
{
    string note = "";

    if (string *ann = map_find(level_annotations, li))
        note += use_colour ? colour_string(*ann, colour) : *ann;

    if (!skip_excl)
        if (string *excl = map_find(level_exclusions, li))
        {
            if (note.length() > 0)
                note += ", ";
            note += *excl;
        }

    if (!skip_uniq)
        if (string *uniq = map_find(level_uniques, li))
        {
            if (note.length() > 0)
                note += ", ";
            note += *uniq;
        }

    return note;
}

static const string _get_coloured_level_annotation(level_id li)
{
    string place = "<yellow>" + li.describe() + "</yellow>";
    int col = level_annotation_has("!", li) ? LIGHTRED : WHITE;
    return place + " " + get_level_annotation(li, false, false, true, col);
}

bool level_annotation_has(string find, level_id li)
{
    string str = get_level_annotation(li);

    return str.find(find) != string::npos;
}

void annotate_level()
{
    level_id li  = level_id::current();
    level_id li2 = level_id::current();

    if (feat_is_stair(grd(you.pos())))
    {
        li2 = level_id::get_next_level_id(you.pos());

        if (li2.depth <= 0)
            li2 = level_id::current();
    }

    if (li2 != level_id::current())
    {
        if (yesno("Annotate level on other end of current stairs?", true, 'n'))
            li = li2;
    }

    do_annotate(li);
}

void do_annotate(level_id& li)
{
    string old = get_level_annotation(li, true, true);
    if (!old.empty())
    {
        mprf(MSGCH_PROMPT, "Current level annotation: <lightgrey>%s</lightgrey>",
             old.c_str());
    }

    const string prompt = "New annotation for " + li.describe()
                          + " (include '!' for warning): ";

    char buf[77];
    if (msgwin_get_line_autohist(prompt, buf, sizeof(buf), old))
        canned_msg(MSG_OK);
    else if (old == buf)
        canned_msg(MSG_OK);
    else if (*buf)
        level_annotations[li] = buf;
    else
    {
        mpr("Cleared annotation.");
        level_annotations.erase(li);
    }
}

void clear_level_annotations(level_id li)
{
    level_annotations.erase(li);

    // Abyss persists its denizens.
    if (li == BRANCH_ABYSS)
        return;

    for (auto i = auto_unique_annotations.begin(), next = i;
         i != auto_unique_annotations.end(); i = next)
    {
        next = i;
        ++next;
        if (i->second == li)
            auto_unique_annotations.erase(i);
    }
    level_uniques.erase(li);
}

void marshallUniqueAnnotations(writer& outf)
{
    marshallShort(outf, auto_unique_annotations.size());
    for (const auto &annot : auto_unique_annotations)
    {
        marshallString(outf, annot.first);
        annot.second.save(outf);
    }
}

void unmarshallUniqueAnnotations(reader& inf)
{
    auto_unique_annotations.clear();
    int num_notes = unmarshallShort(inf);
    for (int i = 0; i < num_notes; ++i)
    {
        string name = unmarshallString(inf);
        level_id level;
        level.load(inf);
        auto_unique_annotations.insert(make_pair(name, level));
    }
}

/**
 * Can the player encounter the given connected branch, given their
 * knowledge of which have been seen so far?
 * @param br A connected branch.
 * @returns True if the branch can exist, false otherwise.
*/
bool connected_branch_can_exist(branch_type br)
{
    if (br == BRANCH_SPIDER && stair_level.count(BRANCH_SNAKE)
        || br == BRANCH_SNAKE && stair_level.count(BRANCH_SPIDER)
        || br == BRANCH_SWAMP && stair_level.count(BRANCH_SHOALS)
        || br == BRANCH_SHOALS && stair_level.count(BRANCH_SWAMP))
    {
        return false;
    }

    return true;
}
