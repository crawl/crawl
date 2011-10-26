/**
 * @file
 * @brief Place related functions.
**/

#include "AppHdr.h"

#include "externs.h"

#include "branch.h"
#include "libutil.h"
#include "place.h"
#include "player.h"
#include "travel.h"

std::string short_place_name(level_id id)
{
    return id.describe();
}

branch_type place_branch(unsigned short place)
{
#if TAG_MAJOR_VERSION == 32
    place = upgrade_packed_place(place);
#endif
    return static_cast<branch_type>((place >> 8) & 0xFF);
}

int place_depth(unsigned short place)
{
#if TAG_MAJOR_VERSION == 32
    place = upgrade_packed_place(place);
#endif
    return place & 0xFF;
}

unsigned short get_packed_place(branch_type branch, int subdepth)
{
    return (static_cast<int>(branch) << 8) | (subdepth & 0xFF);
}

unsigned short get_packed_place()
{
    return get_packed_place(you.where_are_you,
                            subdungeon_depth(you.where_are_you, you.absdepth0));
}

#if TAG_MAJOR_VERSION == 32
unsigned short upgrade_packed_place(unsigned short place)
{
    // D:-1 ("Buggy Badlands") is used as a magic value by vault definitions
    if ((place & 0xFF) != 0xFF || place == 0xFF)
        return place;
    switch (static_cast<old_level_area_type>(place >> 8))
    {
    case OLD_LEVEL_DUNGEON:
        die("old LEVEL_DUNGEON with no depth");
    case OLD_LEVEL_LABYRINTH:
        return get_packed_place(BRANCH_LABYRINTH, 1);
    case OLD_LEVEL_ABYSS:
        return get_packed_place(BRANCH_ABYSS, 1);
    case OLD_LEVEL_PANDEMONIUM:
        return get_packed_place(BRANCH_PANDEMONIUM, 1);
    case OLD_LEVEL_PORTAL_VAULT:
        if (you.old_level_type_name_abbrev == "Spider")
            return get_packed_place(BRANCH_SPIDER_NEST, 1);
        else if (you.old_level_type_name_abbrev == "Bazaar")
            return get_packed_place(BRANCH_BAZAAR, 1);
        else if (you.old_level_type_name_abbrev == "Trove")
            return get_packed_place(BRANCH_TROVE, 1);
        else if (you.old_level_type_name_abbrev == "Sewer")
            return get_packed_place(BRANCH_SEWER, 1);
        else if (you.old_level_type_name_abbrev == "Ossuary")
            return get_packed_place(BRANCH_OSSUARY, 1);
        else if (you.old_level_type_name_abbrev == "Bailey")
            return get_packed_place(BRANCH_BAILEY, 1);
        else if (you.old_level_type_name_abbrev == "IceCv")
            return get_packed_place(BRANCH_ICE_CAVE, 1);
        else if (you.old_level_type_name_abbrev == "Volcano")
            return get_packed_place(BRANCH_VOLCANO, 1);
        else if (you.old_level_type_name_abbrev == "WizLab")
            return get_packed_place(BRANCH_WIZLAB, 1);
        else if (you.old_level_type_name_abbrev.find("Zig:"))
        {
            int zig_depth;
            if (sscanf(you.old_level_type_name_abbrev.c_str(), "Zig:%d", &zig_depth) == 1)
                return get_packed_place(BRANCH_ZIGGURAT, zig_depth);
        }
        // yeah, both cases can happen
        else if (you.old_level_type_name_abbrev == "ziggurat")
            return get_packed_place(BRANCH_ZIGGURAT, 1);
        die("unknown old portal branch: %s", you.old_level_type_name_abbrev.c_str());
        break;
    default:
        die("unknown old level type: %d", place >> 8);
    }
}
#endif

bool single_level_branch(branch_type branch)
{
    return (branch >= 0 && branch < NUM_BRANCHES
            && brdepth[branch] == 1);
}

std::string place_name(unsigned short place, bool long_name,
                        bool include_number)
{
    uint8_t branch = (place >> 8) & 0xFF;
    int lev = place & 0xFF;

    std::string result = (long_name ?
              branches[branch].longname : branches[branch].abbrevname);

    if (include_number && brdepth[branch] != 1)
    {
        if (long_name)
        {
            // decapitalise 'the'
            if (result.find("The") == 0)
                result[0] = 't';
            result = make_stringf("Level %d of %s",
                      lev, result.c_str());
        }
        else if (lev)
            result = make_stringf("%s:%d", result.c_str(), lev);
        else
            result = make_stringf("%s:$", result.c_str());
    }
    return result;
}

// Takes a packed 'place' and returns a compact stringified place name.
// XXX: This is done in several other places; a unified function to
//      describe places would be nice.
std::string short_place_name(unsigned short place)
{
    return place_name(place, false, true);
}

// Prepositional form of branch level name.  For example, "in the
// Abyss" or "on level 3 of the Main Dungeon".
std::string prep_branch_level_name(unsigned short packed_place)
{
    std::string place = place_name(packed_place, true, true);
    if (!place.empty() && place != "Pandemonium")
        place[0] = tolower(place[0]);
    return (place.find("level") == 0 ? "on " + place
                                     : "in " + place);
}

// Use current branch and depth
std::string prep_branch_level_name()
{
    return prep_branch_level_name(get_packed_place());
}

int absdungeon_depth(branch_type branch, int subdepth)
{
    if (branch >= BRANCH_VESTIBULE_OF_HELL && branch <= BRANCH_LAST_HELL)
        return subdepth + 27 - (branch == BRANCH_VESTIBULE_OF_HELL);
    else
    {
        --subdepth;
        while (branch != BRANCH_MAIN_DUNGEON && branch != NUM_BRANCHES)
        {
            subdepth += startdepth[branch];
            branch = branches[branch].parent_branch;
        }
    }
    return subdepth;
}

int subdungeon_depth(branch_type branch, int depth)
{
    return depth - absdungeon_depth(branch, 0);
}

int player_branch_depth()
{
    return subdungeon_depth(you.where_are_you, you.absdepth0);
}

// Returns true if exits from this type of level involve going upstairs.
bool branch_exits_up(branch_type branch)
{
    switch (branch)
    {
    case BRANCH_ZIGGURAT:
    case BRANCH_LABYRINTH:
    case BRANCH_BAZAAR:
    case BRANCH_TROVE:
    case BRANCH_SEWER:
    case BRANCH_OSSUARY:
    case BRANCH_BAILEY:
    case BRANCH_ICE_CAVE:
    case BRANCH_VOLCANO:
    case BRANCH_WIZLAB:
        return true;
    default:
        return false;
    }
}

bool branch_exits_down(branch_type branch)
{
    return (branch == BRANCH_PANDEMONIUM || branch == BRANCH_ABYSS);
}

bool branch_allows_followers(branch_type branch)
{
    return (is_connected_branch(branch) || branch == BRANCH_PANDEMONIUM);
}

bool branch_is_stash_trackable(branch_type branch)
{
    return (branch != BRANCH_ABYSS && branch != BRANCH_LABYRINTH);
}

std::vector<level_id> all_dungeon_ids()
{
    std::vector<level_id> out;
    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        const Branch &branch = branches[i];

        for (int depth = 1; depth <= brdepth[i]; depth++)
            out.push_back(level_id(branch.id, depth));
    }
    return (out);
}
