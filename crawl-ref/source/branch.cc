/*
 *  File:       branch.cc
 *  Created by: haranp on Wed Dec 20 20:08:15 2006 UTC
 */

#include "AppHdr.h"

#include "branch.h"
#include "externs.h"
#include "place.h"
#include "player.h"
#include "spl-transloc.h"
#include "traps.h"
#include "branch-data.h"

Branch& your_branch()
{
    return branches[you.where_are_you];
}

bool at_branch_bottom()
{
    return your_branch().depth == player_branch_depth();
}

level_id branch_entry_level(branch_type branch)
{
    // Hell and its subbranches need obnoxious special-casing:
    if (branch == BRANCH_VESTIBULE_OF_HELL)
    {
        return level_id(you.hell_branch,
                        subdungeon_depth(you.hell_branch, you.hell_exit));
    }
    else if (is_hell_subbranch(branch))
    {
        return level_id(BRANCH_VESTIBULE_OF_HELL, 1);
    }

    const branch_type parent = branches[branch].parent_branch;
    const int subdepth = branches[branch].startdepth;

    // This may be invalid if the branch doesn't exist this game --
    // it's the caller's job to check.
    return level_id(parent, subdepth);
}

static level_id find_parent_dungeon_level(level_id level)
{
    ASSERT(level.level_type == LEVEL_DUNGEON);
    if (!--level.depth)
        return branch_entry_level(level.branch);
    else
        return (level);
}

static level_id find_parent_for_current_level_area()
{
    ASSERT(you.level_type != LEVEL_DUNGEON);

    // Return the level_id saved in where_are_you and absdepth0:
    unwind_var<level_area_type> player_level_type(
        you.level_type, LEVEL_DUNGEON);
    return level_id::current();
}

level_id current_level_parent()
{
    level_id current(level_id::current());
    if (current.level_type == LEVEL_DUNGEON)
        return find_parent_dungeon_level(current);

    return find_parent_for_current_level_area();
}

bool is_hell_subbranch(branch_type branch)
{
    return (branch >= BRANCH_FIRST_HELL
            && branch <= BRANCH_LAST_HELL
            && branch != BRANCH_VESTIBULE_OF_HELL);
}

branch_type str_to_branch(const std::string &branch, branch_type err)
{
    for (int i = 0; i < NUM_BRANCHES; ++i)
        if (branches[i].abbrevname && branches[i].abbrevname == branch)
            return (static_cast<branch_type>(i));

    return (err);
}

int branch_ambient_noise(branch_type branch)
{
    return branches[branch].ambient_noise;
}

int current_level_ambient_noise()
{
    switch (you.level_type) {
    case LEVEL_DUNGEON:
        return branch_ambient_noise(you.where_are_you);
    case LEVEL_ABYSS:
        // should probably randomize this somehow - change it up when the
        // abyss shifts?
        return 0;
    case LEVEL_PANDEMONIUM:
        // same as abyss
        return 0;
    case LEVEL_PORTAL_VAULT:
        // should be overridable in vaults
        return 0;
    case LEVEL_LABYRINTH:
        return 0;
    default:
        dprf("unknown level type in current_level_ambient_noise: %d", you.level_type);
        return 0;
    }
}

static const char *level_type_names[] =
{
    "D", "Lab", "Abyss", "Pan", "Port"
};

const char *level_area_type_name(int level_type)
{
    if (level_type >= 0 && level_type < NUM_LEVEL_AREA_TYPES)
        return level_type_names[level_type];
    return ("");
}

level_area_type str_to_level_area_type(const std::string &s)
{
    for (int i = 0; i < NUM_LEVEL_AREA_TYPES; ++i)
        if (s == level_type_names[i])
            return (static_cast<level_area_type>(i));

    return (LEVEL_DUNGEON);
}

bool set_branch_flags(uint32_t flags, bool silent, branch_type branch)
{
    if (branch == NUM_BRANCHES)
        branch = you.where_are_you;

    bool could_control = allow_control_teleport(true);
    bool could_map     = player_in_mappable_area();

    uint32_t old_flags = branches[branch].branch_flags;
    branches[branch].branch_flags |= flags;

    bool can_control = allow_control_teleport(true);
    bool can_map     = player_in_mappable_area();

    if (you.level_type == LEVEL_DUNGEON && branch == you.where_are_you
        && could_control && !can_control && !silent)
    {
        mpr("You sense the appearance of a powerful magical force "
            "which warps space.", MSGCH_WARN);
    }

    if (you.level_type == LEVEL_DUNGEON && branch == you.where_are_you
        && could_map && !can_map && !silent)
    {
        mpr("A powerful force appears that prevents you from "
            "remembering where you've been.", MSGCH_WARN);
    }

    return (old_flags != branches[branch].branch_flags);
}

bool unset_branch_flags(uint32_t flags, bool silent, branch_type branch)
{
    if (branch == NUM_BRANCHES)
        branch = you.where_are_you;

    const bool could_control = allow_control_teleport(true);
    const bool could_map     = player_in_mappable_area();

    uint32_t old_flags = branches[branch].branch_flags;
    branches[branch].branch_flags &= ~flags;

    const bool can_control = allow_control_teleport(true);
    const bool can_map     = player_in_mappable_area();

    if (you.level_type == LEVEL_DUNGEON && branch == you.where_are_you
        && !could_control && can_control && !silent)
    {
        // Isn't really a "recovery", but I couldn't think of where
        // else to send it.
        mpr("Space seems to straighten in your vicinity.", MSGCH_RECOVERY);
    }

    if (you.level_type == LEVEL_DUNGEON && branch == you.where_are_you
        && !could_map && can_map && !silent)
    {
        // Isn't really a "recovery", but I couldn't think of where
        // else to send it.
        mpr("An oppressive force seems to lift.", MSGCH_RECOVERY);
    }

    return (old_flags != branches[branch].branch_flags);
}

uint32_t get_branch_flags(branch_type branch)
{
    if (branch == NUM_BRANCHES)
    {
        if (you.level_type != LEVEL_DUNGEON)
            return (0);

        branch = you.where_are_you;
    }

    return branches[branch].branch_flags;
}
