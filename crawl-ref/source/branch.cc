/*
 *  Modified for Crawl Reference by $Author: haranp $ on $Date: 2006-12-10 22:28:21 +0200 (Sun, 10 Dec 2006) $
 */

#include "AppHdr.h"
#include "branch.h"
#include "externs.h"
#include "mon-pick.h"

Branch& your_branch()
{
    return branches[static_cast<int>(you.where_are_you)];
}

branch_type str_to_branch(const std::string &branch, branch_type err)
{
    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        if (branches[i].abbrevname && branches[i].abbrevname == branch)
            return (static_cast<branch_type>(i));
    }
    return (err);
}

Branch branches[] = {

    { BRANCH_MAIN_DUNGEON, BRANCH_MAIN_DUNGEON, 27, -1,
      NUM_FEATURES, NUM_FEATURES,  // sentinel values
      "Dungeon", "the Dungeon", "D",
      NULL,
      true, true, LIGHTGREY, BROWN,
      mons_standard_rare, mons_standard_level,
      8, 'D', false },

    { BRANCH_ECUMENICAL_TEMPLE, BRANCH_MAIN_DUNGEON, 1, 5,
      DNGN_ENTER_TEMPLE, DNGN_RETURN_FROM_TEMPLE,
      "Temple", "the Ecumenical Temple", "Temple",
      NULL,
      false, false, LIGHTGREY, LIGHTGREY,
      mons_standard_rare, mons_standard_level, 
      0, 'T', false },

    { BRANCH_ORCISH_MINES, BRANCH_MAIN_DUNGEON, 4, 6,
      DNGN_ENTER_ORCISH_MINES, DNGN_RETURN_FROM_ORCISH_MINES,
      "Orcish Mines", "the Orcish Mines", "Orc",
      NULL,
      true, false, BROWN, BROWN,
      mons_mineorc_rare, mons_mineorc_level,
      20, 'O', false },

    { BRANCH_ELVEN_HALLS, BRANCH_ORCISH_MINES, 7, 4,
      DNGN_ENTER_ELVEN_HALLS, DNGN_RETURN_FROM_ELVEN_HALLS,
      "Elven Halls", "the Elven Halls", "Elf",
      NULL,
      true, true, DARKGREY, LIGHTGREY,
      mons_hallelf_rare, mons_hallelf_level,
      8, 'E', false },

    { BRANCH_LAIR, BRANCH_MAIN_DUNGEON, 10, 8,
      DNGN_ENTER_LAIR, DNGN_RETURN_FROM_LAIR,
      "Lair", "the Lair of Beasts", "Lair",
      NULL,
      true, false, GREEN, BROWN,
      mons_lair_rare, mons_lair_level,
      5, 'L', false },

    { BRANCH_SWAMP, BRANCH_LAIR, 5, 3,
      DNGN_ENTER_SWAMP, DNGN_RETURN_FROM_SWAMP,
      "Swamp", "the Swamp", "Swamp",
      NULL,
      true, true, BROWN, BROWN,
      mons_swamp_rare, mons_swamp_level, 
      0, 'S', false },
    
    { BRANCH_SHOALS, BRANCH_LAIR, 5, 4,
      DNGN_ENTER_SHOALS, DNGN_RETURN_FROM_SHOALS,
      "Shoals", "the Shoals", "Shoal",
      NULL,
      true, true, BROWN, BROWN,
      mons_shoals_rare, mons_shoals_level,
      0, 'A', false },

    { BRANCH_SLIME_PITS, BRANCH_LAIR, 6, 4,
      DNGN_ENTER_SLIME_PITS, DNGN_RETURN_FROM_SLIME_PITS,
      "Slime Pits", "the Pits of Slime", "Slime",
      NULL,
      false, false, GREEN, LIGHTGREEN,
      mons_pitslime_rare, mons_pitslime_level,
      5, 'M', false },

    { BRANCH_SNAKE_PIT, BRANCH_LAIR, 5, 7,
      DNGN_ENTER_SNAKE_PIT, DNGN_RETURN_FROM_SNAKE_PIT,
      "Snake Pit", "the Snake Pit", "Snake",
      NULL,
      true, true, LIGHTGREEN, YELLOW,
      mons_pitsnake_rare, mons_pitsnake_level,
      10, 'P', false },

    { BRANCH_HIVE, BRANCH_MAIN_DUNGEON, 4, 15,
      DNGN_ENTER_HIVE, DNGN_RETURN_FROM_HIVE,
      "Hive", "the Hive", "Hive",
      "You hear a buzzing sound coming from all directions.",
      false, false, YELLOW, BROWN,
      mons_hive_rare, mons_hive_level, 
      0, 'H', false },

    { BRANCH_VAULTS, BRANCH_MAIN_DUNGEON, 8, 17,
      DNGN_ENTER_VAULTS, DNGN_RETURN_FROM_VAULTS,
      "Vaults", "the Vaults", "Vault",
      NULL,
      true, true, LIGHTGREY, BROWN,
      mons_standard_rare, mons_standard_level,
      5, 'V', false },


    { BRANCH_HALL_OF_BLADES, BRANCH_VAULTS, 1, 4,
      DNGN_ENTER_HALL_OF_BLADES, DNGN_RETURN_FROM_HALL_OF_BLADES,
      "Hall of Blades", "the Hall of Blades", "Blade",
      NULL,
      false, true, LIGHTGREY, LIGHTGREY,
      mons_hallblade_rare, mons_hallblade_level, 
      0, 'B', false },

    { BRANCH_CRYPT, BRANCH_VAULTS, 5, 3,
      DNGN_ENTER_CRYPT, DNGN_RETURN_FROM_CRYPT,
      "Crypt", "the Crypt", "Crypt",
      NULL,
      false, true, LIGHTGREY, LIGHTGREY,
      mons_crypt_rare, mons_crypt_level,
      5, 'C', false },

    { BRANCH_TOMB, BRANCH_CRYPT, 3, 5,
      DNGN_ENTER_TOMB, DNGN_RETURN_FROM_TOMB,
      "Tomb", "the Tomb of the Ancients", "Tomb",
      NULL,
      false, true, YELLOW, LIGHTGREY,
      mons_tomb_rare, mons_tomb_level, 
      0, 'G', false },

    { BRANCH_VESTIBULE_OF_HELL, BRANCH_MAIN_DUNGEON, 1, -1,
      DNGN_ENTER_HELL, NUM_FEATURES, // sentinel
      "Hell", "The Vestibule of Hell", "Hell",
      NULL,
      false, true, LIGHTGREY, LIGHTGREY,
      mons_standard_rare, mons_standard_level,
      0, 'U', false },

    { BRANCH_DIS, BRANCH_VESTIBULE_OF_HELL, 7, -1,
      DNGN_ENTER_DIS, NUM_FEATURES, // sentinel
      "Dis", "the Iron City of Dis", "Dis",
      NULL,
      false, false, CYAN, CYAN,
      mons_dis_rare, mons_dis_level,
      0, 'I', true },

    { BRANCH_GEHENNA, BRANCH_VESTIBULE_OF_HELL, 7, -1,
      DNGN_ENTER_GEHENNA, NUM_FEATURES, // sentinel
      "Gehenna", "Gehenna", "Geh",
      NULL,
      false, false, DARKGREY, RED,
      mons_gehenna_rare, mons_gehenna_level,
      0, 'N', true },

    { BRANCH_COCYTUS, BRANCH_VESTIBULE_OF_HELL, 7, -1,
      DNGN_ENTER_COCYTUS, NUM_FEATURES, // sentinel
      "Cocytus", "Cocytus", "Coc",
      NULL,
      false, false, LIGHTBLUE, LIGHTCYAN,
      mons_cocytus_rare, mons_cocytus_level,
      0, 'X', true },

    { BRANCH_TARTARUS, BRANCH_VESTIBULE_OF_HELL, 7, -1,
      DNGN_ENTER_TARTARUS, NUM_FEATURES, // sentinel
      "Tartarus", "Tartarus", "Tar",
      NULL,
      false, false, DARKGREY, DARKGREY,
      mons_tartarus_rare, mons_tartarus_level,
      0, 'Y', true },

    { BRANCH_INFERNO, BRANCH_MAIN_DUNGEON, -1, -1,
      NUM_FEATURES, NUM_FEATURES,
      NULL, NULL, NULL,
      NULL,
      false, false, BLACK, BLACK,
      NULL, NULL,
      0, 'R', false },

    { BRANCH_THE_PIT, BRANCH_MAIN_DUNGEON, -1, -1,
      NUM_FEATURES, NUM_FEATURES,
      NULL, NULL, NULL,
      NULL,
      false, false, BLACK, BLACK,
      NULL, NULL,
      0, '0', false },

    { BRANCH_HALL_OF_ZOT, BRANCH_MAIN_DUNGEON, 5, 27,
      DNGN_ENTER_ZOT, DNGN_RETURN_FROM_ZOT,
      "Zot", "the Realm of Zot", "Zot",
      NULL,
      false, true, BLACK, BLACK,
      mons_hallzot_rare, mons_hallzot_level,
      1, 'Z', false },

    { BRANCH_CAVERNS, BRANCH_MAIN_DUNGEON, -1, -1,
      NUM_FEATURES, NUM_FEATURES,
      NULL, NULL, NULL,
      NULL,
      false, false, BLACK, BLACK,
      NULL, NULL,
      0, 0, false }
};
