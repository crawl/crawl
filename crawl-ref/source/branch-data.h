#include "colour.h"

const Branch branches[NUM_BRANCHES] =
{
    // Branch struct:
    //  branch id, parent branch, mindepth, maxdepth, depth, absdepth,
    //  branch flags
    //  entry stairs, exit stairs, short name, long name, abbrev name
    //  entry message
    //  floor colour, rock colour
    //  travel shortcut, runes, ambient noise level

    { BRANCH_DUNGEON, NUM_BRANCHES, 0, 0, 15, 0,
      BFLAG_NONE,
      NUM_FEATURES, DNGN_EXIT_DUNGEON, NUM_FEATURES,
      "Dungeon", "the Dungeon", "D",
      nullptr,
      LIGHTGREY, BROWN,
      'D', {}, 0 },

    { BRANCH_TEMPLE, BRANCH_DUNGEON, 4, 7, 1, 5,
      BFLAG_NO_ITEMS,
      DNGN_ENTER_TEMPLE, DNGN_EXIT_TEMPLE, NUM_FEATURES,
      "Temple", "the Ecumenical Temple", "Temple",
      nullptr,
      LIGHTGREY, BROWN,
      'T', {}, 0 },

    { BRANCH_ORC, BRANCH_DUNGEON, 9, 12, 2, 10,
      BFLAG_SPOTTY,
      DNGN_ENTER_ORC, DNGN_EXIT_ORC, NUM_FEATURES,
      "Orcish Mines", "the Orcish Mines", "Orc",
      nullptr,
      BROWN, BROWN,
      'O', {}, 0 },

    { BRANCH_ELF, BRANCH_ORC, 2, 2, 3, 15,
      BFLAG_DANGEROUS_END,
      DNGN_ENTER_ELF, DNGN_EXIT_ELF, NUM_FEATURES,
      "Elven Halls", "the Elven Halls", "Elf",
      nullptr,
      WHITE, ETC_ELVEN_BRICK,
      'E', {}, 0 },

#if TAG_MAJOR_VERSION == 34
    { BRANCH_DWARF, BRANCH_ELF, -1, -1, 0, 17,
      BFLAG_NONE,
      DNGN_ENTER_DWARF, DNGN_EXIT_DWARF, NUM_FEATURES,
      "Dwarven Hall", "the Dwarven Hall", "Dwarf",
      nullptr,
      BROWN, BROWN,
      'K', {}, 0 },
#endif

    { BRANCH_LAIR, BRANCH_DUNGEON, 8, 11, 6, 10,
      BFLAG_NONE,
      DNGN_ENTER_LAIR, DNGN_EXIT_LAIR, NUM_FEATURES,
      "Lair", "the Lair of Beasts", "Lair",
      nullptr,
      GREEN, BROWN,
      'L', {}, 0 },

    { BRANCH_SWAMP, BRANCH_LAIR, 2, 4, 4, 15,
      BFLAG_DANGEROUS_END | BFLAG_SPOTTY,
      DNGN_ENTER_SWAMP, DNGN_EXIT_SWAMP, NUM_FEATURES,
      "Swamp", "the Swamp", "Swamp",
      nullptr,
      BROWN, BROWN,
      'S', { RUNE_SWAMP }, 0 },

    { BRANCH_SHOALS, BRANCH_LAIR, 2, 4, 4, 15,
      BFLAG_DANGEROUS_END,
      DNGN_ENTER_SHOALS, DNGN_EXIT_SHOALS, NUM_FEATURES,
      "Shoals", "the Shoals", "Shoals",
      nullptr,
      BROWN, BROWN,
      'A', { RUNE_SHOALS }, 6 },

    { BRANCH_SNAKE, BRANCH_LAIR, 2, 4, 4, 15,
      BFLAG_DANGEROUS_END,
      DNGN_ENTER_SNAKE, DNGN_EXIT_SNAKE, NUM_FEATURES,
      "Snake Pit", "the Snake Pit", "Snake",
      nullptr,
      LIGHTGREEN, YELLOW,
      'P', { RUNE_SNAKE }, 0 },

    { BRANCH_SPIDER, BRANCH_LAIR, 2, 4, 4, 15,
      BFLAG_DANGEROUS_END,
      DNGN_ENTER_SPIDER, DNGN_EXIT_SPIDER, NUM_FEATURES,
      "Spider Nest", "the Spider Nest", "Spider",
      nullptr,
      BROWN, YELLOW,
      'N', { RUNE_SPIDER }, 0 },

    { BRANCH_SLIME, BRANCH_LAIR, 5, 6, 5, 17,
      BFLAG_NO_ITEMS | BFLAG_DANGEROUS_END | BFLAG_SPOTTY,
      DNGN_ENTER_SLIME, DNGN_EXIT_SLIME, NUM_FEATURES,
      "Slime Pits", "the Pits of Slime", "Slime",
      nullptr,
      GREEN, BROWN,
      'M', { RUNE_SLIME }, -6 },

    { BRANCH_VAULTS, BRANCH_DUNGEON, 13, 14, 5, 19,
      BFLAG_DANGEROUS_END,
      DNGN_ENTER_VAULTS, DNGN_EXIT_VAULTS, NUM_FEATURES,
      "Vaults", "the Vaults", "Vaults",
      nullptr,
      LIGHTGREY, BROWN,
      'V', { RUNE_VAULTS }, 0 },
#if TAG_MAJOR_VERSION == 34
    { BRANCH_BLADE, BRANCH_VAULTS, 3, 4, 1, 21,
      BFLAG_NO_ITEMS,
      DNGN_ENTER_BLADE, DNGN_EXIT_BLADE, NUM_FEATURES,
      "Hall of Blades", "the Hall of Blades", "Blade",
      nullptr,
      LIGHTGREY, BROWN,
      'B', {}, -7 },
#endif

    { BRANCH_CRYPT, BRANCH_VAULTS, 2, 3, 3, 19,
      BFLAG_DANGEROUS_END,
      DNGN_ENTER_CRYPT, DNGN_EXIT_CRYPT, NUM_FEATURES,
      "Crypt", "the Crypt", "Crypt",
      nullptr,
      LIGHTGREY, BROWN,
      'C', {}, -6 },

    { BRANCH_TOMB, BRANCH_CRYPT, 3, 3, 3, 21,
      BFLAG_ISLANDED | BFLAG_DANGEROUS_END | BFLAG_NO_SHAFTS,
      DNGN_ENTER_TOMB, DNGN_EXIT_TOMB, NUM_FEATURES,
      "Tomb", "the Tomb of the Ancients", "Tomb",
      nullptr,
      BROWN, BROWN,
      'W', { RUNE_TOMB }, -6 },

    { BRANCH_VESTIBULE, NUM_BRANCHES, 27, 27, 1, 27,
      BFLAG_NO_ITEMS,
      DNGN_ENTER_HELL, DNGN_EXIT_HELL, NUM_FEATURES,
      "Hell", "the Vestibule of Hell", "Hell",
      "Welcome to Hell!\nPlease enjoy your stay.",
      LIGHTGREY, LIGHTRED,
      'H', {}, 0 },

    { BRANCH_DIS, BRANCH_VESTIBULE, 1, 1, 7, 28,
      BFLAG_NO_ITEMS | BFLAG_DANGEROUS_END,
      DNGN_ENTER_DIS, DNGN_ENTER_HELL, DNGN_ENTER_HELL,
      "Dis", "the Iron City of Dis", "Dis",
      nullptr,
      CYAN, BROWN,
      'I', { RUNE_DIS }, 0 },

    { BRANCH_GEHENNA, BRANCH_VESTIBULE, 1, 1, 7, 28,
      BFLAG_NO_ITEMS | BFLAG_DANGEROUS_END,
      DNGN_ENTER_GEHENNA, DNGN_ENTER_HELL, DNGN_ENTER_HELL,
      "Gehenna", "Gehenna", "Geh",
      nullptr,
      BROWN, RED,
      'G', { RUNE_GEHENNA }, 0 },

    { BRANCH_COCYTUS, BRANCH_VESTIBULE, 1, 1, 7, 28,
      BFLAG_NO_ITEMS | BFLAG_DANGEROUS_END,
      DNGN_ENTER_COCYTUS, DNGN_ENTER_HELL, DNGN_ENTER_HELL,
      "Cocytus", "Cocytus", "Coc",
      nullptr,
      LIGHTBLUE, LIGHTCYAN,
      'X', { RUNE_COCYTUS }, 0 },

    { BRANCH_TARTARUS, BRANCH_VESTIBULE, 1, 1, 7, 28,
      BFLAG_NO_ITEMS | BFLAG_DANGEROUS_END,
      DNGN_ENTER_TARTARUS, DNGN_ENTER_HELL, DNGN_ENTER_HELL,
      "Tartarus", "Tartarus", "Tar",
      nullptr,
      MAGENTA, MAGENTA,
      'Y', { RUNE_TARTARUS }, 0 },

    { BRANCH_ZOT, BRANCH_DEPTHS, 5, 5, 5, 27,
      BFLAG_DANGEROUS_END,
      DNGN_ENTER_ZOT, DNGN_EXIT_ZOT, NUM_FEATURES,
      "Zot", "the Realm of Zot", "Zot",
      nullptr,
      BLACK, BLACK, // set per-map
      'Z', {}, 0 },
#if TAG_MAJOR_VERSION == 34
    { BRANCH_FOREST, BRANCH_VAULTS, 2, 3, 5, 19,
      BFLAG_SPOTTY,
      DNGN_ENTER_FOREST, DNGN_EXIT_FOREST, NUM_FEATURES,
      "Forest", "the Enchanted Forest", "Forest",
      nullptr,
      BROWN, BROWN,
      'F', {}, 0 },
#endif

    { BRANCH_ABYSS, NUM_BRANCHES, -1, -1, 5, 24,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_MAP,
      DNGN_ENTER_ABYSS, DNGN_EXIT_ABYSS, DNGN_FLOOR, // can't get trapped in abyss
      "Abyss", "the Abyss", "Abyss",
      nullptr,
      BLACK, BLACK, // set specially
      'J', { RUNE_ABYSSAL }, 0 },

    { BRANCH_PANDEMONIUM, NUM_BRANCHES, -1, -1, 1, 24,
      BFLAG_NO_XLEV_TRAVEL,
      DNGN_ENTER_PANDEMONIUM, DNGN_EXIT_PANDEMONIUM, DNGN_TRANSIT_PANDEMONIUM,
      "Pandemonium", "Pandemonium", "Pan",
      "You enter the halls of Pandemonium!\n"
      "To return, you must find a gate leading back.",
      BLACK, BLACK, // set specially
      'R', { RUNE_DEMONIC, RUNE_MNOLEG, RUNE_LOM_LOBON, RUNE_CEREBOV,
             RUNE_GLOORX_VLOQ }, 0 },

    { BRANCH_ZIGGURAT, BRANCH_DEPTHS, 1, 5, 27, 27,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS,
      DNGN_ENTER_ZIGGURAT, DNGN_EXIT_ZIGGURAT, DNGN_FLOOR,
      "Ziggurat", "a ziggurat", "Zig",
      "You land on top of a ziggurat so tall you cannot make out the ground.",
      BLACK, BLACK,
      'Q', {}, 0 },

    { BRANCH_LABYRINTH, NUM_BRANCHES, -1, -1, 1, 15,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS | BFLAG_NO_MAP,
      DNGN_ENTER_LABYRINTH, DNGN_EXIT_LABYRINTH, DNGN_EXIT_THROUGH_ABYSS,
      "Labyrinth", "a labyrinth", "Lab",
      // XXX: Ideally, we want to hint at the wall rule (rock > metal),
      //      and that the walls can shift occasionally.
      // Are these too long?
      "As you enter the labyrinth, previously moving walls settle noisily into place.\n"
      "You hear the metallic echo of a distant snort before it fades into the rock.",
      BLACK, BLACK,
      '0', {}, 0 },

    { BRANCH_BAZAAR, NUM_BRANCHES, -1, -1, 1, 18,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS,
      DNGN_ENTER_BAZAAR, DNGN_EXIT_BAZAAR, NUM_FEATURES,
      "Bazaar", "a bazaar", "Bazaar",
      "You enter an inter-dimensional bazaar!",
      BLUE, YELLOW,
      '1', {}, 0 },

    { BRANCH_TROVE, NUM_BRANCHES, -1, -1, 1, 18,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS,
      DNGN_ENTER_TROVE, DNGN_EXIT_TROVE, NUM_FEATURES,
      "Trove", "a treasure trove", "Trove",
      "You enter a treasure trove!",
      DARKGREY, BLUE,
      '2', {}, 0 },

    { BRANCH_SEWER, NUM_BRANCHES, -1, -1, 1, 4,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS,
      DNGN_ENTER_SEWER, DNGN_EXIT_SEWER, NUM_FEATURES,
      "Sewer", "a sewer", "Sewer",
      "You enter a sewer!",
      LIGHTGREY, BLUE,
      '3', {}, 0 },

    { BRANCH_OSSUARY, NUM_BRANCHES, -1, -1, 1, 6,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS,
      DNGN_ENTER_OSSUARY, DNGN_EXIT_OSSUARY, NUM_FEATURES,
      "Ossuary", "an ossuary", "Ossuary",
      "You enter an ossuary!",
      WHITE, YELLOW,
      '4', {}, 0 },

    { BRANCH_BAILEY, NUM_BRANCHES, -1, -1, 1, 11,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS,
      DNGN_ENTER_BAILEY, DNGN_EXIT_BAILEY, NUM_FEATURES,
      "Bailey", "a bailey", "Bailey",
      "You enter a bailey!",
      WHITE, LIGHTRED,
      '5', {}, 0 },

    { BRANCH_ICE_CAVE, NUM_BRANCHES, -1, -1, 1, 15,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS,
      DNGN_ENTER_ICE_CAVE, DNGN_EXIT_ICE_CAVE, NUM_FEATURES,
      "Ice Cave", "an ice cave", "IceCv",
      "You enter an ice cave!",
      BLUE, WHITE,
      '6', {}, 0 },

    { BRANCH_VOLCANO, NUM_BRANCHES, -1, -1, 1, 14,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS,
      DNGN_ENTER_VOLCANO, DNGN_EXIT_VOLCANO, NUM_FEATURES,
      "Volcano", "a volcano", "Volcano",
      "You enter a volcano!",
      RED, RED,
      '7', {}, 0 },

    { BRANCH_WIZLAB, NUM_BRANCHES, -1, -1, 1, 24,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS,
      DNGN_ENTER_WIZLAB, DNGN_EXIT_WIZLAB, NUM_FEATURES,
      "Wizlab", "a wizard's laboratory", "WizLab",
      "You enter a wizard's laboratory!",
      LIGHTGREY, BROWN, // set per-map
      '8', {}, 0 },

    { BRANCH_DEPTHS, BRANCH_DUNGEON, 15, 15, 5, 22,
      BFLAG_NONE,
      DNGN_ENTER_DEPTHS, DNGN_EXIT_DEPTHS, NUM_FEATURES,
      "Depths", "the Depths", "Depths",
      nullptr,
      LIGHTGREY, BROWN,
      'U', {}, 0 },

    { BRANCH_DESOLATION, NUM_BRANCHES, -1, -1, 1, 20,
      BFLAG_NO_XLEV_TRAVEL | BFLAG_NO_ITEMS,
      DNGN_ENTER_DESOLATION, DNGN_EXIT_DESOLATION, NUM_FEATURES, // TODO
      "Desolation", "the Desolation of Salt", "Desolation",
      "You enter a great desolation of salt!",
      LIGHTGREY, BROWN, // TODO
      '9', {}, 6 },
};
