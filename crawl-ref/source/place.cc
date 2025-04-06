/**
 * @file
 * @brief Place related functions.
**/

#include "AppHdr.h"

#include "place.h"

#include "branch.h"
#include "libutil.h"
#include "player.h"

// Prepositional form of branch level name. For example, "in the
// Vestibule of Hell" or "on level 3 of the Dungeon".
string prep_branch_level_name(level_id id)
{
    string place = id.describe(true, true);
    if (!place.empty() && place != "Pandemonium")
        place[0] = tolower_safe(place[0]);
    return place.find("level") == 0 ? "on " + place
                                    : "in " + place;
}

bool single_level_branch(branch_type branch)
{
    return branch >= 0 && branch < NUM_BRANCHES
           && brdepth[branch] == 1;
}

int absdungeon_depth(branch_type branch, int subdepth)
{
    // Necropolis always counts as the same depth of whatever location it was
    // entered from.
    if (branch == BRANCH_NECROPOLIS)
    {
        // Protection against wizmode shenanigans
        if (you.level_stack.empty()
            || you.level_stack.back().id.branch == BRANCH_NECROPOLIS)
        {
            return 1;
        }
        return you.level_stack.back().id.absdepth();
    }
    else
        return branches[branch].absdepth + subdepth - 1;
}

vector<level_id> all_dungeon_ids()
{
    vector<level_id> out;
    for (branch_iterator it; it; ++it)
    {
        for (int depth = 1; depth <= brdepth[it->id]; depth++)
            out.emplace_back(it->id, depth);
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
