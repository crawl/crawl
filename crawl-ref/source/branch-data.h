#pragma once

#include "colour.h"
#include "tag-version.h"

// The order of these entries must match the order of the branch-type enum.
const Branch branches[NUM_BRANCHES] =
{
    // Branch struct:
    //  branch id, parent branch, mindepth, maxdepth, depth, absdepth,
    //  branch flags
    //  entry stairs, exit stairs, escape feature,
    //  short name, long name, abbrev name
    //  entry message
    //  floor colour, rock colour
    //  travel shortcut, runes, ambient noise level, mon_die_size
    //  descent mode parent branches

    { BRANCH_DUNGEON, NUM_BRANCHES, 0, 0, 15, 0,
      brflag::none,
      NUM_FEATURES, DNGN_EXIT_DUNGEON, NUM_FEATURES,
      "Dungeon", "the Dungeon", "D",
      nullptr,
      LIGHTGREY, BROWN,
      'D', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE, /*unused*/
      {} },

    { BRANCH_TEMPLE, BRANCH_DUNGEON, 4, 7, 1, 5,
      brflag::no_items | brflag::fully_map,
      DNGN_ENTER_TEMPLE, DNGN_EXIT_TEMPLE, NUM_FEATURES,
      "Temple", "the Ecumenical Temple", "Temple",
      nullptr,
      LIGHTGREY, BROWN,
      'T', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },

    { BRANCH_ORC, BRANCH_DUNGEON, 9, 12, 2, 10,
      brflag::spotty,
      DNGN_ENTER_ORC, DNGN_EXIT_ORC, NUM_FEATURES,
      "Orcish Mines", "the Orcish Mines", "Orc",
      nullptr,
      BROWN, BROWN,
      'O', {}, branch_noise::normal, 6,
      {BRANCH_LAIR} },

    { BRANCH_ELF, BRANCH_ORC, 2, 2, 3, 15,
      brflag::dangerous_end,
      DNGN_ENTER_ELF, DNGN_EXIT_ELF, NUM_FEATURES,
      "Elven Halls", "the Elven Halls", "Elf",
      nullptr,
      WHITE, ETC_ELVEN_BRICK,
      'E', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {BRANCH_SWAMP, BRANCH_SNAKE, BRANCH_SHOALS, BRANCH_SPIDER } },
#if TAG_MAJOR_VERSION == 34

    { BRANCH_DWARF, BRANCH_ELF, -1, -1, 0, 17,
      brflag::none,
      DNGN_ENTER_DWARF, DNGN_EXIT_DWARF, NUM_FEATURES,
      "Dwarven Hall", "the Dwarven Hall", "Dwarf",
      nullptr,
      BROWN, BROWN,
      'K', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {}},
#endif

    { BRANCH_LAIR, BRANCH_DUNGEON, 8, 11, 5, 10,
      brflag::none,
      DNGN_ENTER_LAIR, DNGN_EXIT_LAIR, NUM_FEATURES,
      "Lair", "the Lair of Beasts", "Lair",
      nullptr,
      GREEN, BROWN,
      'L', {}, branch_noise::normal, 8,
      {BRANCH_DUNGEON} },

    { BRANCH_SWAMP, BRANCH_LAIR, 2, 3, 4, 15,
      brflag::dangerous_end | brflag::spotty,
      DNGN_ENTER_SWAMP, DNGN_EXIT_SWAMP, NUM_FEATURES,
      "Swamp", "the Swamp", "Swamp",
      nullptr,
      BROWN, BROWN,
      'S', { RUNE_SWAMP }, branch_noise::loud, 8,
      {BRANCH_ORC} },

    { BRANCH_SHOALS, BRANCH_LAIR, 2, 3, 4, 15,
      brflag::dangerous_end,
      DNGN_ENTER_SHOALS, DNGN_EXIT_SHOALS, NUM_FEATURES,
      "Shoals", "the Shoals", "Shoals",
      nullptr,
      BROWN, BROWN,
      'A', { RUNE_SHOALS }, branch_noise::loud, DEFAULT_MON_DIE_SIZE,
      {BRANCH_ORC} },

    { BRANCH_SNAKE, BRANCH_LAIR, 2, 3, 4, 15,
      brflag::dangerous_end,
      DNGN_ENTER_SNAKE, DNGN_EXIT_SNAKE, NUM_FEATURES,
      "Snake Pit", "the Snake Pit", "Snake",
      nullptr,
      LIGHTGREEN, YELLOW,
      'P', { RUNE_SNAKE }, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {BRANCH_ORC} },

    { BRANCH_SPIDER, BRANCH_LAIR, 2, 3, 4, 15,
      brflag::dangerous_end,
      DNGN_ENTER_SPIDER, DNGN_EXIT_SPIDER, NUM_FEATURES,
      "Spider Nest", "the Spider Nest", "Spider",
      nullptr,
      BROWN, YELLOW,
      'N', { RUNE_SPIDER }, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {BRANCH_ORC} },

    { BRANCH_SLIME, BRANCH_LAIR, 4, 5, 5, 17,
      brflag::no_items | brflag::dangerous_end | brflag::spotty,
      DNGN_ENTER_SLIME, DNGN_EXIT_SLIME, NUM_FEATURES,
      "Slime Pits", "the Pits of Slime", "Slime",
      nullptr,
      GREEN, BROWN,
      'M', { RUNE_SLIME }, branch_noise::quiet, 7,
      {BRANCH_LAIR, BRANCH_VAULTS } },

    { BRANCH_VAULTS, BRANCH_DUNGEON, 13, 14, 5, 19,
      brflag::dangerous_end,
      DNGN_ENTER_VAULTS, DNGN_EXIT_VAULTS, NUM_FEATURES,
      "Vaults", "the Vaults", "Vaults",
      nullptr,
      LIGHTGREY, BROWN,
      'V', { RUNE_VAULTS }, branch_noise::normal, 9,
      {BRANCH_ELF, BRANCH_CRYPT } },
#if TAG_MAJOR_VERSION == 34

    { BRANCH_BLADE, BRANCH_VAULTS, 3, 4, 1, 21,
      brflag::no_items,
      DNGN_ENTER_BLADE, DNGN_EXIT_BLADE, NUM_FEATURES,
      "Hall of Blades", "the Hall of Blades", "Blade",
      nullptr,
      LIGHTGREY, BROWN,
      'B', {}, branch_noise::quiet, DEFAULT_MON_DIE_SIZE,
      {}},
#endif

    { BRANCH_CRYPT, BRANCH_VAULTS, 2, 3, 3, 19,
      brflag::dangerous_end,
      DNGN_ENTER_CRYPT, DNGN_EXIT_CRYPT, NUM_FEATURES,
      "Crypt", "the Crypt", "Crypt",
      nullptr,
      LIGHTGREY, BROWN,
      'C', {}, branch_noise::quiet, 10,
      {BRANCH_SWAMP, BRANCH_SNAKE, BRANCH_SHOALS, BRANCH_SPIDER }},

    { BRANCH_TOMB, BRANCH_CRYPT, 3, 3, 3, 21,
      brflag::islanded | brflag::dangerous_end | brflag::no_shafts,
      DNGN_ENTER_TOMB, DNGN_EXIT_TOMB, NUM_FEATURES,
      "Tomb", "the Tomb of the Ancients", "Tomb",
      nullptr,
      BROWN, BROWN,
      'W', { RUNE_TOMB }, branch_noise::quiet, DEFAULT_MON_DIE_SIZE,
      {} },
#if TAG_MAJOR_VERSION > 34

    { BRANCH_DEPTHS, BRANCH_DUNGEON, 15, 15, 4, 22,
      brflag::none,
      DNGN_ENTER_DEPTHS, DNGN_EXIT_DEPTHS, NUM_FEATURES,
      "Depths", "the Depths", "Depths",
      nullptr,
      LIGHTGREY, BROWN,
      'U', {}, branch_noise::normal, 10,
      {BRANCH_VAULTS, BRANCH_SLIME } },
#endif

    { BRANCH_VESTIBULE, NUM_BRANCHES, 27, 27, 1, 27,
      brflag::no_items,
      DNGN_ENTER_HELL, DNGN_EXIT_HELL, NUM_FEATURES,
      "Hell", "the Vestibule of Hell", "Hell",
      "Welcome to Hell!\nPlease enjoy your stay.",
      LIGHTGREY, LIGHTRED,
      'H', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },

    { BRANCH_DIS, BRANCH_VESTIBULE, 1, 1, 7, 28,
      brflag::no_items | brflag::dangerous_end,
      DNGN_ENTER_DIS, DNGN_EXIT_DIS, DNGN_EXIT_DIS,
      "Dis", "the Iron City of Dis", "Dis",
      "An ancient malice corrodes your equipment.",
      CYAN, BROWN,
      'I', { RUNE_DIS }, branch_noise::normal, HELL_MON_DIE_SIZE,
      {BRANCH_VESTIBULE} },

    { BRANCH_GEHENNA, BRANCH_VESTIBULE, 1, 1, 7, 28,
      brflag::no_items | brflag::dangerous_end,
      DNGN_ENTER_GEHENNA, DNGN_EXIT_GEHENNA, DNGN_EXIT_GEHENNA,
      "Gehenna", "Gehenna", "Geh",
      "Your scrolls appear blurry in the acrid smoke.",
      BROWN, RED,
      'G', { RUNE_GEHENNA }, branch_noise::normal, HELL_MON_DIE_SIZE,
      {BRANCH_VESTIBULE} },

    { BRANCH_COCYTUS, BRANCH_VESTIBULE, 1, 1, 7, 28,
      brflag::no_items | brflag::dangerous_end,
      DNGN_ENTER_COCYTUS, DNGN_EXIT_COCYTUS, DNGN_EXIT_COCYTUS,
      "Cocytus", "Cocytus", "Coc",
      "Your potions freeze solid in the terrible cold.",
      LIGHTBLUE, LIGHTCYAN,
      'X', { RUNE_COCYTUS }, branch_noise::normal, HELL_MON_DIE_SIZE,
      {BRANCH_VESTIBULE}},

    { BRANCH_TARTARUS, BRANCH_VESTIBULE, 1, 1, 7, 28,
      brflag::no_items | brflag::dangerous_end,
      DNGN_ENTER_TARTARUS, DNGN_EXIT_TARTARUS, DNGN_EXIT_TARTARUS,
      "Tartarus", "Tartarus", "Tar",
      "This decaying realm drains your will.",
      MAGENTA, MAGENTA,
      'Y', { RUNE_TARTARUS }, branch_noise::normal, HELL_MON_DIE_SIZE,
      {BRANCH_VESTIBULE} },

    { BRANCH_ZOT, BRANCH_DEPTHS, 4, 4, 5, 27,
      brflag::dangerous_end,
      DNGN_ENTER_ZOT, DNGN_EXIT_ZOT, NUM_FEATURES,
      "Zot", "the Realm of Zot", "Zot",
      "Welcome to the Realm of Zot!\n"
      "You feel the power of the Orb interfering with translocations here.",
      BLACK, BLACK, // set per-map
      'Z', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {BRANCH_DEPTHS} },
#if TAG_MAJOR_VERSION == 34

    { BRANCH_FOREST, BRANCH_VAULTS, 2, 3, 5, 19,
      brflag::spotty,
      DNGN_ENTER_FOREST, DNGN_EXIT_FOREST, NUM_FEATURES,
      "Forest", "the Enchanted Forest", "Forest",
      nullptr,
      BROWN, BROWN,
      'F', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif

    { BRANCH_ABYSS, NUM_BRANCHES, -1, -1, 7, 24,
      brflag::no_x_level_travel | brflag::no_map | brflag::no_items,
      DNGN_ENTER_ABYSS, DNGN_EXIT_ABYSS, DNGN_FLOOR, // can't get trapped in abyss
      "Abyss", "the Abyss", "Abyss",
      nullptr,
      BLACK, BLACK, // set specially
      'J', { RUNE_ABYSSAL }, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },

    { BRANCH_PANDEMONIUM, NUM_BRANCHES, -1, -1, 1, 24,
      brflag::no_x_level_travel,
      DNGN_ENTER_PANDEMONIUM, DNGN_EXIT_PANDEMONIUM, DNGN_TRANSIT_PANDEMONIUM,
      "Pandemonium", "Pandemonium", "Pan",
      "You enter the halls of Pandemonium!\n"
      "To return, you must find a gate leading back.",
      BLACK, BLACK, // set specially
      'R', { RUNE_DEMONIC, RUNE_MNOLEG, RUNE_LOM_LOBON, RUNE_CEREBOV,
             RUNE_GLOORX_VLOQ }, branch_noise::normal, 8,
      {} },

    { BRANCH_ZIGGURAT, BRANCH_DEPTHS, 1, 4, 27, 27,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_ZIGGURAT, DNGN_EXIT_ZIGGURAT, DNGN_FLOOR,
      "Ziggurat", "a ziggurat", "Zig",
      "You land on top of a ziggurat so tall you cannot make out the ground.",
      BLACK, BLACK,
      'Q', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#if TAG_MAJOR_VERSION == 34

    { BRANCH_LABYRINTH, NUM_BRANCHES, -1, -1, 1, 15,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_LABYRINTH, DNGN_EXIT_LABYRINTH, DNGN_EXIT_THROUGH_ABYSS,
      "Labyrinth", "a Labyrinth", "Lab",
      "You enter a labyrinth!",
      BLACK, BLACK,
      '0', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif

    { BRANCH_BAZAAR, NUM_BRANCHES, -1, -1, 1, 18,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_BAZAAR, DNGN_EXIT_BAZAAR, NUM_FEATURES,
      "Bazaar", "a bazaar", "Bazaar",
      "You enter an inter-dimensional bazaar!",
      BLUE, YELLOW,
      '1', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },

    { BRANCH_TROVE, NUM_BRANCHES, -1, -1, 1, 18,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_TROVE, DNGN_EXIT_TROVE, NUM_FEATURES,
      "Trove", "a treasure trove", "Trove",
      "You enter a treasure trove!",
      DARKGREY, BLUE,
      '2', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },

    { BRANCH_SEWER, NUM_BRANCHES, -1, -1, 1, 4,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_SEWER, DNGN_EXIT_SEWER, NUM_FEATURES,
      "Sewer", "a sewer", "Sewer",
      "You enter a sewer!",
      LIGHTGREY, BLUE,
      '3', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },

    { BRANCH_OSSUARY, NUM_BRANCHES, -1, -1, 1, 6,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_OSSUARY, DNGN_EXIT_OSSUARY, NUM_FEATURES,
      "Ossuary", "an ossuary", "Ossuary",
      "You enter an ossuary!",
      WHITE, YELLOW,
      '4', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },

    { BRANCH_BAILEY, NUM_BRANCHES, -1, -1, 1, 11,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_BAILEY, DNGN_EXIT_BAILEY, NUM_FEATURES,
      "Bailey", "a bailey", "Bailey",
      "You enter a bailey!",
      WHITE, LIGHTRED,
      '5', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#if TAG_MAJOR_VERSION > 34

    { BRANCH_GAUNTLET, NUM_BRANCHES, -1, -1, 1, 15,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_GAUNTLET, DNGN_EXIT_GAUNTLET, DNGN_EXIT_THROUGH_ABYSS,
      "Gauntlet", "a Gauntlet", "Gauntlet",
      "You enter a gauntlet!",
      BLACK, BLACK,
      '6', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif

    { BRANCH_ICE_CAVE, NUM_BRANCHES, -1, -1, 1, 15,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_ICE_CAVE, DNGN_EXIT_ICE_CAVE, NUM_FEATURES,
      "Ice Cave", "an ice cave", "IceCv",
      "You enter an ice cave!",
      BLUE, WHITE,
#if TAG_MAJOR_VERSION == 34
      '6', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif
#if TAG_MAJOR_VERSION > 34
      '7', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif

    { BRANCH_VOLCANO, NUM_BRANCHES, -1, -1, 1, 14,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_VOLCANO, DNGN_EXIT_VOLCANO, NUM_FEATURES,
      "Volcano", "a volcano", "Volcano",
      "You enter a volcano!",
      RED, RED,
#if TAG_MAJOR_VERSION == 34
      '7', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif
#if TAG_MAJOR_VERSION > 34
      '8', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif

    { BRANCH_WIZLAB, NUM_BRANCHES, -1, -1, 1, 24,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_WIZLAB, DNGN_EXIT_WIZLAB, NUM_FEATURES,
      "Wizlab", "a wizard's laboratory", "WizLab",
      "You enter a wizard's laboratory!",
      LIGHTGREY, BROWN, // set per-map
#if TAG_MAJOR_VERSION == 34
      '8', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif
#if TAG_MAJOR_VERSION > 34
      '9', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif
#if TAG_MAJOR_VERSION == 34

    { BRANCH_DEPTHS, BRANCH_DUNGEON, 15, 15, 4, 22,
      brflag::none,
      DNGN_ENTER_DEPTHS, DNGN_EXIT_DEPTHS, NUM_FEATURES,
      "Depths", "the Depths", "Depths",
      nullptr,
      LIGHTGREY, BROWN,
      'U', {}, branch_noise::normal, 10,
      {BRANCH_VAULTS, BRANCH_SLIME} },
#endif

    { BRANCH_DESOLATION, NUM_BRANCHES, -1, -1, 1, 20,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_DESOLATION, DNGN_EXIT_DESOLATION, NUM_FEATURES, // TODO
      "Desolation", "the Desolation of Salt", "Desolation",
      "You enter a great desolation of salt!",
      LIGHTGREY, BROWN, // TODO
#if TAG_MAJOR_VERSION == 34
      '9', {}, branch_noise::loud, DEFAULT_MON_DIE_SIZE,
      {} },
#endif
#if TAG_MAJOR_VERSION > 34
      '0', {}, branch_noise::loud, DEFAULT_MON_DIE_SIZE,
      {} },
#endif
#if TAG_MAJOR_VERSION == 34

    { BRANCH_GAUNTLET, NUM_BRANCHES, -1, -1, 1, 15,
      brflag::no_x_level_travel | brflag::no_items,
      DNGN_ENTER_GAUNTLET, DNGN_EXIT_GAUNTLET, DNGN_EXIT_THROUGH_ABYSS,
      "Gauntlet", "a Gauntlet", "Gauntlet",
      "You enter a gauntlet!",
      BLACK, BLACK,
      '!', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif

    { BRANCH_ARENA, NUM_BRANCHES, -1, -1, 1, 0,
      brflag::no_x_level_travel | brflag::no_items | brflag::fully_map,
      DNGN_ENTER_ARENA, DNGN_EXIT_ARENA, NUM_FEATURES,
      "Arena", "the Arena", "Arena",
      "You enter Okawaru's Arena!",
      BLACK, BLACK,
#if TAG_MAJOR_VERSION == 34
      '"', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#else
      '!', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {} },
#endif

    { BRANCH_CRUCIBLE, NUM_BRANCHES, -1, -1, 1, 0,
      brflag::no_x_level_travel | brflag::no_items | brflag::fully_map,
      DNGN_ENTER_CRUCIBLE, DNGN_EXIT_CRUCIBLE, NUM_FEATURES,
      "Crucible", "the Crucible of Flesh", "Crucible",
      "You are dragged down into the Crucible of Flesh!",
      BLACK, BLACK,
      '@', {}, branch_noise::normal, DEFAULT_MON_DIE_SIZE,
      {}
    },
};

/*
 * There's a few more constant data structures related to branches in
 * the early parts of branch.cc (Example: danger_branch_order)
 */
