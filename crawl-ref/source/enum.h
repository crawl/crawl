#ifndef ENUM_H
#define ENUM_H

enum branch_type                // you.where_are_you
{
    BRANCH_MAIN_DUNGEON,
    BRANCH_ECUMENICAL_TEMPLE,
    BRANCH_FIRST_NON_DUNGEON = BRANCH_ECUMENICAL_TEMPLE,
    BRANCH_ORCISH_MINES,
    BRANCH_ELVEN_HALLS,
    BRANCH_LAIR,
    BRANCH_SWAMP,
    BRANCH_SHOALS,
    BRANCH_SLIME_PITS,
    BRANCH_SNAKE_PIT,
    BRANCH_HIVE,
    BRANCH_VAULTS,
    BRANCH_HALL_OF_BLADES,
    BRANCH_CRYPT,
    BRANCH_TOMB,
    BRANCH_VESTIBULE_OF_HELL,
    BRANCH_FIRST_HELL,
    BRANCH_DIS = BRANCH_FIRST_HELL,
    BRANCH_GEHENNA,
    BRANCH_COCYTUS,
    BRANCH_TARTARUS,
    BRANCH_LAST_HELL = BRANCH_TARTARUS,
    BRANCH_HALL_OF_ZOT,
    BRANCH_FOREST,
    BRANCH_SPIDER_NEST,
    BRANCH_DWARF_HALL,
    NUM_BRANCHES
};

// When adding:
//
// * New stairs/portals: update grid_stair_direction.
// * Any: edit view.cc and add a glyph and colour for the feature.
// * Any: edit directn.cc and add a description for the feature.
// * Any: edit dat/descript/features.txt and add a
//        long description if appropriate.
// * Any: check the grid_* functions in misc.cc and make sure
//        they return sane values for your new feature.
// * Any: edit dungeon.cc and add a symbol to map_feature() and
//        vault_grid() for the feature, if you want vault maps to
//        be able to use it.  If you do, also update
//        docs/develop/levels/syntax.txt with the new symbol.
// * Any: edit l_dgngrd.cc and add the feature's name to the dngn_feature_names
//        array, if you want vault map Lua code to be able to use the
//        feature, and/or you want to be able to create the feature
//        using the "create feature by name" wizard command.
// Also take note of MINMOVE and MINSEE above.
//
enum dungeon_feature_type
{
    DNGN_UNSEEN,
    DNGN_CLOSED_DOOR,
    DNGN_DETECTED_SECRET_DOOR,
    DNGN_SECRET_DOOR,
    DNGN_WAX_WALL,
    DNGN_METAL_WALL,
    DNGN_GREEN_CRYSTAL_WALL,
    DNGN_ROCK_WALL,
    DNGN_SLIMY_WALL,
    DNGN_STONE_WALL,
    DNGN_PERMAROCK_WALL,               // for undiggable walls
    DNGN_CLEAR_ROCK_WALL,              // transparent walls
    DNGN_CLEAR_STONE_WALL,
    DNGN_CLEAR_PERMAROCK_WALL,
    DNGN_GRATE,

    // Lowest/highest grid value which is a wall.
    DNGN_MINWALL = DNGN_WAX_WALL,
    DNGN_MAXWALL = DNGN_CLEAR_PERMAROCK_WALL,

    // Highest grid value which is opaque.
    DNGN_MAXOPAQUE = DNGN_PERMAROCK_WALL,

    // Lowest grid value which can be seen through.
    DNGN_MINSEE = DNGN_CLEAR_ROCK_WALL,

    // Highest grid value which can't be reached through.
    DNGN_MAX_NONREACH = DNGN_GRATE,

    DNGN_OPEN_SEA,                     // Shoals equivalent for permarock

    // Can be seen through and reached past.
    DNGN_TREE,
    DNGN_ORCISH_IDOL,
    DNGN_GRANITE_STATUE = 21,
    DNGN_STATUE_RESERVED,

    // Highest solid grid value.
    DNGN_MAXSOLID = DNGN_STATUE_RESERVED,

    // Lowest grid value which can be passed by walking etc.
    DNGN_MINMOVE = 31,

    DNGN_LAVA = 61,
    DNGN_DEEP_WATER,

    DNGN_SHALLOW_WATER = 65,
    DNGN_WATER_RESERVED,

    // Lowest grid value that an item can be placed on.
    DNGN_MINITEM = DNGN_SHALLOW_WATER,

    DNGN_FLOOR_MIN = 67,
    DNGN_FLOOR = DNGN_FLOOR_MIN,
    DNGN_FLOOR_SPECIAL,        // currently only used for colouring bazaars
    DNGN_FLOOR_RESERVED,
    DNGN_FLOOR_MAX = DNGN_FLOOR_RESERVED,

    DNGN_EXIT_HELL,                    //   70
    DNGN_ENTER_HELL,                   //   71
    DNGN_OPEN_DOOR,                    //   72

    DNGN_TRAP_MECHANICAL = 75,         //   75
    DNGN_TRAP_MAGICAL,
    DNGN_TRAP_NATURAL,
    DNGN_UNDISCOVERED_TRAP,            //   78

    DNGN_ENTER_SHOP = 80,              //   80
    DNGN_ENTER_LABYRINTH,

    DNGN_STONE_STAIRS_DOWN_I,
    DNGN_STONE_STAIRS_DOWN_II,
    DNGN_STONE_STAIRS_DOWN_III,
    DNGN_ESCAPE_HATCH_DOWN,            //  85 - was: rock stairs (Stonesoup 0.3)

    // corresponding up stairs (same order as above)
    DNGN_STONE_STAIRS_UP_I,
    DNGN_STONE_STAIRS_UP_II,
    DNGN_STONE_STAIRS_UP_III,
    DNGN_ESCAPE_HATCH_UP,              //  89 - was: rock stairs (Stonesoup 0.3)

    // Various gates
    DNGN_ENTER_DIS = 92,               //   92
    DNGN_ENTER_GEHENNA,
    DNGN_ENTER_COCYTUS,
    DNGN_ENTER_TARTARUS,               //   95
    DNGN_ENTER_ABYSS,
    DNGN_EXIT_ABYSS,
    DNGN_STONE_ARCH,
    DNGN_ENTER_PANDEMONIUM,
    DNGN_EXIT_PANDEMONIUM,             //  100
    DNGN_TRANSIT_PANDEMONIUM,          //  101

    // [enne] should the special_wall be placed between minwall/maxwall?
    DNGN_BUILDER_SPECIAL_WALL = 105,   //  105; builder() only
    DNGN_BUILDER_SPECIAL_FLOOR,        //  106; builder() only

    // Entrances to various branches
    DNGN_ENTER_FIRST_BRANCH = 110,     //  110
    DNGN_ENTER_DWARF_HALL = DNGN_ENTER_FIRST_BRANCH,
    DNGN_ENTER_ORCISH_MINES,
    DNGN_ENTER_HIVE,
    DNGN_ENTER_LAIR,
    DNGN_ENTER_SLIME_PITS,
    DNGN_ENTER_VAULTS,                 //  115
    DNGN_ENTER_CRYPT,
    DNGN_ENTER_HALL_OF_BLADES,
    DNGN_ENTER_ZOT,
    DNGN_ENTER_TEMPLE,
    DNGN_ENTER_SNAKE_PIT,              //  120
    DNGN_ENTER_ELVEN_HALLS,
    DNGN_ENTER_TOMB,
    DNGN_ENTER_SWAMP,                  //  122
    DNGN_ENTER_SHOALS,
    DNGN_ENTER_SPIDER_NEST,
    DNGN_ENTER_FOREST,
    DNGN_ENTER_LAST_BRANCH = DNGN_ENTER_FOREST,

    // Exits from various branches
    // Order must be the same as above
    DNGN_RETURN_FROM_FIRST_BRANCH = 130, //  130
    DNGN_RETURN_FROM_DWARF_HALL = DNGN_RETURN_FROM_FIRST_BRANCH,
    DNGN_RETURN_FROM_ORCISH_MINES,
    DNGN_RETURN_FROM_HIVE,
    DNGN_RETURN_FROM_LAIR,
    DNGN_RETURN_FROM_SLIME_PITS,
    DNGN_RETURN_FROM_VAULTS,           //  135
    DNGN_RETURN_FROM_CRYPT,
    DNGN_RETURN_FROM_HALL_OF_BLADES,
    DNGN_RETURN_FROM_ZOT,
    DNGN_RETURN_FROM_TEMPLE,
    DNGN_RETURN_FROM_SNAKE_PIT,        //  140
    DNGN_RETURN_FROM_ELVEN_HALLS,
    DNGN_RETURN_FROM_TOMB,
    DNGN_RETURN_FROM_SWAMP,            //  142
    DNGN_RETURN_FROM_SHOALS,
    DNGN_RETURN_FROM_SPIDER_NEST,
    DNGN_RETURN_FROM_FOREST,
    DNGN_RETURN_FROM_LAST_BRANCH = DNGN_RETURN_FROM_FOREST,

    // Portals to various places unknown.
    DNGN_ENTER_PORTAL_VAULT = 160,
    DNGN_EXIT_PORTAL_VAULT,
    DNGN_TEMP_PORTAL,

    // Order of altars must match order of gods (god_type)
    DNGN_ALTAR_FIRST_GOD = 180,        // 180
    DNGN_ALTAR_ZIN = DNGN_ALTAR_FIRST_GOD,
    DNGN_ALTAR_SHINING_ONE,
    DNGN_ALTAR_KIKUBAAQUDGHA,
    DNGN_ALTAR_YREDELEMNUL,
    DNGN_ALTAR_XOM,
    DNGN_ALTAR_VEHUMET,                //  185
    DNGN_ALTAR_OKAWARU,
    DNGN_ALTAR_MAKHLEB,
    DNGN_ALTAR_SIF_MUNA,
    DNGN_ALTAR_TROG,
    DNGN_ALTAR_NEMELEX_XOBEH,          //  190
    DNGN_ALTAR_ELYVILON,               //  191
    DNGN_ALTAR_LUGONU,
    DNGN_ALTAR_BEOGH,
    DNGN_ALTAR_JIYVA,
    DNGN_ALTAR_FEDHAS,
    DNGN_ALTAR_CHEIBRIADOS,
    DNGN_ALTAR_ASHENZARI,
    DNGN_ALTAR_LAST_GOD = DNGN_ALTAR_ASHENZARI,

    DNGN_FOUNTAIN_BLUE = 200,          //  200
    DNGN_FOUNTAIN_SPARKLING,           // aka 'Magic Fountain' {dlb}
    DNGN_FOUNTAIN_BLOOD,
    // same order as above!
    DNGN_DRY_FOUNTAIN_BLUE,
    DNGN_DRY_FOUNTAIN_SPARKLING,
    DNGN_DRY_FOUNTAIN_BLOOD,           //  205
    DNGN_PERMADRY_FOUNTAIN,
    DNGN_ABANDONED_SHOP,

    NUM_FEATURES                       //  208
};

enum level_area_type                   // you.level_type
{
    LEVEL_DUNGEON,
    LEVEL_LABYRINTH,
    LEVEL_ABYSS,
    LEVEL_PANDEMONIUM,
    LEVEL_PORTAL_VAULT,

    NUM_LEVEL_AREA_TYPES
};

#endif // ENUM_H
