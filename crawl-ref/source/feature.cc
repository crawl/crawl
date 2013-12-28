#include "AppHdr.h"

#include "feature.h"

#include "colour.h"
#include "options.h"
#include "show.h"
#include "terrain.h"

static FixedVector<feature_def, NUM_FEATURES>   feat_defs;
static FixedVector<feature_def, NUM_SHOW_ITEMS> item_defs;
static feature_def invis_fd, cloud_fd;

const feature_def &get_feature_def(show_type object)
{
    switch(object.cls)
    {
    case SH_INVIS_EXPOSED:
        return invis_fd;
    case SH_CLOUD:
        return cloud_fd;
    case SH_ITEM:
        return item_defs[object.item];
    case SH_MONSTER:
        // If this is a monster that is hidden explicitly, show items if
        // any instead, or the base feature if there are no items.
        if (object.item != SHOW_ITEM_NONE)
            return item_defs[object.item];
    case SH_FEATURE:
        return feat_defs[object.feat];
    default:
        die("invalid show object: class %d", object.cls);
    }
}

const feature_def &get_feature_def(dungeon_feature_type feat)
{
    return feat_defs[feat];
}

static void _fd_symbols(feature_def &f)
{
    if (!f.symbol && f.dchar != NUM_DCHAR_TYPES)
        f.symbol = Options.char_table[f.dchar];

    if (!f.magic_symbol)
        f.magic_symbol = f.symbol;

    if (f.seen_colour == BLACK)
        f.seen_colour = f.map_colour;

    if (f.seen_em_colour == BLACK)
        f.seen_em_colour = f.seen_colour;

    if (f.em_colour == BLACK)
        f.em_colour = f.colour;
}

static void _apply_feature_overrides()
{
    for (map<dungeon_feature_type, feature_def>::const_iterator fo
         = Options.feature_overrides.begin();
         fo != Options.feature_overrides.end();
         ++fo)
    {
        const feature_def           &ofeat  = fo->second;
        feature_def                 &feat   = feat_defs[fo->first];
        ucs_t c;

        if (ofeat.symbol && (c = get_glyph_override(ofeat.symbol)))
            feat.symbol = c;
        if (ofeat.magic_symbol && (c = get_glyph_override(ofeat.magic_symbol)))
            feat.magic_symbol = c;
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

static colour_t _feat_colour(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_ENTER_VAULTS:             return LIGHTGREEN;
    case DNGN_ENTER_ZOT:                return MAGENTA;
    case DNGN_ENTER_HELL:               return RED;
    case DNGN_ENTER_DIS:                return CYAN;
    case DNGN_ENTER_GEHENNA:            return RED;
    case DNGN_ENTER_COCYTUS:            return LIGHTCYAN;
    case DNGN_ENTER_TARTARUS:           return MAGENTA;
    case DNGN_ENTER_ABYSS:              return ETC_RANDOM;
    case DNGN_EXIT_THROUGH_ABYSS:       return ETC_RANDOM;
    case DNGN_ENTER_PANDEMONIUM:        return LIGHTBLUE;
    case DNGN_ENTER_TROVE:              return BLUE;
    case DNGN_ENTER_SEWER:              return LIGHTGREEN;
    case DNGN_ENTER_OSSUARY:            return BROWN;
    case DNGN_ENTER_BAILEY:             return LIGHTRED;
    case DNGN_ENTER_ICE_CAVE:           return WHITE;
    case DNGN_ENTER_VOLCANO:            return RED;
    case DNGN_EXIT_DUNGEON:             return LIGHTBLUE;
    case DNGN_RETURN_FROM_ZOT:          return MAGENTA;
    case DNGN_EXIT_HELL:                return LIGHTRED;
    case DNGN_EXIT_ABYSS:               return ETC_RANDOM;
    case DNGN_EXIT_PANDEMONIUM:         return LIGHTBLUE;
    case DNGN_TRANSIT_PANDEMONIUM:      return LIGHTGREEN;
    case DNGN_EXIT_TROVE:               return BLUE;
    case DNGN_EXIT_SEWER:               return BROWN;
    case DNGN_EXIT_OSSUARY:             return BROWN;
    case DNGN_EXIT_ICE_CAVE:            return WHITE;
    case DNGN_EXIT_VOLCANO:             return RED;

    case DNGN_ALTAR_ZIN:                return LIGHTGREY;
    case DNGN_ALTAR_SHINING_ONE:        return YELLOW;
    case DNGN_ALTAR_KIKUBAAQUDGHA:      return DARKGREY;
    case DNGN_ALTAR_YREDELEMNUL:        return ETC_UNHOLY;
    case DNGN_ALTAR_XOM:                return ETC_RANDOM;
    case DNGN_ALTAR_VEHUMET:            return ETC_VEHUMET;
    case DNGN_ALTAR_OKAWARU:            return CYAN;
    case DNGN_ALTAR_MAKHLEB:            return ETC_FIRE;
    case DNGN_ALTAR_SIF_MUNA:           return BLUE;
    case DNGN_ALTAR_TROG:               return RED;
    case DNGN_ALTAR_NEMELEX_XOBEH:      return LIGHTMAGENTA;
    case DNGN_ALTAR_ELYVILON:           return WHITE;
    case DNGN_ALTAR_LUGONU:             return MAGENTA;
    case DNGN_ALTAR_BEOGH:              return ETC_BEOGH;
    case DNGN_ALTAR_JIYVA:              return ETC_SLIME;
    case DNGN_ALTAR_FEDHAS:             return GREEN;
    case DNGN_ALTAR_CHEIBRIADOS:        return LIGHTCYAN;
    case DNGN_ALTAR_ASHENZARI:          return LIGHTRED;
    default: return 0;
    }
}

static void _init_feat(feature_def &f, dungeon_feature_type feat)
{
    f.colour = f.seen_colour = _feat_colour(feat);

    switch (feat)
    {
    default:
        if (feat >= DNGN_ENTER_FIRST_PORTAL && feat <= DNGN_ENTER_LAST_PORTAL
#if TAG_MAJOR_VERSION == 34
                 || feat == DNGN_ENTER_PORTAL_VAULT
#endif
                 || feat == DNGN_ENTER_LABYRINTH
                 || feat == DNGN_ENTER_HELL
                 || feat >= DNGN_ENTER_DIS && feat <= DNGN_ENTER_ABYSS
                 || feat == DNGN_ENTER_PANDEMONIUM
                 || feat == DNGN_EXIT_THROUGH_ABYSS
                 || feat == DNGN_ENTER_VAULTS
                 || feat == DNGN_ENTER_ZOT)
        {
            if (!f.colour)
                f.colour = f.seen_colour = ETC_SHIMMER_BLUE;
            f.dchar       = DCHAR_ARCH;
            f.map_colour  = LIGHTGREY;
            f.minimap     = MF_STAIR_BRANCH;
            f.flags      |= FFT_NOTABLE;
            if (feat == DNGN_ENTER_LABYRINTH)
                f.flags |= FFT_EXAMINE_HINT;
            break;
        }
        else if (feat >= DNGN_EXIT_FIRST_PORTAL && feat <= DNGN_EXIT_LAST_PORTAL
#if TAG_MAJOR_VERSION == 34
                 || feat == DNGN_EXIT_PORTAL_VAULT
#endif
                 || feat == DNGN_EXIT_HELL
                 || feat == DNGN_EXIT_ABYSS
                 || feat == DNGN_EXIT_PANDEMONIUM
                 || feat == DNGN_TRANSIT_PANDEMONIUM
                 || feat == DNGN_RETURN_FROM_ZOT)
        {
            if (!f.colour)
                f.colour = f.seen_colour = ETC_SHIMMER_BLUE;
            f.dchar       = DCHAR_ARCH;
            f.map_colour  = LIGHTGREY;
            f.minimap     = MF_STAIR_BRANCH;
            break;
        }
        else if (feat >= DNGN_ENTER_FIRST_BRANCH && feat <= DNGN_ENTER_LAST_BRANCH)
        {
            if (!f.colour)
                f.colour = f.seen_colour = YELLOW;
            f.dchar       = DCHAR_STAIRS_DOWN;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = RED;
            f.minimap     = MF_STAIR_BRANCH;
            if (feat == DNGN_ENTER_ORC || feat == DNGN_ENTER_SLIME)
                f.flags  |= FFT_EXAMINE_HINT;
            break;
        }
        else if (feat >= DNGN_RETURN_FROM_FIRST_BRANCH && feat <= DNGN_RETURN_FROM_LAST_BRANCH
                 || feat == DNGN_EXIT_DUNGEON)
        {
            if (!f.colour)
                f.colour = f.seen_colour = YELLOW;
            f.dchar       = DCHAR_STAIRS_UP;
            f.map_colour  = GREEN;
            f.minimap     = MF_STAIR_BRANCH;
            break;
        }
        else if (feat_is_altar(feat) || feat == DNGN_UNKNOWN_ALTAR)
        {
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.minimap     = MF_FEATURE;
            break;

        }
        break;
    case DNGN_UNSEEN:
    case DNGN_EXPLORE_HORIZON:
        break;

    case DNGN_ROCK_WALL:
        f.dchar        = DCHAR_WALL;
        f.colour       = ETC_ROCK;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.minimap      = MF_WALL;
        break;

    case DNGN_STONE_WALL:
        f.dchar        = DCHAR_WALL;
        f.colour       = LIGHTGRAY;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.minimap      = MF_WALL;
        break;

    case DNGN_SLIMY_WALL:
        f.dchar        = DCHAR_WALL;
        f.colour       = LIGHTGREEN;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.minimap      = MF_WALL;
        break;

    case DNGN_PERMAROCK_WALL:
        f.dchar        = DCHAR_PERMAWALL;
        f.colour       = ETC_ROCK;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.minimap      = MF_WALL;
        break;

    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_CLEAR_STONE_WALL:
        f.dchar        = DCHAR_WALL;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.colour       = LIGHTCYAN;
        f.minimap      = MF_WALL;
        break;

    case DNGN_CLEAR_PERMAROCK_WALL:
        f.dchar        = DCHAR_PERMAWALL;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.colour       = LIGHTCYAN;
        f.minimap      = MF_WALL;
        break;

    case DNGN_GRATE:
        f.dchar        = DCHAR_GRATE;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.colour       = LIGHTBLUE;
        f.minimap      = MF_WALL;
        break;

    case DNGN_TREE:
        f.dchar        = DCHAR_TREE;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.colour       = ETC_TREE;
        f.minimap      = MF_WALL;
        break;

    case DNGN_MANGROVE:
        f.dchar        = DCHAR_TREE;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.colour       = ETC_MANGROVE;
        f.minimap      = MF_WALL;
        break;

    case DNGN_OPEN_SEA:
        f.dchar        = DCHAR_WALL;
        f.colour       = BLUE;
        f.minimap      = MF_WATER;
        break;

    case DNGN_LAVA_SEA:
        f.dchar        = DCHAR_WAVY;
        f.colour       = RED;
        f.minimap      = MF_LAVA;
        break;

    case DNGN_OPEN_DOOR:
        f.dchar   = DCHAR_DOOR_OPEN;
        f.colour  = LIGHTGREY;
        f.minimap = MF_DOOR;
        break;

    case DNGN_CLOSED_DOOR:
        f.dchar   = DCHAR_DOOR_CLOSED;
        f.colour  = LIGHTGREY;
        f.minimap = MF_DOOR;
        break;

    case DNGN_RUNED_DOOR:
        f.dchar   = DCHAR_DOOR_CLOSED;
        f.colour  = LIGHTBLUE;
        f.minimap = MF_DOOR;
        f.map_colour = LIGHTBLUE;
        break;

    case DNGN_SEALED_DOOR:
        f.dchar   = DCHAR_DOOR_CLOSED;
        f.colour  = LIGHTGREEN;
        f.minimap = MF_DOOR;
        f.map_colour = LIGHTGREEN;
        break;

    case DNGN_METAL_WALL:
        f.dchar        = DCHAR_WALL;
        f.colour       = CYAN;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.minimap      = MF_WALL;
        break;

    case DNGN_GREEN_CRYSTAL_WALL:
        f.dchar        = DCHAR_WALL;
        f.colour       = GREEN;
        f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
        f.minimap      = MF_WALL;
        break;

    case DNGN_ORCISH_IDOL:
        f.dchar   = DCHAR_STATUE;
        f.colour  = BROWN; // same as clay golem, I hope that's okay
        f.minimap = MF_WALL;
        break;

    case DNGN_GRANITE_STATUE:
        f.dchar   = DCHAR_STATUE;
        f.colour  = DARKGREY;
        f.minimap = MF_WALL;
        break;

    case DNGN_LAVA:
        f.dchar   = DCHAR_WAVY;
        f.colour  = RED;
        f.minimap = MF_LAVA;
        break;

    case DNGN_DEEP_WATER:
        f.dchar   = DCHAR_WAVY;
        f.colour  = BLUE;
        f.minimap = MF_WATER;
        break;

    case DNGN_SHALLOW_WATER:
        f.dchar   = DCHAR_WAVY;
        f.colour  = CYAN;
        f.minimap = MF_WATER;
        break;

    case DNGN_FLOOR:
        f.dchar        = DCHAR_FLOOR;
        f.colour       = ETC_FLOOR;
        f.magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
        f.minimap      = MF_FLOOR;
        break;

    case DNGN_TELEPORTER:
        f.dchar       = DCHAR_TELEPORTER;
        f.colour      = YELLOW;
        f.map_colour  = YELLOW;
        f.minimap     = MF_FEATURE;
        break;

    case DNGN_TRAP_MECHANICAL:
        f.colour     = LIGHTCYAN;
        f.dchar      = DCHAR_TRAP;
        f.map_colour = LIGHTCYAN;
        f.minimap    = MF_TRAP;
        break;

    case DNGN_TRAP_TELEPORT:
        f.colour = f.map_colour = LIGHTBLUE;
        f.dchar      = DCHAR_TRAP;
        f.minimap    = MF_TRAP;
        break;

    case DNGN_TRAP_ALARM:
        f.colour = f.map_colour = LIGHTRED;
        f.dchar      = DCHAR_TRAP;
        f.minimap    = MF_TRAP;
        break;

    case DNGN_TRAP_ZOT:
        f.colour = f.map_colour = LIGHTMAGENTA;
        f.dchar      = DCHAR_TRAP;
        f.minimap    = MF_TRAP;
        break;

    case DNGN_PASSAGE_OF_GOLUBRIA:
        f.colour = f.map_colour = GREEN;
        f.dchar      = DCHAR_TRAP;
        f.minimap    = MF_TRAP;
        break;

    case DNGN_TRAP_SHAFT:
        f.colour     = BROWN;
        f.dchar      = DCHAR_TRAP;
        f.map_colour = BROWN;
        f.minimap    = MF_TRAP;
        break;

    case DNGN_TRAP_WEB:
        f.colour     = LIGHTGREY;
        f.dchar      = DCHAR_TRAP;
        f.map_colour = LIGHTGREY;
        f.minimap    = MF_TRAP;
        break;

    case DNGN_UNDISCOVERED_TRAP:
        f.dchar        = DCHAR_FLOOR;
        f.colour       = ETC_FLOOR;
        f.magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
        f.minimap      = MF_FLOOR;
        break;

    case DNGN_ENTER_SHOP:
        f.dchar       = DCHAR_ARCH;
        f.colour      = YELLOW;
        f.flags      |= FFT_NOTABLE;
        f.map_colour  = LIGHTGREY;
        f.seen_colour = YELLOW;
        f.minimap     = MF_FEATURE;
        break;

    case DNGN_MALIGN_GATEWAY:
        f.dchar       = DCHAR_ARCH;
        f.colour      = ETC_SHIMMER_BLUE;
        f.map_colour  = LIGHTGREY;
        f.colour      = ETC_SHIMMER_BLUE;
        f.minimap     = MF_STAIR_UP;
        break;

    case DNGN_EXPIRED_PORTAL:
        f.dchar        = DCHAR_FLOOR;
        f.colour       = BROWN;
        f.magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
        f.minimap      = MF_FLOOR;
        break;

    case DNGN_ESCAPE_HATCH_DOWN:
        f.dchar      = DCHAR_STAIRS_DOWN;
        f.colour     = BROWN;
        f.map_colour = BROWN;
        f.minimap    = MF_STAIR_DOWN;
        break;

    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        f.dchar          = DCHAR_STAIRS_DOWN;
        f.colour         = RED;
        f.em_colour      = WHITE;
        f.map_colour     = RED;
        f.seen_em_colour = WHITE;
        f.minimap        = MF_STAIR_DOWN;
        break;

    case DNGN_SEALED_STAIRS_DOWN:
        f.dchar          = DCHAR_STAIRS_DOWN;
        f.colour         = LIGHTGREEN;
        f.map_colour     = LIGHTGREEN;
        f.minimap        = MF_STAIR_DOWN;
        break;

    case DNGN_ESCAPE_HATCH_UP:
    case DNGN_EXIT_LABYRINTH:
        f.dchar      = DCHAR_STAIRS_UP;
        f.colour     = BROWN;
        f.map_colour = BROWN;
        f.minimap    = MF_STAIR_UP;
        break;

    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        f.dchar          = DCHAR_STAIRS_UP;
        f.colour         = GREEN;
        f.map_colour     = GREEN;
        f.em_colour      = WHITE;
        f.seen_em_colour = WHITE;
        f.minimap        = MF_STAIR_UP;
        break;

    case DNGN_SEALED_STAIRS_UP:
        f.dchar          = DCHAR_STAIRS_UP;
        f.colour         = LIGHTGREEN;
        f.map_colour     = LIGHTGREEN;
        f.minimap        = MF_STAIR_UP;
        break;

    case DNGN_ABYSSAL_STAIR:
        f.colour     = LIGHTCYAN;
        f.dchar      = DCHAR_STAIRS_DOWN;
        f.map_colour = LIGHTCYAN;
        f.minimap    = MF_STAIR_BRANCH;
        break;

    case DNGN_STONE_ARCH:
        f.colour     = LIGHTGREY;
        f.dchar      = DCHAR_ARCH;
        f.map_colour = LIGHTGREY;
        f.minimap    = MF_FLOOR;
        break;

    case DNGN_FOUNTAIN_BLUE:
        f.colour  = BLUE;
        f.dchar   = DCHAR_FOUNTAIN;
        f.minimap = MF_FEATURE;
        break;

    case DNGN_FOUNTAIN_SPARKLING:
        f.colour  = LIGHTBLUE;
        f.dchar   = DCHAR_FOUNTAIN;
        f.minimap = MF_FEATURE;
        break;

    case DNGN_FOUNTAIN_BLOOD:
        f.colour  = RED;
        f.dchar   = DCHAR_FOUNTAIN;
        f.minimap = MF_FEATURE;
        break;

    case DNGN_DRY_FOUNTAIN:
        f.colour  = LIGHTGREY;
        f.dchar   = DCHAR_FOUNTAIN;
        f.minimap = MF_FEATURE;
        break;

    case DNGN_UNKNOWN_PORTAL:
        f.colour  = LIGHTGREY;
        f.dchar   = DCHAR_ARCH;
        f.minimap = MF_FEATURE;
        break;
    }
}

void init_show_table(void)
{
    show_type obj;
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        dungeon_feature_type feat = static_cast<dungeon_feature_type>(i);
        _init_feat(feat_defs[feat], feat);
    }
    _apply_feature_overrides();
    for (int i = 0; i < NUM_FEATURES; i++)
        _fd_symbols(feat_defs[i]);

    for (int i = 0; i < NUM_SHOW_ITEMS; i++)
    {
        show_item_type si = static_cast<show_item_type>(i);
        // SHOW_ITEM_NONE is bogus, but "invis exposed" is an ok placeholder
        COMPILE_CHECK(DCHAR_ITEM_AMULET - DCHAR_ITEM_DETECTED + 2 == NUM_SHOW_ITEMS);
        item_defs[si].minimap = MF_ITEM;
        item_defs[si].dchar = static_cast<dungeon_char_type>(i
            + DCHAR_ITEM_DETECTED - SHOW_ITEM_DETECTED);
        _fd_symbols(item_defs[si]);
    }

    invis_fd.dchar = DCHAR_INVIS_EXPOSED;
    invis_fd.minimap = MF_MONS_HOSTILE;
    _fd_symbols(invis_fd);

    cloud_fd.dchar = DCHAR_CLOUD;
    cloud_fd.minimap = MF_SKIP;
    _fd_symbols(cloud_fd);
}

dungeon_feature_type magic_map_base_feat(dungeon_feature_type feat)
{
    const feature_def& fdef = get_feature_def(feat);
    switch (fdef.dchar)
    {
    case DCHAR_STATUE:
        return DNGN_GRANITE_STATUE;
    case DCHAR_FLOOR:
    case DCHAR_TRAP:
        return DNGN_FLOOR;
    case DCHAR_WAVY:
        return DNGN_SHALLOW_WATER;
    case DCHAR_ARCH:
        return DNGN_UNKNOWN_PORTAL;
    case DCHAR_FOUNTAIN:
        return DNGN_FOUNTAIN_BLUE;
    case DCHAR_WALL:
        return DNGN_ROCK_WALL;
    case DCHAR_ALTAR:
        return DNGN_UNKNOWN_ALTAR;
    default:
        // We could do more, e.g. map the different upstairs together.
        return feat;
    }
}
