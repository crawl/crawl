/*
 *  File:       branch.cc
 *  Created by: haranp on Wed Dec 20 20:08:15 2006 UTC
 */

#include "AppHdr.h"

#include "branch.h"
#include "externs.h"
#include "mon-pick.h"
#include "place.h"
#include "player.h"
#include "spells3.h"
#include "traps.h"

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

bool set_branch_flags(unsigned long flags, bool silent,
                      branch_type branch)
{
    if (branch == NUM_BRANCHES)
        branch = you.where_are_you;

    bool could_control = allow_control_teleport(true);
    bool could_map     = player_in_mappable_area();

    unsigned long old_flags = branches[branch].branch_flags;
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

bool unset_branch_flags(unsigned long flags, bool silent,
                        branch_type branch)
{
    if (branch == NUM_BRANCHES)
        branch = you.where_are_you;

    const bool could_control = allow_control_teleport(true);
    const bool could_map     = player_in_mappable_area();

    unsigned long old_flags = branches[branch].branch_flags;
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

unsigned long get_branch_flags(branch_type branch)
{
    if (branch == NUM_BRANCHES)
    {
        if (you.level_type != LEVEL_DUNGEON)
            return (0);

        branch = you.where_are_you;
    }

    return branches[branch].branch_flags;
}

Branch branches[] = {
    // Branch struct:
    //  branch id, parent branch, mindepth, maxdepth, depth, startdepth,
    //  branch flags, level flags
    //  entry stairs, exit stairs, short name, long name, abbrev name
    //  entry message
    //  has_shops, has_uniques, floor colour, rock colour
    //  mons rarity function, mons level function
    //  num_traps_function, rand_trap_function, num_fogs_function, rand_fog_function
    //  altar chance (in %), travel shortcut, upstairs exit branch, dangerous branch end

    { BRANCH_MAIN_DUNGEON, BRANCH_MAIN_DUNGEON, -1, -1,
      BRANCH_DUNGEON_DEPTH, -1, 0, 0,
      NUM_FEATURES, NUM_FEATURES,  // sentinel values
      "Dungeon", "the Dungeon", "D",
      NULL,
      20, true, LIGHTGREY, BROWN,
      mons_standard_rare, mons_standard_level,
      NULL, NULL, NULL, NULL,
      8, 'D', false, false },

    { BRANCH_ECUMENICAL_TEMPLE, BRANCH_MAIN_DUNGEON, 4, 7, 1, 5, 0, 0,
      DNGN_ENTER_TEMPLE, DNGN_RETURN_FROM_TEMPLE,
      "Temple", "the Ecumenical Temple", "Temple",
      NULL,
      0, false, LIGHTGREY, LIGHTGREY,
      mons_standard_rare, mons_standard_level,
      traps_zero_number, NULL, NULL, NULL, // No traps in temple
      0, 'T', false, false },

    { BRANCH_ORCISH_MINES, BRANCH_MAIN_DUNGEON, 6, 11, 4, 6, 0, 0,
      DNGN_ENTER_ORCISH_MINES, DNGN_RETURN_FROM_ORCISH_MINES,
      "Orcish Mines", "the Orcish Mines", "Orc",
      NULL,
      30, true, BROWN, BROWN,
      mons_mineorc_rare, mons_mineorc_level,
      NULL, NULL, NULL, NULL,
      20, 'O', false, false },

    { BRANCH_ELVEN_HALLS, BRANCH_ORCISH_MINES, 3, 4, 7, 4, 0, 0,
      DNGN_ENTER_ELVEN_HALLS, DNGN_RETURN_FROM_ELVEN_HALLS,
      "Elven Halls", "the Elven Halls", "Elf",
      NULL,
      40, true, DARKGREY, LIGHTGREY,
      mons_hallelf_rare, mons_hallelf_level,
      NULL, NULL, NULL, NULL,
      8, 'E', false, true },

    { BRANCH_LAIR, BRANCH_MAIN_DUNGEON, 8, 13, 8, 8, 0, 0,
      DNGN_ENTER_LAIR, DNGN_RETURN_FROM_LAIR,
      "Lair", "the Lair of Beasts", "Lair",
      NULL,
      0, true, GREEN, BROWN,
      mons_lair_rare, mons_lair_level,
      NULL, NULL, NULL, NULL,
      5, 'L', false, false },

    { BRANCH_SWAMP, BRANCH_LAIR, 2, 5, 5, 3, BFLAG_ISLANDED, 0,
      DNGN_ENTER_SWAMP, DNGN_RETURN_FROM_SWAMP,
      "Swamp", "the Swamp", "Swamp",
      NULL,
      0, true, BROWN, BROWN,
      mons_swamp_rare, mons_swamp_level,
      NULL, NULL, NULL, NULL,
      0, 'S', false, true },

    { BRANCH_SHOALS, BRANCH_LAIR, 3, 6, 5, 4, BFLAG_ISLANDED, 0,
      DNGN_ENTER_SHOALS, DNGN_RETURN_FROM_SHOALS,
      "Shoals", "the Shoals", "Shoals",
      NULL,
      20, true, BROWN, BROWN,
      mons_shoals_rare, mons_shoals_level,
      NULL, NULL, NULL, NULL,
      0, 'A', false, true },

    { BRANCH_SLIME_PITS, BRANCH_LAIR, 6, 8, 6, 4, 0, 0,
      DNGN_ENTER_SLIME_PITS, DNGN_RETURN_FROM_SLIME_PITS,
      "Slime Pits", "the Pits of Slime", "Slime",
      NULL,
      0, true, GREEN, LIGHTGREEN,
      mons_pitslime_rare, mons_pitslime_level,
      NULL, random_trap_slime, NULL, NULL,
      5, 'M', false, true },

    { BRANCH_SNAKE_PIT, BRANCH_LAIR, 3, 6, 5, 7, 0, 0,
      DNGN_ENTER_SNAKE_PIT, DNGN_RETURN_FROM_SNAKE_PIT,
      "Snake Pit", "the Snake Pit", "Snake",
      NULL,
      20, true, LIGHTGREEN, YELLOW,
      mons_pitsnake_rare, mons_pitsnake_level,
      NULL, NULL, NULL, NULL,
      10, 'P', false, true },

    { BRANCH_HIVE, BRANCH_MAIN_DUNGEON, 11, 16, 2, 15, 0, 0,
      DNGN_ENTER_HIVE, DNGN_RETURN_FROM_HIVE,
      "Hive", "the Hive", "Hive",
      "You hear a buzzing sound coming from all directions.",
      0, false, YELLOW, BROWN,
      mons_hive_rare, mons_hive_level,
      NULL, NULL, NULL, NULL,
      0, 'H', false, true },

    { BRANCH_VAULTS, BRANCH_MAIN_DUNGEON, 14, 19, 8, 17, 0, 0,
      DNGN_ENTER_VAULTS, DNGN_RETURN_FROM_VAULTS,
      "Vaults", "the Vaults", "Vault",
      NULL,
      40, true, LIGHTGREY, BROWN,
      mons_standard_rare, mons_standard_level,
      NULL, NULL, NULL, NULL,
      5, 'V', false, true },


    { BRANCH_HALL_OF_BLADES, BRANCH_VAULTS, 4, 6, 1, 4, 0, 0,
      DNGN_ENTER_HALL_OF_BLADES, DNGN_RETURN_FROM_HALL_OF_BLADES,
      "Hall of Blades", "the Hall of Blades", "Blade",
      NULL,
      0, true, LIGHTGREY, LIGHTGREY,
      mons_hallblade_rare, mons_hallblade_level,
      NULL, NULL, NULL, NULL,
      0, 'B', false, false },

    { BRANCH_CRYPT, BRANCH_VAULTS, 2, 4, 5, 3, 0, 0,
      DNGN_ENTER_CRYPT, DNGN_RETURN_FROM_CRYPT,
      "Crypt", "the Crypt", "Crypt",
      NULL,
      0, true, LIGHTGREY, LIGHTGREY,
      mons_crypt_rare, mons_crypt_level,
      NULL, NULL, NULL, NULL,
      5, 'C', false, false },

    { BRANCH_TOMB, BRANCH_CRYPT, 2, 3, 3, 5,
      BFLAG_ISLANDED | BFLAG_NO_TELE_CONTROL, 0,
      DNGN_ENTER_TOMB, DNGN_RETURN_FROM_TOMB,
      "Tomb", "the Tomb of the Ancients", "Tomb",
      NULL,
      0, true, YELLOW, LIGHTGREY,
      mons_tomb_rare, mons_tomb_level,
      NULL, NULL, NULL, NULL,
      0, 'G', false, true },

    { BRANCH_VESTIBULE_OF_HELL, BRANCH_MAIN_DUNGEON, 21, 27, 1, -1, 0, 0,
      DNGN_ENTER_HELL, DNGN_EXIT_HELL, // sentinel
      "Hell", "the Vestibule of Hell", "Hell",
      NULL,
      0, true, LIGHTGREY, LIGHTGREY,
      mons_standard_rare, mons_standard_level,
      NULL, NULL, NULL, NULL,
      0, 'U', false, false },

    { BRANCH_DIS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_DIS, NUM_FEATURES, // sentinel
      "Dis", "the Iron City of Dis", "Dis",
      NULL,
      0, false, CYAN, CYAN,
      mons_dis_rare, mons_dis_level,
      NULL, NULL, NULL, NULL,
      0, 'I', true, true },

    { BRANCH_GEHENNA, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_GEHENNA, NUM_FEATURES, // sentinel
      "Gehenna", "Gehenna", "Geh",
      NULL,
      0, false, DARKGREY, RED,
      mons_gehenna_rare, mons_gehenna_level,
      NULL, NULL, NULL, NULL,
      0, 'N', true, true },

    { BRANCH_COCYTUS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_COCYTUS, NUM_FEATURES, // sentinel
      "Cocytus", "Cocytus", "Coc",
      NULL,
      0, false, LIGHTBLUE, LIGHTCYAN,
      mons_cocytus_rare, mons_cocytus_level,
      NULL, NULL, NULL, NULL,
      0, 'X', true, true },

    { BRANCH_TARTARUS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_TARTARUS, NUM_FEATURES, // sentinel
      "Tartarus", "Tartarus", "Tar",
      NULL,
      0, false, DARKGREY, DARKGREY,
      mons_tartarus_rare, mons_tartarus_level,
      NULL, NULL, NULL, NULL,
      0, 'Y', true, true },

    { BRANCH_HALL_OF_ZOT, BRANCH_MAIN_DUNGEON, 27, 27, 5, 27, BFLAG_HAS_ORB, 0,
      DNGN_ENTER_ZOT, DNGN_RETURN_FROM_ZOT,
      "Zot", "the Realm of Zot", "Zot",
      NULL,
      0, true, BLACK, BLACK,
      mons_hallzot_rare, mons_hallzot_level,
      NULL, NULL, NULL, NULL,
      1, 'Z', false, true },
};
