/*
 *  File:       branch.cc
 *  Created by: haranp on Wed Dec 20 20:08:15 2006 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "branch.h"
#include "cloud.h"
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
        && you.skills[SK_TRANSLOCATIONS] > 0
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
        && you.skills[SK_TRANSLOCATIONS] > 0
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

    { BRANCH_MAIN_DUNGEON, BRANCH_MAIN_DUNGEON, 27, -1, 0, 0,
      NUM_FEATURES, NUM_FEATURES,  // sentinel values
      "Dungeon", "the Dungeon", "D",
      NULL,
      true, true, LIGHTGREY, BROWN,
      mons_standard_rare, mons_standard_level,
      NULL, NULL, NULL, NULL,
      8, 'D', false, false },

    { BRANCH_ECUMENICAL_TEMPLE, BRANCH_MAIN_DUNGEON, 1, 5, 0, 0,
      DNGN_ENTER_TEMPLE, DNGN_RETURN_FROM_TEMPLE,
      "Temple", "the Ecumenical Temple", "Temple",
      NULL,
      false, false, LIGHTGREY, LIGHTGREY,
      mons_standard_rare, mons_standard_level,
      traps_zero_number, NULL, NULL, NULL, // No traps in temple
      0, 'T', false, false },

    { BRANCH_ORCISH_MINES, BRANCH_MAIN_DUNGEON, 4, 6, 0, 0,
      DNGN_ENTER_ORCISH_MINES, DNGN_RETURN_FROM_ORCISH_MINES,
      "Orcish Mines", "the Orcish Mines", "Orc",
      NULL,
      true, false, BROWN, BROWN,
      mons_mineorc_rare, mons_mineorc_level,
      NULL, NULL, NULL, NULL,
      20, 'O', false, false },

    { BRANCH_ELVEN_HALLS, BRANCH_ORCISH_MINES, 7, 4, 0, 0,
      DNGN_ENTER_ELVEN_HALLS, DNGN_RETURN_FROM_ELVEN_HALLS,
      "Elven Halls", "the Elven Halls", "Elf",
      NULL,
      true, true, DARKGREY, LIGHTGREY,
      mons_hallelf_rare, mons_hallelf_level,
      NULL, NULL, NULL, NULL,
      8, 'E', false, true },

    { BRANCH_LAIR, BRANCH_MAIN_DUNGEON, 10, 8, 0, 0,
      DNGN_ENTER_LAIR, DNGN_RETURN_FROM_LAIR,
      "Lair", "the Lair of Beasts", "Lair",
      NULL,
      true, false, GREEN, BROWN,
      mons_lair_rare, mons_lair_level,
      NULL, NULL, NULL, NULL,
      5, 'L', false, false },

    { BRANCH_SWAMP, BRANCH_LAIR, 5, 3, BFLAG_ISLANDED, 0,
      DNGN_ENTER_SWAMP, DNGN_RETURN_FROM_SWAMP,
      "Swamp", "the Swamp", "Swamp",
      NULL,
      true, true, BROWN, BROWN,
      mons_swamp_rare, mons_swamp_level,
      NULL, NULL, NULL, NULL,
      0, 'S', false, true },

    { BRANCH_SHOALS, BRANCH_LAIR, 5, 4, BFLAG_ISLANDED, 0,
      DNGN_ENTER_SHOALS, DNGN_RETURN_FROM_SHOALS,
      "Shoals", "the Shoals", "Shoal",
      NULL,
      true, true, BROWN, BROWN,
      mons_shoals_rare, mons_shoals_level,
      NULL, NULL, NULL, NULL,
      0, 'A', false, true },

    { BRANCH_SLIME_PITS, BRANCH_LAIR, 6, 4, 0, 0,
      DNGN_ENTER_SLIME_PITS, DNGN_RETURN_FROM_SLIME_PITS,
      "Slime Pits", "the Pits of Slime", "Slime",
      NULL,
      false, true, GREEN, LIGHTGREEN,
      mons_pitslime_rare, mons_pitslime_level,
      NULL, NULL, NULL, NULL,
      5, 'M', false, true },

    { BRANCH_SNAKE_PIT, BRANCH_LAIR, 5, 7, 0, 0,
      DNGN_ENTER_SNAKE_PIT, DNGN_RETURN_FROM_SNAKE_PIT,
      "Snake Pit", "the Snake Pit", "Snake",
      NULL,
      true, true, LIGHTGREEN, YELLOW,
      mons_pitsnake_rare, mons_pitsnake_level,
      NULL, NULL, NULL, NULL,
      10, 'P', false, true },

    { BRANCH_HIVE, BRANCH_MAIN_DUNGEON, 2, 15, 0, 0,
      DNGN_ENTER_HIVE, DNGN_RETURN_FROM_HIVE,
      "Hive", "the Hive", "Hive",
      "You hear a buzzing sound coming from all directions.",
      false, false, YELLOW, BROWN,
      mons_hive_rare, mons_hive_level,
      NULL, NULL, NULL, NULL,
      0, 'H', false, true },

    { BRANCH_VAULTS, BRANCH_MAIN_DUNGEON, 8, 17, 0, 0,
      DNGN_ENTER_VAULTS, DNGN_RETURN_FROM_VAULTS,
      "Vaults", "the Vaults", "Vault",
      NULL,
      true, true, LIGHTGREY, BROWN,
      mons_standard_rare, mons_standard_level,
      NULL, NULL, NULL, NULL,
      5, 'V', false, true },


    { BRANCH_HALL_OF_BLADES, BRANCH_VAULTS, 1, 4, 0, 0,
      DNGN_ENTER_HALL_OF_BLADES, DNGN_RETURN_FROM_HALL_OF_BLADES,
      "Hall of Blades", "the Hall of Blades", "Blade",
      NULL,
      false, true, LIGHTGREY, LIGHTGREY,
      mons_hallblade_rare, mons_hallblade_level,
      NULL, NULL, NULL, NULL,
      0, 'B', false, false },

    { BRANCH_CRYPT, BRANCH_VAULTS, 5, 3, 0, 0,
      DNGN_ENTER_CRYPT, DNGN_RETURN_FROM_CRYPT,
      "Crypt", "the Crypt", "Crypt",
      NULL,
      false, true, LIGHTGREY, LIGHTGREY,
      mons_crypt_rare, mons_crypt_level,
      NULL, NULL, NULL, NULL,
      5, 'C', false, false },

    { BRANCH_TOMB, BRANCH_CRYPT, 3, 5, BFLAG_ISLANDED, 0,
      DNGN_ENTER_TOMB, DNGN_RETURN_FROM_TOMB,
      "Tomb", "the Tomb of the Ancients", "Tomb",
      NULL,
      false, true, YELLOW, LIGHTGREY,
      mons_tomb_rare, mons_tomb_level,
      NULL, NULL, NULL, NULL,
      0, 'G', false, true },

    { BRANCH_VESTIBULE_OF_HELL, BRANCH_MAIN_DUNGEON, 1, -1, 0, 0,
      DNGN_ENTER_HELL, DNGN_EXIT_HELL, // sentinel
      "Hell", "The Vestibule of Hell", "Hell",
      NULL,
      false, true, LIGHTGREY, LIGHTGREY,
      mons_standard_rare, mons_standard_level,
      NULL, NULL, NULL, NULL,
      0, 'U', false, false },

    { BRANCH_DIS, BRANCH_VESTIBULE_OF_HELL, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_DIS, NUM_FEATURES, // sentinel
      "Dis", "the Iron City of Dis", "Dis",
      NULL,
      false, false, CYAN, CYAN,
      mons_dis_rare, mons_dis_level,
      NULL, NULL, NULL, NULL,
      0, 'I', true, true },

    { BRANCH_GEHENNA, BRANCH_VESTIBULE_OF_HELL, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_GEHENNA, NUM_FEATURES, // sentinel
      "Gehenna", "Gehenna", "Geh",
      NULL,
      false, false, DARKGREY, RED,
      mons_gehenna_rare, mons_gehenna_level,
      NULL, NULL, NULL, NULL,
      0, 'N', true, true },

    { BRANCH_COCYTUS, BRANCH_VESTIBULE_OF_HELL, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_COCYTUS, NUM_FEATURES, // sentinel
      "Cocytus", "Cocytus", "Coc",
      NULL,
      false, false, LIGHTBLUE, LIGHTCYAN,
      mons_cocytus_rare, mons_cocytus_level,
      NULL, NULL, NULL, NULL,
      0, 'X', true, true },

    { BRANCH_TARTARUS, BRANCH_VESTIBULE_OF_HELL, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_TARTARUS, NUM_FEATURES, // sentinel
      "Tartarus", "Tartarus", "Tar",
      NULL,
      false, false, DARKGREY, DARKGREY,
      mons_tartarus_rare, mons_tartarus_level,
      NULL, NULL, NULL, NULL,
      0, 'Y', true, true },

    { BRANCH_INFERNO, BRANCH_MAIN_DUNGEON, -1, -1, 0, 0,
      NUM_FEATURES, NUM_FEATURES,
      NULL, NULL, NULL,
      NULL,
      false, false, BLACK, BLACK,
      NULL, NULL,
      NULL, NULL, NULL, NULL,
      0, 'R', false, false },

    { BRANCH_THE_PIT, BRANCH_MAIN_DUNGEON, -1, -1, 0, 0,
      NUM_FEATURES, NUM_FEATURES,
      NULL, NULL, NULL,
      NULL,
      false, false, BLACK, BLACK,
      NULL, NULL,
      NULL, NULL, NULL, NULL,
      0, '0', false, false },

    { BRANCH_HALL_OF_ZOT, BRANCH_MAIN_DUNGEON, 5, 27, BFLAG_HAS_ORB, 0,
      DNGN_ENTER_ZOT, DNGN_RETURN_FROM_ZOT,
      "Zot", "the Realm of Zot", "Zot",
      NULL,
      false, true, BLACK, BLACK,
      mons_hallzot_rare, mons_hallzot_level,
      NULL, NULL, NULL, NULL,
      1, 'Z', false, true },

    { BRANCH_CAVERNS, BRANCH_MAIN_DUNGEON, -1, -1, 0, 0,
      NUM_FEATURES, NUM_FEATURES,
      NULL, NULL, NULL,
      NULL,
      false, false, BLACK, BLACK,
      NULL, NULL,
      NULL, NULL, NULL, NULL,
      0, 0, false, false }
};
