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

string short_place_name(level_id id)
{
    return id.describe();
}

branch_type place_branch(unsigned short place)
{
    return static_cast<branch_type>((place >> 8) & 0xFF);
}

int place_depth(unsigned short place)
{
    return (int8_t)(place & 0xFF);
}

unsigned short get_packed_place(branch_type branch, int subdepth)
{
    return (static_cast<int>(branch) << 8) | (subdepth & 0xFF);
}

unsigned short get_packed_place()
{
    return get_packed_place(you.where_are_you, you.depth);
}

bool single_level_branch(branch_type branch)
{
    return branch >= 0 && branch < NUM_BRANCHES
           && brdepth[branch] == 1;
}

string place_name(unsigned short place, bool long_name, bool include_number)
{
    branch_type branch = static_cast<branch_type>((place >> 8) & 0xFF);
    int lev = place & 0xFF;
    ASSERT(branch < NUM_BRANCHES);

    string result = (long_name ? branches[branch].longname
                               : branches[branch].abbrevname);

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
string short_place_name(unsigned short place)
{
    return place_name(place, false, true);
}

// Prepositional form of branch level name.  For example, "in the
// Abyss" or "on level 3 of the Main Dungeon".
string prep_branch_level_name(unsigned short packed_place)
{
    string place = place_name(packed_place, true, true);
    if (!place.empty() && place != "Pandemonium")
        place[0] = tolower(place[0]);
    return place.find("level") == 0 ? "on " + place
                                    : "in " + place;
}

// Use current branch and depth
string prep_branch_level_name()
{
    return prep_branch_level_name(get_packed_place());
}

int absdungeon_depth(branch_type branch, int subdepth)
{
    return branches[branch].absdepth + subdepth - 1;
}

bool branch_allows_followers(branch_type branch)
{
    return is_connected_branch(branch) || branch == BRANCH_PANDEMONIUM;
}

vector<level_id> all_dungeon_ids()
{
    vector<level_id> out;
    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        const Branch &branch = branches[i];

        for (int depth = 1; depth <= brdepth[i]; depth++)
            out.push_back(level_id(branch.id, depth));
    }
    return out;
}

bool is_level_on_stack(level_id lev)
{
    for (int i = you.level_stack.size() - 1; i >= 0; i--)
        if (you.level_stack[i].id == lev)
            return true;

    return false;
}
