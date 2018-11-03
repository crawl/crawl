// In the default case, these translations hold:
// unseen_colour -> seen_colour
// seen_colour -> seen_em_colour
// colour -> em_colour
// So use a macro:
#define COLOURS(colour, map) colour, map, map, colour, map
// And with the default (darkgrey) map colour:
#define COLOUR_IS(colour) COLOURS(colour, DARKGREY)
// And for when colour and unseen_colour are equal:
#define COLOUR_AND_MAP(colour) COLOURS(colour, colour)
static feature_def feat_defs[] =
{

{
    // feat, name, vaultname
    DNGN_UNSEEN, "", "unseen",
    // dchar, magic_dchar
    NUM_DCHAR_TYPES, NUM_DCHAR_TYPES,
    // colour, unseen_colour, seen_colour, em_colour, seen_em_colour
    BLACK, DARKGREY, DARKGREY, BLACK, DARKGREY,
    // flags, minimap
    FFT_OPAQUE | FFT_SOLID, MF_UNSEEN,
},

{
    DNGN_EXPLORE_HORIZON, "explore horizon", "explore_horizon",
    NUM_DCHAR_TYPES, NUM_DCHAR_TYPES,
    COLOUR_IS(BLACK),
    FFT_NONE, MF_UNSEEN,
},

{
    DNGN_CLOSED_DOOR, "closed door", "closed_door",
    DCHAR_DOOR_CLOSED, NUM_DCHAR_TYPES,
    COLOUR_IS(LIGHTGREY),
    FFT_OPAQUE | FFT_SOLID, MF_DOOR,
},

{
    DNGN_RUNED_DOOR, "runed door", "runed_door",
    DCHAR_DOOR_CLOSED, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(LIGHTBLUE),
    FFT_OPAQUE | FFT_SOLID | FFT_NOTABLE, MF_DOOR,
},

{
    DNGN_SEALED_DOOR, "sealed door", "sealed_door",
    DCHAR_DOOR_CLOSED, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(LIGHTGREEN),
    FFT_OPAQUE | FFT_SOLID, MF_DOOR,
},

{
    DNGN_TREE, "tree", "tree",
    DCHAR_TREE, DCHAR_WALL_MAGIC,
    COLOUR_IS(ETC_TREE),
    FFT_OPAQUE | FFT_SOLID, MF_WALL,
},

{
    DNGN_METAL_WALL, "metal wall", "metal_wall",
    DCHAR_WALL, DCHAR_WALL_MAGIC,
    COLOUR_IS(CYAN),
    FFT_OPAQUE | FFT_WALL | FFT_SOLID, MF_WALL,
},

{
    DNGN_CRYSTAL_WALL, "crystal wall", "crystal_wall",
    DCHAR_WALL, DCHAR_WALL_MAGIC,
    COLOUR_IS(GREEN),
    FFT_OPAQUE | FFT_WALL | FFT_SOLID, MF_WALL,
},

{
    DNGN_ROCK_WALL, "rock wall", "rock_wall",
    DCHAR_WALL, DCHAR_WALL_MAGIC,
    COLOUR_IS(ETC_ROCK),
    FFT_OPAQUE | FFT_WALL | FFT_SOLID, MF_WALL,
},

{
    DNGN_SLIMY_WALL, "slime covered rock wall", "slimy_wall",
    DCHAR_WALL, DCHAR_WALL_MAGIC,
    COLOUR_IS(LIGHTGREEN),
    FFT_OPAQUE | FFT_WALL | FFT_SOLID, MF_WALL,
},

{
    DNGN_STONE_WALL, "stone wall", "stone_wall",
    DCHAR_WALL, DCHAR_WALL_MAGIC,
    COLOUR_IS(LIGHTGREY),
    FFT_OPAQUE | FFT_WALL | FFT_SOLID, MF_WALL,
},

{
    DNGN_PERMAROCK_WALL, "unnaturally hard rock wall", "permarock_wall",
    DCHAR_PERMAWALL, DCHAR_WALL_MAGIC,
    COLOUR_IS(ETC_ROCK),
    FFT_OPAQUE | FFT_WALL | FFT_SOLID, MF_WALL,
},

{
    DNGN_CLEAR_ROCK_WALL, "translucent rock wall", "clear_rock_wall",
    DCHAR_WALL, DCHAR_WALL_MAGIC,
    COLOUR_IS(LIGHTCYAN),
    FFT_WALL | FFT_SOLID, MF_WALL,
},

{
    DNGN_CLEAR_STONE_WALL, "translucent stone wall", "clear_stone_wall",
    DCHAR_WALL, DCHAR_WALL_MAGIC,
    COLOUR_IS(LIGHTCYAN),
    FFT_WALL | FFT_SOLID, MF_WALL,
},

{
    DNGN_CLEAR_PERMAROCK_WALL, "translucent unnaturally hard rock wall", "clear_permarock_wall",
    DCHAR_PERMAWALL, DCHAR_WALL_MAGIC,
    COLOUR_IS(LIGHTCYAN),
    FFT_WALL | FFT_SOLID, MF_WALL,
},

{
    DNGN_GRATE, "iron grate", "iron_grate",
    DCHAR_GRATE, DCHAR_WALL_MAGIC,
    COLOUR_IS(LIGHTBLUE),
    FFT_SOLID, MF_WALL,
},

{
    DNGN_OPEN_SEA, "the open sea", "open_sea",
    DCHAR_WALL, NUM_DCHAR_TYPES,
    COLOUR_IS(BLUE),
    FFT_SOLID, MF_DEEP_WATER,
},

{
    DNGN_LAVA_SEA, "the endless lava", "endless_lava",
    DCHAR_WALL, NUM_DCHAR_TYPES,
    COLOUR_IS(RED),
    FFT_SOLID, MF_LAVA,
},

{
    DNGN_ENDLESS_SALT, "endless expanse of salt", "endless_salt",
    DCHAR_WALL, NUM_DCHAR_TYPES,
    COLOUR_IS(WHITE),
    FFT_SOLID, MF_FLOOR,
},

{
    DNGN_ORCISH_IDOL, "orcish idol", "orcish_idol",
    DCHAR_STATUE, NUM_DCHAR_TYPES,
    COLOUR_IS(BROWN),
    FFT_SOLID, MF_WALL,
},

{
    DNGN_GRANITE_STATUE, "granite statue", "granite_statue",
    DCHAR_STATUE, NUM_DCHAR_TYPES,
    COLOUR_IS(DARKGREY),
    FFT_SOLID, MF_WALL,
},

{
    DNGN_MALIGN_GATEWAY, "portal to somewhere", "malign_gateway",
    DCHAR_ARCH, NUM_DCHAR_TYPES,
    COLOURS(ETC_SHIMMER_BLUE, LIGHTGREY),
    FFT_SOLID, MF_STAIR_UP,
},

{
    DNGN_LAVA, "some lava", "lava",
    DCHAR_WAVY, NUM_DCHAR_TYPES,
    COLOUR_IS(RED),
    FFT_NONE, MF_LAVA,
},

{
    DNGN_DEEP_WATER, "some deep water", "deep_water",
    DCHAR_WAVY, NUM_DCHAR_TYPES,
    COLOUR_IS(BLUE),
    FFT_NONE, MF_DEEP_WATER,
},

{
    DNGN_SHALLOW_WATER, "some shallow water", "shallow_water",
    DCHAR_WAVY, NUM_DCHAR_TYPES,
    COLOUR_IS(CYAN),
    FFT_NONE, MF_WATER,
},

{
    DNGN_FLOOR, "floor", "floor",
    DCHAR_FLOOR, DCHAR_FLOOR_MAGIC,
    COLOUR_IS(ETC_FLOOR),
    FFT_NONE, MF_FLOOR,
},

#if TAG_MAJOR_VERSION == 34
{
    DNGN_BADLY_SEALED_DOOR, "", "badly_sealed_door",
    DCHAR_FLOOR, DCHAR_FLOOR_MAGIC,
    COLOUR_IS(ETC_FLOOR),
    FFT_NONE, MF_FLOOR,
},
#endif

{
    DNGN_EXPIRED_PORTAL, "collapsed entrance", "expired_portal",
    DCHAR_FLOOR, DCHAR_FLOOR_MAGIC,
    COLOUR_IS(BROWN),
    FFT_NONE, MF_FLOOR,
},

{
    DNGN_OPEN_DOOR, "open door", "open_door",
    DCHAR_DOOR_OPEN, NUM_DCHAR_TYPES,
    COLOUR_IS(LIGHTGREY),
    FFT_NONE, MF_DOOR,
},

#define TRAP(enum, name, vaultname, colour)\
{\
    enum, name, vaultname,\
    DCHAR_TRAP, NUM_DCHAR_TYPES,\
    COLOUR_AND_MAP(colour),\
    FFT_TRAP, MF_TRAP,\
}

TRAP(DNGN_TRAP_MECHANICAL, "mechanical trap", "trap_mechanical", LIGHTCYAN),
TRAP(DNGN_TRAP_DISPERSAL, "disperal trap", "trap_dispersal", MAGENTA),
TRAP(DNGN_TRAP_TELEPORT, "teleport trap", "trap_teleport", LIGHTBLUE),
#if TAG_MAJOR_VERSION == 34
TRAP(DNGN_TRAP_SHADOW, "shadow trap", "trap_shadow", BLUE),
TRAP(DNGN_TRAP_SHADOW_DORMANT, "dormant shadow trap", "trap_shadow_dormant", BLUE),
#endif
TRAP(DNGN_TRAP_ALARM, "alarm trap", "trap_alarm", LIGHTRED),
TRAP(DNGN_TRAP_ZOT, "Zot trap", "trap_zot", LIGHTMAGENTA),
TRAP(DNGN_PASSAGE_OF_GOLUBRIA, "passage of Golubria", "passage of golubria", GREEN),
TRAP(DNGN_TRAP_SHAFT, "shaft", "shaft", BROWN),
TRAP(DNGN_TRAP_WEB, "web", "trap_web", LIGHTGREY),

#if TAG_MAJOR_VERSION == 34
{
    DNGN_UNDISCOVERED_TRAP, "floor", "undiscovered_trap",
    DCHAR_FLOOR, DCHAR_FLOOR_MAGIC,
    COLOUR_IS(ETC_FLOOR),
    FFT_NONE, MF_FLOOR,
},
#endif

{
    DNGN_ENTER_SHOP, "shop", "enter_shop",
    DCHAR_ARCH, NUM_DCHAR_TYPES,
    YELLOW, LIGHTGREY, YELLOW, YELLOW, LIGHTGREY,
    FFT_NOTABLE, MF_FEATURE,
},

{
    DNGN_ABANDONED_SHOP, "abandoned shop", "abandoned_shop",
    DCHAR_ARCH, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(LIGHTGREY),
    FFT_NONE, MF_FLOOR,
},

{
    DNGN_STONE_ARCH, "empty arch of ancient stone", "stone_arch",
    DCHAR_ARCH, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(LIGHTGREY),
    FFT_NONE, MF_FLOOR,
},

{
    DNGN_UNKNOWN_PORTAL, "detected shop or portal", "unknown_portal",
    DCHAR_ARCH, NUM_DCHAR_TYPES,
    COLOURS(BLACK, LIGHTGREY),
    FFT_NONE, MF_PORTAL,
},

#define STONE_STAIRS_DOWN(num, num2)\
{\
    DNGN_STONE_STAIRS_DOWN_##num, "stone staircase leading down", "stone_stairs_down_"#num2,\
    DCHAR_STAIRS_DOWN, NUM_DCHAR_TYPES,\
    RED, RED, RED, WHITE, WHITE,\
    FFT_NONE, MF_STAIR_DOWN,\
}

STONE_STAIRS_DOWN(I, i),
STONE_STAIRS_DOWN(II, ii),
STONE_STAIRS_DOWN(III, iii),

#define STONE_STAIRS_UP(num, num2)\
{\
    DNGN_STONE_STAIRS_UP_##num, "stone staircase leading up", "stone_stairs_up_"#num2,\
    DCHAR_STAIRS_UP, NUM_DCHAR_TYPES,\
    GREEN, GREEN, GREEN, WHITE, WHITE,\
    FFT_NONE, MF_STAIR_UP,\
}

STONE_STAIRS_UP(I, i),
STONE_STAIRS_UP(II, ii),
STONE_STAIRS_UP(III, iii),

{
    DNGN_ESCAPE_HATCH_DOWN, "escape hatch in the floor", "escape_hatch_down",
    DCHAR_STAIRS_DOWN, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(BROWN),
    FFT_NONE, MF_STAIR_DOWN,
},

{
    DNGN_ESCAPE_HATCH_UP, "escape hatch in the ceiling", "escape_hatch_up",
    DCHAR_STAIRS_UP, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(BROWN),
    FFT_NONE, MF_STAIR_UP,
},
#if TAG_MAJOR_VERSION == 34
{
    DNGN_EXIT_LABYRINTH, "escape hatch in the ceiling", "exit_labyrinth",
    DCHAR_STAIRS_UP, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(BROWN),
    FFT_NONE, MF_STAIR_UP,
},

{
    DNGN_ENTER_LABYRINTH, "labyrinth entrance", "enter_labyrinth",
    DCHAR_ARCH, NUM_DCHAR_TYPES,
    ETC_SHIMMER_BLUE, LIGHTGREY, ETC_SHIMMER_BLUE, ETC_SHIMMER_BLUE, ETC_SHIMMER_BLUE,
    (FFT_NOTABLE | FFT_EXAMINE_HINT), MF_PORTAL,
},
#endif

#define PORTAL_ENTRANCE(enum, name, vaultname, colour)\
{\
    enum, name, vaultname,\
    DCHAR_ARCH, NUM_DCHAR_TYPES,\
    colour, LIGHTGREY, colour, colour, colour,\
    FFT_NOTABLE, MF_PORTAL,\
}

#define PORTAL_EXIT(enum, name, vaultname, colour)\
{\
    enum, name, vaultname,\
    DCHAR_ARCH, NUM_DCHAR_TYPES,\
    colour, LIGHTGREY, colour, colour, colour,\
    FFT_NONE, MF_PORTAL,\
}

PORTAL_ENTRANCE(DNGN_ENTER_DIS, "gateway to the Iron City of Dis", "enter_dis", CYAN),
PORTAL_ENTRANCE(DNGN_ENTER_GEHENNA, "gateway to the ashen valley of Gehenna", "enter_gehenna", RED),
PORTAL_ENTRANCE(DNGN_ENTER_COCYTUS, "gateway to the freezing wastes of Cocytus", "enter_cocytus", LIGHTCYAN),
PORTAL_ENTRANCE(DNGN_ENTER_TARTARUS, "gateway to the decaying netherworld of Tartarus", "enter_tartarus", MAGENTA),
PORTAL_ENTRANCE(DNGN_ENTER_HELL, "gateway to Hell", "enter_hell", RED),
PORTAL_EXIT(DNGN_EXIT_HELL, "gateway back into the Dungeon", "exit_hell", LIGHTRED),

PORTAL_ENTRANCE(DNGN_ENTER_ABYSS, "one-way gate to the infinite horrors of the Abyss", "enter_abyss", ETC_WARP),
PORTAL_ENTRANCE(DNGN_EXIT_THROUGH_ABYSS, "exit through the horrors of the Abyss", "exit_through_abyss", ETC_WARP),
PORTAL_EXIT(DNGN_EXIT_ABYSS, "gateway leading out of the Abyss", "exit_abyss", ETC_WARP),

PORTAL_ENTRANCE(DNGN_ENTER_PANDEMONIUM, "one-way gate leading to the halls of Pandemonium", "enter_pandemonium", LIGHTBLUE),
PORTAL_EXIT(DNGN_TRANSIT_PANDEMONIUM, "gate leading to another region of Pandemonium", "transit_pandemonium", LIGHTGREEN),
PORTAL_EXIT(DNGN_EXIT_PANDEMONIUM, "gate leading out of Pandemonium", "exit_pandemonium", LIGHTBLUE),

PORTAL_ENTRANCE(DNGN_ENTER_VAULTS, "gate to the Vaults", "enter_vaults", LIGHTGREEN),
PORTAL_EXIT(DNGN_EXIT_VAULTS, "gate leading back out of this place", "exit_vaults", LIGHTGREEN),

PORTAL_ENTRANCE(DNGN_ENTER_ZOT, "gate to the Realm of Zot", "enter_zot", MAGENTA),
PORTAL_EXIT(DNGN_EXIT_ZOT, "gate leading back out of this place", "exit_zot", MAGENTA),

#if TAG_MAJOR_VERSION == 34
PORTAL_ENTRANCE(DNGN_ENTER_PORTAL_VAULT, "gate leading to a distant place", "enter_portal_vault", ETC_SHIMMER_BLUE),
#endif
PORTAL_ENTRANCE(DNGN_ENTER_ZIGGURAT, "gateway to a ziggurat", "enter_ziggurat", ETC_SHIMMER_BLUE),
PORTAL_ENTRANCE(DNGN_ENTER_BAZAAR, "gateway to a bazaar", "enter_bazaar", ETC_SHIMMER_BLUE),
PORTAL_ENTRANCE(DNGN_ENTER_TROVE, "portal to a secret trove of treasure", "enter_trove", BLUE),
PORTAL_ENTRANCE(DNGN_ENTER_SEWER, "glowing drain", "enter_sewer", LIGHTGREEN),
PORTAL_ENTRANCE(DNGN_ENTER_OSSUARY, "sand-covered staircase", "enter_ossuary", BROWN),
PORTAL_ENTRANCE(DNGN_ENTER_BAILEY, "flagged portal", "enter_bailey", LIGHTRED),
PORTAL_ENTRANCE(DNGN_ENTER_GAUNTLET, "gate leading to a gauntlet", "enter_gauntlet", ETC_SHIMMER_BLUE),
PORTAL_ENTRANCE(DNGN_ENTER_ICE_CAVE, "frozen archway", "enter_ice_cave", WHITE),
PORTAL_ENTRANCE(DNGN_ENTER_VOLCANO, "dark tunnel", "enter_volcano", RED),
PORTAL_ENTRANCE(DNGN_ENTER_WIZLAB, "magical portal", "enter_wizlab", ETC_SHIMMER_BLUE),
PORTAL_ENTRANCE(DNGN_ENTER_DESOLATION, "ruined gateway", "enter_desolation", WHITE),
#if TAG_MAJOR_VERSION == 34
PORTAL_EXIT(DNGN_EXIT_PORTAL_VAULT, "gate leading back out of this place", "exit_portal_vault", ETC_SHIMMER_BLUE),
#endif
PORTAL_EXIT(DNGN_EXIT_ZIGGURAT, "gate leading back out of this place", "exit_ziggurat", ETC_SHIMMER_BLUE),
PORTAL_EXIT(DNGN_EXIT_BAZAAR, "gate leading back out of this place", "exit_bazaar", ETC_SHIMMER_BLUE),
PORTAL_EXIT(DNGN_EXIT_TROVE, "gate leading back out of this place", "exit_trove", BLUE),
PORTAL_EXIT(DNGN_EXIT_SEWER, "gate leading back out of this place", "exit_sewer", BROWN),
PORTAL_EXIT(DNGN_EXIT_OSSUARY, "gate leading back out of this place", "exit_ossuary", BROWN),
PORTAL_EXIT(DNGN_EXIT_BAILEY, "gate leading back out of this place", "exit_bailey", ETC_SHIMMER_BLUE),
PORTAL_EXIT(DNGN_EXIT_GAUNTLET, "gate leading back out of this place", "exit_gauntlet", ETC_SHIMMER_BLUE),
PORTAL_EXIT(DNGN_EXIT_ICE_CAVE, "ice covered gate leading back out of this place", "exit_ice_cave", WHITE),
PORTAL_EXIT(DNGN_EXIT_VOLCANO, "rocky tunnel leading out of this place", "exit_volcano", RED),
PORTAL_EXIT(DNGN_EXIT_WIZLAB, "portal leading out of this place", "exit_wizlab", ETC_SHIMMER_BLUE),
PORTAL_EXIT(DNGN_EXIT_DESOLATION, "gate leading back out of this place", "exit_desolation", WHITE),

#define BRANCH_ENTRANCE(enum, name, vaultname)\
{\
    enum, name, vaultname,\
    DCHAR_STAIRS_DOWN, NUM_DCHAR_TYPES,\
    YELLOW, RED, YELLOW, YELLOW, YELLOW,\
    FFT_NOTABLE, MF_STAIR_BRANCH,\
}

#define BRANCH_EXIT(enum, name, vaultname)\
{\
    enum, name, vaultname,\
    DCHAR_STAIRS_UP, NUM_DCHAR_TYPES,\
    YELLOW, GREEN, YELLOW, YELLOW, YELLOW,\
    FFT_NONE, MF_STAIR_UP,\
}

{
    DNGN_EXIT_DUNGEON, "staircase leading out of the dungeon", "exit_dungeon",
    DCHAR_STAIRS_UP, NUM_DCHAR_TYPES,
    LIGHTBLUE, GREEN, LIGHTBLUE, LIGHTBLUE, LIGHTBLUE,
    FFT_NONE, MF_STAIR_UP,
},

{
    DNGN_ENTER_SLIME, "staircase to the Slime Pits", "enter_slime_pits",
    DCHAR_STAIRS_DOWN, NUM_DCHAR_TYPES,
    YELLOW, RED, YELLOW, YELLOW, YELLOW,
    (FFT_NOTABLE | FFT_EXAMINE_HINT), MF_STAIR_BRANCH,
},
BRANCH_EXIT(DNGN_EXIT_SLIME, "staircase back to the Lair", "exit_slime_pits"),

{
    DNGN_ENTER_ORC, "staircase to the Orcish Mines", "enter_orcish_mines",
    DCHAR_STAIRS_DOWN, NUM_DCHAR_TYPES,
    YELLOW, RED, YELLOW, YELLOW, YELLOW,
    (FFT_NOTABLE | FFT_EXAMINE_HINT), MF_STAIR_BRANCH,
},
BRANCH_EXIT(DNGN_EXIT_ORC, "staircase back to the Dungeon", "exit_orcish_mines"),

#if TAG_MAJOR_VERSION == 34
BRANCH_ENTRANCE(DNGN_ENTER_DWARF, "staircase to the Dwarven Hall", "enter_dwarven_hall"),
BRANCH_ENTRANCE(DNGN_ENTER_FOREST, "staircase to the Enchanted Forest", "enter_forest"),
BRANCH_ENTRANCE(DNGN_ENTER_BLADE, "staircase to the Hall of Blades", "enter_hall_of_blades"),
BRANCH_EXIT(DNGN_EXIT_DWARF, "staircase back to the Vaults", "exit_dwarven_hall"),
BRANCH_EXIT(DNGN_EXIT_FOREST, "staircase back to the Vaults", "exit_forest"),
BRANCH_EXIT(DNGN_EXIT_BLADE, "staircase back to the Vaults", "exit_hall_of_blades"),
#endif

BRANCH_ENTRANCE(DNGN_ENTER_LAIR, "staircase to the Lair", "enter_lair"),
BRANCH_EXIT(DNGN_EXIT_LAIR, "staircase back to the Dungeon", "exit_lair"),

BRANCH_ENTRANCE(DNGN_ENTER_CRYPT, "staircase to the Crypt", "enter_crypt"),
BRANCH_EXIT(DNGN_EXIT_CRYPT, "staircase back to the Vaults", "exit_crypt"),

BRANCH_ENTRANCE(DNGN_ENTER_TEMPLE, "staircase to the Ecumenical Temple", "enter_temple"),
BRANCH_EXIT(DNGN_EXIT_TEMPLE, "staircase back to the Dungeon", "exit_temple"),

BRANCH_ENTRANCE(DNGN_ENTER_SNAKE, "staircase to the Snake Pit", "enter_snake_pit"),
BRANCH_EXIT(DNGN_EXIT_SNAKE, "staircase back to the Lair", "exit_snake_pit"),

BRANCH_ENTRANCE(DNGN_ENTER_ELF, "staircase to the Elven Halls", "enter_elven_halls"),
BRANCH_EXIT(DNGN_EXIT_ELF, "staircase back to the Mines", "exit_elven_halls"),

BRANCH_ENTRANCE(DNGN_ENTER_TOMB, "staircase to the Tomb", "enter_tomb"),
BRANCH_EXIT(DNGN_EXIT_TOMB, "staircase back to the Crypt", "exit_tomb"),

BRANCH_ENTRANCE(DNGN_ENTER_SWAMP, "staircase to the Swamp", "enter_swamp"),
BRANCH_EXIT(DNGN_EXIT_SWAMP, "staircase back to the Lair", "exit_swamp"),

BRANCH_ENTRANCE(DNGN_ENTER_SHOALS, "staircase to the Shoals", "enter_shoals"),
BRANCH_EXIT(DNGN_EXIT_SHOALS, "staircase back to the Lair", "exit_shoals"),

BRANCH_ENTRANCE(DNGN_ENTER_SPIDER, "hole to the Spider Nest", "enter_spider_nest"),
BRANCH_EXIT(DNGN_EXIT_SPIDER, "crawl-hole back to the Lair", "exit_spider_nest"),

BRANCH_ENTRANCE(DNGN_ENTER_DEPTHS, "staircase to the Depths", "enter_depths"),
BRANCH_EXIT(DNGN_EXIT_DEPTHS, "staircase back to the Dungeon", "exit_depths"),

#define ALTAR(enum, name, vaultname, colour)\
{\
    enum, name, vaultname,\
    DCHAR_ALTAR, NUM_DCHAR_TYPES,\
    colour, DARKGREY, colour, colour, colour,\
    FFT_NOTABLE, MF_FEATURE,\
}

ALTAR(DNGN_UNKNOWN_ALTAR, "detected altar", "unknown_altar", DARKGREY),
ALTAR(DNGN_ALTAR_ZIN, "glowing silver altar of Zin", "altar_zin", LIGHTGREY),
ALTAR(DNGN_ALTAR_SHINING_ONE, "glowing golden altar of the Shining One", "altar_the_shining_one", YELLOW),
ALTAR(DNGN_ALTAR_KIKUBAAQUDGHA, "ancient bone altar of Kikubaaqudgha", "altar_kikubaaqudgha", DARKGREY),
ALTAR(DNGN_ALTAR_YREDELEMNUL, "basalt altar of Yredelemnul", "altar_yredelemnul", ETC_UNHOLY),
ALTAR(DNGN_ALTAR_XOM, "shimmering altar of Xom", "altar_xom", ETC_RANDOM),
ALTAR(DNGN_ALTAR_VEHUMET, "radiant altar of Vehumet", "altar_vehumet", ETC_VEHUMET),
ALTAR(DNGN_ALTAR_OKAWARU, "iron altar of Okawaru", "altar_okawaru", CYAN),
ALTAR(DNGN_ALTAR_MAKHLEB, "burning altar of Makhleb", "altar_makhleb", ETC_FIRE),
ALTAR(DNGN_ALTAR_SIF_MUNA, "deep blue altar of Sif Muna", "altar_sif_muna", BLUE),
ALTAR(DNGN_ALTAR_TROG, "bloodstained altar of Trog", "altar_trog", RED),
ALTAR(DNGN_ALTAR_NEMELEX_XOBEH, "sparkling altar of Nemelex Xobeh", "altar_nemelex_xobeh", LIGHTMAGENTA),
ALTAR(DNGN_ALTAR_ELYVILON, "white marble altar of Elyvilon", "altar_elyvilon", WHITE),
ALTAR(DNGN_ALTAR_LUGONU, "corrupted altar of Lugonu", "altar_lugonu", MAGENTA),
ALTAR(DNGN_ALTAR_BEOGH, "roughly hewn altar of Beogh", "altar_beogh", ETC_BEOGH),
ALTAR(DNGN_ALTAR_JIYVA, "viscous altar of Jiyva", "altar_jiyva", ETC_SLIME),
ALTAR(DNGN_ALTAR_FEDHAS, "blossoming altar of Fedhas", "altar_fedhas", GREEN),
ALTAR(DNGN_ALTAR_CHEIBRIADOS, "snail-covered altar of Cheibriados", "altar_cheibriados", LIGHTCYAN),
ALTAR(DNGN_ALTAR_ASHENZARI, "shattered altar of Ashenzari", "altar_ashenzari", LIGHTRED),
ALTAR(DNGN_ALTAR_DITHMENOS, "shadowy altar of Dithmenos", "altar_dithmenos", ETC_DITHMENOS),
ALTAR(DNGN_ALTAR_GOZAG, "opulent altar of Gozag", "altar_gozag", ETC_GOLD), // for the Gold God!
ALTAR(DNGN_ALTAR_QAZLAL, "stormy altar of Qazlal", "altar_qazlal", ETC_ELEMENTAL),
ALTAR(DNGN_ALTAR_RU, "sacrificial altar of Ru", "altar_ru", BROWN),
ALTAR(DNGN_ALTAR_ECUMENICAL, "faded altar of an unknown god", "altar_ecumenical", ETC_DARK),
ALTAR(DNGN_ALTAR_PAKELLAS, "oddly glowing altar of Pakellas", "altar_pakellas", ETC_PAKELLAS),
ALTAR(DNGN_ALTAR_USKAYAW, "hide-covered altar of Uskayaw", "altar_uskayaw", ETC_INCARNADINE),
ALTAR(DNGN_ALTAR_HEPLIAKLQANA, "hazy altar of Hepliaklqana", "altar_hepliaklqana", LIGHTGREEN),
ALTAR(DNGN_ALTAR_WU_JIAN, "ornate altar of the Wu Jian Council", "altar_wu_jian", ETC_WU_JIAN),

#define FOUNTAIN(enum, name, vaultname, colour)\
{\
    enum, name, vaultname,\
    DCHAR_FOUNTAIN, NUM_DCHAR_TYPES,\
    COLOUR_IS(colour),\
    FFT_NONE, MF_FLOOR,\
}
FOUNTAIN(DNGN_FOUNTAIN_BLUE, "fountain of clear blue water", "fountain_blue", BLUE),
FOUNTAIN(DNGN_FOUNTAIN_SPARKLING, "fountain of sparkling water", "fountain_sparkling", LIGHTBLUE),
FOUNTAIN(DNGN_FOUNTAIN_BLOOD, "fountain of blood", "fountain_blood", RED),
FOUNTAIN(DNGN_DRY_FOUNTAIN, "dry fountain", "dry_fountain", LIGHTGREY),
#if TAG_MAJOR_VERSION == 34
FOUNTAIN(DNGN_DRY_FOUNTAIN_BLUE, "dry fountain", "non-fountain_blue", LIGHTGREY),
FOUNTAIN(DNGN_DRY_FOUNTAIN_SPARKLING, "dry fountain", "non-fountain_sparkling", LIGHTGREY),
FOUNTAIN(DNGN_DRY_FOUNTAIN_BLOOD, "dry fountain", "non-fountain_blood", LIGHTGREY),
#endif

#if TAG_MAJOR_VERSION == 34
{
    DNGN_TELEPORTER, "short-range portal", "teleporter",
    DCHAR_TELEPORTER, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(YELLOW),
    FFT_NONE, MF_FEATURE,
},
#endif
{
    DNGN_TRANSPORTER, "transporter", "transporter",
    DCHAR_TRANSPORTER, NUM_DCHAR_TYPES,
    RED, RED, RED, WHITE, WHITE,
    FFT_NOTABLE, MF_TRANSPORTER,
},

{
    DNGN_TRANSPORTER_LANDING, "transporter landing site", "transporter_landing",
    DCHAR_TRANSPORTER_LANDING, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(DARKGREY),
    FFT_NONE, MF_TRANSPORTER_LANDING,
},

{
    DNGN_SEALED_STAIRS_UP, "sealed passage leading up", "sealed_stair_up",
    DCHAR_STAIRS_UP, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(LIGHTGREEN),
    FFT_NONE, MF_STAIR_UP,
},

{
    DNGN_SEALED_STAIRS_DOWN, "sealed passage leading down", "sealed_stair_down",
    DCHAR_STAIRS_DOWN, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(LIGHTGREEN),
    FFT_NONE, MF_STAIR_DOWN,
},

{
    DNGN_ABYSSAL_STAIR, "gateway leading deeper into the Abyss", "abyssal_stair",
    DCHAR_STAIRS_DOWN, NUM_DCHAR_TYPES,
    COLOUR_AND_MAP(LIGHTCYAN),
    FFT_NONE, MF_STAIR_BRANCH,
},

};
