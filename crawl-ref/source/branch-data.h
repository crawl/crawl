#ifndef BRANCHDATA_H
#define BRANCHDATA_H

#include "colour.h"
#include "enum.h"
#include "mon-pick.h"

const Branch branches[NUM_BRANCHES] = {
    // Branch struct:
    //  branch id, parent branch, mindepth, maxdepth, depth, startdepth,
    //  branch flags, level flags
    //  entry stairs, exit stairs, short name, long name, abbrev name
    //  entry message
    //  has_uniques, floor colour, rock colour
    //  mons rarity function, mons level function
    //  travel shortcut, upstairs exit branch, dangerous branch end, ambient noise level

    { BRANCH_MAIN_DUNGEON, BRANCH_MAIN_DUNGEON, -1, -1,
          BRANCH_DUNGEON_DEPTH, 0, 0,
      NUM_FEATURES, NUM_FEATURES,  // sentinel values
      "Dungeon", "the Dungeon", "D",
      NULL,
      true, LIGHTGREY, BROWN,
      mons_standard_rare, mons_standard_level,
      'D', false, false, 0 },

    { BRANCH_ECUMENICAL_TEMPLE, BRANCH_MAIN_DUNGEON, 4, 7, 1, 0, 0,
      DNGN_ENTER_TEMPLE, DNGN_RETURN_FROM_TEMPLE,
      "Temple", "the Ecumenical Temple", "Temple",
      NULL,
      false, LIGHTGREY, BROWN,
      mons_null_rare, mons_null_level,
      'T', false, false, 0 },

    { BRANCH_ORCISH_MINES, BRANCH_MAIN_DUNGEON, 6, 11, 4, 0, 0,
      DNGN_ENTER_ORCISH_MINES, DNGN_RETURN_FROM_ORCISH_MINES,
      "Orcish Mines", "the Orcish Mines", "Orc",
      NULL,
      true, BROWN, BROWN,
      mons_mineorc_rare, mons_mineorc_level,
      'O', false, false, 4 },

    { BRANCH_ELVEN_HALLS, BRANCH_ORCISH_MINES, 3, 4, 5, 0, 0,
      DNGN_ENTER_ELVEN_HALLS, DNGN_RETURN_FROM_ELVEN_HALLS,
      "Elven Halls", "the Elven Halls", "Elf",
      NULL,
      true, WHITE, ETC_ELVEN_BRICK,
      mons_hallelf_rare, mons_hallelf_level,
      'E', false, true, 0 },

    { BRANCH_LAIR, BRANCH_MAIN_DUNGEON, 8, 13, 8, 0, 0,
      DNGN_ENTER_LAIR, DNGN_RETURN_FROM_LAIR,
      "Lair", "the Lair of Beasts", "Lair",
      NULL,
      true, GREEN, BROWN,
      mons_lair_rare, mons_lair_level,
      'L', false, false, 4 },

    { BRANCH_SWAMP, BRANCH_LAIR, 2, 5, 5, BFLAG_ISLANDED, 0,
      DNGN_ENTER_SWAMP, DNGN_RETURN_FROM_SWAMP,
      "Swamp", "the Swamp", "Swamp",
      NULL,
      true, BROWN, BROWN,
      mons_swamp_rare, mons_swamp_level,
      'S', false, true, 0 },

    { BRANCH_SHOALS, BRANCH_LAIR, 3, 6, 5, BFLAG_ISLANDED, 0,
      DNGN_ENTER_SHOALS, DNGN_RETURN_FROM_SHOALS,
      "Shoals", "the Shoals", "Shoals",
      NULL,
      true, BROWN, BROWN,
      mons_shoals_rare, mons_shoals_level,
      'A', false, true, 3 },

    { BRANCH_SLIME_PITS, BRANCH_LAIR, 6, 8, 6, 0, 0,
      DNGN_ENTER_SLIME_PITS, DNGN_RETURN_FROM_SLIME_PITS,
      "Slime Pits", "the Pits of Slime", "Slime",
      NULL,
      true, GREEN, BROWN,
      mons_pitslime_rare, mons_pitslime_level,
      'M', false, true, -5 },

    { BRANCH_SNAKE_PIT, BRANCH_LAIR, 3, 6, 5, 0, 0,
      DNGN_ENTER_SNAKE_PIT, DNGN_RETURN_FROM_SNAKE_PIT,
      "Snake Pit", "the Snake Pit", "Snake",
      NULL,
      true, LIGHTGREEN, YELLOW,
      mons_pitsnake_rare, mons_pitsnake_level,
      'P', false, true, 0 },

    { BRANCH_HIVE, BRANCH_MAIN_DUNGEON, 11, 16, 2, 0, 0,
      DNGN_ENTER_HIVE, DNGN_RETURN_FROM_HIVE,
      "Hive", "the Hive", "Hive",
      "You hear a buzzing sound coming from all directions.",
      false, YELLOW, BROWN,
      mons_hive_rare, mons_hive_level,
      'H', false, true, 5 },

    { BRANCH_VAULTS, BRANCH_MAIN_DUNGEON, 14, 19, 8, 0, 0,
      DNGN_ENTER_VAULTS, DNGN_RETURN_FROM_VAULTS,
      "Vaults", "the Vaults", "Vault",
      NULL,
      true, LIGHTGREY, BROWN,
      mons_standard_rare, mons_standard_level,
      'V', false, true, 0 },

    { BRANCH_HALL_OF_BLADES, BRANCH_VAULTS, 4, 6, 1, 0, 0,
      DNGN_ENTER_HALL_OF_BLADES, DNGN_RETURN_FROM_HALL_OF_BLADES,
      "Hall of Blades", "the Hall of Blades", "Blade",
      NULL,
      true, LIGHTGREY, BROWN,
      mons_hallblade_rare, mons_hallblade_level,
      'B', false, false, -7 },

    { BRANCH_CRYPT, BRANCH_VAULTS, 2, 4, 5, 0, 0,
      DNGN_ENTER_CRYPT, DNGN_RETURN_FROM_CRYPT,
      "Crypt", "the Crypt", "Crypt",
      NULL,
      true, LIGHTGREY, BROWN,
      mons_crypt_rare, mons_crypt_level,
      'C', false, false, -20 },

    { BRANCH_TOMB, BRANCH_CRYPT, 2, 3, 3,
      BFLAG_ISLANDED, LFLAG_NO_TELE_CONTROL,
      DNGN_ENTER_TOMB, DNGN_RETURN_FROM_TOMB,
      "Tomb", "the Tomb of the Ancients", "Tomb",
      NULL,
      true, BROWN, BROWN,
      mons_tomb_rare, mons_tomb_level,
      'W', false, true, -10 },

    { BRANCH_VESTIBULE_OF_HELL, BRANCH_MAIN_DUNGEON, 21, 27, 1, 0, 0,
      DNGN_ENTER_HELL, DNGN_EXIT_HELL, // sentinel
      "Hell", "the Vestibule of Hell", "Hell",
      NULL,
      true, LIGHTGREY, LIGHTRED,
      mons_vestibule_rare, mons_vestibule_level,
      'U', false, false, 0 },

    { BRANCH_DIS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, BFLAG_ISLANDED, 0,
      DNGN_ENTER_DIS, NUM_FEATURES, // sentinel
      "Dis", "the Iron City of Dis", "Dis",
      NULL,
      false, CYAN, BROWN,
      mons_dis_rare, mons_dis_level,
      'I', true, true, 0 },

    { BRANCH_GEHENNA, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, BFLAG_ISLANDED, 0,
      DNGN_ENTER_GEHENNA, NUM_FEATURES, // sentinel
      "Gehenna", "Gehenna", "Geh",
      NULL,
      false, YELLOW, RED,
      mons_gehenna_rare, mons_gehenna_level,
      'G', true, true, 0 },

    { BRANCH_COCYTUS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, BFLAG_ISLANDED, 0,
      DNGN_ENTER_COCYTUS, NUM_FEATURES, // sentinel
      "Cocytus", "Cocytus", "Coc",
      NULL,
      false, LIGHTBLUE, LIGHTCYAN,
      mons_cocytus_rare, mons_cocytus_level,
      'X', true, true, 0 },

    { BRANCH_TARTARUS, BRANCH_VESTIBULE_OF_HELL, 1, 1, 7, BFLAG_ISLANDED, 0,
      DNGN_ENTER_TARTARUS, NUM_FEATURES, // sentinel
      "Tartarus", "Tartarus", "Tar",
      NULL,
      false, MAGENTA, MAGENTA,
      mons_tartarus_rare, mons_tartarus_level,
      'Y', true, true, 0 },

    { BRANCH_HALL_OF_ZOT, BRANCH_MAIN_DUNGEON, 27, 27, 5, 0, 0,
      DNGN_ENTER_ZOT, DNGN_RETURN_FROM_ZOT,
      "Zot", "the Realm of Zot", "Zot",
      NULL,
      true, BLACK, BLACK, // set per-map
      mons_hallzot_rare, mons_hallzot_level,
      'Z', false, true, 0 },

    { BRANCH_FOREST, BRANCH_MAIN_DUNGEON, 3, 6, 5, 0, 0,
      DNGN_ENTER_FOREST, DNGN_RETURN_FROM_FOREST,
      "Forest", "the Enchanted Forest", "Forest",
      NULL,
      true, BROWN, BROWN,
      mons_forest_rare, mons_forest_level,
      'F', false, true, 0 },

    { BRANCH_SPIDER_NEST, BRANCH_LAIR, 3, 6, 5, 0, 0,
      DNGN_ENTER_SPIDER_NEST, DNGN_RETURN_FROM_SPIDER_NEST,
      "Spider Nest", "the Spider Nest", "Spider",
      NULL,
      true, BROWN, YELLOW,
      mons_spidernest_rare, mons_spidernest_level,
      'N', false, true, 0 },

    { BRANCH_DWARVEN_HALL, BRANCH_MAIN_DUNGEON, 5, 7, 1, 0, 0,
      DNGN_ENTER_DWARVEN_HALL, DNGN_RETURN_FROM_DWARVEN_HALL,
      "Dwarven Hall", "the Dwarven Hall", "Dwarf",
      NULL,
      true, YELLOW, BROWN,
      mons_dwarf_rare, mons_dwarf_level,
      'K', false, false, 0 },

    { BRANCH_ABYSS, NUM_BRANCHES, DEPTH_ABYSS, DEPTH_ABYSS, 1,
          BFLAG_NO_XLEV_TRAVEL, LFLAG_NO_TELE_CONTROL | LFLAG_NO_MAP,
      DNGN_ENTER_ABYSS, DNGN_EXIT_ABYSS,
      "Abyss", "the Abyss", "Abyss",
      NULL,
      false, BLACK, BLACK, // set specially
      mons_abyss_rare, mons_abyss_level,
      '0', false, false, 0 },

    { BRANCH_PANDEMONIUM, NUM_BRANCHES, DEPTH_PAN, DEPTH_PAN,
          1, BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PANDEMONIUM, DNGN_EXIT_PANDEMONIUM,
      "Pandemonium", "Pandemonium", "Pan",
      NULL,
      true, BLACK, BLACK, // set specially from monster set
      mons_pan_rare, mons_pan_level,
      '0', false, false, 0 },

    { BRANCH_ZIGGURAT, NUM_BRANCHES, 27, 27, 27, BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Ziggurat", "a ziggurat", "Zig",
      NULL,
      false, BLACK, BLACK,
      mons_null_rare, mons_null_level, // overridden later
      '0', false, false /* maybe, but Zig:26 is almost as bad */, 0 },

    { BRANCH_LABYRINTH, NUM_BRANCHES, 10, 20, 1, BFLAG_NO_XLEV_TRAVEL,
          LFLAG_NO_TELE_CONTROL | LFLAG_NO_MAP,
      DNGN_ENTER_LABYRINTH, DNGN_ESCAPE_HATCH_UP,
      "Labyrinth", "a Labyrinth", "Lab",
      NULL,
      false, BLACK, BLACK,
      mons_null_rare, mons_null_level,
      '0', false, false, 0 },

    { BRANCH_BAZAAR, NUM_BRANCHES, 10, 27, 1, BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Bazaar", "a bazaar", "Bazaar",
      NULL,
      false, BLUE, YELLOW,
      mons_null_rare, mons_null_level,
      '0', false, false, 0 },

    { BRANCH_TROVE, NUM_BRANCHES, 10, 27, 1, BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Trove", "a treasure trove", "Trove",
      NULL,
      false, DARKGREY, BLUE,
      mons_null_rare, mons_null_level,
      '0', false, false, 0 },

    { BRANCH_SEWER, NUM_BRANCHES, 3, 6, 1, BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Sewer", "the sewers", "Sewer",
      NULL,
      false, LIGHTGREY, DARKGREY,
      mons_null_rare, mons_null_level,
      '0', false, false, 0 },

    { BRANCH_OSSUARY, NUM_BRANCHES, 4, 8, 1, BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Ossuary", "a tomb", "Ossuary",
      NULL,
      false, WHITE, YELLOW,
      mons_null_rare, mons_null_level,
      '0', false, false, 0 },

    { BRANCH_BAILEY, NUM_BRANCHES, 7, 16, 1, BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Bailey", "the bailey", "Bailey",
      NULL,
      false, WHITE, LIGHTGREY,
      mons_null_rare, mons_null_level,
      '0', false, false, 0 },

    { BRANCH_ICE_CAVE, NUM_BRANCHES, 11, 19, 1, BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Ice Cave", "an ice cave", "IceCv",
      NULL,
      false, BLUE, WHITE,
      mons_null_rare, mons_null_level,
      '0', false, false, 0 },

    { BRANCH_VOLCANO, NUM_BRANCHES, 8, 21, 1, BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Volcano", "a volcano", "Volcano",
      NULL,
      false, RED, RED,
      mons_null_rare, mons_null_level,
      '0', false, false, 0 },

    { BRANCH_WIZLAB, NUM_BRANCHES, 22, 27, 1, BFLAG_NO_XLEV_TRAVEL, 0,
      DNGN_ENTER_PORTAL_VAULT, DNGN_EXIT_PORTAL_VAULT,
      "Wizlab", "a Wizard's Laboratory", "WizLab",
      NULL,
      false, LIGHTGREY, BROWN, // set per-map
      mons_null_rare, mons_null_level,
      '0', false, false, 0 },
};

#endif
