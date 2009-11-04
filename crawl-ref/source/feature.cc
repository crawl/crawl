#include "AppHdr.h"

#include "feature.h"

#include "colour.h"
#include "fixvec.h"
#include "options.h"

static FixedVector<feature_def, NUM_FEATURES> Feature;

const feature_def &get_feature_def(dungeon_feature_type feat)
{
    return (Feature[feat]);
}

void apply_feature_overrides()
{
    for (int i = 0, size = Options.feature_overrides.size(); i < size; ++i)
    {
        const feature_override      &fov    = Options.feature_overrides[i];
        const feature_def           &ofeat  = fov.override;
        feature_def                 &feat   = Feature[fov.feat];

        if (ofeat.symbol)
            feat.symbol = ofeat.symbol;
        if (ofeat.magic_symbol)
            feat.magic_symbol = ofeat.magic_symbol;
        if (ofeat.colour)
            feat.colour = ofeat.colour;
        if (ofeat.map_colour)
            feat.map_colour = ofeat.map_colour;
        if (ofeat.seen_colour)
            feat.seen_colour = ofeat.seen_colour;
        if (ofeat.seen_em_colour)
            feat.seen_em_colour = ofeat.seen_em_colour;
        if (ofeat.em_colour)
            feat.em_colour = ofeat.em_colour;
    }
}

void init_feature_table(void)
{
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        Feature[i].dchar          = NUM_DCHAR_TYPES;
        Feature[i].symbol         = 0;
        Feature[i].colour         = BLACK;   // means must be set some other way
        Feature[i].flags          = FFT_NONE;
        Feature[i].magic_symbol   = 0;       // set to symbol if unchanged
        Feature[i].map_colour     = DARKGREY;
        Feature[i].seen_colour    = BLACK;   // -> no special seen map handling
        Feature[i].seen_em_colour = BLACK;
        Feature[i].em_colour      = BLACK;
        Feature[i].minimap        = MF_UNSEEN;

        switch (i)
        {
        case DNGN_UNSEEN:
        default:
            break;

        case DNGN_ROCK_WALL:
        case DNGN_PERMAROCK_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = ETC_ROCK;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_STONE_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = ETC_STONE;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_CLEAR_ROCK_WALL:
        case DNGN_CLEAR_STONE_WALL:
        case DNGN_CLEAR_PERMAROCK_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].colour       = LIGHTCYAN;
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_TREES:
            Feature[i].dchar        = DCHAR_TREES;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].colour       = BLACK; // overridden later
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_OPEN_SEA:
#ifdef USE_TILE
            Feature[i].dchar        = DCHAR_WAVY;
#else
            Feature[i].dchar        = DCHAR_WALL;
#endif
            Feature[i].colour       = BLUE;
            Feature[i].minimap      = MF_WATER;
            break;

        case DNGN_OPEN_DOOR:
            Feature[i].dchar   = DCHAR_DOOR_OPEN;
            Feature[i].colour  = LIGHTGREY;
            Feature[i].minimap = MF_DOOR;
            break;

        case DNGN_CLOSED_DOOR:
        case DNGN_DETECTED_SECRET_DOOR:
            Feature[i].dchar   = DCHAR_DOOR_CLOSED;
            Feature[i].colour  = LIGHTGREY;
            Feature[i].minimap = MF_DOOR;
            break;

        case DNGN_METAL_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = CYAN;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_SECRET_DOOR:
            // Note: get_secret_door_appearance means this probably isn't used.
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = ETC_ROCK;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_GREEN_CRYSTAL_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = GREEN;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_ORCISH_IDOL:
            Feature[i].dchar   = DCHAR_STATUE;
            Feature[i].colour  = BROWN; // same as clay golem, I hope that's okay
            Feature[i].minimap = MF_WALL;
            break;

        case DNGN_WAX_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = YELLOW;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_GRANITE_STATUE:
            Feature[i].dchar   = DCHAR_STATUE;
            Feature[i].colour  = DARKGREY;
            Feature[i].minimap = MF_WALL;
            break;

        case DNGN_LAVA:
            Feature[i].dchar   = DCHAR_WAVY;
            Feature[i].colour  = RED;
            Feature[i].minimap = MF_LAVA;
            break;

        case DNGN_DEEP_WATER:
            Feature[i].dchar   = DCHAR_WAVY;
            Feature[i].colour  = BLUE;
            Feature[i].minimap = MF_WATER;
            break;

        case DNGN_SHALLOW_WATER:
            Feature[i].dchar   = DCHAR_WAVY;
            Feature[i].colour  = CYAN;
            Feature[i].minimap = MF_WATER;
            break;

        case DNGN_FLOOR:
            Feature[i].dchar        = DCHAR_FLOOR;
            Feature[i].colour       = ETC_FLOOR;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            Feature[i].minimap      = MF_FLOOR;
            break;

        case DNGN_FLOOR_SPECIAL:
            Feature[i].dchar        = DCHAR_FLOOR;
            Feature[i].colour       = YELLOW;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            Feature[i].minimap      = MF_FLOOR;
            break;

        case DNGN_EXIT_HELL:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = LIGHTRED;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = LIGHTRED;
            Feature[i].minimap     = MF_STAIR_UP;
            break;

        case DNGN_ENTER_HELL:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = RED;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = RED;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_TRAP_MECHANICAL:
            Feature[i].colour     = LIGHTCYAN;
            Feature[i].dchar      = DCHAR_TRAP;
            Feature[i].map_colour = LIGHTCYAN;
            Feature[i].minimap    = MF_TRAP;
            break;

        case DNGN_TRAP_MAGICAL:
            Feature[i].colour     = MAGENTA;
            Feature[i].dchar      = DCHAR_TRAP;
            Feature[i].map_colour = MAGENTA;
            Feature[i].minimap    = MF_TRAP;
            break;

        case DNGN_TRAP_NATURAL:
            Feature[i].colour     = BROWN;
            Feature[i].dchar      = DCHAR_TRAP;
            Feature[i].map_colour = BROWN;
            Feature[i].minimap    = MF_TRAP;
            break;

        case DNGN_UNDISCOVERED_TRAP:
            Feature[i].dchar        = DCHAR_FLOOR;
            Feature[i].colour       = ETC_FLOOR;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            Feature[i].minimap      = MF_FLOOR;
            break;

        case DNGN_ENTER_SHOP:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = YELLOW;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = YELLOW;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ABANDONED_SHOP:
            Feature[i].colour     = LIGHTGREY;
            Feature[i].dchar      = DCHAR_ARCH;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].minimap    = MF_FLOOR;
            break;

        case DNGN_ENTER_LABYRINTH:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = CYAN;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = CYAN;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_PORTAL_VAULT:
            Feature[i].flags |= FFT_NOTABLE;
            // fall through

        case DNGN_EXIT_PORTAL_VAULT:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = ETC_SHIMMER_BLUE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = ETC_SHIMMER_BLUE;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ESCAPE_HATCH_DOWN:
            Feature[i].dchar      = DCHAR_STAIRS_DOWN;
            Feature[i].colour     = BROWN;
            Feature[i].map_colour = BROWN;
            Feature[i].minimap    = MF_STAIR_DOWN;
            break;

        case DNGN_STONE_STAIRS_DOWN_I:
        case DNGN_STONE_STAIRS_DOWN_II:
        case DNGN_STONE_STAIRS_DOWN_III:
            Feature[i].dchar          = DCHAR_STAIRS_DOWN;
            Feature[i].colour         = LIGHTGREY;
            Feature[i].em_colour      = WHITE;
            Feature[i].map_colour     = RED;
            Feature[i].seen_em_colour = WHITE;
            Feature[i].minimap        = MF_STAIR_DOWN;
            break;

        case DNGN_ESCAPE_HATCH_UP:
            Feature[i].dchar      = DCHAR_STAIRS_UP;
            Feature[i].colour     = BROWN;
            Feature[i].map_colour = BROWN;
            Feature[i].minimap    = MF_STAIR_UP;
            break;

        case DNGN_STONE_STAIRS_UP_I:
        case DNGN_STONE_STAIRS_UP_II:
        case DNGN_STONE_STAIRS_UP_III:
            Feature[i].dchar          = DCHAR_STAIRS_UP;
            Feature[i].colour         = LIGHTGREY;
            Feature[i].map_colour     = GREEN;
            Feature[i].em_colour      = WHITE;
            Feature[i].seen_em_colour = WHITE;
            Feature[i].minimap        = MF_STAIR_UP;
            break;

        case DNGN_ENTER_DIS:
            Feature[i].colour      = CYAN;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = CYAN;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_GEHENNA:
            Feature[i].colour      = RED;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = RED;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_COCYTUS:
            Feature[i].colour      = LIGHTCYAN;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = LIGHTCYAN;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_TARTARUS:
            Feature[i].colour      = DARKGREY;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = DARKGREY;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_ABYSS:
            Feature[i].colour      = ETC_RANDOM;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = ETC_RANDOM;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_EXIT_ABYSS:
            Feature[i].colour     = ETC_RANDOM;
            Feature[i].dchar      = DCHAR_ARCH;
            Feature[i].map_colour = ETC_RANDOM;
            Feature[i].minimap    = MF_STAIR_BRANCH;
            break;

        case DNGN_STONE_ARCH:
            Feature[i].colour     = LIGHTGREY;
            Feature[i].dchar      = DCHAR_ARCH;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].minimap    = MF_FLOOR;
            break;

        case DNGN_ENTER_PANDEMONIUM:
            Feature[i].colour      = LIGHTBLUE;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = LIGHTBLUE;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_EXIT_PANDEMONIUM:
            // Note: Has special handling for colouring with mutation.
            Feature[i].colour      = LIGHTBLUE;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = LIGHTBLUE;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_TRANSIT_PANDEMONIUM:
            Feature[i].colour      = LIGHTGREEN;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = LIGHTGREEN;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_ORCISH_MINES:
        case DNGN_ENTER_HIVE:
        case DNGN_ENTER_LAIR:
        case DNGN_ENTER_SLIME_PITS:
        case DNGN_ENTER_VAULTS:
        case DNGN_ENTER_CRYPT:
        case DNGN_ENTER_HALL_OF_BLADES:
        case DNGN_ENTER_TEMPLE:
        case DNGN_ENTER_SNAKE_PIT:
        case DNGN_ENTER_ELVEN_HALLS:
        case DNGN_ENTER_TOMB:
        case DNGN_ENTER_SWAMP:
        case DNGN_ENTER_SHOALS:
            Feature[i].colour      = YELLOW;
            Feature[i].dchar       = DCHAR_STAIRS_DOWN;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = RED;
            Feature[i].seen_colour = YELLOW;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_ZOT:
            Feature[i].colour      = MAGENTA;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = MAGENTA;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_RETURN_FROM_ORCISH_MINES:
        case DNGN_RETURN_FROM_HIVE:
        case DNGN_RETURN_FROM_LAIR:
        case DNGN_RETURN_FROM_SLIME_PITS:
        case DNGN_RETURN_FROM_VAULTS:
        case DNGN_RETURN_FROM_CRYPT:
        case DNGN_RETURN_FROM_HALL_OF_BLADES:
        case DNGN_RETURN_FROM_TEMPLE:
        case DNGN_RETURN_FROM_SNAKE_PIT:
        case DNGN_RETURN_FROM_ELVEN_HALLS:
        case DNGN_RETURN_FROM_TOMB:
        case DNGN_RETURN_FROM_SWAMP:
        case DNGN_RETURN_FROM_SHOALS:
            Feature[i].colour      = YELLOW;
            Feature[i].dchar       = DCHAR_STAIRS_UP;
            Feature[i].map_colour  = GREEN;
            Feature[i].seen_colour = YELLOW;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_RETURN_FROM_ZOT:
            Feature[i].colour      = MAGENTA;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = MAGENTA;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ALTAR_ZIN:
            Feature[i].colour      = WHITE;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = WHITE;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_SHINING_ONE:
            Feature[i].colour      = YELLOW;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = YELLOW;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_KIKUBAAQUDGHA:
            Feature[i].colour      = DARKGREY;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = DARKGREY;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_YREDELEMNUL:
            Feature[i].colour      = ETC_UNHOLY;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_UNHOLY;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_XOM:
            Feature[i].colour      = ETC_RANDOM;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_RANDOM;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_VEHUMET:
            Feature[i].colour      = ETC_VEHUMET;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_VEHUMET;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_OKAWARU:
            Feature[i].colour      = CYAN;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = CYAN;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_MAKHLEB:
            Feature[i].colour      = ETC_FIRE;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_FIRE;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_SIF_MUNA:
            Feature[i].colour      = BLUE;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = BLUE;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_TROG:
            Feature[i].colour      = RED;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = RED;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_NEMELEX_XOBEH:
            Feature[i].colour      = LIGHTMAGENTA;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = LIGHTMAGENTA;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_ELYVILON:
            Feature[i].colour      = LIGHTGREY;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = LIGHTGREY;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_LUGONU:
            Feature[i].colour      = MAGENTA;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = MAGENTA;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_BEOGH:
            Feature[i].colour      = ETC_BEOGH;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_BEOGH;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_JIYVA:
            Feature[i].colour      = ETC_SLIME;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_SLIME;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_FEAWN:
            Feature[i].colour      = GREEN;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = GREEN;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_CHEIBRIADOS:
            Feature[i].colour      = LIGHTCYAN;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = LIGHTCYAN;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_FOUNTAIN_BLUE:
            Feature[i].colour  = BLUE;
            Feature[i].dchar   = DCHAR_FOUNTAIN;
            Feature[i].minimap = MF_FEATURE;
            break;

        case DNGN_FOUNTAIN_SPARKLING:
            Feature[i].colour  = LIGHTBLUE;
            Feature[i].dchar   = DCHAR_FOUNTAIN;
            Feature[i].minimap = MF_FEATURE;
            break;

        case DNGN_FOUNTAIN_BLOOD:
            Feature[i].colour  = RED;
            Feature[i].dchar   = DCHAR_FOUNTAIN;
            Feature[i].minimap = MF_FEATURE;
            break;

        case DNGN_DRY_FOUNTAIN_BLUE:
        case DNGN_DRY_FOUNTAIN_SPARKLING:
        case DNGN_DRY_FOUNTAIN_BLOOD:
        case DNGN_PERMADRY_FOUNTAIN:
            Feature[i].colour  = LIGHTGREY;
            Feature[i].dchar   = DCHAR_FOUNTAIN;
            Feature[i].minimap = MF_FEATURE;
            break;

        case DNGN_INVIS_EXPOSED:
            Feature[i].dchar   = DCHAR_INVIS_EXPOSED;
            Feature[i].minimap = MF_MONS_HOSTILE;
            break;

        case DNGN_ITEM_DETECTED:
            Feature[i].dchar   = DCHAR_ITEM_DETECTED;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_ORB:
            Feature[i].dchar   = DCHAR_ITEM_ORB;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_WEAPON:
            Feature[i].dchar   = DCHAR_ITEM_WEAPON;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_ARMOUR:
            Feature[i].dchar   = DCHAR_ITEM_ARMOUR;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_WAND:
            Feature[i].dchar   = DCHAR_ITEM_WAND;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_FOOD:
            Feature[i].dchar   = DCHAR_ITEM_FOOD;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_SCROLL:
            Feature[i].dchar   = DCHAR_ITEM_SCROLL;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_RING:
            Feature[i].dchar   = DCHAR_ITEM_RING;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_POTION:
            Feature[i].dchar   = DCHAR_ITEM_POTION;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_MISSILE:
            Feature[i].dchar   = DCHAR_ITEM_MISSILE;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_BOOK:
            Feature[i].dchar   = DCHAR_ITEM_BOOK;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_STAVE:
            Feature[i].dchar   = DCHAR_ITEM_STAVE;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_MISCELLANY:
            Feature[i].dchar   = DCHAR_ITEM_MISCELLANY;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_CORPSE:
            Feature[i].dchar   = DCHAR_ITEM_CORPSE;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_GOLD:
            Feature[i].dchar   = DCHAR_ITEM_GOLD;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_AMULET:
            Feature[i].dchar   = DCHAR_ITEM_AMULET;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_CLOUD:
            Feature[i].dchar   = DCHAR_CLOUD;
            Feature[i].minimap = MF_SKIP;
            break;
        }

        if (i == DNGN_ENTER_ORCISH_MINES || i == DNGN_ENTER_SLIME_PITS
            || i == DNGN_ENTER_LABYRINTH)
        {
            Feature[i].flags |= FFT_EXAMINE_HINT;
        }

        if (Feature[i].dchar != NUM_DCHAR_TYPES)
            Feature[i].symbol = Options.char_table[ Feature[i].dchar ];
    }

    apply_feature_overrides();

    for (int i = 0; i < NUM_FEATURES; ++i)
    {
        feature_def &f(Feature[i]);

        if (!f.magic_symbol)
            f.magic_symbol = f.symbol;

        if (f.seen_colour == BLACK)
            f.seen_colour = f.map_colour;

        if (f.seen_em_colour == BLACK)
            f.seen_em_colour = f.seen_colour;

        if (f.em_colour == BLACK)
            f.em_colour = f.colour;
    }
}
