#include "AppHdr.h"

#include "tilepick.h"

#include "artefact.h"
#include "art-enum.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "decks.h"
#include "describe.h"
#include "env.h"
#include "files.h"
#include "food.h"
#include "ghost.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "mon-death.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "rot.h"
#include "shopping.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tiledef-gui.h"
#include "tiledef-main.h"
#include "tiledef-player.h"
#include "tiledef-unrand.h"
#include "tilemcache.h"
#include "tileview.h"
#include "transform.h"
#include "traps.h"
#include "viewgeom.h"

// This should not be changed.
COMPILE_CHECK(TILE_DNGN_UNSEEN == 0);

// NOTE: If one of the following asserts fail, it's because the corresponding
// enum in itemprop-enum.h was modified, but rltiles/dc-item.txt was not
// modified in parallel.

// These brands start with "normal" which there's no tile for, so subtract 1.
COMPILE_CHECK(NUM_REAL_SPECIAL_WEAPONS - 1
              == TILE_BRAND_WEP_LAST - TILE_BRAND_WEP_FIRST + 1);
COMPILE_CHECK(NUM_REAL_SPECIAL_ARMOURS - 1
              == TILE_BRAND_ARM_LAST - TILE_BRAND_ARM_FIRST + 1);

COMPILE_CHECK(NUM_RINGS == TILE_RING_ID_LAST - TILE_RING_ID_FIRST + 1);
COMPILE_CHECK(NUM_JEWELLERY - AMU_FIRST_AMULET
              == TILE_AMU_ID_LAST - TILE_AMU_ID_FIRST + 1);
COMPILE_CHECK(NUM_SCROLLS == TILE_SCR_ID_LAST - TILE_SCR_ID_FIRST + 1);
COMPILE_CHECK(NUM_STAVES == TILE_STAFF_ID_LAST - TILE_STAFF_ID_FIRST + 1);
COMPILE_CHECK(NUM_RODS == TILE_ROD_ID_LAST - TILE_ROD_ID_FIRST + 1);
COMPILE_CHECK(NUM_WANDS == TILE_WAND_ID_LAST - TILE_WAND_ID_FIRST + 1);
COMPILE_CHECK(NUM_POTIONS == TILE_POT_ID_LAST - TILE_POT_ID_FIRST + 1);

TextureID get_dngn_tex(tileidx_t idx)
{
    ASSERT(idx < TILE_FEAT_MAX);
    if (idx < TILE_FLOOR_MAX)
        return TEX_FLOOR;
    else if (idx < TILE_WALL_MAX)
        return TEX_WALL;
    else
        return TEX_FEAT;
}

static tileidx_t _tileidx_trap(trap_type type)
{
    switch (type)
    {
    case TRAP_ARROW:
        return TILE_DNGN_TRAP_ARROW;
    case TRAP_SPEAR:
        return TILE_DNGN_TRAP_SPEAR;
    case TRAP_TELEPORT:
        return TILE_DNGN_TRAP_TELEPORT;
    case TRAP_TELEPORT_PERMANENT:
        return TILE_DNGN_TRAP_TELEPORT_PERMANENT;
    case TRAP_ALARM:
        return TILE_DNGN_TRAP_ALARM;
    case TRAP_BLADE:
        return TILE_DNGN_TRAP_BLADE;
    case TRAP_BOLT:
        return TILE_DNGN_TRAP_BOLT;
    case TRAP_NET:
        return TILE_DNGN_TRAP_NET;
    case TRAP_ZOT:
        return TILE_DNGN_TRAP_ZOT;
    case TRAP_NEEDLE:
        return TILE_DNGN_TRAP_NEEDLE;
    case TRAP_SHAFT:
        return TILE_DNGN_TRAP_SHAFT;
    case TRAP_GOLUBRIA:
        return TILE_DNGN_TRAP_GOLUBRIA;
    case TRAP_PLATE:
        return TILE_DNGN_TRAP_PLATE;
    case TRAP_WEB:
        return TILE_DNGN_TRAP_WEB;
    default:
        return TILE_DNGN_ERROR;
    }
}

static tileidx_t _tileidx_shop(coord_def where)
{
    const shop_struct *shop = shop_at(where);

    if (!shop)
        return TILE_DNGN_ERROR;

    switch (shop->type)
    {
        case SHOP_WEAPON:
        case SHOP_WEAPON_ANTIQUE:
            return TILE_SHOP_WEAPONS;
        case SHOP_ARMOUR:
        case SHOP_ARMOUR_ANTIQUE:
            return TILE_SHOP_ARMOUR;
        case SHOP_JEWELLERY:
            return TILE_SHOP_JEWELLERY;
        case SHOP_EVOKABLES:
            return TILE_SHOP_GADGETS;
        case SHOP_FOOD:
            return TILE_SHOP_FOOD;
        case SHOP_BOOK:
            return TILE_SHOP_BOOKS;
        case SHOP_SCROLL:
            return TILE_SHOP_SCROLLS;
        case SHOP_DISTILLERY:
            return TILE_SHOP_POTIONS;
        case SHOP_GENERAL:
        case SHOP_GENERAL_ANTIQUE:
            return TILE_SHOP_GENERAL;
        default:
            return TILE_DNGN_ERROR;
    }
}

tileidx_t tileidx_feature_base(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_UNSEEN:
        return TILE_DNGN_UNSEEN;
    case DNGN_ROCK_WALL:
        return TILE_WALL_NORMAL;
    case DNGN_PERMAROCK_WALL:
        return TILE_WALL_PERMAROCK;
    case DNGN_SLIMY_WALL:
        return TILE_WALL_SLIME;
    case DNGN_RUNED_DOOR:
        return TILE_DNGN_RUNED_DOOR;
    case DNGN_SEALED_DOOR:
        return TILE_DNGN_SEALED_DOOR;
    case DNGN_GRATE:
        return TILE_DNGN_GRATE;
    case DNGN_CLEAR_ROCK_WALL:
        return TILE_DNGN_TRANSPARENT_WALL;
    case DNGN_CLEAR_STONE_WALL:
        return TILE_DNGN_TRANSPARENT_STONE;
    case DNGN_CLEAR_PERMAROCK_WALL:
        return TILE_WALL_PERMAROCK_CLEAR;
    case DNGN_STONE_WALL:
        return TILE_DNGN_STONE_WALL;
    case DNGN_CLOSED_DOOR:
        return TILE_DNGN_CLOSED_DOOR;
    case DNGN_METAL_WALL:
        return TILE_DNGN_METAL_WALL;
    case DNGN_CRYSTAL_WALL:
        return TILE_DNGN_CRYSTAL_WALL;
    case DNGN_ORCISH_IDOL:
        return TILE_DNGN_ORCISH_IDOL;
    case DNGN_TREE:
        return player_in_branch(BRANCH_SWAMP) ? TILE_DNGN_MANGROVE : TILE_DNGN_TREE;
    case DNGN_GRANITE_STATUE:
        return TILE_DNGN_GRANITE_STATUE;
    case DNGN_LAVA:
        return TILE_DNGN_LAVA;
    case DNGN_LAVA_SEA:
        return TILE_DNGN_LAVA_SEA;
    case DNGN_DEEP_WATER:
        return TILE_DNGN_DEEP_WATER;
    case DNGN_SHALLOW_WATER:
        return TILE_DNGN_SHALLOW_WATER;
    case DNGN_OPEN_SEA:
        return TILE_DNGN_OPEN_SEA;
    case DNGN_FLOOR:
    case DNGN_UNDISCOVERED_TRAP:
        return TILE_FLOOR_NORMAL;
    case DNGN_ENDLESS_SALT:
        return TILE_DNGN_ENDLESS_SALT;
    case DNGN_ENTER_HELL:
        if (player_in_hell())
            return TILE_DNGN_RETURN_VESTIBULE;
        return TILE_DNGN_ENTER_HELL;
    case DNGN_OPEN_DOOR:
        return TILE_DNGN_OPEN_DOOR;
    case DNGN_TRAP_MECHANICAL:
        return TILE_DNGN_TRAP_ARROW;
    case DNGN_TRAP_TELEPORT:
        return TILE_DNGN_TRAP_TELEPORT;
    case DNGN_TRAP_ALARM:
        return TILE_DNGN_TRAP_ALARM;
    case DNGN_TRAP_ZOT:
        return TILE_DNGN_TRAP_ZOT;
    case DNGN_TRAP_SHAFT:
        return TILE_DNGN_TRAP_SHAFT;
    case DNGN_TRAP_WEB:
        return TILE_DNGN_TRAP_WEB;
    case DNGN_TELEPORTER:
        return TILE_DNGN_TRAP_GOLUBRIA;
    case DNGN_ENTER_SHOP:
        return TILE_SHOP_GENERAL;
    case DNGN_ABANDONED_SHOP:
        return TILE_DNGN_ABANDONED_SHOP;
    case DNGN_ENTER_LABYRINTH:
        return TILE_DNGN_PORTAL_LABYRINTH;
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        return TILE_DNGN_STONE_STAIRS_DOWN;
    case DNGN_ESCAPE_HATCH_DOWN:
        return TILE_DNGN_ESCAPE_HATCH_DOWN;
    case DNGN_SEALED_STAIRS_DOWN:
        return TILE_DNGN_SEALED_STAIRS_DOWN;
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        return TILE_DNGN_STONE_STAIRS_UP;
    case DNGN_EXIT_LABYRINTH:
    case DNGN_ESCAPE_HATCH_UP:
        return TILE_DNGN_ESCAPE_HATCH_UP;
    case DNGN_SEALED_STAIRS_UP:
        return TILE_DNGN_SEALED_STAIRS_UP;
    case DNGN_EXIT_DUNGEON:
        return TILE_DNGN_EXIT_DUNGEON;
    case DNGN_ENTER_DIS:
        return TILE_DNGN_ENTER_DIS;
    case DNGN_ENTER_GEHENNA:
        return TILE_DNGN_ENTER_GEHENNA;
    case DNGN_ENTER_COCYTUS:
        return TILE_DNGN_ENTER_COCYTUS;
    case DNGN_ENTER_TARTARUS:
        return TILE_DNGN_ENTER_TARTARUS;
    case DNGN_ENTER_ABYSS:
    case DNGN_EXIT_THROUGH_ABYSS:
        return TILE_DNGN_ENTER_ABYSS;
    case DNGN_ABYSSAL_STAIR:
        return TILE_DNGN_ABYSSAL_STAIR;
    case DNGN_EXIT_HELL:
        return TILE_DNGN_RETURN_HELL;
    case DNGN_EXIT_ABYSS:
        return TILE_DNGN_EXIT_ABYSS;
    case DNGN_STONE_ARCH:
        if (player_in_branch(BRANCH_VESTIBULE))
            return TILE_DNGN_STONE_ARCH_HELL;
        return TILE_DNGN_STONE_ARCH;
    case DNGN_ENTER_PANDEMONIUM:
        return TILE_DNGN_ENTER_PANDEMONIUM;
    case DNGN_TRANSIT_PANDEMONIUM:
        return TILE_DNGN_TRANSIT_PANDEMONIUM;
    case DNGN_EXIT_PANDEMONIUM:
        return TILE_DNGN_EXIT_PANDEMONIUM;

    // branch entry stairs
#if TAG_MAJOR_VERSION == 34
    case DNGN_ENTER_DWARF:
    case DNGN_ENTER_FOREST:
    case DNGN_ENTER_BLADE:
        return TILE_DNGN_ENTER;
#endif
    case DNGN_ENTER_TEMPLE:
        return TILE_DNGN_ENTER_TEMPLE;
    case DNGN_ENTER_ORC:
        return TILE_DNGN_ENTER_ORC;
    case DNGN_ENTER_ELF:
        return TILE_DNGN_ENTER_ELF;
    case DNGN_ENTER_LAIR:
        return TILE_DNGN_ENTER_LAIR;
    case DNGN_ENTER_SNAKE:
        return TILE_DNGN_ENTER_SNAKE;
    case DNGN_ENTER_SWAMP:
        return TILE_DNGN_ENTER_SWAMP;
    case DNGN_ENTER_SPIDER:
        return TILE_DNGN_ENTER_SPIDER;
    case DNGN_ENTER_SHOALS:
        return TILE_DNGN_ENTER_SHOALS;
    case DNGN_ENTER_SLIME:
        return TILE_DNGN_ENTER_SLIME;
    case DNGN_ENTER_DEPTHS:
        return TILE_DNGN_ENTER_DEPTHS;
    case DNGN_ENTER_VAULTS:
        return is_existing_level(level_id(BRANCH_VAULTS, 1)) ? TILE_DNGN_ENTER_VAULTS_OPEN
                              : TILE_DNGN_ENTER_VAULTS_CLOSED;
    case DNGN_ENTER_CRYPT:
        return TILE_DNGN_ENTER_CRYPT;
    case DNGN_ENTER_TOMB:
        return TILE_DNGN_ENTER_TOMB;
    case DNGN_ENTER_ZOT:
        return is_existing_level(level_id(BRANCH_ZOT, 1)) ? TILE_DNGN_ENTER_ZOT_OPEN
                              : TILE_DNGN_ENTER_ZOT_CLOSED;
    case DNGN_ENTER_ZIGGURAT:
        return TILE_DNGN_PORTAL_ZIGGURAT;
    case DNGN_ENTER_BAZAAR:
        return TILE_DNGN_PORTAL_BAZAAR;
    case DNGN_ENTER_TROVE:
        return TILE_DNGN_PORTAL_TROVE;
    case DNGN_ENTER_SEWER:
        return TILE_DNGN_PORTAL_SEWER;
    case DNGN_ENTER_OSSUARY:
        return TILE_DNGN_PORTAL_OSSUARY;
    case DNGN_ENTER_BAILEY:
        return TILE_DNGN_PORTAL_BAILEY;
    case DNGN_ENTER_ICE_CAVE:
        return TILE_DNGN_PORTAL_ICE_CAVE;
    case DNGN_ENTER_VOLCANO:
        return TILE_DNGN_PORTAL_VOLCANO;
    case DNGN_ENTER_WIZLAB:
        return TILE_DNGN_PORTAL_WIZARD_LAB;
    case DNGN_ENTER_DESOLATION:
        return TILE_DNGN_PORTAL_DESOLATION;

    // branch exit stairs
#if TAG_MAJOR_VERSION == 34
    case DNGN_EXIT_DWARF:
    case DNGN_EXIT_FOREST:
    case DNGN_EXIT_BLADE:
        return TILE_DNGN_RETURN;
#endif
    case DNGN_EXIT_TEMPLE:
        return TILE_DNGN_EXIT_TEMPLE;
    case DNGN_EXIT_ORC:
        return TILE_DNGN_EXIT_ORC;
    case DNGN_EXIT_ELF:
        return TILE_DNGN_EXIT_ELF;
    case DNGN_EXIT_LAIR:
        return TILE_DNGN_EXIT_LAIR;
    case DNGN_EXIT_SNAKE:
        return TILE_DNGN_EXIT_SNAKE;
    case DNGN_EXIT_SWAMP:
        return TILE_DNGN_EXIT_SWAMP;
    case DNGN_EXIT_SPIDER:
        return TILE_DNGN_EXIT_SPIDER;
    case DNGN_EXIT_SHOALS:
        return TILE_DNGN_EXIT_SHOALS;
    case DNGN_EXIT_SLIME:
        return TILE_DNGN_EXIT_SLIME;
    case DNGN_EXIT_DEPTHS:
        return TILE_DNGN_RETURN_DEPTHS;
    case DNGN_EXIT_VAULTS:
        return TILE_DNGN_EXIT_VAULTS;
    case DNGN_EXIT_CRYPT:
        return TILE_DNGN_EXIT_CRYPT;
    case DNGN_EXIT_TOMB:
        return TILE_DNGN_EXIT_TOMB;
    case DNGN_EXIT_ZOT:
        return TILE_DNGN_RETURN_ZOT;

    case DNGN_EXIT_ZIGGURAT:
    case DNGN_EXIT_BAZAAR:
    case DNGN_EXIT_TROVE:
    case DNGN_EXIT_SEWER:
    case DNGN_EXIT_OSSUARY:
    case DNGN_EXIT_BAILEY:
    case DNGN_EXIT_DESOLATION:
        return TILE_DNGN_PORTAL;
    case DNGN_EXIT_ICE_CAVE:
        return TILE_DNGN_PORTAL_ICE_CAVE;
    case DNGN_EXIT_VOLCANO:
        return TILE_DNGN_EXIT_VOLCANO;
    case DNGN_EXIT_WIZLAB:
        return TILE_DNGN_PORTAL_WIZARD_LAB;

#if TAG_MAJOR_VERSION == 34
    case DNGN_ENTER_PORTAL_VAULT:
    case DNGN_EXIT_PORTAL_VAULT:
        return TILE_DNGN_PORTAL;
#endif
    case DNGN_EXPIRED_PORTAL:
        return TILE_DNGN_PORTAL_EXPIRED;
    case DNGN_MALIGN_GATEWAY:
        return TILE_DNGN_STARRY_PORTAL;

    // altars
    case DNGN_ALTAR_ZIN:
        return TILE_DNGN_ALTAR_ZIN;
    case DNGN_ALTAR_SHINING_ONE:
        return TILE_DNGN_ALTAR_SHINING_ONE;
    case DNGN_ALTAR_KIKUBAAQUDGHA:
        return TILE_DNGN_ALTAR_KIKUBAAQUDGHA;
    case DNGN_ALTAR_YREDELEMNUL:
        return TILE_DNGN_ALTAR_YREDELEMNUL;
    case DNGN_ALTAR_XOM:
        return TILE_DNGN_ALTAR_XOM;
    case DNGN_ALTAR_VEHUMET:
        return TILE_DNGN_ALTAR_VEHUMET;
    case DNGN_ALTAR_OKAWARU:
        return TILE_DNGN_ALTAR_OKAWARU;
    case DNGN_ALTAR_MAKHLEB:
        return TILE_DNGN_ALTAR_MAKHLEB;
    case DNGN_ALTAR_SIF_MUNA:
        return TILE_DNGN_ALTAR_SIF_MUNA;
    case DNGN_ALTAR_TROG:
        return TILE_DNGN_ALTAR_TROG;
    case DNGN_ALTAR_NEMELEX_XOBEH:
        return TILE_DNGN_ALTAR_NEMELEX_XOBEH;
    case DNGN_ALTAR_ELYVILON:
        return TILE_DNGN_ALTAR_ELYVILON;
    case DNGN_ALTAR_LUGONU:
        return TILE_DNGN_ALTAR_LUGONU;
    case DNGN_ALTAR_BEOGH:
        return TILE_DNGN_ALTAR_BEOGH;
    case DNGN_ALTAR_JIYVA:
        return TILE_DNGN_ALTAR_JIYVA;
    case DNGN_ALTAR_FEDHAS:
        return TILE_DNGN_ALTAR_FEDHAS;
    case DNGN_ALTAR_CHEIBRIADOS:
        return TILE_DNGN_ALTAR_CHEIBRIADOS;
    case DNGN_ALTAR_ASHENZARI:
        return TILE_DNGN_ALTAR_ASHENZARI;
    case DNGN_ALTAR_DITHMENOS:
        return TILE_DNGN_ALTAR_DITHMENOS;
    case DNGN_ALTAR_GOZAG:
        return TILE_DNGN_ALTAR_GOZAG;
    case DNGN_ALTAR_QAZLAL:
        return TILE_DNGN_ALTAR_QAZLAL;
    case DNGN_ALTAR_RU:
        return TILE_DNGN_ALTAR_RU;
    case DNGN_ALTAR_PAKELLAS:
        return TILE_DNGN_ALTAR_PAKELLAS;
    case DNGN_ALTAR_USKAYAW:
        return TILE_DNGN_ALTAR_USKAYAW;
    case DNGN_ALTAR_HEPLIAKLQANA:
        return TILE_DNGN_ALTAR_HEPLIAKLQANA;
    case DNGN_ALTAR_ECUMENICAL:
        return TILE_DNGN_UNKNOWN_ALTAR;
    case DNGN_FOUNTAIN_BLUE:
        return TILE_DNGN_BLUE_FOUNTAIN;
    case DNGN_FOUNTAIN_SPARKLING:
        return TILE_DNGN_SPARKLING_FOUNTAIN;
    case DNGN_FOUNTAIN_BLOOD:
        return TILE_DNGN_BLOOD_FOUNTAIN;
    case DNGN_DRY_FOUNTAIN:
        return TILE_DNGN_DRY_FOUNTAIN;
    case DNGN_PASSAGE_OF_GOLUBRIA:
        return TILE_DNGN_TRAP_GOLUBRIA;
    case DNGN_UNKNOWN_ALTAR:
        return TILE_DNGN_UNKNOWN_ALTAR;
    case DNGN_UNKNOWN_PORTAL:
        return TILE_DNGN_UNKNOWN_PORTAL;
    default:
        return TILE_DNGN_ERROR;
    }
}

bool is_door_tile(tileidx_t tile)
{
    return tile >= TILE_DNGN_CLOSED_DOOR &&
        tile < TILE_DNGN_STONE_ARCH;
}

tileidx_t tileidx_feature(const coord_def &gc)
{
    dungeon_feature_type feat = env.map_knowledge(gc).feat();

    tileidx_t override = env.tile_flv(gc).feat;
    bool can_override = !feat_is_door(feat)
                        && feat != DNGN_FLOOR
                        && feat != DNGN_UNSEEN
                        && feat != DNGN_PASSAGE_OF_GOLUBRIA
                        && feat != DNGN_MALIGN_GATEWAY;
    if (override && can_override)
        return override;

    // Any grid-specific tiles.
    switch (feat)
    {
    case DNGN_FLOOR:
        // branches that can have slime walls (premature optimization?)
        if (player_in_branch(BRANCH_SLIME)
            || player_in_branch(BRANCH_TEMPLE)
            || player_in_branch(BRANCH_LAIR))
        {
            bool slimy = false;
            for (adjacent_iterator ai(gc); ai; ++ai)
            {
                if (env.map_knowledge(*ai).feat() == DNGN_SLIMY_WALL)
                {
                    slimy = true;
                    break;
                }
            }
            if (slimy)
                return TILE_FLOOR_SLIME_ACIDIC;
        }
        // deliberate fall-through
    case DNGN_ROCK_WALL:
    case DNGN_STONE_WALL:
    case DNGN_CRYSTAL_WALL:
    case DNGN_PERMAROCK_WALL:
    case DNGN_CLEAR_PERMAROCK_WALL:
    {
        unsigned colour = env.map_knowledge(gc).feat_colour();
        if (colour == 0)
        {
            colour = feat == DNGN_FLOOR     ? env.floor_colour :
                     feat == DNGN_ROCK_WALL ? env.rock_colour
                                            : 0; // meh
        }
        if (colour >= ETC_FIRST)
        {
            tileidx_t idx = (feat == DNGN_FLOOR) ? env.tile_flv(gc).floor :
                (feat == DNGN_ROCK_WALL) ? env.tile_flv(gc).wall
                : tileidx_feature_base(feat);

#ifdef USE_TILE
            if (feat == DNGN_STONE_WALL)
                apply_variations(env.tile_flv(gc), &idx, gc);
#endif

            tileidx_t base = tile_dngn_basetile(idx);
            tileidx_t spec = idx - base;
            unsigned rc = real_colour(colour, gc);
            return tile_dngn_coloured(base, rc) + spec; // XXX
        }
        return tileidx_feature_base(feat);
    }

    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_TELEPORT:
        return _tileidx_trap(env.map_knowledge(gc).trap());

    case DNGN_TRAP_WEB:
    {
        /*
        trap_type this_trap_type = get_trap_type(gc);
        // There's room here to have different types of webs (acid? fire? ice? different strengths?)
        if (this_trap_type==TRAP_WEB) {*/

        // Determine web connectivity on all sides
        const coord_def neigh[4] =
        {
            coord_def(gc.x, gc.y - 1),
            coord_def(gc.x + 1, gc.y),
            coord_def(gc.x, gc.y + 1),
            coord_def(gc.x - 1, gc.y),
        };
        int solid = 0;
        for (int i = 0; i < 4; i++)
            if (feat_is_solid(env.map_knowledge(neigh[i]).feat())
                || env.map_knowledge(neigh[i]).trap() == TRAP_WEB)
            {
                solid |= 1 << i;
            }
        if (solid)
            return TILE_DNGN_TRAP_WEB_N - 1 + solid;
        return TILE_DNGN_TRAP_WEB;
    }
    case DNGN_ENTER_SHOP:
        return _tileidx_shop(gc);
    case DNGN_DEEP_WATER:
        if (env.map_knowledge(gc).feat_colour() == GREEN
            || env.map_knowledge(gc).feat_colour() == LIGHTGREEN)
        {
            return TILE_DNGN_DEEP_WATER_MURKY;
        }
        else if (player_in_branch(BRANCH_SHOALS))
            return TILE_SHOALS_DEEP_WATER;

        return TILE_DNGN_DEEP_WATER;
    case DNGN_SHALLOW_WATER:
        {
            tileidx_t t = TILE_DNGN_SHALLOW_WATER;
            if (env.map_knowledge(gc).feat_colour() == GREEN
                || env.map_knowledge(gc).feat_colour() == LIGHTGREEN)
            {
                t = TILE_DNGN_SHALLOW_WATER_MURKY;
            }
            else if (player_in_branch(BRANCH_SHOALS))
                t = TILE_SHOALS_SHALLOW_WATER;

            if (env.map_knowledge(gc).invisible_monster())
            {
                // Add disturbance to tile.
                t += tile_dngn_count(t);
            }

            return t;
        }
    default:
        return tileidx_feature_base(feat);
    }
}

static tileidx_t _mon_random(tileidx_t tile)
{
    int count = tile_player_count(tile);
    return tile + ui_random(count);
}

static bool _mons_is_kraken_tentacle(const int mtype)
{
    return mtype == MONS_KRAKEN_TENTACLE
           || mtype == MONS_KRAKEN_TENTACLE_SEGMENT;
}

tileidx_t tileidx_tentacle(const monster_info& mon)
{
    ASSERT(mons_is_tentacle_or_tentacle_segment(mon.type));

    // If the tentacle is submerged, we shouldn't even get here.
    ASSERT(!mon.is(MB_SUBMERGED));

    // Get tentacle position.
    const coord_def t_pos = mon.pos;
    // No parent tentacle, or the connection to the head is unknown.
    bool no_head_connect  = !mon.props.exists("inwards");
    coord_def h_pos       = coord_def(); // head position
    if (!no_head_connect)
    {
        // Get the parent tentacle's location.
        h_pos = t_pos + mon.props["inwards"].get_coord();
    }
    if (no_head_connect && (mon.type == MONS_SNAPLASHER_VINE
                            || mon.type == MONS_SNAPLASHER_VINE_SEGMENT))
    {
        // Find an adjacent tree to pretend we're connected to.
        for (adjacent_iterator ai(t_pos); ai; ++ai)
        {
            if (feat_is_tree(grd(*ai)))
            {
                h_pos = *ai;
                no_head_connect = false;
                break;
            }
        }
    }

    // Is there a connection to the given direction?
    // (either through head or next)
    bool north = false, east = false,
        south = false, west = false,
        north_east = false, south_east = false,
        south_west = false, north_west = false;

    if (!no_head_connect)
    {
        if (h_pos.x == t_pos.x)
        {
            if (h_pos.y < t_pos.y)
                north = true;
            else
                south = true;
        }
        else if (h_pos.y == t_pos.y)
        {
            if (h_pos.x < t_pos.x)
                west = true;
            else
                east = true;
        }
        else if (h_pos.x < t_pos.x)
        {
            if (h_pos.y < t_pos.y)
                north_west = true;
            else
                south_west = true;
        }
        else if (h_pos.x > t_pos.x)
        {
            if (h_pos.y < t_pos.y)
                north_east = true;
            else
                south_east = true;
        }
    }

    // Tentacle only requires checking of head position.
    if (mons_is_tentacle(mon.type))
    {
        if (no_head_connect)
        {
            if (_mons_is_kraken_tentacle(mon.type))
                return _mon_random(TILEP_MONS_KRAKEN_TENTACLE_WATER);
            else return _mon_random(TILEP_MONS_ELDRITCH_TENTACLE_PORTAL);
        }

        // Different handling according to relative positions.
        if (north)
            return TILEP_MONS_KRAKEN_TENTACLE_N;
        if (south)
            return TILEP_MONS_KRAKEN_TENTACLE_S;
        if (west)
            return TILEP_MONS_KRAKEN_TENTACLE_W;
        if (east)
            return TILEP_MONS_KRAKEN_TENTACLE_E;
        if (north_west)
            return TILEP_MONS_KRAKEN_TENTACLE_NW;
        if (south_west)
            return TILEP_MONS_KRAKEN_TENTACLE_SW;
        if (north_east)
            return TILEP_MONS_KRAKEN_TENTACLE_NE;
        if (south_east)
            return TILEP_MONS_KRAKEN_TENTACLE_SE;
        die("impossible kraken direction");
    }
    // Only tentacle segments from now on.
    ASSERT(mons_is_tentacle_segment(mon.type));

    // For segments, we also need the next segment (or end piece).
    coord_def n_pos;
    bool no_next_connect = !mon.props.exists("outwards");
    if (!no_next_connect)
        n_pos = t_pos + mon.props["outwards"].get_coord();

    if (no_head_connect && no_next_connect)
    {
        // Both head and next are submerged.
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_WATER;
    }

    if (!no_next_connect)
    {
        if (n_pos.x == t_pos.x)
        {
            if (n_pos.y < t_pos.y)
                north = true;
            else
                south = true;
        }
        else if (n_pos.y == t_pos.y)
        {
            if (n_pos.x < t_pos.x)
                west = true;
            else
                east = true;
        }
        else if (n_pos.x < t_pos.x)
        {
            if (n_pos.y < t_pos.y)
                north_west = true;
            else
                south_west = true;
        }
        else if (n_pos.x > t_pos.x)
        {
            if (n_pos.y < t_pos.y)
                north_east = true;
            else
                south_east = true;
        }
    }

    if (no_head_connect || no_next_connect)
    {
        // One segment end goes into water, the other
        // into the direction of head or next.

        if (north)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N;
        if (south)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S;
        if (west)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W;
        if (east)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E;
        if (north_west)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW;
        if (south_west)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SW;
        if (north_east)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE;
        if (south_east)
            return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE;
        die("impossible kraken direction");
    }

    // Okay, neither head nor next are submerged.
    // Compare all three positions.

    // Straight lines first: Vertical.
    if (north && south)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_S;
    // Horizontal.
    if (east && west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_W;
    // Diagonals.
    if (north_west && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SE;
    if (north_east && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SW;

    // Curved segments.
    if (east && north)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_N;
    if (east && south)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_S;
    if (south && west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_W;
    if (north && west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_W;
    if (north_east && north_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_NW;
    if (south_east && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE_SW;
    if (north_west && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SW;
    if (north_east && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SE;

    // Connect corners and edges.
    if (north && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SW;
    if (north && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SE;
    if (south && north_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NW;
    if (south && north_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NE;
    if (west && north_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_NE;
    if (west && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SE;
    if (east && north_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_NW;
    if (east && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_SW;

    // Connections at acute angles; can currently only happen for vines.
    if (north && north_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_NW;
    if (north && north_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_NE;
    if (east && north_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_NE;
    if (east && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_SE;
    if (south && south_east)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_SE;
    if (south && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_SW;
    if (west && south_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SW;
    if (west && north_west)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_NW;

    return TILEP_MONS_PROGRAM_BUG;
}

#ifdef USE_TILE
tileidx_t tileidx_out_of_bounds(int branch)
{
    if (branch == BRANCH_SHOALS)
        return TILE_DNGN_OPEN_SEA | TILE_FLAG_UNSEEN;
    else
        return TILE_DNGN_UNSEEN | TILE_FLAG_UNSEEN;
}

void tileidx_out_of_los(tileidx_t *fg, tileidx_t *bg, tileidx_t *cloud, const coord_def& gc)
{
    // Player memory.
    tileidx_t mem_fg = env.tile_bk_fg(gc);
    tileidx_t mem_bg = env.tile_bk_bg(gc);
    tileidx_t mem_cloud = env.tile_bk_cloud(gc);

    // Detected info is just stored in map_knowledge and doesn't get
    // written to what the player remembers. We'll feather that in here.

    const map_cell &cell = env.map_knowledge(gc);

    // Override terrain for magic mapping.
    if (!cell.seen() && env.map_knowledge(gc).mapped())
        *bg = tileidx_feature_base(cell.feat());
    else
        *bg = mem_bg;
    *bg |= tileidx_unseen_flag(gc);
    // Don't draw rays out of los.
    *bg &= ~(TILE_FLAG_RAY_MULTI | TILE_FLAG_RAY_OOR | TILE_FLAG_RAY | TILE_FLAG_LANDING);

    // Override foreground for monsters/items
    if (env.map_knowledge(gc).detected_monster())
    {
        ASSERT(cell.monster() == MONS_SENSED);
        *fg = tileidx_monster_base(cell.monsterinfo()->base_type);
    }
    else if (env.map_knowledge(gc).detected_item())
        *fg = tileidx_item(*cell.item());
    else
        *fg = mem_fg;

    *cloud = mem_cloud;
}

static tileidx_t _zombie_tile_to_spectral(const tileidx_t z_tile)
{
    switch (z_tile)
    {
    case TILEP_MONS_ZOMBIE_SMALL:
    case TILEP_MONS_ZOMBIE_SPRIGGAN:
    case TILEP_MONS_ZOMBIE_GOBLIN:
    case TILEP_MONS_ZOMBIE_HOBGOBLIN:
    case TILEP_MONS_ZOMBIE_GNOLL:
    case TILEP_MONS_ZOMBIE_KOBOLD:
    case TILEP_MONS_ZOMBIE_ORC:
    case TILEP_MONS_ZOMBIE_HUMAN:
    case TILEP_MONS_ZOMBIE_DRACONIAN:
    case TILEP_MONS_ZOMBIE_ELF:
    case TILEP_MONS_ZOMBIE_FAUN:
    case TILEP_MONS_ZOMBIE_MERFOLK:
    case TILEP_MONS_ZOMBIE_MINOTAUR:
    case TILEP_MONS_ZOMBIE_MONKEY:
        return TILEP_MONS_SPECTRAL_SMALL;
    case TILEP_MONS_ZOMBIE_LARGE:
    case TILEP_MONS_ZOMBIE_OGRE:
    case TILEP_MONS_ZOMBIE_TROLL:
    case TILEP_MONS_ZOMBIE_JUGGERNAUT:
        return TILEP_MONS_SPECTRAL_LARGE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_SMALL:
    case TILEP_MONS_ZOMBIE_RAT:
    case TILEP_MONS_ZOMBIE_QUOKKA:
    case TILEP_MONS_ZOMBIE_JACKAL:
    case TILEP_MONS_ZOMBIE_HOUND:
    case TILEP_MONS_ZOMBIE_CRAB:
    case TILEP_MONS_ZOMBIE_TURTLE:
        return TILEP_MONS_SPECTRAL_QUADRUPED_SMALL;
    case TILEP_MONS_ZOMBIE_QUADRUPED_LARGE:
    case TILEP_MONS_ZOMBIE_ELEPHANT:
        return TILEP_MONS_SPECTRAL_QUADRUPED_LARGE;
    case TILEP_MONS_ZOMBIE_FROG:
        return TILEP_MONS_SPECTRAL_FROG;
    case TILEP_MONS_ZOMBIE_BAT:
        return TILEP_MONS_SPECTRAL_BAT;
    case TILEP_MONS_ZOMBIE_BEE:
        return TILEP_MONS_SPECTRAL_BEE;
    case TILEP_MONS_ZOMBIE_BEETLE:
    case TILEP_MONS_ZOMBIE_ROACH:
    case TILEP_MONS_ZOMBIE_BUG:
        return TILEP_MONS_SPECTRAL_BUG;
    case TILEP_MONS_ZOMBIE_FISH:
        return TILEP_MONS_SPECTRAL_FISH;
    case TILEP_MONS_ZOMBIE_CENTAUR:
    case TILEP_MONS_ZOMBIE_YAKTAUR:
        return TILEP_MONS_SPECTRAL_CENTAUR;
    case TILEP_MONS_ZOMBIE_NAGA:
    case TILEP_MONS_ZOMBIE_GUARDIAN_SERPENT:
    case TILEP_MONS_ZOMBIE_SALAMANDER:
        return TILEP_MONS_SPECTRAL_NAGA;
    case TILEP_MONS_ZOMBIE_SNAKE:
    case TILEP_MONS_ZOMBIE_ADDER:
    case TILEP_MONS_ZOMBIE_WORM:
    case TILEP_MONS_ZOMBIE_LINDWURM:
        return TILEP_MONS_SPECTRAL_SNAKE;
    case TILEP_MONS_ZOMBIE_LIZARD:
        return TILEP_MONS_SPECTRAL_LIZARD;
    case TILEP_MONS_ZOMBIE_SCORPION:
    case TILEP_MONS_ZOMBIE_SPIDER_LARGE:
    case TILEP_MONS_ZOMBIE_SPIDER_SMALL:
        return TILEP_MONS_SPECTRAL_SPIDER;
    case TILEP_MONS_ZOMBIE_DRAGON:
    case TILEP_MONS_ZOMBIE_IRON_DRAGON:
    case TILEP_MONS_ZOMBIE_GOLDEN_DRAGON:
    case TILEP_MONS_ZOMBIE_QUICKSILVER_DRAGON:
        return TILEP_MONS_SPECTRAL_DRAGON;
    case TILEP_MONS_ZOMBIE_DRAKE:
    case TILEP_MONS_ZOMBIE_WYVERN:
        return TILEP_MONS_SPECTRAL_DRAKE;
    case TILEP_MONS_ZOMBIE_KRAKEN:
        return TILEP_MONS_SPECTRAL_KRAKEN;
    default:
        if (tile_player_basetile(z_tile) == TILEP_MONS_ZOMBIE_HYDRA)
        {
            return TILEP_MONS_SPECTRAL_HYDRA
                   + (z_tile - TILEP_MONS_ZOMBIE_HYDRA);
        }
    }
    return TILEP_MONS_SPECTRAL_SMALL;
}

static tileidx_t _zombie_tile_to_simulacrum(const tileidx_t z_tile)
{
    switch (z_tile)
    {
    case TILEP_MONS_ZOMBIE_SMALL:
    case TILEP_MONS_ZOMBIE_SPRIGGAN:
    case TILEP_MONS_ZOMBIE_GOBLIN:
    case TILEP_MONS_ZOMBIE_HOBGOBLIN:
    case TILEP_MONS_ZOMBIE_GNOLL:
    case TILEP_MONS_ZOMBIE_KOBOLD:
    case TILEP_MONS_ZOMBIE_ORC:
    case TILEP_MONS_ZOMBIE_HUMAN:
    case TILEP_MONS_ZOMBIE_DRACONIAN:
    case TILEP_MONS_ZOMBIE_ELF:
    case TILEP_MONS_ZOMBIE_FAUN:
    case TILEP_MONS_ZOMBIE_MERFOLK:
    case TILEP_MONS_ZOMBIE_MINOTAUR:
    case TILEP_MONS_ZOMBIE_MONKEY:
        return TILEP_MONS_SIMULACRUM_SMALL;
    case TILEP_MONS_ZOMBIE_LARGE:
    case TILEP_MONS_ZOMBIE_OGRE:
    case TILEP_MONS_ZOMBIE_TROLL:
    case TILEP_MONS_ZOMBIE_JUGGERNAUT:
        return TILEP_MONS_SIMULACRUM_LARGE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_SMALL:
    case TILEP_MONS_ZOMBIE_RAT:
    case TILEP_MONS_ZOMBIE_QUOKKA:
    case TILEP_MONS_ZOMBIE_JACKAL:
    case TILEP_MONS_ZOMBIE_HOUND:
    case TILEP_MONS_ZOMBIE_CRAB:
    case TILEP_MONS_ZOMBIE_TURTLE:
        return TILEP_MONS_SIMULACRUM_QUADRUPED_SMALL;
    case TILEP_MONS_ZOMBIE_QUADRUPED_LARGE:
    case TILEP_MONS_ZOMBIE_FROG:
    case TILEP_MONS_ZOMBIE_ELEPHANT:
        return TILEP_MONS_SIMULACRUM_QUADRUPED_LARGE;
    case TILEP_MONS_ZOMBIE_BAT:
        return TILEP_MONS_SIMULACRUM_BAT;
    case TILEP_MONS_ZOMBIE_BEE:
        return TILEP_MONS_SIMULACRUM_BEE;
    case TILEP_MONS_ZOMBIE_BEETLE:
    case TILEP_MONS_ZOMBIE_ROACH:
    case TILEP_MONS_ZOMBIE_BUG:
        return TILEP_MONS_SIMULACRUM_BUG;
    case TILEP_MONS_ZOMBIE_FISH:
        return TILEP_MONS_SIMULACRUM_FISH;
    case TILEP_MONS_ZOMBIE_CENTAUR:
    case TILEP_MONS_ZOMBIE_YAKTAUR:
        return TILEP_MONS_SIMULACRUM_CENTAUR;
    case TILEP_MONS_ZOMBIE_NAGA:
    case TILEP_MONS_ZOMBIE_GUARDIAN_SERPENT:
    case TILEP_MONS_ZOMBIE_SALAMANDER:
        return TILEP_MONS_SIMULACRUM_NAGA;
    case TILEP_MONS_ZOMBIE_SNAKE:
    case TILEP_MONS_ZOMBIE_ADDER:
    case TILEP_MONS_ZOMBIE_WORM:
    case TILEP_MONS_ZOMBIE_LINDWURM:
        return TILEP_MONS_SIMULACRUM_SNAKE;
    case TILEP_MONS_ZOMBIE_LIZARD:
        return TILEP_MONS_SIMULACRUM_LIZARD;
    case TILEP_MONS_ZOMBIE_SCORPION:
    case TILEP_MONS_ZOMBIE_SPIDER_LARGE:
    case TILEP_MONS_ZOMBIE_SPIDER_SMALL:
        return TILEP_MONS_SIMULACRUM_SPIDER;
    case TILEP_MONS_ZOMBIE_DRAGON:
    case TILEP_MONS_ZOMBIE_IRON_DRAGON:
    case TILEP_MONS_ZOMBIE_GOLDEN_DRAGON:
    case TILEP_MONS_ZOMBIE_QUICKSILVER_DRAGON:
        return TILEP_MONS_SIMULACRUM_DRAGON;
    case TILEP_MONS_ZOMBIE_DRAKE:
    case TILEP_MONS_ZOMBIE_WYVERN:
        return TILEP_MONS_SIMULACRUM_DRAKE;
    case TILEP_MONS_ZOMBIE_KRAKEN:
        return TILEP_MONS_SIMULACRUM_KRAKEN;
    default:
        if (tile_player_basetile(z_tile) == TILEP_MONS_ZOMBIE_HYDRA)
        {
            return TILEP_MONS_SIMULACRUM_HYDRA
                   + (z_tile - TILEP_MONS_ZOMBIE_HYDRA);
        }
    }
    return TILEP_MONS_SIMULACRUM_SMALL;
}

static tileidx_t _zombie_tile_to_skeleton(const tileidx_t z_tile)
{
    switch (z_tile)
    {
    case TILEP_MONS_ZOMBIE_SMALL:
    case TILEP_MONS_ZOMBIE_SPRIGGAN:
    case TILEP_MONS_ZOMBIE_GOBLIN:
    case TILEP_MONS_ZOMBIE_KOBOLD:
    case TILEP_MONS_ZOMBIE_MONKEY:
        return TILEP_MONS_SKELETON_SMALL;
    case TILEP_MONS_ZOMBIE_HOBGOBLIN:
    case TILEP_MONS_ZOMBIE_GNOLL:
    case TILEP_MONS_ZOMBIE_ORC:
    case TILEP_MONS_ZOMBIE_HUMAN:
    case TILEP_MONS_ZOMBIE_DRACONIAN:
    case TILEP_MONS_ZOMBIE_ELF:
    case TILEP_MONS_ZOMBIE_MERFOLK:
    case TILEP_MONS_ZOMBIE_MINOTAUR:
    case TILEP_MONS_ZOMBIE_FAUN:
        return TILEP_MONS_SKELETON_MEDIUM;
    case TILEP_MONS_ZOMBIE_TROLL:
        return TILEP_MONS_SKELETON_TROLL;
    case TILEP_MONS_ZOMBIE_LARGE:
    case TILEP_MONS_ZOMBIE_OGRE:
    case TILEP_MONS_ZOMBIE_JUGGERNAUT:
        return TILEP_MONS_SKELETON_LARGE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_SMALL:
    case TILEP_MONS_ZOMBIE_RAT:
    case TILEP_MONS_ZOMBIE_QUOKKA:
    case TILEP_MONS_ZOMBIE_JACKAL:
    case TILEP_MONS_ZOMBIE_HOUND:
    case TILEP_MONS_ZOMBIE_BEETLE:
    case TILEP_MONS_ZOMBIE_ROACH:
    case TILEP_MONS_ZOMBIE_BUG:
        return TILEP_MONS_SKELETON_QUADRUPED_SMALL;
    case TILEP_MONS_ZOMBIE_LIZARD:
    case TILEP_MONS_ZOMBIE_CRAB:
        return TILEP_MONS_SKELETON_LIZARD;
    case TILEP_MONS_ZOMBIE_TURTLE:
        return TILEP_MONS_SKELETON_TURTLE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_LARGE:
    case TILEP_MONS_ZOMBIE_ELEPHANT:
        return TILEP_MONS_SKELETON_QUADRUPED_LARGE;
    case TILEP_MONS_ZOMBIE_FROG:
        return TILEP_MONS_SKELETON_FROG;
    case TILEP_MONS_ZOMBIE_QUADRUPED_WINGED:
        return TILEP_MONS_SKELETON_QUADRUPED_WINGED;
    case TILEP_MONS_ZOMBIE_BAT:
        return TILEP_MONS_SKELETON_BAT;
    case TILEP_MONS_ZOMBIE_HARPY:
    case TILEP_MONS_ZOMBIE_BIRD:
        return TILEP_MONS_SKELETON_BIRD;
    case TILEP_MONS_ZOMBIE_FISH:
        return TILEP_MONS_SKELETON_FISH;
    case TILEP_MONS_ZOMBIE_CENTAUR:
    case TILEP_MONS_ZOMBIE_YAKTAUR:
        return TILEP_MONS_SKELETON_CENTAUR;
    case TILEP_MONS_ZOMBIE_NAGA:
    case TILEP_MONS_ZOMBIE_GUARDIAN_SERPENT:
    case TILEP_MONS_ZOMBIE_SALAMANDER:
        return TILEP_MONS_SKELETON_NAGA;
    case TILEP_MONS_ZOMBIE_SNAKE:
    case TILEP_MONS_ZOMBIE_ADDER:
    case TILEP_MONS_ZOMBIE_WORM:
    case TILEP_MONS_ZOMBIE_LINDWURM:
        return TILEP_MONS_SKELETON_SNAKE;
    case TILEP_MONS_ZOMBIE_DRAGON:
    case TILEP_MONS_ZOMBIE_IRON_DRAGON:
    case TILEP_MONS_ZOMBIE_GOLDEN_DRAGON:
    case TILEP_MONS_ZOMBIE_QUICKSILVER_DRAGON:
        return TILEP_MONS_SKELETON_DRAGON;
    case TILEP_MONS_ZOMBIE_DRAKE:
    case TILEP_MONS_ZOMBIE_WYVERN:
        return TILEP_MONS_SKELETON_DRAKE;
    case TILEP_MONS_ZOMBIE_UGLY_THING:
        return TILEP_MONS_SKELETON_UGLY_THING;
    default:
        if (tile_player_basetile(z_tile) == TILEP_MONS_ZOMBIE_HYDRA)
        {
            return TILEP_MONS_SKELETON_HYDRA
                   + (z_tile - TILEP_MONS_ZOMBIE_HYDRA);
        }
    }
    return TILEP_MONS_SKELETON_SMALL;
}

/**
 * For a given monster, what tile is appropriate for that monster if it's a
 * zombie?
 *
 * If it's another kind of derived undead (e.g. a skeleton), the actual tile to
 * be used will be derived from the zombie tile we return here.
 *
 * @param mon   The monster in question.
 * @return      An appropriate zombie tile; e.g. TILEP_MONS_ZOMBIE_DRAGON.
 */
static tileidx_t _mon_to_zombie_tile(const monster_info &mon)
{
    const monster_type subtype = mon.base_type;

    // hydras get special casing

    if (subtype == MONS_LERNAEAN_HYDRA && mon.type == MONS_ZOMBIE)
    {
        // Step down the number of heads to get the appropriate tile:
        // for the last five heads, use tiles 1-5, for greater amounts
        // use the next tile for every 5 more heads.
        return tileidx_mon_clamp(TILEP_MONS_LERNAEAN_HYDRA_ZOMBIE,
                                 mon.number <= 5 ?
                                 mon.number - 1 :
                                 4 + (mon.number - 1)/5);
    }
    if (mons_genus(subtype) == MONS_HYDRA)
        return TILEP_MONS_ZOMBIE_HYDRA + min(mon.num_heads, 5) - 1;

    // specific per-species zombies - use to override genuses
    static const map<monster_type, tileidx_t> species_tiles = {
        { MONS_JUGGERNAUT,              TILEP_MONS_ZOMBIE_JUGGERNAUT },
        { MONS_MOTTLED_DRAGON,          TILEP_MONS_ZOMBIE_DRAKE },
        { MONS_STEAM_DRAGON,            TILEP_MONS_ZOMBIE_DRAKE },
        { MONS_JACKAL,                  TILEP_MONS_ZOMBIE_JACKAL },
        { MONS_ADDER,                   TILEP_MONS_ZOMBIE_ADDER },
        { MONS_WOLF_SPIDER,             TILEP_MONS_ZOMBIE_SPIDER_LARGE },
        { MONS_EMPEROR_SCORPION,        TILEP_MONS_ZOMBIE_SPIDER_LARGE },
        { MONS_HOWLER_MONKEY,           TILEP_MONS_ZOMBIE_MONKEY },
        { MONS_IRON_DRAGON,             TILEP_MONS_ZOMBIE_IRON_DRAGON },
        { MONS_GOLDEN_DRAGON,           TILEP_MONS_ZOMBIE_GOLDEN_DRAGON },
        { MONS_QUICKSILVER_DRAGON,      TILEP_MONS_ZOMBIE_QUICKSILVER_DRAGON },
        { MONS_LINDWURM,                TILEP_MONS_ZOMBIE_LINDWURM, },
    };
    // per-genus zombies - use by default
    static const map<monster_type, tileidx_t> genus_tiles = {
        { MONS_GOBLIN,                  TILEP_MONS_ZOMBIE_GOBLIN },
        { MONS_HOBGOBLIN,               TILEP_MONS_ZOMBIE_HOBGOBLIN },
        { MONS_GNOLL,                   TILEP_MONS_ZOMBIE_GNOLL },
        { MONS_HUMAN,                   TILEP_MONS_ZOMBIE_HUMAN },
        { MONS_GHOUL,                   TILEP_MONS_ZOMBIE_HUMAN }, // for skel
        { MONS_KOBOLD,                  TILEP_MONS_ZOMBIE_KOBOLD },
        { MONS_ORC,                     TILEP_MONS_ZOMBIE_ORC },
        { MONS_TROLL,                   TILEP_MONS_ZOMBIE_TROLL },
        { MONS_OGRE,                    TILEP_MONS_ZOMBIE_OGRE },
        { MONS_HARPY,                   TILEP_MONS_ZOMBIE_HARPY },
        { MONS_DRACONIAN,               TILEP_MONS_ZOMBIE_DRACONIAN },
        { MONS_DRAGON,                  TILEP_MONS_ZOMBIE_DRAGON },
        { MONS_WYVERN,                  TILEP_MONS_ZOMBIE_WYVERN },
        { MONS_DRAKE,                   TILEP_MONS_ZOMBIE_DRAKE },
        { MONS_GIANT_LIZARD,            TILEP_MONS_ZOMBIE_LIZARD },
        { MONS_CROCODILE,               TILEP_MONS_ZOMBIE_LIZARD },
        { MONS_RAT,                     TILEP_MONS_ZOMBIE_RAT },
        { MONS_QUOKKA,                  TILEP_MONS_ZOMBIE_QUOKKA },
        { MONS_HOUND,                   TILEP_MONS_ZOMBIE_HOUND },
        { MONS_FROG,                    TILEP_MONS_ZOMBIE_FROG },
        { MONS_CRAB,                    TILEP_MONS_ZOMBIE_CRAB },
        { MONS_SNAPPING_TURTLE,         TILEP_MONS_ZOMBIE_TURTLE },
        { MONS_WORM,                    TILEP_MONS_ZOMBIE_WORM },
        { MONS_BEETLE,                  TILEP_MONS_ZOMBIE_BEETLE },
        { MONS_GIANT_COCKROACH,         TILEP_MONS_ZOMBIE_ROACH },
        { MONS_SCORPION,                TILEP_MONS_ZOMBIE_SCORPION },
        { MONS_KRAKEN,                  TILEP_MONS_ZOMBIE_KRAKEN },
        { MONS_OCTOPODE,                TILEP_MONS_ZOMBIE_OCTOPODE },
        { MONS_UGLY_THING,              TILEP_MONS_ZOMBIE_UGLY_THING },
        { MONS_ELEPHANT,                TILEP_MONS_ZOMBIE_ELEPHANT },
        { MONS_ELF,                     TILEP_MONS_ZOMBIE_ELF },
        { MONS_FAUN,                    TILEP_MONS_ZOMBIE_FAUN },
        { MONS_SATYR,                   TILEP_MONS_ZOMBIE_FAUN },
        { MONS_GUARDIAN_SERPENT,        TILEP_MONS_ZOMBIE_GUARDIAN_SERPENT, },
        { MONS_MERFOLK,                 TILEP_MONS_ZOMBIE_MERFOLK, },
        { MONS_MINOTAUR,                TILEP_MONS_ZOMBIE_MINOTAUR, },
        { MONS_SALAMANDER,              TILEP_MONS_ZOMBIE_SALAMANDER, },
        { MONS_SPRIGGAN,                TILEP_MONS_ZOMBIE_SPRIGGAN, },
        { MONS_YAKTAUR,                 TILEP_MONS_ZOMBIE_YAKTAUR, },
    };

    struct shape_size_tiles {
        tileidx_t small; ///< Z_SMALL and default tile
        tileidx_t big;   ///< Z_BIG tile
    };
    const shape_size_tiles GENERIC_ZOMBIES = { TILEP_MONS_ZOMBIE_SMALL,
                                               TILEP_MONS_ZOMBIE_LARGE };
    static const map<mon_body_shape, shape_size_tiles> shape_tiles = {
        { MON_SHAPE_CENTAUR,            {TILEP_MONS_ZOMBIE_CENTAUR} },
        { MON_SHAPE_NAGA,               {TILEP_MONS_ZOMBIE_NAGA} },
        { MON_SHAPE_QUADRUPED_WINGED,   {TILEP_MONS_ZOMBIE_QUADRUPED_WINGED} },
        { MON_SHAPE_BAT,                {TILEP_MONS_ZOMBIE_BAT} },
        { MON_SHAPE_BIRD,               {TILEP_MONS_ZOMBIE_BIRD} },
        { MON_SHAPE_SNAKE,              {TILEP_MONS_ZOMBIE_SNAKE} },
        { MON_SHAPE_SNAIL,              {TILEP_MONS_ZOMBIE_SNAKE} },
        { MON_SHAPE_FISH,               {TILEP_MONS_ZOMBIE_FISH} },
        { MON_SHAPE_INSECT,             {TILEP_MONS_ZOMBIE_BUG} },
        { MON_SHAPE_CENTIPEDE,          {TILEP_MONS_ZOMBIE_BUG} },
        { MON_SHAPE_INSECT_WINGED,      {TILEP_MONS_ZOMBIE_BEE} },
        { MON_SHAPE_ARACHNID,           {TILEP_MONS_ZOMBIE_SPIDER_SMALL} },
        { MON_SHAPE_QUADRUPED_TAILLESS, {TILEP_MONS_ZOMBIE_QUADRUPED_SMALL,
                                         TILEP_MONS_ZOMBIE_QUADRUPED_LARGE} },
        { MON_SHAPE_QUADRUPED,          {TILEP_MONS_ZOMBIE_QUADRUPED_SMALL,
                                         TILEP_MONS_ZOMBIE_QUADRUPED_LARGE} },
        { MON_SHAPE_HUMANOID,           GENERIC_ZOMBIES },
        { MON_SHAPE_HUMANOID_WINGED,    GENERIC_ZOMBIES },
        { MON_SHAPE_HUMANOID_TAILED,    GENERIC_ZOMBIES },
        { MON_SHAPE_HUMANOID_WINGED_TAILED,   GENERIC_ZOMBIES },
    };

    const tileidx_t *subtype_tile = map_find(species_tiles, subtype);
    if (subtype_tile)
        return *subtype_tile;

    const tileidx_t *genus_tile = map_find(genus_tiles, mons_genus(subtype));
    if (genus_tile)
        return *genus_tile;

    const int z_size = mons_zombie_size(subtype);
    const shape_size_tiles *shape_tile_pair
        = map_find(shape_tiles, get_mon_shape(subtype));
    if (shape_tile_pair)
    {
        if (z_size == Z_BIG && shape_tile_pair->big)
            return shape_tile_pair->big;
        return shape_tile_pair->small;
    }

    return TILEP_ERROR;
}

/// What tile should be used for a given derived undead monster?
static tileidx_t _tileidx_monster_zombified(const monster_info& mon)
{
    const tileidx_t zombie_tile = _mon_to_zombie_tile(mon);
    switch (mon.type)
    {
        case MONS_SKELETON:
            return _zombie_tile_to_skeleton(zombie_tile);
        case MONS_SPECTRAL_THING:
            return _zombie_tile_to_spectral(zombie_tile);
        case MONS_SIMULACRUM:
            return _zombie_tile_to_simulacrum(zombie_tile);
        default:
            return zombie_tile;
    }
}

// Special case for *taurs which have a different tile
// for when they have a bow.
static bool _bow_offset(const monster_info& mon)
{
    if (!mon.inv[MSLOT_WEAPON].get())
        return true;

    switch (mon.inv[MSLOT_WEAPON]->sub_type)
    {
    case WPN_SHORTBOW:
    case WPN_LONGBOW:
    case WPN_ARBALEST:
        return false;
    default:
        return true;
    }
}

static tileidx_t _mon_mod(tileidx_t tile, int offset)
{
    int count = tile_player_count(tile);
    return tile + offset % count;
}

tileidx_t tileidx_mon_clamp(tileidx_t tile, int offset)
{
    int count = tile_player_count(tile);
    return tile + min(max(offset, 0), count - 1);
}

// actually, a triangle wave, but it's up to the actual tiles
static tileidx_t _mon_sinus(tileidx_t tile)
{
    int count = tile_player_count(tile);
    ASSERT(count > 0);
    ASSERT(count > 1); // technically, staying put would work
    int n = you.frame_no % (2 * count - 2);
    return (n < count) ? (tile + n) : (tile + 2 * count - 2 - n);
}

static tileidx_t _mon_cycle(tileidx_t tile, int offset)
{
    int count = tile_player_count(tile);
    return tile + ((offset + you.frame_no) % count);
}

static tileidx_t _modrng(int mod, tileidx_t first, tileidx_t last)
{
    return first + mod % (last - first + 1);
}

// This function allows for getting a monster from "just" the type.
// To avoid needless duplication of a cases in tileidx_monster, some
// extra parameters that have reasonable defaults for monsters where
// only the type is known are pushed here.
tileidx_t tileidx_monster_base(int type, bool in_water, int colour, int number,
                               int tile_num_prop)
{
    switch (type)
    {
    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
    {
        const tileidx_t ugly_tile = (type == MONS_VERY_UGLY_THING) ?
            TILEP_MONS_VERY_UGLY_THING : TILEP_MONS_UGLY_THING;
        int colour_offset = ugly_thing_colour_offset(colour);
        return tileidx_mon_clamp(ugly_tile, colour_offset);
    }

    case MONS_HYDRA:
        // Number of heads
        return tileidx_mon_clamp(TILEP_MONS_HYDRA, number - 1);
    case MONS_SLIME_CREATURE:
    case MONS_MERGED_SLIME_CREATURE:
        return tileidx_mon_clamp(TILEP_MONS_SLIME_CREATURE, number - 1);
    case MONS_LERNAEAN_HYDRA:
        // Step down the number of heads to get the appropriate tile:
        // For the last five heads, use tiles 1-5, for greater amounts
        // use the next tile for every 5 more heads.
        return tileidx_mon_clamp(TILEP_MONS_LERNAEAN_HYDRA,
                                 number <= 5 ?
                                 number - 1 : 4 + (number - 1)/5);

    // draconian ('d')
    case MONS_TIAMAT:
    {
        int offset = 0;
        switch (colour)
        {
        case BLUE:          offset = 0; break;
        case YELLOW:        offset = 1; break;
        case GREEN:         offset = 2; break;
        case LIGHTGREY:     offset = 3; break;
        case LIGHTMAGENTA:  offset = 4; break;
        case CYAN:          offset = 5; break;
        case MAGENTA:       offset = 6; break;
        case RED:           offset = 7; break;
        case WHITE:         offset = 8; break;
        }

        return TILEP_MONS_TIAMAT + offset;
    }
    }

    const monster_type mtype = static_cast<monster_type>(type);
    const tileidx_t base_tile = get_mon_base_tile(mtype);
    const mon_type_tile_variation vary_type = get_mon_tile_variation(mtype);
    switch (vary_type)
    {
    case TVARY_NONE:
        return base_tile;
    case TVARY_MOD:
        return _mon_mod(base_tile, tile_num_prop);
    case TVARY_CYCLE:
        return _mon_cycle(base_tile, tile_num_prop);
    case TVARY_RANDOM:
        return _mon_random(base_tile);
    case TVARY_WATER:
        return base_tile + (in_water ? 1 : 0);
    default:
        die("Unknown tile variation type %d for mon %d!", vary_type, mtype);
    }
}

enum main_dir
{
    NORTH = 0,
    EAST,
    SOUTH,
    WEST
};

enum tentacle_type
{
    TYPE_KRAKEN = 0,
    TYPE_ELDRITCH = 1,
    TYPE_STARSPAWN = 2,
    TYPE_VINE = 3
};

static void _add_tentacle_overlay(const coord_def pos,
                                  const main_dir dir,
                                  tentacle_type type)
{
    /* This adds the corner overlays; e.g. in the following situation:
         .#
         #.
        we need to add corners to the floor tiles since the tentacle
        will otherwise look weird. So when placing the upper tentacle
        tile, this function is called with dir WEST, so the upper
        floor tile gets a corner in the south-east; and similarly,
        when placing the lower tentacle tile, we get called with dir
        EAST to give the lower floor tile a NW overlay.
     */
    coord_def next = pos;
    switch (dir)
    {
        case NORTH: next += coord_def( 0, -1); break;
        case EAST:  next += coord_def( 1,  0); break;
        case SOUTH: next += coord_def( 0,  1); break;
        case WEST:  next += coord_def(-1,  0); break;
        default:
            die("invalid direction");
    }
    if (!in_bounds(next))
        return;

    const coord_def next_showpos(grid2show(next));
    if (!show_bounds(next_showpos))
        return;

    tile_flags flag;
    switch (dir)
    {
        case NORTH: flag = TILE_FLAG_TENTACLE_SW; break;
        case EAST: flag = TILE_FLAG_TENTACLE_NW; break;
        case SOUTH: flag = TILE_FLAG_TENTACLE_NE; break;
        case WEST: flag = TILE_FLAG_TENTACLE_SE; break;
        default: die("invalid direction");
    }
    env.tile_bg(next_showpos) |= flag;

    switch (type)
    {
        case TYPE_ELDRITCH: flag = TILE_FLAG_TENTACLE_ELDRITCH; break;
        case TYPE_STARSPAWN: flag = TILE_FLAG_TENTACLE_STARSPAWN; break;
        case TYPE_VINE: flag = TILE_FLAG_TENTACLE_VINE; break;
        default: flag = TILE_FLAG_TENTACLE_KRAKEN;
    }
    env.tile_bg(next_showpos) |= flag;
}

static void _handle_tentacle_overlay(const coord_def pos,
                                     const tileidx_t tile,
                                     tentacle_type type)
{
    switch (tile)
    {
    case TILEP_MONS_KRAKEN_TENTACLE_NW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_NW:
        _add_tentacle_overlay(pos, NORTH, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_NE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_NE:
        _add_tentacle_overlay(pos, EAST, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SE:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SE:
        _add_tentacle_overlay(pos, SOUTH, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SW:
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_SW:
        _add_tentacle_overlay(pos, WEST, type);
        break;
    // diagonals
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SE:
        _add_tentacle_overlay(pos, NORTH, type);
        _add_tentacle_overlay(pos, SOUTH, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SW:
        _add_tentacle_overlay(pos, EAST, type);
        _add_tentacle_overlay(pos, WEST, type);
        break;
    // other
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_NW:
        _add_tentacle_overlay(pos, NORTH, type);
        _add_tentacle_overlay(pos, EAST, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SE:
        _add_tentacle_overlay(pos, EAST, type);
        _add_tentacle_overlay(pos, SOUTH, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE_SW:
        _add_tentacle_overlay(pos, SOUTH, type);
        _add_tentacle_overlay(pos, WEST, type);
        break;
    case TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SW:
        _add_tentacle_overlay(pos, NORTH, type);
        _add_tentacle_overlay(pos, WEST, type);
        break;
    }
}

static tentacle_type _get_tentacle_type(const int mtype)
{
    switch (mtype)
    {
        case MONS_KRAKEN_TENTACLE:
        case MONS_KRAKEN_TENTACLE_SEGMENT:
            return TYPE_KRAKEN;
        case MONS_ELDRITCH_TENTACLE:
        case MONS_ELDRITCH_TENTACLE_SEGMENT:
            return TYPE_ELDRITCH;
        case MONS_STARSPAWN_TENTACLE:
        case MONS_STARSPAWN_TENTACLE_SEGMENT:
            return TYPE_STARSPAWN;
        case MONS_SNAPLASHER_VINE:
        case MONS_SNAPLASHER_VINE_SEGMENT:
            return TYPE_VINE;

        default:
            die("Invalid tentacle type!");
            return TYPE_KRAKEN; // Silence a warning
    }
}

static bool _tentacle_tile_not_flying(tileidx_t tile)
{
    // All tiles between these two enums feature tentacles
    // emerging from water.
    return tile >= TILEP_FIRST_TENTACLE_IN_WATER
           && tile <= TILEP_LAST_TENTACLE_IN_WATER;
}

static tileidx_t _tileidx_monster_no_props(const monster_info& mon)
{
    const bool in_water = feat_is_water(env.map_knowledge(mon.pos).feat());

    // Show only base class for detected monsters.
    if (mons_class_is_zombified(mon.type))
        return _tileidx_monster_zombified(mon);

    if (mon.props.exists("monster_tile"))
    {
        tileidx_t t = mon.props["monster_tile"].get_short();
        if (t == TILEP_MONS_HELL_WIZARD)
            return _mon_sinus(t);
        return t;
    }

    int tile_num = 0;
    if (mon.props.exists(TILE_NUM_KEY))
        tile_num = mon.props[TILE_NUM_KEY].get_short();

    const tileidx_t base = tileidx_monster_base(mon.type, in_water,
                                                mon.colour(true),
                                                mon.number, tile_num);

    switch (mon.type)
    {
        // use a different tile not using a standard ranged weapon.
        case MONS_CENTAUR:
        case MONS_CENTAUR_WARRIOR:
        case MONS_YAKTAUR:
        case MONS_YAKTAUR_CAPTAIN:
            return base + (_bow_offset(mon) ? 1 : 0);

        case MONS_SLAVE:
            return base + (mon.mname == "freed slave" ? 1 : 0);

        case MONS_BALLISTOMYCETE:
            return base + (mon.is_active ? 1 : 0);

        case MONS_DUVESSA:
        case MONS_DOWAN:
            return mon.props.exists(ELVEN_IS_ENERGIZED_KEY) ? base + 1 : base;

        case MONS_ARACHNE:
        {
            // Arachne normally is drawn with her staff wielded two-handed,
            // but will use a regular stance if she picks up a shield
            // (enhancer staves are compatible with those).
            const item_def* weapon = mon.inv[MSLOT_WEAPON].get();
            if (!mon.inv[MSLOT_SHIELD].get() && weapon
                && (weapon->is_type(OBJ_STAVES, STAFF_POISON)
                    || is_unrandom_artefact(*weapon, UNRAND_OLGREB)))
            {
                return base;
            }
            else
                return base + 1;
        }

        case MONS_AGNES:
        {
            // For if Agnes loses her lajatang
            const item_def * const weapon = mon.inv[MSLOT_WEAPON].get();
            if (weapon && weapon->is_type(OBJ_WEAPONS, WPN_LAJATANG))
                return TILEP_MONS_AGNES;
            else
                return TILEP_MONS_AGNES_STAVELESS;
        }


        case MONS_BUSH:
            if (env.map_knowledge(mon.pos).cloud() == CLOUD_FIRE)
                return TILEP_MONS_BUSH_BURNING;
            return base;

        case MONS_DANCING_WEAPON:
        {
            // Use item tile.
            const item_def& item = *mon.inv[MSLOT_WEAPON];
            return tileidx_item(item) | TILE_FLAG_ANIM_WEP;
        }

        case MONS_SPECTRAL_WEAPON:
        {
            if (!mon.inv[MSLOT_WEAPON].get())
                return TILEP_MONS_SPECTRAL_SBL;

            // Tiles exist for each class of weapon.
            const item_def& item = *mon.inv[MSLOT_WEAPON];
            switch (item_attack_skill(item))
            {
            case SK_LONG_BLADES:
                return TILEP_MONS_SPECTRAL_LBL;
            case SK_AXES:
                return TILEP_MONS_SPECTRAL_AXE;
            case SK_POLEARMS:
                return TILEP_MONS_SPECTRAL_SPEAR;
            case SK_STAVES:
                return TILEP_MONS_SPECTRAL_STAFF;
            case SK_MACES_FLAILS:
                {
                    const weapon_type wt = (weapon_type)item.sub_type;
                    return (wt == WPN_WHIP || wt == WPN_FLAIL
                            || wt == WPN_DIRE_FLAIL || wt == WPN_DEMON_WHIP
                            || wt == WPN_SACRED_SCOURGE) ?
                        TILEP_MONS_SPECTRAL_WHIP : TILEP_MONS_SPECTRAL_MACE;
                }
            default:
                return TILEP_MONS_SPECTRAL_SBL;
            }
        }

        case MONS_KRAKEN_TENTACLE:
        case MONS_KRAKEN_TENTACLE_SEGMENT:
        case MONS_ELDRITCH_TENTACLE:
        case MONS_ELDRITCH_TENTACLE_SEGMENT:
        case MONS_STARSPAWN_TENTACLE:
        case MONS_STARSPAWN_TENTACLE_SEGMENT:
        case MONS_SNAPLASHER_VINE:
        case MONS_SNAPLASHER_VINE_SEGMENT:
        {
            tileidx_t tile = tileidx_tentacle(mon);
            _handle_tentacle_overlay(mon.pos, tile, _get_tentacle_type(mon.type));

            if (!_mons_is_kraken_tentacle(mon.type)
                && tile >= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N
                && (tile <= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SE
                    || tile <= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_NW &&
                       mon.type == MONS_SNAPLASHER_VINE_SEGMENT))
            {
                tile += TILEP_MONS_ELDRITCH_TENTACLE_PORTAL_N;
                tile -= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N;

                if (mon.type == MONS_STARSPAWN_TENTACLE
                    || mon.type == MONS_STARSPAWN_TENTACLE_SEGMENT)
                {
                    tile += TILEP_MONS_STARSPAWN_TENTACLE_N;
                    tile -= TILEP_MONS_ELDRITCH_TENTACLE_N;
                }
                else if (mon.type == MONS_SNAPLASHER_VINE
                         || mon.type == MONS_SNAPLASHER_VINE_SEGMENT)
                {
                    tile += TILEP_MONS_VINE_N;
                    tile -= TILEP_MONS_ELDRITCH_TENTACLE_N;
                }
            }

            return tile;
        }

        case MONS_SENSED:
        {
            // Should be always out of LOS, though...
            if (base == TILEP_MONS_PROGRAM_BUG)
                return TILE_UNSEEN_MONSTER;
            return base;
        }

        default:
            return base;
    }
}

tileidx_t tileidx_monster(const monster_info& mons)
{
    tileidx_t ch = _tileidx_monster_no_props(mons);

    if ((!mons.ground_level() && !_tentacle_tile_not_flying(ch)))
        ch |= TILE_FLAG_FLYING;
    if (mons.is(MB_CAUGHT))
        ch |= TILE_FLAG_NET;
    if (mons.is(MB_WEBBED))
        ch |= TILE_FLAG_WEB;
    if (mons.is(MB_POISONED))
        ch |= TILE_FLAG_POISON;
    if (mons.is(MB_BURNING))
        ch |= TILE_FLAG_STICKY_FLAME;
    if (mons.is(MB_INNER_FLAME))
        ch |= TILE_FLAG_INNER_FLAME;
    if (!mons.constrictor_name.empty())
        ch |= TILE_FLAG_CONSTRICTED;
    if (mons.is(MB_BERSERK))
        ch |= TILE_FLAG_BERSERK;
    if (mons.is(MB_GLOWING))
        ch |= TILE_FLAG_GLOWING;
    if (mons.is(MB_SLOWED))
        ch |= TILE_FLAG_SLOWED;
    if (mons.is(MB_MIRROR_DAMAGE))
        ch |= TILE_FLAG_PAIN_MIRROR;
    if (mons.is(MB_HASTED))
        ch |= TILE_FLAG_HASTED;
    if (mons.is(MB_STRONG))
        ch |= TILE_FLAG_MIGHT;
    if (mons.is(MB_PETRIFYING))
        ch |= TILE_FLAG_PETRIFYING;
    if (mons.is(MB_PETRIFIED))
        ch |= TILE_FLAG_PETRIFIED;
    if (mons.is(MB_BLIND))
        ch |= TILE_FLAG_BLIND;
    if (mons.is(MB_SUMMONED))
        ch |= TILE_FLAG_SUMMONED;
    if (mons.is(MB_PERM_SUMMON))
        ch |= TILE_FLAG_PERM_SUMMON;
    if (mons.is(MB_WORD_OF_RECALL))
        ch |= TILE_FLAG_RECALL;
    if (mons.is(MB_LIGHTLY_DRAINED) || mons.is(MB_HEAVILY_DRAINED))
        ch |= TILE_FLAG_DRAIN;
    if (mons.is(MB_IDEALISED))
        ch |= TILE_FLAG_IDEALISED;
    if (mons.is(MB_BOUND_SOUL))
        ch |= TILE_FLAG_BOUND_SOUL;
    if (mons.is(MB_INFESTATION))
        ch |= TILE_FLAG_INFESTED;

    if (mons.attitude == ATT_FRIENDLY)
        ch |= TILE_FLAG_PET;
    else if (mons.attitude == ATT_GOOD_NEUTRAL)
        ch |= TILE_FLAG_GD_NEUTRAL;
    else if (mons.neutral())
        ch |= TILE_FLAG_NEUTRAL;
    else if (mons.is(MB_FLEEING))
        ch |= TILE_FLAG_FLEEING;
    else if (mons.is(MB_STABBABLE) || mons.is(MB_SLEEPING)
             || mons.is(MB_DORMANT))
    {
        // XXX: should we have different tile flags for "stabbable" versus
        // "sleeping"?
        ch |= TILE_FLAG_STAB;
    }
    // Should petrify show the '?' symbol?
    else if (mons.is(MB_DISTRACTED) && !mons.is(MB_PETRIFYING) && !mons.is(MB_PETRIFIED))
        ch |= TILE_FLAG_MAY_STAB;

    mon_dam_level_type damage_level = mons.dam;

    switch (damage_level)
    {
    case MDAM_DEAD:
    case MDAM_ALMOST_DEAD:
        ch |= TILE_FLAG_MDAM_ADEAD;
        break;
    case MDAM_SEVERELY_DAMAGED:
        ch |= TILE_FLAG_MDAM_SEV;
        break;
    case MDAM_HEAVILY_DAMAGED:
        ch |= TILE_FLAG_MDAM_HEAVY;
        break;
    case MDAM_MODERATELY_DAMAGED:
        ch |= TILE_FLAG_MDAM_MOD;
        break;
    case MDAM_LIGHTLY_DAMAGED:
        ch |= TILE_FLAG_MDAM_LIGHT;
        break;
    case MDAM_OKAY:
    default:
        // no flag for okay.
        break;
    }

#ifdef USE_TILE_LOCAL
    // handled on client side in WebTiles
    if (Options.tile_show_demon_tier)
    {
#endif
        // FIXME: non-linear bits suck, should be a simple addition
        switch (mons_demon_tier(mons.type))
        {
        case 1:
            ch |= TILE_FLAG_DEMON_1;
            break;
        case 2:
            ch |= TILE_FLAG_DEMON_2;
            break;
        case 3:
            ch |= TILE_FLAG_DEMON_3;
            break;
        case 4:
            ch |= TILE_FLAG_DEMON_4;
            break;
        case 5:
            ch |= TILE_FLAG_DEMON_5;
            break;
        }
#ifdef USE_TILE_LOCAL
    }
#endif

    return ch;
}

static tileidx_t tileidx_draco_base(monster_type draco)
{
    return TILEP_DRACO_BASE + (draco - MONS_DRACONIAN);
}

tileidx_t tileidx_draco_base(const monster_info& mon)
{
    return tileidx_draco_base(mon.draco_or_demonspawn_subspecies());
}

tileidx_t tileidx_draco_job(const monster_info& mon)
{
    if (mons_is_draconian_job(mon.type))
        return get_mon_base_tile(mon.type);
    return 0;
}

tileidx_t tileidx_demonspawn_base(const monster_info& mon)
{
    return get_mon_base_tile(mon.draco_or_demonspawn_subspecies());
}

tileidx_t tileidx_demonspawn_job(const monster_info& mon)
{
    if (mons_is_demonspawn_job(mon.type))
        return get_mon_base_tile(mon.type);
    return 0;
}

/**
 * Return the monster tile used for the player based on a monster type.
 *
 * When using the player species monster or a monster in general instead of an
 * explicit tile name, this function cleans up the tiles for certain monsters
 * where there's an alternate tile that's better than the base one for doll
 * purposes.
 * @returns The tile id of the tile that will be used.
*/
tileidx_t tileidx_player_mons()
{
    ASSERT(Options.tile_use_monster != MONS_0);

    monster_type mons;
    if (Options.tile_player_tile)
        return Options.tile_player_tile;

    if (Options.tile_use_monster == MONS_PLAYER)
        mons = player_mons(false);
    else
        mons = Options.tile_use_monster;

    if (mons_is_base_draconian(mons))
        return tileidx_draco_base(mons);

    switch (mons)
    {
    case MONS_CENTAUR:         return TILEP_MONS_CENTAUR_MELEE;
    case MONS_CENTAUR_WARRIOR: return TILEP_MONS_CENTAUR_WARRIOR_MELEE;
    case MONS_YAKTAUR:         return TILEP_MONS_YAKTAUR_MELEE;
    case MONS_YAKTAUR_CAPTAIN: return TILEP_MONS_YAKTAUR_CAPTAIN_MELEE;
    default:                   return tileidx_monster_base(mons);
    }
}

static tileidx_t _tileidx_unrand_artefact(int idx)
{
    const tileidx_t tile = unrandart_to_tile(idx);
    return tile ? tile : TILE_TODO;
}

static tileidx_t _tileidx_weapon_base(const item_def &item)
{
    switch (item.sub_type)
    {
    case WPN_DAGGER:                return TILE_WPN_DAGGER;
    case WPN_SHORT_SWORD:           return TILE_WPN_SHORT_SWORD;
    case WPN_QUICK_BLADE:           return TILE_WPN_QUICK_BLADE;
    case WPN_RAPIER:                return TILE_WPN_RAPIER;
    case WPN_FALCHION:              return TILE_WPN_FALCHION;
    case WPN_LONG_SWORD:            return TILE_WPN_LONG_SWORD;
    case WPN_GREAT_SWORD:           return TILE_WPN_GREAT_SWORD;
    case WPN_SCIMITAR:              return TILE_WPN_SCIMITAR;
    case WPN_DOUBLE_SWORD:          return TILE_WPN_DOUBLE_SWORD;
    case WPN_TRIPLE_SWORD:          return TILE_WPN_TRIPLE_SWORD;
    case WPN_HAND_AXE:              return TILE_WPN_HAND_AXE;
    case WPN_WAR_AXE:               return TILE_WPN_WAR_AXE;
    case WPN_BROAD_AXE:             return TILE_WPN_BROAD_AXE;
    case WPN_BATTLEAXE:             return TILE_WPN_BATTLEAXE;
    case WPN_EXECUTIONERS_AXE:      return TILE_WPN_EXECUTIONERS_AXE;
    case WPN_BLOWGUN:               return TILE_WPN_BLOWGUN;
    case WPN_HUNTING_SLING:         return TILE_WPN_HUNTING_SLING;
    case WPN_GREATSLING:            return TILE_WPN_GREATSLING;
    case WPN_SHORTBOW:              return TILE_WPN_SHORTBOW;
    case WPN_HAND_CROSSBOW:         return TILE_WPN_HAND_CROSSBOW;
    case WPN_ARBALEST:              return TILE_WPN_ARBALEST;
    case WPN_TRIPLE_CROSSBOW:       return TILE_WPN_TRIPLE_CROSSBOW;
    case WPN_SPEAR:                 return TILE_WPN_SPEAR;
    case WPN_TRIDENT:               return TILE_WPN_TRIDENT;
    case WPN_HALBERD:               return TILE_WPN_HALBERD;
    case WPN_SCYTHE:                return TILE_WPN_SCYTHE;
    case WPN_GLAIVE:                return TILE_WPN_GLAIVE;
#if TAG_MAJOR_VERSION == 34
    case WPN_STAFF:                 return TILE_WPN_STAFF;
#endif
    case WPN_QUARTERSTAFF:          return TILE_WPN_QUARTERSTAFF;
    case WPN_CLUB:                  return TILE_WPN_CLUB;
    case WPN_MACE:                  return TILE_WPN_MACE;
    case WPN_FLAIL:                 return TILE_WPN_FLAIL;
    case WPN_GREAT_MACE:            return TILE_WPN_GREAT_MACE;
    case WPN_DIRE_FLAIL:            return TILE_WPN_DIRE_FLAIL;
    case WPN_MORNINGSTAR:           return TILE_WPN_MORNINGSTAR;
    case WPN_EVENINGSTAR:           return TILE_WPN_EVENINGSTAR;
    case WPN_GIANT_CLUB:            return TILE_WPN_GIANT_CLUB;
    case WPN_GIANT_SPIKED_CLUB:     return TILE_WPN_GIANT_SPIKED_CLUB;
    case WPN_WHIP:                  return TILE_WPN_WHIP;
    case WPN_DEMON_BLADE:           return TILE_WPN_DEMON_BLADE;
    case WPN_EUDEMON_BLADE:         return TILE_WPN_BLESSED_BLADE;
    case WPN_DEMON_WHIP:            return TILE_WPN_DEMON_WHIP;
    case WPN_SACRED_SCOURGE:        return TILE_WPN_SACRED_SCOURGE;
    case WPN_DEMON_TRIDENT:         return TILE_WPN_DEMON_TRIDENT;
    case WPN_TRISHULA:              return TILE_WPN_TRISHULA;
    case WPN_LONGBOW:               return TILE_WPN_LONGBOW;
    case WPN_LAJATANG:              return TILE_WPN_LAJATANG;
    case WPN_BARDICHE:              return TILE_WPN_BARDICHE;
    }

    return TILE_ERROR;
}

static tileidx_t _tileidx_weapon(const item_def &item)
{
    tileidx_t tile = _tileidx_weapon_base(item);
    return tileidx_enchant_equ(item, tile);
}

static tileidx_t _tileidx_missile_base(const item_def &item)
{
    int brand = item.brand;
    // 0 indicates no ego at all

    switch (item.sub_type)
    {
    case MI_STONE:        return TILE_MI_STONE;
    case MI_LARGE_ROCK:   return TILE_MI_LARGE_ROCK;
    case MI_THROWING_NET: return TILE_MI_THROWING_NET;
    case MI_TOMAHAWK:
        switch (brand)
        {
        default:             return TILE_MI_TOMAHAWK + 1;
        case 0:              return TILE_MI_TOMAHAWK;
        case SPMSL_STEEL:    return TILE_MI_TOMAHAWK_STEEL;
        case SPMSL_SILVER:   return TILE_MI_TOMAHAWK_SILVER;
        }

    case MI_NEEDLE:
        switch (brand)
        {
        default:             return TILE_MI_NEEDLE + 1;
        case 0:              return TILE_MI_NEEDLE;
        case SPMSL_POISONED: return TILE_MI_NEEDLE_P;
        case SPMSL_CURARE:   return TILE_MI_NEEDLE_CURARE;
        }

    case MI_ARROW:
        switch (brand)
        {
        default:             return TILE_MI_ARROW + 1;
        case 0:              return TILE_MI_ARROW;
        case SPMSL_STEEL:    return TILE_MI_ARROW_STEEL;
        case SPMSL_SILVER:   return TILE_MI_ARROW_SILVER;
        }

    case MI_BOLT:
        switch (brand)
        {
        default:             return TILE_MI_BOLT + 1;
        case 0:              return TILE_MI_BOLT;
        case SPMSL_STEEL:    return TILE_MI_BOLT_STEEL;
        case SPMSL_SILVER:   return TILE_MI_BOLT_SILVER;
        }

    case MI_SLING_BULLET:
        switch (brand)
        {
        default:             return TILE_MI_SLING_BULLET + 1;
        case 0:              return TILE_MI_SLING_BULLET;
        case SPMSL_STEEL:    return TILE_MI_SLING_BULLET_STEEL;
        case SPMSL_SILVER:   return TILE_MI_SLING_BULLET_SILVER;
        }

    case MI_JAVELIN:
        switch (brand)
        {
        default:             return TILE_MI_JAVELIN + 1;
        case 0:              return TILE_MI_JAVELIN;
        case SPMSL_STEEL:    return TILE_MI_JAVELIN_STEEL;
        case SPMSL_SILVER:   return TILE_MI_JAVELIN_SILVER;
        }
    }

    return TILE_ERROR;
}

static tileidx_t _tileidx_missile(const item_def &item)
{
    int tile = _tileidx_missile_base(item);
    return tileidx_enchant_equ(item, tile);
}

static tileidx_t _tileidx_armour_base(const item_def &item)
{
    int type  = item.sub_type;
    switch (type)
    {
    case ARM_ROBE:
        return TILE_ARM_ROBE;

    case ARM_LEATHER_ARMOUR:
        return TILE_ARM_LEATHER_ARMOUR;

    case ARM_RING_MAIL:
        return TILE_ARM_RING_MAIL;

    case ARM_SCALE_MAIL:
        return TILE_ARM_SCALE_MAIL;

    case ARM_CHAIN_MAIL:
        return TILE_ARM_CHAIN_MAIL;

    case ARM_PLATE_ARMOUR:
        return TILE_ARM_PLATE_ARMOUR;

    case ARM_CRYSTAL_PLATE_ARMOUR:
        return TILE_ARM_CRYSTAL_PLATE_ARMOUR;

    case ARM_SHIELD:
        return TILE_ARM_SHIELD;

    case ARM_CLOAK:
        return TILE_ARM_CLOAK;

    case ARM_HAT:
        return TILE_THELM_HAT;

#if TAG_MAJOR_VERSION == 34
    case ARM_CAP:
        return TILE_THELM_CAP;
#endif

    case ARM_HELMET:
        return TILE_THELM_HELM;

    case ARM_GLOVES:
        return TILE_ARM_GLOVES;

    case ARM_BOOTS:
        return TILE_ARM_BOOTS;

    case ARM_BUCKLER:
        return TILE_ARM_BUCKLER;

    case ARM_LARGE_SHIELD:
        return TILE_ARM_LARGE_SHIELD;

    case ARM_CENTAUR_BARDING:
        return TILE_ARM_CENTAUR_BARDING;

    case ARM_NAGA_BARDING:
        return TILE_ARM_NAGA_BARDING;

    case ARM_ANIMAL_SKIN:
        return TILE_ARM_ANIMAL_SKIN;

    case ARM_TROLL_HIDE:
        return TILE_ARM_TROLL_HIDE;

    case ARM_TROLL_LEATHER_ARMOUR:
        return TILE_ARM_TROLL_LEATHER_ARMOUR;

    case ARM_FIRE_DRAGON_HIDE:
        return TILE_ARM_FIRE_DRAGON_HIDE;

    case ARM_FIRE_DRAGON_ARMOUR:
        return TILE_ARM_FIRE_DRAGON_ARMOUR;

    case ARM_ICE_DRAGON_HIDE:
        return TILE_ARM_ICE_DRAGON_HIDE;

    case ARM_ICE_DRAGON_ARMOUR:
        return TILE_ARM_ICE_DRAGON_ARMOUR;

    case ARM_STEAM_DRAGON_HIDE:
        return TILE_ARM_STEAM_DRAGON_HIDE;

    case ARM_STEAM_DRAGON_ARMOUR:
        return TILE_ARM_STEAM_DRAGON_ARMOUR;

    case ARM_MOTTLED_DRAGON_HIDE:
        return TILE_ARM_MOTTLED_DRAGON_HIDE;

    case ARM_MOTTLED_DRAGON_ARMOUR:
        return TILE_ARM_MOTTLED_DRAGON_ARMOUR;

    case ARM_QUICKSILVER_DRAGON_HIDE:
        return TILE_ARM_QUICKSILVER_DRAGON_HIDE;

    case ARM_QUICKSILVER_DRAGON_ARMOUR:
        return TILE_ARM_QUICKSILVER_DRAGON_ARMOUR;

    case ARM_STORM_DRAGON_HIDE:
        return TILE_ARM_STORM_DRAGON_HIDE;

    case ARM_STORM_DRAGON_ARMOUR:
        return TILE_ARM_STORM_DRAGON_ARMOUR;

    case ARM_SHADOW_DRAGON_HIDE:
        return TILE_ARM_SHADOW_DRAGON_HIDE;

    case ARM_SHADOW_DRAGON_ARMOUR:
        return TILE_ARM_SHADOW_DRAGON_ARMOUR;

    case ARM_GOLD_DRAGON_HIDE:
        return TILE_ARM_GOLD_DRAGON_HIDE;

    case ARM_GOLD_DRAGON_ARMOUR:
        return TILE_ARM_GOLD_DRAGON_ARMOUR;

    case ARM_PEARL_DRAGON_HIDE:
        return TILE_ARM_PEARL_DRAGON_HIDE;

    case ARM_PEARL_DRAGON_ARMOUR:
        return TILE_ARM_PEARL_DRAGON_ARMOUR;

    case ARM_SWAMP_DRAGON_HIDE:
        return TILE_ARM_SWAMP_DRAGON_HIDE;

    case ARM_SWAMP_DRAGON_ARMOUR:
        return TILE_ARM_SWAMP_DRAGON_ARMOUR;
    }

    return TILE_ERROR;
}

static tileidx_t _tileidx_armour(const item_def &item)
{
    tileidx_t tile = _tileidx_armour_base(item);
    return tileidx_enchant_equ(item, tile);
}

static tileidx_t _tileidx_chunk(const item_def &item)
{
    if (is_inedible(item))
        return TILE_FOOD_CHUNK_INEDIBLE;

    if (is_mutagenic(item))
        return TILE_FOOD_CHUNK_MUTAGENIC;

    if (is_noxious(item))
        return TILE_FOOD_CHUNK_ROTTING;

    if (is_forbidden_food(item))
        return TILE_FOOD_CHUNK_FORBIDDEN;

    return TILE_FOOD_CHUNK;
}

static tileidx_t _tileidx_food(const item_def &item)
{
    switch (item.sub_type)
    {
    case FOOD_MEAT_RATION:  return TILE_FOOD_MEAT_RATION;
    case FOOD_BREAD_RATION: return TILE_FOOD_BREAD_RATION;
    case FOOD_FRUIT:        return _modrng(item.rnd, TILE_FOOD_FRUIT_FIRST, TILE_FOOD_FRUIT_LAST);
    case FOOD_ROYAL_JELLY:  return TILE_FOOD_ROYAL_JELLY;
    case FOOD_PIZZA:        return TILE_FOOD_PIZZA;
    case FOOD_BEEF_JERKY:   return TILE_FOOD_BEEF_JERKY;
    case FOOD_CHUNK:        return _tileidx_chunk(item);
    case NUM_FOODS:         return TILE_FOOD_BREAD_RATION;
    }

    return TILE_ERROR;
}

// Returns index of skeleton tiles.
// Parameter item holds the skeleton.
static tileidx_t _tileidx_bone(const item_def &item)
{
    const monster_type mc = item.mon_type;
    const size_type st = get_monster_data(mc)->size;
    int cs = 0;

    switch (st)
    {
    default:
        cs = 0; break;
    case SIZE_MEDIUM:
        cs = 1; break;
    case SIZE_LARGE:
    case SIZE_BIG:
        cs = 2; break;
    case SIZE_GIANT:
        cs = 3; break;
    }

    switch (get_mon_shape(item.mon_type))
    {
    case MON_SHAPE_HUMANOID:
    case MON_SHAPE_HUMANOID_TAILED:
    case MON_SHAPE_HUMANOID_WINGED:
    case MON_SHAPE_HUMANOID_WINGED_TAILED:
        return TILE_FOOD_BONE_HUMANOID + cs;
    default:
        return TILE_FOOD_BONE + cs;
    }
}

// Returns index of corpse tiles.
// Parameter item holds the corpse.
static tileidx_t _tileidx_corpse(const item_def &item)
{
    const int type = item.plus;
    const tileidx_t base = get_mon_base_corpse_tile((monster_type)type);

    switch (type)
    {
    case MONS_KILLER_KLOWN:
    {
        const int count = tile_main_count(TILE_CORPSE_KILLER_KLOWN);
        return base + ui_random(count);
    }

    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
    {
        int colour_offset = ugly_thing_colour_offset(item.get_colour());
        if (colour_offset == -1)
            colour_offset = 0;
        return base + colour_offset;
    }

    default:
        return base;
    }
}

static tileidx_t _tileidx_rune(const item_def &item)
{
    switch (item.sub_type)
    {
    // the hell runes:
    case RUNE_DIS:         return TILE_RUNE_DIS;
    case RUNE_GEHENNA:     return TILE_RUNE_GEHENNA;
    case RUNE_COCYTUS:     return TILE_RUNE_COCYTUS;
    case RUNE_TARTARUS:    return TILE_RUNE_TARTARUS;

    // special pandemonium runes:
    case RUNE_MNOLEG:      return TILE_RUNE_MNOLEG;
    case RUNE_LOM_LOBON:   return TILE_RUNE_LOM_LOBON;
    case RUNE_CEREBOV:     return TILE_RUNE_CEREBOV;
    case RUNE_GLOORX_VLOQ: return TILE_RUNE_GLOORX_VLOQ;

    case RUNE_DEMONIC:     return TILE_RUNE_DEMONIC
        + ((uint32_t)item.rnd) % tile_main_count(TILE_RUNE_DEMONIC);
    case RUNE_ABYSSAL:     return TILE_RUNE_ABYSS;

    case RUNE_SNAKE:       return TILE_RUNE_SNAKE;
    case RUNE_SPIDER:      return TILE_RUNE_SPIDER;
    case RUNE_SLIME:       return TILE_RUNE_SLIME;
    case RUNE_VAULTS:      return TILE_RUNE_VAULTS;
    case RUNE_TOMB:        return TILE_RUNE_TOMB;
    case RUNE_SWAMP:       return TILE_RUNE_SWAMP;
    case RUNE_SHOALS:      return TILE_RUNE_SHOALS;
    case RUNE_ELF:         return TILE_RUNE_ELVEN;

    case RUNE_FOREST:
    default:               return TILE_MISC_RUNE_OF_ZOT;
    }
}

static tileidx_t _tileidx_misc(const item_def &item)
{
    if (is_deck(item, true))
    {
        tileidx_t ch = TILE_ERROR;
        switch (item.deck_rarity)
        {
            case DECK_RARITY_LEGENDARY:
                ch = TILE_MISC_DECK_LEGENDARY;
                break;
            case DECK_RARITY_RARE:
                ch = TILE_MISC_DECK_RARE;
                break;
            case DECK_RARITY_COMMON:
            default:
                ch = TILE_MISC_DECK;
                break;
        }

        if (item.flags & ISFLAG_KNOW_TYPE
#if TAG_MAJOR_VERSION == 34
            && item.sub_type != MISC_DECK_OF_ODDITIES // non-contiguous
#endif
            )
        {
            // NOTE: order of tiles must be identical to order of decks.
            int offset = item.sub_type - MISC_FIRST_DECK + 1;
            ch += offset;
        }
        return ch;
    }

    switch (item.sub_type)
    {
#if TAG_MAJOR_VERSION == 34
    case MISC_BOTTLED_EFREET:
        return TILE_MISC_BOTTLED_EFREET;
#endif

    case MISC_FAN_OF_GALES:
        return evoker_is_charged(item) ? TILE_MISC_FAN_OF_GALES
                                       : TILE_MISC_FAN_OF_GALES_INERT;

    case MISC_LAMP_OF_FIRE:
        return evoker_is_charged(item) ? TILE_MISC_LAMP_OF_FIRE
                                       : TILE_MISC_LAMP_OF_FIRE_INERT;

#if TAG_MAJOR_VERSION == 34
    case MISC_STONE_OF_TREMORS:
        return TILE_MISC_STONE_OF_TREMORS_INERT;
#endif

    case MISC_PHIAL_OF_FLOODS:
        return evoker_is_charged(item) ? TILE_MISC_PHIAL_OF_FLOODS
                                       : TILE_MISC_PHIAL_OF_FLOODS_INERT;

#if TAG_MAJOR_VERSION == 34
    case MISC_BUGGY_LANTERN_OF_SHADOWS:
        return TILE_MISC_LANTERN_OF_SHADOWS;
#endif

    case MISC_HORN_OF_GERYON:
        return TILE_MISC_HORN_OF_GERYON;

    case MISC_BOX_OF_BEASTS:
        return TILE_MISC_BOX_OF_BEASTS;

    case MISC_CRYSTAL_BALL_OF_ENERGY:
        return TILE_MISC_CRYSTAL_BALL_OF_ENERGY;

    case MISC_DISC_OF_STORMS:
        return TILE_MISC_DISC_OF_STORMS;

    case MISC_SACK_OF_SPIDERS:
        return TILE_MISC_SACK_OF_SPIDERS;

    case MISC_PHANTOM_MIRROR:
        return TILE_MISC_PHANTOM_MIRROR;

    case MISC_ZIGGURAT:
        return TILE_MISC_ZIGGURAT;

    case MISC_QUAD_DAMAGE:
        return TILE_MISC_QUAD_DAMAGE;
    }

    return TILE_ERROR;
}

static tileidx_t _tileidx_gold(const item_def &item)
{
    if (item.quantity >= 1 && item.quantity <= 10)
        return TILE_GOLD01 + item.quantity - 1;
    if (item.quantity < 20)
        return TILE_GOLD16;
    if (item.quantity < 30)
        return TILE_GOLD19;
    if (item.quantity < 100)
        return TILE_GOLD23;
    return TILE_GOLD25;
}

tileidx_t tileidx_item(const item_def &item)
{
    if (item.props.exists("item_tile"))
        return item.props["item_tile"].get_short();

    const int clas        = item.base_type;
    const int type        = item.sub_type;
    const int subtype_rnd = item.subtype_rnd;
    const int rnd         = item.rnd;

    switch (clas)
    {
    case OBJ_WEAPONS:
        if (is_unrandom_artefact(item) && !is_randapp_artefact(item))
            return _tileidx_unrand_artefact(find_unrandart_index(item));
        else
            return _tileidx_weapon(item);

    case OBJ_MISSILES:
        return _tileidx_missile(item);

    case OBJ_ARMOUR:
        if (is_unrandom_artefact(item) && !is_randapp_artefact(item))
            return _tileidx_unrand_artefact(find_unrandart_index(item));
        else
            return _tileidx_armour(item);

    case OBJ_WANDS:
        if (item.flags & ISFLAG_KNOW_TYPE)
            return TILE_WAND_ID_FIRST + type;
        else
            return TILE_WAND_OFFSET + subtype_rnd % NDSC_WAND_PRI;

    case OBJ_FOOD:
        return _tileidx_food(item);

    case OBJ_SCROLLS:
        if (item.flags & ISFLAG_KNOW_TYPE)
            return TILE_SCR_ID_FIRST + type;
        return TILE_SCROLL;

    case OBJ_GOLD:
        return _tileidx_gold(item);

    case OBJ_JEWELLERY:
        if (is_unrandom_artefact(item) && !is_randapp_artefact(item))
            return _tileidx_unrand_artefact(find_unrandart_index(item));

        // rings
        if (!jewellery_is_amulet(item))
        {
            if (is_artefact(item))
            {
                const int offset = item.rnd
                                   % tile_main_count(TILE_RING_RANDART_OFFSET);
                return TILE_RING_RANDART_OFFSET + offset;
            }

            if (item.flags & ISFLAG_KNOW_TYPE)
            {
                return TILE_RING_ID_FIRST + type - RING_FIRST_RING
#if TAG_MAJOR_VERSION == 34
                       + 1 // we have a save-compat ring tile before FIRST_RING
#endif
                    ;
            }

            return TILE_RING_NORMAL_OFFSET + subtype_rnd % NDSC_JEWEL_PRI;
        }

        // amulets
        if (is_artefact(item))
        {
            const int offset = item.rnd
                               % tile_main_count(TILE_AMU_RANDART_OFFSET);
            return TILE_AMU_RANDART_OFFSET + offset;
        }

        if (item.flags & ISFLAG_KNOW_TYPE)
            return TILE_AMU_ID_FIRST + type - AMU_FIRST_AMULET;
        return TILE_AMU_NORMAL_OFFSET + subtype_rnd % NDSC_JEWEL_PRI;

    case OBJ_POTIONS:
        if (item.flags & ISFLAG_KNOW_TYPE)
            return TILE_POT_ID_FIRST + type;
        else
            return TILE_POTION_OFFSET + item.subtype_rnd % NDSC_POT_PRI;

    case OBJ_BOOKS:
        if (is_random_artefact(item))
        {
            const int offset = rnd % tile_main_count(TILE_BOOK_RANDART_OFFSET);
            return TILE_BOOK_RANDART_OFFSET + offset;
        }

        if (item.sub_type == BOOK_MANUAL)
            return TILE_BOOK_MANUAL + rnd % tile_main_count(TILE_BOOK_MANUAL);

        return TILE_BOOK_OFFSET
               + rnd % tile_main_count(TILE_BOOK_OFFSET);

    case OBJ_STAVES:
        if (item.flags & ISFLAG_KNOW_TYPE)
            return TILE_STAFF_ID_FIRST + type;

        return TILE_STAFF_OFFSET
               + (subtype_rnd / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;

    case OBJ_RODS:
        return TILE_ROD + item.rnd % tile_main_count(TILE_ROD);

    case OBJ_CORPSES:
        if (item.sub_type == CORPSE_SKELETON)
            return _tileidx_bone(item);
        else
            return _tileidx_corpse(item);

    case OBJ_ORBS:
        return TILE_ORB + ui_random(tile_main_count(TILE_ORB));

    case OBJ_MISCELLANY:
        return _tileidx_misc(item);

    case OBJ_RUNES:
        return _tileidx_rune(item);

    case OBJ_DETECTED:
        return TILE_UNSEEN_ITEM;

    default:
        return TILE_ERROR;
    }
}

//  Determine Octant of missile direction
//   .---> X+
//   |
//   |  701
//   Y  6O2
//   +  543
//
// The octant boundary slope tan(pi/8)=sqrt(2)-1 = 0.414 is approximated by 2/5.
static int _tile_bolt_dir(int dx, int dy)
{
    int ax = abs(dx);
    int ay = abs(dy);

    if (5*ay < 2*ax)
        return (dx > 0) ? 2 : 6;
    else if (5*ax < 2*ay)
        return (dy > 0) ? 4 : 0;
    else if (dx > 0)
        return (dy > 0) ? 3 : 1;
    else
        return (dy > 0) ? 5: 7;
}

tileidx_t tileidx_item_throw(const item_def &item, int dx, int dy)
{
    if (item.base_type == OBJ_MISSILES)
    {
        int ch = -1;
        int dir = _tile_bolt_dir(dx, dy);

        // Thrown items with multiple directions
        switch (item.sub_type)
        {
            case MI_ARROW:
                ch = TILE_MI_ARROW0;
                break;
            case MI_BOLT:
                ch = TILE_MI_BOLT0;
                break;
            case MI_NEEDLE:
                ch = TILE_MI_NEEDLE0;
                break;
            case MI_JAVELIN:
                ch = TILE_MI_JAVELIN0;
                break;
            case MI_THROWING_NET:
                ch = TILE_MI_THROWING_NET0;
                break;
            case MI_TOMAHAWK:
                ch = TILE_MI_TOMAHAWK0;
            default:
                break;
        }
        if (ch != -1)
            return ch + dir;

        // Thrown items with a single direction
        switch (item.sub_type)
        {
            case MI_STONE:
                ch = TILE_MI_STONE0;
                break;
            case MI_SLING_BULLET:
                switch (item.brand)
                {
                default:
                    ch = TILE_MI_SLING_BULLET0;
                    break;
                case SPMSL_STEEL:
                    ch = TILE_MI_SLING_BULLET_STEEL0;
                    break;
                case SPMSL_SILVER:
                    ch = TILE_MI_SLING_BULLET_SILVER0;
                    break;
                }
                break;
            case MI_LARGE_ROCK:
                ch = TILE_MI_LARGE_ROCK0;
                break;
            case MI_THROWING_NET:
                ch = TILE_MI_THROWING_NET0;
                break;
            default:
                break;
        }
        if (ch != -1)
            return tileidx_enchant_equ(item, ch);
    }

    // If not a special case, just return the default tile.
    return tileidx_item(item);
}

// For items with randomized descriptions, only the overlay label is
// placed in the tile page. This function looks up what the base item
// is based on the randomized description. It returns 0 if there is none.
tileidx_t tileidx_known_base_item(tileidx_t label)
{
    if (label >= TILE_POT_ID_FIRST && label <= TILE_POT_ID_LAST)
    {
        int type = label - TILE_POT_ID_FIRST;
        int desc = you.item_description[IDESC_POTIONS][type] % NDSC_POT_PRI;

        if (!get_ident_type(OBJ_POTIONS, type))
            return TILE_UNSEEN_POTION;
        else
            return TILE_POTION_OFFSET + desc;
    }

    if (label >= TILE_RING_ID_FIRST && label <= TILE_RING_ID_LAST)
    {
        int type = label - TILE_RING_ID_FIRST + RING_FIRST_RING
#if TAG_MAJOR_VERSION == 34
                   - 1 // we have a save-compat ring tile before FIRST_RING
#endif
            ;
        int desc = you.item_description[IDESC_RINGS][type] % NDSC_JEWEL_PRI;

        if (!get_ident_type(OBJ_JEWELLERY, type))
            return TILE_UNSEEN_RING;
        else
            return TILE_RING_NORMAL_OFFSET + desc;
    }

    if (label >= TILE_AMU_ID_FIRST && label <= TILE_AMU_ID_LAST)
    {
        int type = label - TILE_AMU_ID_FIRST + AMU_FIRST_AMULET;
        int desc = you.item_description[IDESC_RINGS][type] % NDSC_JEWEL_PRI;

        if (!get_ident_type(OBJ_JEWELLERY, type))
            return TILE_UNSEEN_AMULET;
        else
            return TILE_AMU_NORMAL_OFFSET + desc;
    }

    if (label >= TILE_SCR_ID_FIRST && label <= TILE_SCR_ID_LAST)
        return TILE_SCROLL;

    if (label >= TILE_WAND_ID_FIRST && label <= TILE_WAND_ID_LAST)
    {
        int type = label - TILE_WAND_ID_FIRST;
        int desc = you.item_description[IDESC_WANDS][type] % NDSC_WAND_PRI;

        if (!get_ident_type(OBJ_WANDS, type))
            return TILE_UNSEEN_WAND;
        else
            return TILE_WAND_OFFSET + desc;
    }

    if (label >= TILE_STAFF_ID_FIRST && label <= TILE_STAFF_ID_LAST)
    {
        int type = label - TILE_STAFF_ID_FIRST;
        int desc = you.item_description[IDESC_STAVES][type];
        desc = (desc / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;

        if (!get_ident_type(OBJ_STAVES, type))
            return TILE_UNSEEN_STAFF;
        else
            return TILE_STAFF_OFFSET + desc;
    }

    return 0;
}

tileidx_t tileidx_cloud(const cloud_info &cl)
{
    const cloud_type type  = cl.type;
    const int colour = cl.colour;
    const unsigned int dur = cl.duration;

    tileidx_t ch = cl.tile;

    if (ch == 0)
    {
        const cloud_tile_info &tile_info = cloud_type_tile_info(type);

        switch (tile_info.variation)
        {
            case CTVARY_NONE:
                ch = tile_info.base;
                break;
            case CTVARY_DUR:
                ch = tile_info.base + min(dur,
                                          tile_main_count(tile_info.base) - 1);
                break;
            case CTVARY_RANDOM:
                ch = tile_info.base
                     + ui_random(tile_main_count(tile_info.base));
                break;
        }

        if (!ch || ch == TILE_ERROR)
            ch = TILE_CLOUD_GREY_SMOKE;

        switch (type)
        {
            case CLOUD_MUTAGENIC:
                ch = (dur == 0 ? TILE_CLOUD_MUTAGENIC_0 :
                      dur == 1 ? TILE_CLOUD_MUTAGENIC_1
                               : TILE_CLOUD_MUTAGENIC_2);
                ch += ui_random(tile_main_count(ch));
                break;

            case CLOUD_TORNADO:
                ch = get_tornado_phase(cl.pos) ? TILE_CLOUD_RAGING_WINDS_0
                                               : TILE_CLOUD_RAGING_WINDS_1;
                break;

            default:
                break;
        }
    }

    if (colour != -1)
        ch = tile_main_coloured(ch, colour);

    // XXX: Should be no need for TILE_FLAG_FLYING anymore since clouds are
    // drawn in a separate layer but I'll leave it for now in case anything changes --mumra
    return ch | TILE_FLAG_FLYING;
}

tileidx_t tileidx_bolt(const bolt &bolt)
{
    const int col = bolt.colour;
    const coord_def diff = bolt.target - bolt.source;
    const int dir = _tile_bolt_dir(diff.x, diff.y);

    switch (col)
    {
    case WHITE:
        if (bolt.name == "crystal spear")
            return TILE_BOLT_CRYSTAL_SPEAR + dir;
        else if (bolt.name == "puff of frost")
            return TILE_BOLT_FROST;
        else if (bolt.name == "shard of ice")
            return TILE_BOLT_ICICLE + dir;
        else if (bolt.name == "searing ray")
            return TILE_BOLT_SEARING_RAY_III;
        break;

    case LIGHTCYAN:
        if (bolt.name == "iron shot")
            return TILE_BOLT_IRON_SHOT + dir;
        else if (bolt.name == "zap")
            return TILE_BOLT_ZAP + dir % tile_main_count(TILE_BOLT_ZAP);
        break;

    case RED:
        if (bolt.name == "puff of flame")
            return TILE_BOLT_FLAME;
        break;

    case LIGHTRED:
        if (bolt.name.find("damnation") != string::npos)
            return TILE_BOLT_DAMNATION;
        break;

    case LIGHTMAGENTA:
        if (bolt.name == "magic dart")
            return TILE_BOLT_MAGIC_DART;
        else if (bolt.name == "searing ray")
            return TILE_BOLT_SEARING_RAY_II;
        break;

    case BROWN:
        if (bolt.name == "rocky blast"
            || bolt.name == "large rocky blast"
            || bolt.name == "blast of sand")
        {
            return TILE_BOLT_SANDBLAST;
        }
        break;

    case GREEN:
        if (bolt.name == "sting")
            return TILE_BOLT_STING;
        break;

    case LIGHTGREEN:
        if (bolt.name == "poison arrow")
            return TILE_BOLT_POISON_ARROW + dir;
        break;

    case LIGHTGREY:
        if (bolt.name == "stone arrow")
            return TILE_BOLT_STONE_ARROW + dir;
        break;

    case DARKGREY:
        if (bolt.name == "bolt of negative energy")
            return TILE_BOLT_DRAIN;
        break;

    case MAGENTA:
        if (bolt.name == "searing ray")
            return TILE_BOLT_SEARING_RAY_I;
        break;

    case CYAN:
        if (bolt.name == "slug dart")
            return TILE_BOLT_STONE_ARROW + dir;
        break;

    case ETC_MUTAGENIC:
        if (bolt.name == "irradiate" || bolt.name == "unravelling")
            return TILE_BOLT_IRRADIATE;
        break;
    }

    return tileidx_zap(col);
}

tileidx_t vary_bolt_tile(tileidx_t tile, int dist)
{
    switch (tile)
    {
    case TILE_BOLT_FROST:
    case TILE_BOLT_MAGIC_DART:
    case TILE_BOLT_SANDBLAST:
    case TILE_BOLT_STING:
        return tile + dist % tile_main_count(tile);
    case TILE_BOLT_FLAME:
    case TILE_BOLT_IRRADIATE:
        return tile + ui_random(tile_main_count(tile));
    default:
        return tile;
    }
}

tileidx_t tileidx_zap(int colour)
{
    switch (colour)
    {
    case ETC_HOLY:
        colour = YELLOW;
        break;
    default:
        colour = element_colour(colour);
        break;
    }

    if (colour < 1)
        colour = 7;
    else if (colour > 8)
        colour -= 8;

    return TILE_SYM_BOLT_OFS - 1 + colour;
}

tileidx_t tileidx_spell(spell_type spell)
{
    if (spell == NUM_SPELLS)
        return TILEG_MEMORISE; // XXX: Hack!
    return get_spell_tile(spell);
}

/**
 * Get the appropriate tile for the given skill @ the given training level.
 *
 * @param skill     The skill in question; e.g. SK_FIGHTING.
 * @param train     The training_status to render at; e.g. TRAINING_DISABLED.
 * @return          An appropriate tile; e.g. TILEG_FIGHTING_OFF>
 */
tileidx_t tileidx_skill(skill_type skill, int train)
{
    tileidx_t ch;
    switch (skill)
    {
    case SK_FIGHTING:       ch = TILEG_FIGHTING_ON; break;
    case SK_SHORT_BLADES:   ch = TILEG_SHORT_BLADES_ON; break;
    case SK_LONG_BLADES:    ch = TILEG_LONG_BLADES_ON; break;
    case SK_AXES:           ch = TILEG_AXES_ON; break;
    case SK_MACES_FLAILS:   ch = TILEG_MACES_FLAILS_ON; break;
    case SK_POLEARMS:       ch = TILEG_POLEARMS_ON; break;
    case SK_STAVES:         ch = TILEG_STAVES_ON; break;
    case SK_SLINGS:         ch = TILEG_SLINGS_ON; break;
    case SK_BOWS:           ch = TILEG_BOWS_ON; break;
    case SK_CROSSBOWS:      ch = TILEG_CROSSBOWS_ON; break;
    case SK_THROWING:       ch = TILEG_THROWING_ON; break;
    case SK_ARMOUR:         ch = TILEG_ARMOUR_ON; break;
    case SK_DODGING:        ch = TILEG_DODGING_ON; break;
    case SK_STEALTH:        ch = TILEG_STEALTH_ON; break;
    case SK_SHIELDS:        ch = TILEG_SHIELDS_ON; break;
    case SK_UNARMED_COMBAT:
        {
            const string hand = you.hand_name(false);
            if (hand == "hand")
                ch = TILEG_UNARMED_COMBAT_ON;
            else if (hand == "paw")
                ch = TILEG_UNARMED_COMBAT_PAW_ON;
            else if (hand == "tentacle")
                ch = TILEG_UNARMED_COMBAT_TENTACLE_ON;
            else
                ch = TILEG_UNARMED_COMBAT_CLAW_ON;
        }
        break;
    case SK_SPELLCASTING:   ch = TILEG_SPELLCASTING_ON; break;
    case SK_CONJURATIONS:   ch = TILEG_CONJURATIONS_ON; break;
    case SK_HEXES:          ch = TILEG_HEXES_ON; break;
    case SK_CHARMS:         ch = TILEG_CHARMS_ON; break;
    case SK_SUMMONINGS:     ch = TILEG_SUMMONINGS_ON; break;
    case SK_NECROMANCY:
        ch = you.religion == GOD_KIKUBAAQUDGHA ? TILEG_NECROMANCY_K_ON
                                               : TILEG_NECROMANCY_ON; break;
    case SK_TRANSLOCATIONS: ch = TILEG_TRANSLOCATIONS_ON; break;
    case SK_TRANSMUTATIONS: ch = TILEG_TRANSMUTATIONS_ON; break;
    case SK_FIRE_MAGIC:     ch = TILEG_FIRE_MAGIC_ON; break;
    case SK_ICE_MAGIC:      ch = TILEG_ICE_MAGIC_ON; break;
    case SK_AIR_MAGIC:      ch = TILEG_AIR_MAGIC_ON; break;
    case SK_EARTH_MAGIC:    ch = TILEG_EARTH_MAGIC_ON; break;
    case SK_POISON_MAGIC:   ch = TILEG_POISON_MAGIC_ON; break;
    case SK_EVOCATIONS:
        {
            switch (you.religion)
            {
            case GOD_NEMELEX_XOBEH:
                ch = TILEG_EVOCATIONS_N_ON; break;
            case GOD_PAKELLAS:
                ch = TILEG_EVOCATIONS_P_ON; break;
            default:
                ch = TILEG_EVOCATIONS_ON;
            }
        }
        break;
    case SK_INVOCATIONS:
        {
            switch (you.religion)
            {
            // Gods who use invo get a unique tile.
            case GOD_SHINING_ONE:
                ch = TILEG_INVOCATIONS_1_ON; break;
            case GOD_BEOGH:
                ch = TILEG_INVOCATIONS_B_ON; break;
            case GOD_CHEIBRIADOS:
                ch = TILEG_INVOCATIONS_C_ON; break;
            case GOD_DITHMENOS:
                ch = TILEG_INVOCATIONS_D_ON; break;
            case GOD_ELYVILON:
                ch = TILEG_INVOCATIONS_E_ON; break;
            case GOD_FEDHAS:
                ch = TILEG_INVOCATIONS_F_ON; break;
            case GOD_HEPLIAKLQANA:
                ch = TILEG_INVOCATIONS_H_ON; break;
            case GOD_LUGONU:
                ch = TILEG_INVOCATIONS_L_ON; break;
            case GOD_MAKHLEB:
                ch = TILEG_INVOCATIONS_M_ON; break;
            case GOD_OKAWARU:
                ch = TILEG_INVOCATIONS_O_ON; break;
            case GOD_QAZLAL:
                ch = TILEG_INVOCATIONS_Q_ON; break;
            case GOD_SIF_MUNA:
                ch = TILEG_INVOCATIONS_S_ON; break;
            case GOD_USKAYAW:
                ch = TILEG_INVOCATIONS_U_ON; break;
            case GOD_YREDELEMNUL:
                ch = TILEG_INVOCATIONS_Y_ON; break;
            case GOD_ZIN:
                ch = TILEG_INVOCATIONS_Z_ON; break;
            default:
                ch = TILEG_INVOCATIONS_ON;
            }
        }
        break;
    default:                return TILEG_TODO;
    }

    switch (train)
    {
    case TRAINING_DISABLED:
        return ch + TILEG_FIGHTING_OFF - TILEG_FIGHTING_ON;
    case TRAINING_INACTIVE:
        return ch + TILEG_FIGHTING_INACTIVE - TILEG_FIGHTING_ON;
    case TRAINING_ENABLED:
        return ch;
    case TRAINING_FOCUSED:
        return ch + TILEG_FIGHTING_FOCUS - TILEG_FIGHTING_ON;
    case TRAINING_MASTERED:
        return ch + TILEG_FIGHTING_MAX - TILEG_FIGHTING_ON;
    default:
        die("invalid skill tile type");
    }

}

tileidx_t tileidx_command(const command_type cmd)
{
    switch (cmd)
    {
    case CMD_REST:
        return TILEG_CMD_REST;
    case CMD_EXPLORE:
        return TILEG_CMD_EXPLORE;
    case CMD_INTERLEVEL_TRAVEL:
        return TILEG_CMD_INTERLEVEL_TRAVEL;
#ifdef CLUA_BINDINGS
    // might not be defined if building without LUA
    case CMD_AUTOFIGHT:
        return TILEG_CMD_AUTOFIGHT;
#endif
    case CMD_WAIT:
        return TILEG_CMD_WAIT;
    case CMD_USE_ABILITY:
        return TILEG_CMD_USE_ABILITY;
    case CMD_PRAY:
        return TILEG_CMD_PRAY;
    case CMD_SEARCH_STASHES:
        return TILEG_CMD_SEARCH_STASHES;
    case CMD_REPLAY_MESSAGES:
        return TILEG_CMD_REPLAY_MESSAGES;
    case CMD_RESISTS_SCREEN:
        return TILEG_CMD_RESISTS_SCREEN;
    case CMD_DISPLAY_OVERMAP:
        return TILEG_CMD_DISPLAY_OVERMAP;
    case CMD_DISPLAY_RELIGION:
        return TILEG_CMD_DISPLAY_RELIGION;
    case CMD_DISPLAY_MUTATIONS:
        return TILEG_CMD_DISPLAY_MUTATIONS;
    case CMD_DISPLAY_SKILLS:
        return TILEG_CMD_DISPLAY_SKILLS;
    case CMD_DISPLAY_CHARACTER_STATUS:
        return TILEG_CMD_DISPLAY_CHARACTER_STATUS;
    case CMD_DISPLAY_KNOWN_OBJECTS:
        return TILEG_CMD_KNOWN_ITEMS;
    case CMD_SAVE_GAME_NOW:
        return TILEG_CMD_SAVE_GAME_NOW;
    case CMD_EDIT_PLAYER_TILE:
        return TILEG_CMD_EDIT_PLAYER_TILE;
    case CMD_DISPLAY_COMMANDS:
        return TILEG_CMD_DISPLAY_COMMANDS;
    case CMD_LOOKUP_HELP:
        return TILEG_CMD_LOOKUP_HELP;
    case CMD_CHARACTER_DUMP:
        return TILEG_CMD_CHARACTER_DUMP;
    case CMD_DISPLAY_INVENTORY:
        return TILEG_CMD_DISPLAY_INVENTORY;
    case CMD_CAST_SPELL:
        return TILEG_CMD_CAST_SPELL;
    case CMD_BUTCHER:
        return TILEG_CMD_BUTCHER;
    case CMD_MEMORISE_SPELL:
        return TILEG_CMD_MEMORISE_SPELL;
    case CMD_DROP:
        return TILEG_CMD_DROP;
    case CMD_DISPLAY_MAP:
        return TILEG_CMD_DISPLAY_MAP;
    case CMD_MAP_GOTO_TARGET:
        return TILEG_CMD_MAP_GOTO_TARGET;
    case CMD_MAP_NEXT_LEVEL:
        return TILEG_CMD_MAP_NEXT_LEVEL;
    case CMD_MAP_PREV_LEVEL:
        return TILEG_CMD_MAP_PREV_LEVEL;
    case CMD_MAP_GOTO_LEVEL:
        return TILEG_CMD_MAP_GOTO_LEVEL;
    case CMD_MAP_EXCLUDE_AREA:
        return TILEG_CMD_MAP_EXCLUDE_AREA;
    case CMD_MAP_FIND_EXCLUDED:
        return TILEG_CMD_MAP_FIND_EXCLUDED;
    case CMD_MAP_CLEAR_EXCLUDES:
        return TILEG_CMD_MAP_CLEAR_EXCLUDES;
    case CMD_MAP_ADD_WAYPOINT:
        return TILEG_CMD_MAP_ADD_WAYPOINT;
    case CMD_MAP_FIND_WAYPOINT:
        return TILEG_CMD_MAP_FIND_WAYPOINT;
    case CMD_MAP_FIND_UPSTAIR:
        return TILEG_CMD_MAP_FIND_UPSTAIR;
    case CMD_MAP_FIND_DOWNSTAIR:
        return TILEG_CMD_MAP_FIND_DOWNSTAIR;
    case CMD_MAP_FIND_YOU:
        return TILEG_CMD_MAP_FIND_YOU;
    case CMD_MAP_FIND_PORTAL:
        return TILEG_CMD_MAP_FIND_PORTAL;
    case CMD_MAP_FIND_TRAP:
        return TILEG_CMD_MAP_FIND_TRAP;
    case CMD_MAP_FIND_ALTAR:
        return TILEG_CMD_MAP_FIND_ALTAR;
    case CMD_MAP_FIND_STASH:
        return TILEG_CMD_MAP_FIND_STASH;
#ifdef TOUCH_UI
    case CMD_SHOW_KEYBOARD:
        return TILEG_CMD_KEYBOARD;
#endif
    default:
        return TILEG_TODO;
    }
}

tileidx_t tileidx_gametype(const game_type gtype)
{
    switch (gtype)
    {
    case GAME_TYPE_NORMAL:
        return TILEG_STARTUP_STONESOUP;
    case GAME_TYPE_TUTORIAL:
        return TILEG_STARTUP_TUTORIAL;
    case GAME_TYPE_HINTS:
        return TILEG_STARTUP_HINTS;
    case GAME_TYPE_SPRINT:
        return TILEG_STARTUP_SPRINT;
    case GAME_TYPE_INSTRUCTIONS:
        return TILEG_STARTUP_INSTRUCTIONS;
    case GAME_TYPE_ARENA:
        return TILEG_STARTUP_ARENA;
    case GAME_TYPE_HIGH_SCORES:
        return TILEG_STARTUP_HIGH_SCORES;
    default:
        return TILEG_ERROR;
    }
}

tileidx_t tileidx_ability(const ability_type ability)
{
    switch (ability)
    {
    // Innate abilities and (Demonspaw) mutations.
    case ABIL_SPIT_POISON:
        return TILEG_ABILITY_SPIT_POISON;
    case ABIL_BREATHE_FIRE:
        return TILEG_ABILITY_BREATHE_FIRE;
    case ABIL_BREATHE_FROST:
        return TILEG_ABILITY_BREATHE_FROST;
    case ABIL_BREATHE_POISON:
        return TILEG_ABILITY_BREATHE_POISON;
    case ABIL_BREATHE_LIGHTNING:
        return TILEG_ABILITY_BREATHE_LIGHTNING;
    case ABIL_BREATHE_POWER:
        return TILEG_ABILITY_BREATHE_ENERGY;
    case ABIL_BREATHE_STICKY_FLAME:
        return TILEG_ABILITY_BREATHE_STICKY_FLAME;
    case ABIL_BREATHE_STEAM:
        return TILEG_ABILITY_BREATHE_STEAM;
    case ABIL_BREATHE_MEPHITIC:
        return TILEG_ABILITY_BREATHE_MEPHITIC;
    case ABIL_SPIT_ACID:
        return TILEG_ABILITY_SPIT_ACID;
    case ABIL_BLINK:
        return TILEG_ABILITY_BLINK;

    // Others
    case ABIL_DELAYED_FIREBALL:
        return TILEG_ABILITY_DELAYED_FIREBALL;
    case ABIL_END_TRANSFORMATION:
        return TILEG_ABILITY_END_TRANSFORMATION;
    case ABIL_STOP_RECALL:
        return TILEG_ABILITY_STOP_RECALL;
    case ABIL_STOP_SINGING:
        return TILEG_ABILITY_STOP_SINGING;

    // Species-specific abilities.
    // Demonspawn-only
    case ABIL_DAMNATION:
        return TILEG_ABILITY_HURL_DAMNATION;
    // Tengu, Draconians
    case ABIL_FLY:
        return TILEG_ABILITY_FLIGHT;
    case ABIL_STOP_FLYING:
        return TILEG_ABILITY_FLIGHT_END;
    // Vampires
    case ABIL_TRAN_BAT:
        return TILEG_ABILITY_BAT_FORM;
    // Deep Dwarves
    case ABIL_RECHARGING:
        return TILEG_ABILITY_RECHARGE;
    // Formicids
    case ABIL_DIG:
        return TILEG_ABILITY_DIG;
    case ABIL_SHAFT_SELF:
        return TILEG_ABILITY_SHAFT_SELF;

    // Evoking items.
    case ABIL_EVOKE_BERSERK:
        return TILEG_ABILITY_EVOKE_BERSERK;
    case ABIL_EVOKE_BLINK:
        return TILEG_ABILITY_BLINK;
    case ABIL_EVOKE_TURN_INVISIBLE:
        return TILEG_ABILITY_EVOKE_INVISIBILITY;
    case ABIL_EVOKE_TURN_VISIBLE:
        return TILEG_ABILITY_EVOKE_INVISIBILITY_END;
    case ABIL_EVOKE_FLIGHT:
        return TILEG_ABILITY_EVOKE_FLIGHT;
    case ABIL_EVOKE_FOG:
        return TILEG_ABILITY_EVOKE_FOG;

    // Divine abilities
    // Zin
    case ABIL_ZIN_SUSTENANCE:
        return TILEG_TODO;
    case ABIL_ZIN_RECITE:
        return TILEG_ABILITY_ZIN_RECITE;
    case ABIL_ZIN_VITALISATION:
        return TILEG_ABILITY_ZIN_VITALISATION;
    case ABIL_ZIN_IMPRISON:
        return TILEG_ABILITY_ZIN_IMPRISON;
    case ABIL_ZIN_SANCTUARY:
        return TILEG_ABILITY_ZIN_SANCTUARY;
    case ABIL_ZIN_CURE_ALL_MUTATIONS:
        return TILEG_ABILITY_ZIN_CURE_MUTATIONS;
    case ABIL_ZIN_DONATE_GOLD:
        return TILEG_ABILITY_ZIN_DONATE_GOLD;
    // TSO
    case ABIL_TSO_DIVINE_SHIELD:
        return TILEG_ABILITY_TSO_DIVINE_SHIELD;
    case ABIL_TSO_CLEANSING_FLAME:
        return TILEG_ABILITY_TSO_CLEANSING_FLAME;
    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
        return TILEG_ABILITY_TSO_DIVINE_WARRIOR;
    case ABIL_TSO_BLESS_WEAPON:
        return TILEG_ABILITY_TSO_BLESS_WEAPON;
    // Kiku
    case ABIL_KIKU_RECEIVE_CORPSES:
        return TILEG_ABILITY_KIKU_RECEIVE_CORPSES;
    case ABIL_KIKU_TORMENT:
        return TILEG_ABILITY_KIKU_TORMENT;
    case ABIL_KIKU_BLESS_WEAPON:
        return TILEG_ABILITY_KIKU_BLESS_WEAPON;
    case ABIL_KIKU_GIFT_NECRONOMICON:
        return TILEG_ABILITY_KIKU_NECRONOMICON;
    // Yredelemnul
    case ABIL_YRED_INJURY_MIRROR:
        return TILEG_ABILITY_YRED_INJURY_MIRROR;
    case ABIL_YRED_ANIMATE_REMAINS:
        return TILEG_ABILITY_YRED_ANIMATE_REMAINS;
    case ABIL_YRED_RECALL_UNDEAD_SLAVES:
        return TILEG_ABILITY_YRED_RECALL;
    case ABIL_YRED_ANIMATE_DEAD:
        return TILEG_ABILITY_YRED_ANIMATE_DEAD;
    case ABIL_YRED_DRAIN_LIFE:
        return TILEG_ABILITY_YRED_DRAIN_LIFE;
    case ABIL_YRED_ENSLAVE_SOUL:
        return TILEG_ABILITY_YRED_ENSLAVE_SOUL;
    // Xom, Vehumet = 90
    // Okawaru
    case ABIL_OKAWARU_HEROISM:
        return TILEG_ABILITY_OKAWARU_HEROISM;
    case ABIL_OKAWARU_FINESSE:
        return TILEG_ABILITY_OKAWARU_FINESSE;
    // Makhleb
    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
        return TILEG_ABILITY_MAKHLEB_MINOR_DESTRUCTION;
    case ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB:
        return TILEG_ABILITY_MAKHLEB_LESSER_SERVANT;
    case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
        return TILEG_ABILITY_MAKHLEB_MAJOR_DESTRUCTION;
    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
        return TILEG_ABILITY_MAKHLEB_GREATER_SERVANT;
    // Sif Muna
    case ABIL_SIF_MUNA_DIVINE_ENERGY:
        return TILEG_ABILITY_SIF_MUNA_DIVINE_ENERGY;
    case ABIL_SIF_MUNA_STOP_DIVINE_ENERGY:
        return TILEG_ABILITY_SIF_MUNA_STOP_DIVINE_ENERGY;
    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
        return TILEG_ABILITY_SIF_MUNA_CHANNEL;
    case ABIL_SIF_MUNA_FORGET_SPELL:
        return TILEG_ABILITY_SIF_MUNA_AMNESIA;
    // Trog
    case ABIL_TROG_BURN_SPELLBOOKS:
        return TILEG_ABILITY_TROG_BURN_SPELLBOOKS;
    case ABIL_TROG_BERSERK:
        return TILEG_ABILITY_TROG_BERSERK;
    case ABIL_TROG_REGEN_MR:
        return TILEG_ABILITY_TROG_HAND;
    case ABIL_TROG_BROTHERS_IN_ARMS:
        return TILEG_ABILITY_TROG_BROTHERS_IN_ARMS;
    // Elyvilon
    case ABIL_ELYVILON_LIFESAVING:
        return TILEG_ABILITY_ELYVILON_DIVINE_PROTECTION;
    case ABIL_ELYVILON_LESSER_HEALING:
        return TILEG_ABILITY_ELYVILON_LESSER_HEALING;
    case ABIL_ELYVILON_PURIFICATION:
        return TILEG_ABILITY_ELYVILON_PURIFICATION;
    case ABIL_ELYVILON_GREATER_HEALING:
        return TILEG_ABILITY_ELYVILON_GREATER_HEALING;
    case ABIL_ELYVILON_HEAL_OTHER:
        return TILEG_ABILITY_ELYVILON_HEAL_OTHER;
    case ABIL_ELYVILON_DIVINE_VIGOUR:
        return TILEG_ABILITY_ELYVILON_DIVINE_VIGOUR;
    // Lugonu
    case ABIL_LUGONU_ABYSS_EXIT:
        return TILEG_ABILITY_LUGONU_EXIT_ABYSS;
    case ABIL_LUGONU_BEND_SPACE:
        return TILEG_ABILITY_LUGONU_BEND_SPACE;
    case ABIL_LUGONU_BANISH:
        return TILEG_ABILITY_LUGONU_BANISH;
    case ABIL_LUGONU_CORRUPT:
        return TILEG_ABILITY_LUGONU_CORRUPT;
    case ABIL_LUGONU_ABYSS_ENTER:
        return TILEG_ABILITY_LUGONU_ENTER_ABYSS;
    case ABIL_LUGONU_BLESS_WEAPON:
        return TILEG_ABILITY_LUGONU_BLESS_WEAPON;
    // Nemelex
    case ABIL_NEMELEX_TRIPLE_DRAW:
        return TILEG_ABILITY_NEMELEX_TRIPLE_DRAW;
    case ABIL_NEMELEX_DEAL_FOUR:
        return TILEG_ABILITY_NEMELEX_DEAL_FOUR;
    case ABIL_NEMELEX_STACK_FIVE:
        return TILEG_ABILITY_NEMELEX_STACK_FIVE;
    // Beogh
    case ABIL_BEOGH_GIFT_ITEM:
        return TILEG_ABILITY_BEOGH_GIFT_ITEM;
    case ABIL_BEOGH_SMITING:
        return TILEG_ABILITY_BEOGH_SMITE;
    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
        return TILEG_ABILITY_BEOGH_RECALL;
    case ABIL_CONVERT_TO_BEOGH:
        return TILEG_ABILITY_CONVERT_TO_BEOGH;
    case ABIL_BEOGH_RESURRECTION:
        return TILEG_ABILITY_BEOGH_RESURRECTION;
    // Jiyva
    case ABIL_JIYVA_CALL_JELLY:
        return TILEG_ABILITY_JIYVA_REQUEST_JELLY;
    case ABIL_JIYVA_JELLY_PARALYSE:
        return TILEG_ABILITY_JIYVA_PARALYSE_JELLY;
    case ABIL_JIYVA_SLIMIFY:
        return TILEG_ABILITY_JIYVA_SLIMIFY;
    case ABIL_JIYVA_CURE_BAD_MUTATION:
        return TILEG_ABILITY_JIYVA_CURE_BAD_MUTATIONS;
    // Fedhas
    case ABIL_FEDHAS_SUNLIGHT:
        return TILEG_ABILITY_FEDHAS_SUNLIGHT;
    case ABIL_FEDHAS_RAIN:
        return TILEG_ABILITY_FEDHAS_RAIN;
    case ABIL_FEDHAS_PLANT_RING:
        return TILEG_ABILITY_FEDHAS_PLANT_RING;
    case ABIL_FEDHAS_SPAWN_SPORES:
        return TILEG_ABILITY_FEDHAS_SPAWN_SPORES;
    case ABIL_FEDHAS_EVOLUTION:
        return TILEG_ABILITY_FEDHAS_EVOLUTION;
    // Cheibriados
    case ABIL_CHEIBRIADOS_TIME_STEP:
        return TILEG_ABILITY_CHEIBRIADOS_TIME_STEP;
    case ABIL_CHEIBRIADOS_TIME_BEND:
        return TILEG_ABILITY_CHEIBRIADOS_BEND_TIME;
    case ABIL_CHEIBRIADOS_SLOUCH:
        return TILEG_ABILITY_CHEIBRIADOS_SLOUCH;
    case ABIL_CHEIBRIADOS_DISTORTION:
        return TILEG_ABILITY_CHEIBRIADOS_TEMPORAL_DISTORTION;
    // Ashenzari
    case ABIL_ASHENZARI_CURSE:
        return TILEG_ABILITY_ASHENZARI_CURSE;
    case ABIL_ASHENZARI_SCRYING:
        return TILEG_ABILITY_ASHENZARI_SCRY;
    case ABIL_ASHENZARI_TRANSFER_KNOWLEDGE:
        return TILEG_ABILITY_ASHENZARI_TRANSFER_KNOWLEDGE;
    case ABIL_ASHENZARI_END_TRANSFER:
        return TILEG_ABILITY_ASHENZARI_TRANSFER_KNOWLEDGE_END;
    // Dithmenos
    case ABIL_DITHMENOS_SHADOW_STEP:
        return TILEG_ABILITY_DITHMENOS_SHADOW_STEP;
    case ABIL_DITHMENOS_SHADOW_FORM:
        return TILEG_ABILITY_DITHMENOS_SHADOW_FORM;
    // Gozag
    case ABIL_GOZAG_POTION_PETITION:
        return TILEG_ABILITY_GOZAG_POTION_PETITION;
    case ABIL_GOZAG_CALL_MERCHANT:
        return TILEG_ABILITY_GOZAG_CALL_MERCHANT;
    case ABIL_GOZAG_BRIBE_BRANCH:
        return TILEG_ABILITY_GOZAG_BRIBE_BRANCH;
    // Qazlal
    case ABIL_QAZLAL_UPHEAVAL:
        return TILEG_ABILITY_QAZLAL_UPHEAVAL;
    case ABIL_QAZLAL_ELEMENTAL_FORCE:
        return TILEG_ABILITY_QAZLAL_ELEMENTAL_FORCE;
    case ABIL_QAZLAL_DISASTER_AREA:
        return TILEG_ABILITY_QAZLAL_DISASTER_AREA;
    // Ru
    case ABIL_RU_DRAW_OUT_POWER:
        return TILEG_ABILITY_RU_DRAW_OUT_POWER;
    case ABIL_RU_POWER_LEAP:
        return TILEG_ABILITY_RU_POWER_LEAP;
    case ABIL_RU_APOCALYPSE:
        return TILEG_ABILITY_RU_APOCALYPSE;

    case ABIL_RU_SACRIFICE_PURITY:
        return TILEG_ABILITY_RU_SACRIFICE_PURITY;
    case ABIL_RU_SACRIFICE_WORDS:
        return TILEG_ABILITY_RU_SACRIFICE_WORDS;
    case ABIL_RU_SACRIFICE_DRINK:
        return TILEG_ABILITY_RU_SACRIFICE_DRINK;
    case ABIL_RU_SACRIFICE_ESSENCE:
        return TILEG_ABILITY_RU_SACRIFICE_ESSENCE;
    case ABIL_RU_SACRIFICE_HEALTH:
        return TILEG_ABILITY_RU_SACRIFICE_HEALTH;
    case ABIL_RU_SACRIFICE_STEALTH:
        return TILEG_ABILITY_RU_SACRIFICE_STEALTH;
    case ABIL_RU_SACRIFICE_ARTIFICE:
        return TILEG_ABILITY_RU_SACRIFICE_ARTIFICE;
    case ABIL_RU_SACRIFICE_LOVE:
        return TILEG_ABILITY_RU_SACRIFICE_LOVE;
    case ABIL_RU_SACRIFICE_COURAGE:
        return TILEG_ABILITY_RU_SACRIFICE_COURAGE;
    case ABIL_RU_SACRIFICE_ARCANA:
        return TILEG_ABILITY_RU_SACRIFICE_ARCANA;
    case ABIL_RU_SACRIFICE_NIMBLENESS:
        return TILEG_ABILITY_RU_SACRIFICE_NIMBLENESS;
    case ABIL_RU_SACRIFICE_DURABILITY:
        return TILEG_ABILITY_RU_SACRIFICE_DURABILITY;
    case ABIL_RU_SACRIFICE_HAND:
        return TILEG_ABILITY_RU_SACRIFICE_HAND;
    case ABIL_RU_SACRIFICE_EXPERIENCE:
        return TILEG_ABILITY_RU_SACRIFICE_EXPERIENCE;
    case ABIL_RU_SACRIFICE_SKILL:
        return TILEG_ABILITY_RU_SACRIFICE_SKILL;
    case ABIL_RU_SACRIFICE_EYE:
        return TILEG_ABILITY_RU_SACRIFICE_EYE;
    case ABIL_RU_SACRIFICE_RESISTANCE:
        return TILEG_ABILITY_RU_SACRIFICE_RESISTANCE;
    case ABIL_RU_REJECT_SACRIFICES:
        return TILEG_ABILITY_RU_REJECT_SACRIFICES;
    // Pakellas
    case ABIL_PAKELLAS_DEVICE_SURGE:
        return TILEG_ABILITY_PAKELLAS_DEVICE_SURGE;
    case ABIL_PAKELLAS_QUICK_CHARGE:
        return TILEG_ABILITY_PAKELLAS_QUICK_CHARGE;
    case ABIL_PAKELLAS_SUPERCHARGE:
        return TILEG_ABILITY_PAKELLAS_SUPERCHARGE;
    // Hepliaklqana
    case ABIL_HEPLIAKLQANA_RECALL:
        return TILEG_ABILITY_HEP_RECALL;
    case ABIL_HEPLIAKLQANA_IDEALISE:
        return TILEG_ABILITY_HEP_IDEALISE;
    case ABIL_HEPLIAKLQANA_TRANSFERENCE:
        return TILEG_ABILITY_HEP_TRANSFERENCE;
    case ABIL_HEPLIAKLQANA_IDENTITY:
        return TILEG_ABILITY_HEP_IDENTITY;
    case ABIL_HEPLIAKLQANA_TYPE_KNIGHT:
        return TILEG_ABILITY_HEP_KNIGHT;
    case ABIL_HEPLIAKLQANA_TYPE_BATTLEMAGE:
        return TILEG_ABILITY_HEP_BATTLEMAGE;
    case ABIL_HEPLIAKLQANA_TYPE_HEXER:
        return TILEG_ABILITY_HEP_HEXER;
    // usk
   case ABIL_USKAYAW_STOMP:
        return TILEG_ABILITY_USKAYAW_STOMP;
   case ABIL_USKAYAW_LINE_PASS:
        return TILEG_ABILITY_USKAYAW_LINE_PASS;
   case ABIL_USKAYAW_GRAND_FINALE:
        return TILEG_ABILITY_USKAYAW_GRAND_FINALE;

    // General divine (pseudo) abilities.
    case ABIL_RENOUNCE_RELIGION:
        return TILEG_ABILITY_RENOUNCE_RELIGION;

    default:
        return TILEG_ERROR;
    }
}

tileidx_t tileidx_known_brand(const item_def &item)
{
    if (!item_type_known(item))
        return 0;

    if (item.base_type == OBJ_WEAPONS)
    {
        const int brand = get_weapon_brand(item);
        if (brand != SPWPN_NORMAL)
            return TILE_BRAND_WEP_FIRST + get_weapon_brand(item) - 1;
    }
    else if (item.base_type == OBJ_ARMOUR)
    {
        const int brand = get_armour_ego_type(item);
        if (brand != SPARM_NORMAL)
            return TILE_BRAND_ARM_FIRST + get_armour_ego_type(item) - 1;
    }
    else if (item.base_type == OBJ_MISSILES)
    {
        switch (get_ammo_brand(item))
        {
#if TAG_MAJOR_VERSION == 34
        case SPMSL_FLAME:
            return TILE_BRAND_FLAME;
        case SPMSL_FROST:
            return TILE_BRAND_FROST;
#endif
        case SPMSL_POISONED:
            return TILE_BRAND_POISONED;
        case SPMSL_CURARE:
            return TILE_BRAND_CURARE;
        case SPMSL_RETURNING:
            return TILE_BRAND_RETURNING;
        case SPMSL_CHAOS:
            return TILE_BRAND_CHAOS;
        case SPMSL_PENETRATION:
            return TILE_BRAND_PENETRATION;
        case SPMSL_DISPERSAL:
            return TILE_BRAND_DISPERSAL;
        case SPMSL_EXPLODING:
            return TILE_BRAND_EXPLOSION;
        case SPMSL_CONFUSION:
            return TILE_BRAND_CONFUSION;
        case SPMSL_PARALYSIS:
            return TILE_BRAND_PARALYSIS;
#if TAG_MAJOR_VERSION == 34
        case SPMSL_SLOW:
            return TILE_BRAND_SLOWING;
        case SPMSL_SICKNESS:
            return TILE_BRAND_SICKNESS;
#endif
        case SPMSL_FRENZY:
            return TILE_BRAND_FRENZY;
        case SPMSL_SLEEP:
            return TILE_BRAND_SLEEP;
        default:
            break;
        }
    }
    else if (item.base_type == OBJ_RODS)
    {
        // Technically not a brand, but still handled here
        return TILE_ROD_ID_FIRST + item.sub_type;
    }
    return 0;
}

tileidx_t tileidx_corpse_brand(const item_def &item)
{
    if (item.base_type != OBJ_CORPSES || item.sub_type != CORPSE_BODY)
        return 0;

    // Vampires are only interested in fresh blood.
    if (you.species == SP_VAMPIRE && !mons_has_blood(item.mon_type))
        return TILE_FOOD_INEDIBLE;

    // Harmful chunk effects > religious rules > reduced nutrition.
    if (is_mutagenic(item))
        return TILE_FOOD_MUTAGENIC;

    if (is_noxious(item))
        return TILE_FOOD_ROTTING;

    if (is_forbidden_food(item))
        return TILE_FOOD_FORBIDDEN;

    return 0;
}

tileidx_t tileidx_unseen_flag(const coord_def &gc)
{
    if (!map_bounds(gc))
        return TILE_FLAG_UNSEEN;
    else if (env.map_knowledge(gc).known()
                && !env.map_knowledge(gc).seen()
             || env.map_knowledge(gc).detected_item()
             || env.map_knowledge(gc).detected_monster()
           )
    {
        return TILE_FLAG_MM_UNSEEN;
    }
    else
        return TILE_FLAG_UNSEEN;
}

int enchant_to_int(const item_def &item)
{
    if (is_random_artefact(item))
        return 4;

    switch (item.flags & ISFLAG_COSMETIC_MASK)
    {
        default:
            return 0;
        case ISFLAG_EMBROIDERED_SHINY:
            return 1;
        case ISFLAG_RUNED:
            return 2;
        case ISFLAG_GLOWING:
            return 3;
    }
}

tileidx_t tileidx_enchant_equ(const item_def &item, tileidx_t tile, bool player)
{
    static const int etable[5][5] =
    {
      {0, 0, 0, 0, 0},  // all variants look the same
      {0, 1, 1, 1, 1},  // normal, ego/randart
      {0, 1, 1, 1, 2},  // normal, ego, randart
      {0, 1, 1, 2, 3},  // normal, ego (shiny/runed), ego (glowing), randart
      {0, 1, 2, 3, 4}   // normal, shiny, runed, glowing, randart
    };

    const int etype = enchant_to_int(item);

    // XXX: only helmets and robes have variants, but it would be nice
    // if this weren't hardcoded.
    if (tile == TILE_THELM_HELM)
    {
        switch (etype)
        {
            case 1:
            case 2:
            case 3:
                tile = _modrng(item.rnd, TILE_THELM_EGO_FIRST, TILE_THELM_EGO_LAST);
                break;
            case 4:
                tile = _modrng(item.rnd, TILE_THELM_ART_FIRST, TILE_THELM_ART_LAST);
                break;
            default:
                tile = _modrng(item.rnd, TILE_THELM_FIRST, TILE_THELM_LAST);
        }
        return tile;
    }

    if (tile == TILE_ARM_ROBE)
    {
        switch (etype)
        {
            case 1:
            case 2:
            case 3:
                tile = _modrng(item.rnd, TILE_ARM_ROBE_EGO_FIRST, TILE_ARM_ROBE_EGO_LAST);
                break;
            case 4:
                tile = _modrng(item.rnd, TILE_ARM_ROBE_ART_FIRST, TILE_ARM_ROBE_ART_LAST);
                break;
            default:
                tile = _modrng(item.rnd, TILE_ARM_ROBE_FIRST, TILE_ARM_ROBE_LAST);
        }
        return tile;
    }

    int idx;
    if (player)
        idx = tile_player_count(tile) - 1;
    else
        idx = tile_main_count(tile) - 1;
    ASSERT(idx < 5);

    tile += etable[idx][etype];

    return tile;
}

string tile_debug_string(tileidx_t fg, tileidx_t bg, tileidx_t cloud, char prefix)
{
    tileidx_t fg_idx = fg & TILE_FLAG_MASK;
    tileidx_t bg_idx = bg & TILE_FLAG_MASK;

    string fg_name;
    if (fg_idx < TILE_MAIN_MAX)
        fg_name = tile_main_name(fg_idx);
    else if (fg_idx < TILEP_MCACHE_START)
        fg_name = (tile_player_name(fg_idx));
    else
    {
        fg_name = "mc:";
        mcache_entry *entry = mcache.get(fg_idx);
        if (entry)
        {
            tile_draw_info dinfo[mcache_entry::MAX_INFO_COUNT];
            unsigned int count = entry->info(&dinfo[0]);
            for (unsigned int i = 0; i < count; ++i)
            {
                tileidx_t mc_idx = dinfo[i].idx;
                if (mc_idx < TILE_MAIN_MAX)
                    fg_name += tile_main_name(mc_idx);
                else if (mc_idx < TILEP_PLAYER_MAX)
                    fg_name += tile_player_name(mc_idx);
                else
                    fg_name += "[invalid index]";

                if (i < count - 1)
                    fg_name += ", ";
            }
        }
        else
            fg_name += "[not found]";
    }

    string tile_string = make_stringf(
        "%cFG: %4" PRIu64" | 0x%8" PRIx64" (%s)\n"
        "%cBG: %4" PRIu64" | 0x%8" PRIx64" (%s)\n",
        prefix,
        fg_idx,
        fg & ~TILE_FLAG_MASK,
        fg_name.c_str(),
        prefix,
        bg_idx,
        bg & ~TILE_FLAG_MASK,
        tile_dngn_name(bg_idx));

    return tile_string;
}

void bind_item_tile(item_def &item)
{
    if (item.props.exists("item_tile_name"))
    {
        string tile = item.props["item_tile_name"].get_string();
        dprf("Binding non-worn item tile: \"%s\".", tile.c_str());
        tileidx_t index;
        if (!tile_main_index(tile.c_str(), &index))
        {
            // If invalid tile name, complain and discard the props.
            dprf("bad tile name: \"%s\".", tile.c_str());
            item.props.erase("item_tile_name");
            item.props.erase("item_tile");
        }
        else
            item.props["item_tile"] = short(index);
    }

    if (item.props.exists("worn_tile_name"))
    {
        string tile = item.props["worn_tile_name"].get_string();
        dprf("Binding worn item tile: \"%s\".", tile.c_str());
        tileidx_t index;
        if (!tile_player_index(tile.c_str(), &index))
        {
            // If invalid tile name, complain and discard the props.
            dprf("bad tile name: \"%s\".", tile.c_str());
            item.props.erase("worn_tile_name");
            item.props.erase("worn_tile");
        }
        else
            item.props["worn_tile"] = short(index);
    }
}
#endif

void tile_init_props(monster* mon)
{
    // Only monsters using mon_mod or mon_cycle need a tile_num.
    switch (mon->type)
    {
        case MONS_SLAVE:
        case MONS_TOADSTOOL:
        case MONS_FUNGUS:
        case MONS_PLANT:
        case MONS_BUSH:
        case MONS_FIRE_VORTEX:
        case MONS_TWISTER:
        case MONS_SPATIAL_VORTEX:
        case MONS_SPATIAL_MAELSTROM:
        case MONS_ABOMINATION_SMALL:
        case MONS_ABOMINATION_LARGE:
        case MONS_BLOCK_OF_ICE:
        case MONS_BUTTERFLY:
        case MONS_HUMAN:
            break;
        default:
            return;
    }

    // Already overridden or set.
    if (mon->props.exists("monster_tile") || mon->props.exists(TILE_NUM_KEY))
        return;

    mon->props[TILE_NUM_KEY] = short(random2(256));
}
