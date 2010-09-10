/*
 *  File:       branch-data.h
 */

#ifndef BRANCHDATA_H
#define BRANCHDATA_H

#include "colour.h"
#include "enum.h"
#include "mon-pick.h"

Branch branches[] = {
    // Branch struct:
    //  branch id, parent branch, mindepth, maxdepth, depth, startdepth,
    //  branch flags, level flags
    //  entry stairs, exit stairs, short name, long name, abbrev name
    //  entry message
    //  has_shops, has_uniques, floor colour, rock colour
    //  mons rarity function, mons level function
    //  num_traps_function, rand_trap_function, num_fogs_function, rand_fog_function
    //  altar chance (in %), travel shortcut, upstairs exit branch, dangerous branch end, ambient noise level

    { BRANCH_MAIN_DUNGEON, BRANCH_MAIN_DUNGEON, -1, -1,
      BRANCH_DUNGEON_DEPTH, -1, 0, 0,
      NUM_FEATURES, NUM_FEATURES,  // sentinel values
      "Dungeon", "the Dungeon", "D",
      NULL,
      20, true, LIGHTGREY, BROWN,
      mons_standard_rare, mons_standard_level,
      NULL, NULL, NULL, NULL,
      8, 'D', false, false, 0 },

    { BRANCH_ECUMENICAL_TEMPLE, BRANCH_MAIN_DUNGEON, 4, 7, 1, 5, 0, 0,
      DNGN_ENTER_TEMPLE, DNGN_RETURN_FROM_TEMPLE,
      "Temple", "the Ecumenical Temple", "Temple",
      NULL,
      0, false, LIGHTGREY, LIGHTGREY,
      mons_standard_rare, mons_standard_level,
      traps_zero_number, NULL, NULL, NULL, // No traps in temple
      0, 'T', false, false, 0 },

    { BRANCH_ORCISH_MINES, BRANCH_MAIN_DUNGEON, 6, 11, 4, 6, 0, 0,
      DNGN_ENTER_ORCISH_MINES, DNGN_RETURN_FROM_ORCISH_MINES,
      "Orcish Mines", "the Orcish Mines", "Orc",
      NULL,
      30, true, BROWN, BROWN,
      mons_mineorc_rare, mons_mineorc_level,
      NULL, NULL, NULL, NULL,
      20, 'O', false, false, 5 },

    { BRANCH_ELVEN_HALLS, BRANCH_ORCISH_MINES, 3, 4, 7, 4, 0, 0,
      DNGN_ENTER_ELVEN_HALLS, DNGN_RETURN_FROM_ELVEN_HALLS,
      "Elven Halls", "the Elven Halls", "Elf",
      NULL,
      40, true, WHITE, ETC_ELVEN_BRICK,
      mons_hallelf_rare, mons_hallelf_level,
      NULL, NULL, NULL, NULL,
      8, 'E', false, true, 0 },

    { BRANCH_LAIR, BRANCH_MAIN_DUNGEON, 8, 13, 8, 8, 0, 0,
      DNGN_ENTER_LAIR, DNGN_RETURN_FROM_LAIR,
      "Lair", "the Lair of Beasts", "Lair",
      NULL,
      0, true, GREEN, BROWN,
      mons_lair_rare, mons_lair_level,
      NULL, NULL, NULL, NULL,
      5, 'L', false, false, 5 },

    { BRANCH_SWAMP, BRANCH_LAIR, 2, 5, 5, 3, BFLAG_ISLANDED, 0,
      DNGN_ENTER_SWAMP, DNGN_RETURN_FROM_SWAMP,
      "Swamp", "the Swamp", "Swamp",
      NULL,
      0, true, BROWN, BROWN,
      mons_swamp_rare, mons_swamp_level,
      NULL, NULL, NULL, NULL,
      0, 'S', false, true, 0 },

    { BRANCH_SHOALS, BRANCH_LAIR, 3, 6, 5, 4, BFLAG_ISLANDED, 0,
      DNGN_ENTER_SHOALS, DNGN_RETURN_FROM_SHOALS,
      "Shoals", "the Shoals", "Shoals",
      NULL,
      20, true, BROWN, BROWN,
      mons_shoals_rare, mons_shoals_level,
      NULL, NULL, NULL, NULL,
      0, 'A', false, true, 3 },

    { BRANCH_SLIME_PITS, BRANCH_LAIR, 6, 8, 6, 4, 0, 0,
      DNGN_ENTER_SLIME_PITS, DNGN_RETURN_FROM_SLIME_PITS,
      "Slime Pits", "the Pits of Slime", "Slime",
      NULL,
      0, true, GREEN, LIGHTGREEN,
      mons_pitslime_rare, mons_pitslime_level,
      NULL, random_trap_slime, NULL, NULL,
      5, 'M', false, true, -5 },

    { BRANCH_SNAKE_PIT, BRANCH_LAIR, 3, 6, 5, 7, 0, 0,
      DNGN_ENTER_SNAKE_PIT, DNGN_RETURN_FROM_SNAKE_PIT,
      "Snake Pit", "the Snake Pit", "Snake",
      NULL,
      20, true, LIGHTGREEN, YELLOW,
      mons_pitsnake_rare, mons_pitsnake_level,
      NULL, NULL, NULL, NULL,
      10, 'P', false, true, 0 },

    { BRANCH_HIVE, BRANCH_MAIN_DUNGEON, 11, 16, 2, 15, 0, 0,
      DNGN_ENTER_HIVE, DNGN_RETURN_FROM_HIVE,
      "Hive", "the Hive", "Hive",
      "You hear a buzzing sound coming from all directions.",
      0, false, YELLOW, BROWN,
      mons_hive_rare, mons_hive_level,
      NULL, NULL, NULL, NULL,
      0, 'H', false, true, 20 },

    { BRANCH_VAULTS, BRANCH_MAIN_DUNGEON, 14, 19, 8, 17, 0, 0,
      DNGN_ENTER_VAULTS, DNGN_RETURN_FROM_VAULTS,
      "Vaults", "the Vaults", "Vault",
      NULL,
      40, true, LIGHTGREY, BROWN,
      mons_standard_rare, mons_standard_level,
      NULL, NULL, NULL, NULL,
      5, 'V', false, true, 0 },

    { BRANCH_HALL_OF_BLADES, BRANCH_VAULTS, 4, 6, 1, 4, 0, 0,
      DNGN_ENTER_HALL_OF_BLADES, DNGN_RETURN_FROM_HALL_OF_BLADES,
      "Hall of Blades", "the Hall of Blades", "Blade",
      NULL,
      0, true, LIGHTGREY, LIGHTGREY,
      mons_hallblade_rare, mons_hallblade_level,
      NULL, NULL, NULL, NULL,
      0, 'B', false, false, -7 },

    { BRANCH_CRYPT, BRANCH_VAULTS, 2, 4, 5, 3, 0, 0,
      DNGN_ENTER_CRYPT, DNGN_RETURN_FROM_CRYPT,
      "Crypt", "the Crypt", "Crypt",
      NULL,
      0, true, LIGHTGREY, LIGHTGREY,
      mons_crypt_rare, mons_crypt_level,
      NULL, NULL, NULL, NULL,
      5, 'C', false, false, -20 },

    { BRANCH_TOMB, BRANCH_CRYPT, 2, 3, 3, 5,
      BFLAG_ISLANDED | BFLAG_NO_TELE_CONTROL, 0,
      DNGN_ENTER_TOMB, DNGN_RETURN_FROM_TOMB,
      "Tomb", "the Tomb of the Ancients", "Tomb",
      NULL,
      0, true, YELLOW, LIGHTGREY,
      mons_tomb_rare, mons_tomb_level,
      NULL, NULL, NULL, NULL,
      0, 'W', false, true, -10 },

    { BRANCH_VESTIBULE_OF_HELL, BRANCH_MAIN_DUNGEON, 21, 27, 1, -1, 0, 0,
      DNGN_ENTER_HELL, DNGN_EXIT_HELL, // sentinel
      "Hell", "the Vestibule of Hell", "Hell",
      NULL,
      0, true, LIGHTGREY, LIGHTGREY,
      mons_standard_rare, mons_standard_level,
      NULL, NULL, NULL, NULL,
      0, 'U', false, false, 0 },

    { BRANCH_DIS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_DIS, NUM_FEATURES, // sentinel
      "Dis", "the Iron City of Dis", "Dis",
      NULL,
      0, false, CYAN, CYAN,
      mons_dis_rare, mons_dis_level,
      NULL, NULL, NULL, NULL,
      0, 'I', true, true, 0 },

    { BRANCH_GEHENNA, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_GEHENNA, NUM_FEATURES, // sentinel
      "Gehenna", "Gehenna", "Geh",
      NULL,
      0, false, YELLOW, RED,
      mons_gehenna_rare, mons_gehenna_level,
      NULL, NULL, NULL, NULL,
      0, 'G', true, true, 0 },

    { BRANCH_COCYTUS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_COCYTUS, NUM_FEATURES, // sentinel
      "Cocytus", "Cocytus", "Coc",
      NULL,
      0, false, LIGHTBLUE, LIGHTCYAN,
      mons_cocytus_rare, mons_cocytus_level,
      NULL, NULL, NULL, NULL,
      0, 'X', true, true, 0 },

    { BRANCH_TARTARUS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, -1, BFLAG_ISLANDED, 0,
      DNGN_ENTER_TARTARUS, NUM_FEATURES, // sentinel
      "Tartarus", "Tartarus", "Tar",
      NULL,
      0, false, MAGENTA, MAGENTA,
      mons_tartarus_rare, mons_tartarus_level,
      NULL, NULL, NULL, NULL,
      0, 'Y', true, true, 0 },

    { BRANCH_HALL_OF_ZOT, BRANCH_MAIN_DUNGEON, 27, 27, 5, 27, BFLAG_HAS_ORB, 0,
      DNGN_ENTER_ZOT, DNGN_RETURN_FROM_ZOT,
      "Zot", "the Realm of Zot", "Zot",
      NULL,
      0, true, BLACK, BLACK,
      mons_hallzot_rare, mons_hallzot_level,
      NULL, NULL, NULL, NULL,
      1, 'Z', false, true, 0 },

    { BRANCH_FOREST, BRANCH_MAIN_DUNGEON, 3, 6, 5, 7, 0, 0,
      DNGN_ENTER_FOREST, DNGN_RETURN_FROM_FOREST,
      "Forest", "the Enchanted Forest", "Forest",
      NULL,
      20, true, BROWN, BROWN,
      mons_forest_rare, mons_forest_level,
      NULL, NULL, NULL, NULL,
      10, 'F', false, true, 0 },

    { BRANCH_SPIDER_NEST, BRANCH_LAIR, 3, 6, 5, 7, 0, 0,
      DNGN_ENTER_SPIDER_NEST, DNGN_RETURN_FROM_SPIDER_NEST,
      "Spider Nest", "the Spider Nest", "Spider",
      NULL,
      20, true, BROWN, YELLOW,
      mons_spidernest_rare, mons_spidernest_level,
      NULL, NULL, NULL, NULL,
      0, 'N', false, true, 0 },

    { BRANCH_DWARF_HALL, BRANCH_MAIN_DUNGEON, 3, 7, 5, 6, 0, 0,
      DNGN_ENTER_DWARF_HALL, DNGN_RETURN_FROM_DWARF_HALL,
      "Dwarf Hall", "the Dwarf Hall of Fallen Heroes", "Dwarf",
      NULL,
      30, true, YELLOW, LIGHTGREY,
      mons_dwarf_rare, mons_dwarf_level,
      NULL, NULL, NULL, NULL,
      10, 'K', false, false, 0 },
};

#endif
