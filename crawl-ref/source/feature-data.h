// symbol and magic_symbol are generally constructed from dchar in
// _create_symbols(). They're necessary in feature_def mostly to make
// the `feature` option work better.

// In the default case, these translations hold:
// map_colour -> seen_colour
// seen_colour -> seen_em_colour
// colour -> em_colour
// So use a macro:
#define COLOURS(colour, map) colour, map, map, colour, map
// And with the default (darkgrey) map colour:
#define COLOUR_IS(colour) COLOURS(colour, DARKGREY)
// And for when colour and map_colour are equal:
#define COLOUR_AND_MAP(colour) COLOURS(colour, colour)
static feature_def feat_defs[] =
{

{
    // feat, name
    DNGN_UNSEEN, "",
    // dchar, symbol, magic_symbol
    NUM_DCHAR_TYPES, 0, 0,
    // colour, map_colour, seen_colour, em_colour, seen_em_colour
    BLACK, DARKGREY, DARKGREY, BLACK, DARKGREY,
    // flags, minimap
    FFT_NONE, MF_UNSEEN,
},

{
    DNGN_EXPLORE_HORIZON, "explore horizon",
    NUM_DCHAR_TYPES, 0, 0,
    COLOUR_IS(BLACK),
    FFT_NONE, MF_UNSEEN,
},

{
    DNGN_CLOSED_DOOR, "closed door",
    DCHAR_DOOR_CLOSED, 0, 0,
    COLOUR_IS(LIGHTGREY),
    FFT_NONE, MF_DOOR,
},

{
    DNGN_RUNED_DOOR, "runed door",
    DCHAR_DOOR_CLOSED, 0, 0,
    COLOUR_AND_MAP(LIGHTBLUE),
    FFT_NONE, MF_DOOR,
},

{
    DNGN_SEALED_DOOR, "sealed door",
    DCHAR_DOOR_CLOSED, 0, 0,
    COLOUR_AND_MAP(LIGHTGREEN),
    FFT_NONE, MF_DOOR,
},

{
    DNGN_TREE, "tree",
    DCHAR_TREE, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(ETC_TREE),
    FFT_NONE, MF_WALL,
},

{
    DNGN_METAL_WALL, "metal wall",
    DCHAR_WALL, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(CYAN),
    FFT_NONE, MF_WALL,
},

{
    DNGN_GREEN_CRYSTAL_WALL, "wall of green crystal",
    DCHAR_WALL, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(GREEN),
    FFT_NONE, MF_WALL,
},

{
    DNGN_ROCK_WALL, "rock wall",
    DCHAR_WALL, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(ETC_ROCK),
    FFT_NONE, MF_WALL,
},

{
    DNGN_SLIMY_WALL, "slime covered rock wall",
    DCHAR_WALL, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(LIGHTGREEN),
    FFT_NONE, MF_WALL,
},

{
    DNGN_STONE_WALL, "stone wall",
    DCHAR_WALL, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(LIGHTGREY),
    FFT_NONE, MF_WALL,
},

{
    DNGN_PERMAROCK_WALL, "unnaturally hard rock wall",
    DCHAR_PERMAWALL, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(ETC_ROCK),
    FFT_NONE, MF_WALL,
},

{
    DNGN_CLEAR_ROCK_WALL, "translucent rock wall",
    DCHAR_WALL, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(LIGHTCYAN),
    FFT_NONE, MF_WALL,
},

{
    DNGN_CLEAR_STONE_WALL, "translucent stone wall",
    DCHAR_WALL, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(LIGHTCYAN),
    FFT_NONE, MF_WALL,
},

{
    DNGN_CLEAR_PERMAROCK_WALL, "translucent unnaturally hard rock wall",
    DCHAR_PERMAWALL, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(LIGHTCYAN),
    FFT_NONE, MF_WALL,
},

{
    DNGN_GRATE, "iron grate",
    DCHAR_GRATE, 0, Options.char_table[ DCHAR_WALL_MAGIC ],
    COLOUR_IS(LIGHTBLUE),
    FFT_NONE, MF_WALL,
},

{
    DNGN_OPEN_SEA, "the open sea",
    DCHAR_WALL, 0, 0,
    COLOUR_IS(BLUE),
    FFT_NONE, MF_DEEP_WATER,
},

{
    DNGN_LAVA_SEA, "the endless lava",
    DCHAR_WALL, 0, 0,
    COLOUR_IS(RED),
    FFT_NONE, MF_LAVA,
},

{
    DNGN_ORCISH_IDOL, "orcish idol",
    DCHAR_STATUE, 0, 0,
    COLOUR_IS(BROWN),
    FFT_NONE, MF_WALL,
},

{
    DNGN_GRANITE_STATUE, "granite statue",
    DCHAR_STATUE, 0, 0,
    COLOUR_IS(DARKGREY),
    FFT_NONE, MF_WALL,
},

{
    DNGN_MALIGN_GATEWAY, "portal to somewhere",
    DCHAR_ARCH, 0, 0,
    COLOURS(ETC_SHIMMER_BLUE, LIGHTGREY),
    FFT_NONE, MF_STAIR_UP,
},

{
    DNGN_LAVA, "some lava",
    DCHAR_WAVY, 0, 0,
    COLOUR_IS(RED),
    FFT_NONE, MF_LAVA,
},

{
    DNGN_DEEP_WATER, "some deep water",
    DCHAR_WAVY, 0, 0,
    COLOUR_IS(BLUE),
    FFT_NONE, MF_DEEP_WATER,
},

{
    DNGN_SHALLOW_WATER, "some shallow water",
    DCHAR_WAVY, 0, 0,
    COLOUR_IS(CYAN),
    FFT_NONE, MF_WATER,
},

{
    DNGN_FLOOR, "floor",
    DCHAR_FLOOR, 0, Options.char_table[ DCHAR_FLOOR_MAGIC ],
    COLOUR_IS(ETC_FLOOR),
    FFT_NONE, MF_FLOOR,
},

#if TAG_MAJOR_VERSION == 34
{
    DNGN_BADLY_SEALED_DOOR, "",
    DCHAR_FLOOR, 0, Options.char_table[ DCHAR_FLOOR_MAGIC ],
    COLOUR_IS(ETC_FLOOR),
    FFT_NONE, MF_FLOOR,
},
#endif

{
    DNGN_EXPIRED_PORTAL, "collapsed entrance",
    DCHAR_FLOOR, 0, Options.char_table[ DCHAR_FLOOR_MAGIC ],
    COLOUR_IS(BROWN),
    FFT_NONE, MF_FLOOR,
},

{
    DNGN_OPEN_DOOR, "open door",
    DCHAR_DOOR_OPEN, 0, 0,
    COLOUR_IS(LIGHTGREY),
    FFT_NONE, MF_DOOR,
},

#define TRAP(enum, name, colour)\
{\
    enum, name,\
    DCHAR_TRAP, 0, 0,\
    COLOUR_AND_MAP(colour),\
    FFT_NONE, MF_TRAP,\
}

TRAP(DNGN_TRAP_MECHANICAL, "mechanical trap", LIGHTCYAN),
TRAP(DNGN_TRAP_TELEPORT, "teleport trap", LIGHTBLUE),
TRAP(DNGN_TRAP_ALARM, "alarm trap", LIGHTRED),
TRAP(DNGN_TRAP_ZOT, "zot trap", LIGHTMAGENTA),
TRAP(DNGN_PASSAGE_OF_GOLUBRIA, "passage of Golubria", GREEN),
TRAP(DNGN_TRAP_SHAFT, "shaft", BROWN),
TRAP(DNGN_TRAP_WEB, "web", LIGHTGREY),

{
    DNGN_UNDISCOVERED_TRAP, "floor",
    DCHAR_FLOOR, 0, Options.char_table[ DCHAR_FLOOR_MAGIC ],
    COLOUR_IS(ETC_FLOOR),
    FFT_NONE, MF_FLOOR,
},

{
    DNGN_ENTER_SHOP, "shop",
    DCHAR_ARCH, 0, 0,
    YELLOW, LIGHTGREY, YELLOW, YELLOW, LIGHTGREY,
    FFT_NOTABLE, MF_FEATURE,
},

{
    DNGN_ABANDONED_SHOP, "abandoned shop",
    DCHAR_ARCH, 0, 0,
    COLOUR_AND_MAP(LIGHTGREY),
    FFT_NONE, MF_FLOOR,
},

{
    DNGN_STONE_ARCH, "empty arch of ancient stone",
    DCHAR_ARCH, 0, 0,
    COLOUR_AND_MAP(LIGHTGREY),
    FFT_NONE, MF_FLOOR,
},

{
    DNGN_UNKNOWN_PORTAL, "detected shop or portal",
    DCHAR_ARCH, 0, 0,
    COLOURS(BLACK, LIGHTGREY),
    FFT_NONE, MF_PORTAL,
},

#define STONE_STAIRS_DOWN(num)\
{\
    DNGN_STONE_STAIRS_DOWN_##num, "stone staircase leading down",\
    DCHAR_STAIRS_DOWN, 0, 0,\
    RED, RED, RED, WHITE, WHITE,\
    FFT_NONE, MF_STAIR_DOWN,\
}

STONE_STAIRS_DOWN(I),
STONE_STAIRS_DOWN(II),
STONE_STAIRS_DOWN(III),

#define STONE_STAIRS_UP(num)\
{\
    DNGN_STONE_STAIRS_UP_##num, "stone staircase leading up",\
    DCHAR_STAIRS_UP, 0, 0,\
    GREEN, GREEN, GREEN, WHITE, WHITE,\
    FFT_NONE, MF_STAIR_UP,\
}

STONE_STAIRS_UP(I),
STONE_STAIRS_UP(II),
STONE_STAIRS_UP(III),

{
    DNGN_ESCAPE_HATCH_DOWN, "escape hatch in the floor",
    DCHAR_STAIRS_DOWN, 0, 0,
    COLOUR_AND_MAP(BROWN),
    FFT_NONE, MF_STAIR_DOWN,
},

{
    DNGN_ESCAPE_HATCH_UP, "escape hatch in the ceiling",
    DCHAR_STAIRS_UP, 0, 0,
    COLOUR_AND_MAP(BROWN),
    FFT_NONE, MF_STAIR_UP,
},

{
    DNGN_EXIT_LABYRINTH, "escape hatch in the ceiling",
    DCHAR_STAIRS_UP, 0, 0,
    COLOUR_AND_MAP(BROWN),
    FFT_NONE, MF_STAIR_UP,
},

{
    DNGN_ENTER_LABYRINTH, "labyrinth entrance",
    DCHAR_ARCH, 0, 0,
    ETC_SHIMMER_BLUE, LIGHTGREY, ETC_SHIMMER_BLUE, ETC_SHIMMER_BLUE, ETC_SHIMMER_BLUE,
    (FFT_NOTABLE | FFT_EXAMINE_HINT), MF_PORTAL,
},

#define PORTAL_ENTRANCE(enum, name, colour)\
{\
    enum, name,\
    DCHAR_ARCH, 0, 0,\
    colour, LIGHTGREY, colour, colour, colour,\
    FFT_NOTABLE, MF_PORTAL,\
}

#define PORTAL_EXIT(enum, name, colour)\
{\
    enum, name,\
    DCHAR_ARCH, 0, 0,\
    colour, LIGHTGREY, colour, colour, colour,\
    FFT_NONE, MF_PORTAL,\
}

PORTAL_ENTRANCE(DNGN_ENTER_DIS, "gateway to the Iron City of Dis", CYAN),
PORTAL_ENTRANCE(DNGN_ENTER_GEHENNA, "gateway to the ashen valley of Gehenna", RED),
PORTAL_ENTRANCE(DNGN_ENTER_COCYTUS, "gateway to the freezing wastes of Cocytus", LIGHTCYAN),
PORTAL_ENTRANCE(DNGN_ENTER_TARTARUS, "gateway to the decaying netherworld of Tartarus", MAGENTA),
PORTAL_ENTRANCE(DNGN_ENTER_HELL, "gateway to Hell", RED),
PORTAL_EXIT(DNGN_EXIT_HELL, "gateway back into the Dungeon", LIGHTRED),

PORTAL_ENTRANCE(DNGN_ENTER_ABYSS, "one-way gate to the infinite horrors of the Abyss", ETC_WARP),
PORTAL_ENTRANCE(DNGN_EXIT_THROUGH_ABYSS, "exit through the horrors of the Abyss", ETC_WARP),
PORTAL_EXIT(DNGN_EXIT_ABYSS, "gateway leading out of the Abyss", ETC_WARP),

PORTAL_ENTRANCE(DNGN_ENTER_PANDEMONIUM, "one-way gate leading to the halls of Pandemonium", LIGHTBLUE),
PORTAL_EXIT(DNGN_TRANSIT_PANDEMONIUM, "gate leading to another region of Pandemonium", LIGHTGREEN),
PORTAL_EXIT(DNGN_EXIT_PANDEMONIUM, "gate leading out of Pandemonium", LIGHTBLUE),

PORTAL_ENTRANCE(DNGN_ENTER_VAULTS, "gate to the Vaults", LIGHTGREEN),
PORTAL_EXIT(DNGN_RETURN_FROM_VAULTS, "gate leading back out of this place", ETC_SHIMMER_BLUE),

PORTAL_ENTRANCE(DNGN_ENTER_ZOT, "gate to the Realm of Zot", MAGENTA),
PORTAL_EXIT(DNGN_RETURN_FROM_ZOT, "gate leading back out of this place", MAGENTA),

#if TAG_MAJOR_VERSION == 34
PORTAL_ENTRANCE(DNGN_ENTER_PORTAL_VAULT, "gate leading to a distant place", ETC_SHIMMER_BLUE),
#endif
PORTAL_ENTRANCE(DNGN_ENTER_ZIGGURAT, "gateway to a ziggurat", ETC_SHIMMER_BLUE),
PORTAL_ENTRANCE(DNGN_ENTER_BAZAAR, "gateway to a bazaar", ETC_SHIMMER_BLUE),
PORTAL_ENTRANCE(DNGN_ENTER_TROVE, "portal to a secret trove of treasure", BLUE),
PORTAL_ENTRANCE(DNGN_ENTER_SEWER, "glowing drain", LIGHTGREEN),
PORTAL_ENTRANCE(DNGN_ENTER_OSSUARY, "sand-covered staircase", BROWN),
PORTAL_ENTRANCE(DNGN_ENTER_BAILEY, "flagged portal", LIGHTRED),
PORTAL_ENTRANCE(DNGN_ENTER_ICE_CAVE, "frozen archway", WHITE),
PORTAL_ENTRANCE(DNGN_ENTER_VOLCANO, "dark tunnel", RED),
PORTAL_ENTRANCE(DNGN_ENTER_WIZLAB, "magical portal", ETC_SHIMMER_BLUE),
PORTAL_ENTRANCE(DNGN_UNUSED_ENTER_PORTAL_1, "", ETC_SHIMMER_BLUE),
#if TAG_MAJOR_VERSION == 34
PORTAL_EXIT(DNGN_EXIT_PORTAL_VAULT, "gate leading back out of here", ETC_SHIMMER_BLUE),
#endif
PORTAL_EXIT(DNGN_EXIT_ZIGGURAT, "gate leading back out of here", ETC_SHIMMER_BLUE),
PORTAL_EXIT(DNGN_EXIT_BAZAAR, "gate leading back out of here", ETC_SHIMMER_BLUE),
PORTAL_EXIT(DNGN_EXIT_TROVE, "gate leading back out of here", BLUE),
PORTAL_EXIT(DNGN_EXIT_SEWER, "gate leading back out of here", BROWN),
PORTAL_EXIT(DNGN_EXIT_OSSUARY, "gate leading back out of here", BROWN),
PORTAL_EXIT(DNGN_EXIT_BAILEY, "gate leading back out of here", ETC_SHIMMER_BLUE),
PORTAL_EXIT(DNGN_EXIT_ICE_CAVE, "ice covered gate leading back out of here", WHITE),
PORTAL_EXIT(DNGN_EXIT_VOLCANO, "rocky tunnel leading out of this place", RED),
PORTAL_EXIT(DNGN_EXIT_WIZLAB, "portal leading out of here", ETC_SHIMMER_BLUE),
PORTAL_EXIT(DNGN_UNUSED_EXIT_PORTAL_1, "", ETC_SHIMMER_BLUE),

#define BRANCH_ENTRANCE(enum, name)\
{\
    enum, name,\
    DCHAR_STAIRS_DOWN, 0, 0,\
    YELLOW, RED, YELLOW, YELLOW, YELLOW,\
    FFT_NOTABLE, MF_STAIR_BRANCH,\
}

#define BRANCH_EXIT(enum, name)\
{\
    enum, name,\
    DCHAR_STAIRS_UP, 0, 0,\
    YELLOW, GREEN, YELLOW, YELLOW, YELLOW,\
    FFT_NONE, MF_STAIR_UP,\
}

{
    DNGN_EXIT_DUNGEON, "staircase leading out of the dungeon",
    DCHAR_STAIRS_UP, 0, 0,
    LIGHTBLUE, GREEN, LIGHTBLUE, LIGHTBLUE, LIGHTBLUE,
    FFT_NONE, MF_STAIR_UP,
},

{
    DNGN_ENTER_SLIME, "staircase to the Slime Pits",
    DCHAR_STAIRS_DOWN, 0, 0,
    YELLOW, RED, YELLOW, YELLOW, YELLOW,
    (FFT_NOTABLE | FFT_EXAMINE_HINT), MF_STAIR_BRANCH,
},
BRANCH_EXIT(DNGN_RETURN_FROM_SLIME, "staircase back to the Lair"),

{
    DNGN_ENTER_ORC, "staircase to the Orcish Mines",
    DCHAR_STAIRS_DOWN, 0, 0,
    YELLOW, RED, YELLOW, YELLOW, YELLOW,
    (FFT_NOTABLE | FFT_EXAMINE_HINT), MF_STAIR_BRANCH,
},
BRANCH_EXIT(DNGN_RETURN_FROM_ORC, "staircase back to the Dungeon"),

#if TAG_MAJOR_VERSION == 34
BRANCH_ENTRANCE(DNGN_ENTER_DWARF, "staircase to the Dwarven Hall"),
BRANCH_ENTRANCE(DNGN_ENTER_FOREST, "staircase to the Enchanted Forest"),
BRANCH_ENTRANCE(DNGN_ENTER_BLADE, "staircase to the Hall of Blades"),
BRANCH_EXIT(DNGN_RETURN_FROM_DWARF, "staircase back to the Vaults"),
BRANCH_EXIT(DNGN_RETURN_FROM_FOREST, "staircase back to the Vaults"),
BRANCH_EXIT(DNGN_RETURN_FROM_BLADE, "staircase back to the Vaults"),
#endif

BRANCH_ENTRANCE(DNGN_ENTER_LAIR, "staircase to the Lair"),
BRANCH_EXIT(DNGN_RETURN_FROM_LAIR, "staircase back to the Dungeon"),

BRANCH_ENTRANCE(DNGN_ENTER_CRYPT, "staircase to the Crpyt"),
BRANCH_EXIT(DNGN_RETURN_FROM_CRYPT, "staircase back to the Vaults"),

BRANCH_ENTRANCE(DNGN_ENTER_TEMPLE, "staircase to the Ecumenical Temple"),
BRANCH_EXIT(DNGN_RETURN_FROM_TEMPLE, "staircase back to the Dungeon"),

BRANCH_ENTRANCE(DNGN_ENTER_SNAKE, "staircase to the Snake Pit"),
BRANCH_EXIT(DNGN_RETURN_FROM_SNAKE, "staircase back to the Lair"),

BRANCH_ENTRANCE(DNGN_ENTER_ELF, "staircase to the Elven Halls"),
BRANCH_EXIT(DNGN_RETURN_FROM_ELF, "staircase back to the Mines"),

BRANCH_ENTRANCE(DNGN_ENTER_TOMB, "staircase to the Tomb"),
BRANCH_EXIT(DNGN_RETURN_FROM_TOMB, "staircase back to the Crypt"),

BRANCH_ENTRANCE(DNGN_ENTER_SWAMP, "staircase to the Swamp"),
BRANCH_EXIT(DNGN_RETURN_FROM_SWAMP, "staircase back to the Lair"),

BRANCH_ENTRANCE(DNGN_ENTER_SHOALS, "staircase to the Shoals"),
BRANCH_EXIT(DNGN_RETURN_FROM_SHOALS, "staircase back to the Lair"),

BRANCH_ENTRANCE(DNGN_ENTER_SPIDER, "hole to the Spider Nest"),
BRANCH_EXIT(DNGN_RETURN_FROM_SPIDER, "crawl-hole back to the Lair"),

BRANCH_ENTRANCE(DNGN_ENTER_DEPTHS, "staircase to the Depths"),
BRANCH_EXIT(DNGN_RETURN_FROM_DEPTHS, "staircase back to the Dungeon"),

#define ALTAR(enum, name, colour)\
{\
    enum, name,\
    DCHAR_ALTAR, 0, 0,\
    colour, DARKGREY, colour, colour, colour,\
    FFT_NOTABLE, MF_FEATURE,\
}

ALTAR(DNGN_UNKNOWN_ALTAR, "detected altar", BLACK),
ALTAR(DNGN_ALTAR_ZIN, "glowing silver altar of Zin", LIGHTGREY),
ALTAR(DNGN_ALTAR_SHINING_ONE, "glowing golden altar of the Shining One", YELLOW),
ALTAR(DNGN_ALTAR_KIKUBAAQUDGHA, "ancient bone altar of Kikubaaqudgha", DARKGREY),
ALTAR(DNGN_ALTAR_YREDELEMNUL, "basalt altar of Yredelemnul", ETC_UNHOLY),
ALTAR(DNGN_ALTAR_XOM, "shimmering altar of Xom", ETC_RANDOM),
ALTAR(DNGN_ALTAR_VEHUMET, "radiant altar of Vehumet", ETC_VEHUMET),
ALTAR(DNGN_ALTAR_OKAWARU, "iron altar of Okawaru", CYAN),
ALTAR(DNGN_ALTAR_MAKHLEB, "burning altar of Makhleb", ETC_FIRE),
ALTAR(DNGN_ALTAR_SIF_MUNA, "deep blue altar of Sif Muna", BLUE),
ALTAR(DNGN_ALTAR_TROG, "bloodstained altar of Trog", RED),
ALTAR(DNGN_ALTAR_NEMELEX_XOBEH, "sparkling altar of Nemelex Xobeh", LIGHTMAGENTA),
ALTAR(DNGN_ALTAR_ELYVILON, "white marble altar of Elyvilon", WHITE),
ALTAR(DNGN_ALTAR_LUGONU, "corrupted altar of Lugonu", MAGENTA),
ALTAR(DNGN_ALTAR_BEOGH, "roughly hewn altar of Beogh", ETC_BEOGH),
ALTAR(DNGN_ALTAR_JIYVA, "viscous altar of Jiyva", ETC_SLIME),
ALTAR(DNGN_ALTAR_FEDHAS, "blossoming altar of Fedhas", GREEN),
ALTAR(DNGN_ALTAR_CHEIBRIADOS, "snail-covered altar of Cheibriados", LIGHTCYAN),
ALTAR(DNGN_ALTAR_ASHENZARI, "shattered altar of Ashenzari", LIGHTRED),
ALTAR(DNGN_ALTAR_DITHMENOS, "shadowy altar of Dithmenos", ETC_DITHMENOS),
ALTAR(DNGN_ALTAR_GOZAG, "opulent altar of Gozag", ETC_GOLD), // for the Gold God!
ALTAR(DNGN_ALTAR_QAZLAL, "stormy altar of Qazlal", ETC_ELEMENTAL),

#define FOUNTAIN(enum, name, colour)\
{\
    enum, name,\
    DCHAR_FOUNTAIN, 0, 0,\
    COLOUR_IS(colour),\
    FFT_NONE, MF_FEATURE,\
}
FOUNTAIN(DNGN_FOUNTAIN_BLUE, "fountain of clear blue water", BLUE),
FOUNTAIN(DNGN_FOUNTAIN_SPARKLING, "fountain of sparkling water", LIGHTBLUE),
FOUNTAIN(DNGN_FOUNTAIN_BLOOD, "fountain of blood", RED),
FOUNTAIN(DNGN_DRY_FOUNTAIN, "dry fountain", LIGHTGREY),
#if TAG_MAJOR_VERSION == 34
FOUNTAIN(DNGN_DRY_FOUNTAIN_BLUE, "dry fountain", LIGHTGREY),
FOUNTAIN(DNGN_DRY_FOUNTAIN_SPARKLING, "dry fountain", LIGHTGREY),
FOUNTAIN(DNGN_DRY_FOUNTAIN_BLOOD, "dry fountain", LIGHTGREY),
#endif

{
    DNGN_TELEPORTER, "short-range portal",
    DCHAR_TELEPORTER, 0, 0,
    COLOUR_AND_MAP(YELLOW),
    FFT_NONE, MF_FEATURE,
},

{
    DNGN_SEALED_STAIRS_UP, "sealed passage leading up",
    DCHAR_STAIRS_UP, 0, 0,
    COLOUR_AND_MAP(LIGHTGREEN),
    FFT_NONE, MF_STAIR_UP,
},

{
    DNGN_SEALED_STAIRS_DOWN, "sealed passage leading down",
    DCHAR_STAIRS_DOWN, 0, 0,
    COLOUR_AND_MAP(LIGHTGREEN),
    FFT_NONE, MF_STAIR_DOWN,
},

{
    DNGN_ABYSSAL_STAIR, "gateway leading deeper into the Abyss",
    DCHAR_STAIRS_DOWN, 0, 0,
    COLOUR_AND_MAP(LIGHTCYAN),
    FFT_NONE, MF_STAIR_BRANCH,
},

};
