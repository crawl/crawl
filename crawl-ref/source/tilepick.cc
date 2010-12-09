/*
 *  File:       tilepick.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE
#include "tilepick.h"

#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "decks.h"
#include "describe.h"
#include "env.h"
#include "food.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "mon-iter.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "shopping.h"
#include "showsymb.h"
#include "state.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tiledef-gui.h"
#include "tiledef-main.h"
#include "tiledef-player.h"
#include "tiledef-unrand.h"
#include "tilemcache.h"
#include "traps.h"

// This should not be changed.
COMPILE_CHECK(TILE_DNGN_UNSEEN == 0, c0);

// NOTE: If one of the following asserts fail, it's because the corresponding
// enum in itemprop-enum.h was modified, but rltiles/dc-item.txt was not
// modified in parallel.

// These brands start with "normal" which there's no tile for, so subtract 1.
COMPILE_CHECK(NUM_REAL_SPECIAL_WEAPONS - 1
              == TILE_BRAND_WEP_LAST - TILE_BRAND_WEP_FIRST + 1, c1);
COMPILE_CHECK(NUM_SPECIAL_ARMOURS - 1
              == TILE_BRAND_ARM_LAST - TILE_BRAND_ARM_FIRST + 1, c2);

COMPILE_CHECK(NUM_RINGS == TILE_RING_ID_LAST - TILE_RING_ID_FIRST + 1, c4);
COMPILE_CHECK(NUM_JEWELLERY - AMU_FIRST_AMULET
              == TILE_AMU_ID_LAST - TILE_AMU_ID_FIRST + 1, c5);
COMPILE_CHECK(NUM_SCROLLS == TILE_SCR_ID_LAST - TILE_SCR_ID_FIRST + 1, c6);
// Note: STAFF_FIRST_ROD == total number of staves that aren't rods
COMPILE_CHECK(STAFF_FIRST_ROD
              == TILE_STAFF_ID_LAST - TILE_STAFF_ID_FIRST + 1, c7);
COMPILE_CHECK(NUM_STAVES - STAFF_FIRST_ROD
              == TILE_ROD_ID_LAST - TILE_ROD_ID_FIRST + 1, c8);
COMPILE_CHECK(NUM_WANDS == TILE_WAND_ID_LAST - TILE_WAND_ID_FIRST + 1, c9);
COMPILE_CHECK(NUM_POTIONS == TILE_POT_ID_LAST - TILE_POT_ID_FIRST + 1, c10);

// Some tile sets have a corresponding tile for every colour (excepting black).
// If this assert breaks, then it means that either there are tiles that are
// being unused in the tileset or an incorrect type of tile will get used.
COMPILE_CHECK(MAX_TERM_COLOUR - 1
              == TILE_RING_COL_LAST - TILE_RING_COL_FIRST + 1, c11);
COMPILE_CHECK(MAX_TERM_COLOUR - 1
              == TILE_AMU_COL_LAST - TILE_AMU_COL_FIRST + 1, c12);
COMPILE_CHECK(MAX_TERM_COLOUR - 1
              == TILE_BOOK_COL_LAST - TILE_BOOK_COL_FIRST + 1, c12);

static tileidx_t _tileidx_monster_base(int type,
                                       bool in_water = false,
                                       int colour = 0,
                                       int number = 0,
                                       int tile_num_prop = 0);

static tileidx_t _tileidx_trap(trap_type type)
{
    switch (type)
    {
    case TRAP_DART:
        return TILE_DNGN_TRAP_DART;
    case TRAP_ARROW:
        return TILE_DNGN_TRAP_ARROW;
    case TRAP_SPEAR:
        return TILE_DNGN_TRAP_SPEAR;
    case TRAP_AXE:
        return TILE_DNGN_TRAP_AXE;
    case TRAP_TELEPORT:
        return TILE_DNGN_TRAP_TELEPORT;
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
    default:
        return TILE_DNGN_ERROR;
    }
}

static tileidx_t _tileidx_shop(coord_def where)
{
    const shop_struct *shop = get_shop(where);
    shop_type stype;

    if (feature_mimic_at(where))
    {
        monster *mimic = monster_at(where);
        if (mimic->props.exists("shop_type"))
            stype = static_cast<shop_type>(mimic->props["shop_type"].get_short());
        else
            return TILE_DNGN_ERROR;
    }
    else if (shop)
       stype = shop->type;
    else
        return TILE_DNGN_ERROR;

    switch (stype)
    {
        case SHOP_WEAPON:
        case SHOP_WEAPON_ANTIQUE:
            return TILE_SHOP_WEAPONS;
        case SHOP_ARMOUR:
        case SHOP_ARMOUR_ANTIQUE:
            return TILE_SHOP_ARMOUR;
        case SHOP_JEWELLERY:
            return TILE_SHOP_JEWELLERY;
        case SHOP_WAND:
            return TILE_SHOP_WANDS;
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

static tileidx_t _tileidx_feature_base(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_UNSEEN:
        return TILE_DNGN_UNSEEN;
    case DNGN_FLOOR_SPECIAL:
    case DNGN_ROCK_WALL:
    case DNGN_PERMAROCK_WALL:
        return TILE_WALL_NORMAL;
    case DNGN_SLIMY_WALL:
        return TILE_WALL_SLIME;
    case DNGN_OPEN_SEA:
        return TILE_DNGN_OPEN_SEA;
    case DNGN_SECRET_DOOR:
        return TILE_WALL_NORMAL;
    case DNGN_DETECTED_SECRET_DOOR:
        return TILE_DNGN_CLOSED_DOOR;
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_CLEAR_STONE_WALL:
    case DNGN_CLEAR_PERMAROCK_WALL:
        return TILE_DNGN_TRANSPARENT_WALL;
    case DNGN_STONE_WALL:
        return TILE_DNGN_STONE_WALL;
    case DNGN_CLOSED_DOOR:
        return TILE_DNGN_CLOSED_DOOR;
    case DNGN_METAL_WALL:
        return TILE_DNGN_METAL_WALL;
    case DNGN_GREEN_CRYSTAL_WALL:
        return TILE_DNGN_GREEN_CRYSTAL_WALL;
    case DNGN_ORCISH_IDOL:
        return TILE_DNGN_ORCISH_IDOL;
    case DNGN_WAX_WALL:
        return TILE_DNGN_WAX_WALL;
    case DNGN_TREE:
        return TILE_DNGN_TREE;
    case DNGN_GRANITE_STATUE:
        return TILE_DNGN_GRANITE_STATUE;
    case DNGN_LAVA:
        return TILE_DNGN_LAVA;
    case DNGN_DEEP_WATER:
        return TILE_DNGN_DEEP_WATER;
    case DNGN_SHALLOW_WATER:
        return TILE_DNGN_SHALLOW_WATER;
    case DNGN_FLOOR:
    case DNGN_UNDISCOVERED_TRAP:
        return TILE_FLOOR_NORMAL;
    case DNGN_ENTER_HELL:
        return TILE_DNGN_ENTER_HELL;
    case DNGN_OPEN_DOOR:
        return TILE_DNGN_OPEN_DOOR;
    case DNGN_TRAP_MECHANICAL:
        return TILE_DNGN_TRAP_DART;
    case DNGN_TRAP_MAGICAL:
        return TILE_DNGN_TRAP_ZOT;
    case DNGN_TRAP_NATURAL:
        return TILE_DNGN_TRAP_SHAFT;
    case DNGN_ENTER_SHOP:
        return TILE_SHOP_GENERAL;
    case DNGN_ABANDONED_SHOP:
        return TILE_DNGN_ABANDONED_SHOP;
    case DNGN_ENTER_LABYRINTH:
        return TILE_DNGN_ENTER_LABYRINTH;
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        return TILE_DNGN_STONE_STAIRS_DOWN;
    case DNGN_ESCAPE_HATCH_DOWN:
        return TILE_DNGN_ESCAPE_HATCH_DOWN;
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        if (player_in_hell())
            return TILE_DNGN_RETURN_HELL;
        return TILE_DNGN_STONE_STAIRS_UP;
    case DNGN_ESCAPE_HATCH_UP:
        return TILE_DNGN_ESCAPE_HATCH_UP;
    case DNGN_ENTER_DIS:
        return TILE_DNGN_ENTER_DIS;
    case DNGN_ENTER_GEHENNA:
        return TILE_DNGN_ENTER_GEHENNA;
    case DNGN_ENTER_COCYTUS:
        return TILE_DNGN_ENTER_COCYTUS;
    case DNGN_ENTER_TARTARUS:
        return TILE_DNGN_ENTER_TARTARUS;
    case DNGN_ENTER_ABYSS:
        return TILE_DNGN_ENTER_ABYSS;
    case DNGN_EXIT_ABYSS:
    case DNGN_EXIT_HELL:
    case DNGN_EXIT_PANDEMONIUM:
        return TILE_DNGN_EXIT_ABYSS;
    case DNGN_STONE_ARCH:
        return TILE_DNGN_STONE_ARCH;
    case DNGN_ENTER_PANDEMONIUM:
        return TILE_DNGN_ENTER_PANDEMONIUM;
    case DNGN_TRANSIT_PANDEMONIUM:
        return TILE_DNGN_TRANSIT_PANDEMONIUM;
    case DNGN_ENTER_DWARVEN_HALL:
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
        return TILE_DNGN_ENTER;

    case DNGN_ENTER_ZOT:
        if (you.opened_zot)
            return TILE_DNGN_ENTER_ZOT_OPEN;
        return TILE_DNGN_ENTER_ZOT_CLOSED;

    case DNGN_RETURN_FROM_DWARVEN_HALL:
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
        return TILE_DNGN_RETURN;
    case DNGN_RETURN_FROM_ZOT:
        return TILE_DNGN_RETURN_ZOT;
    case DNGN_ENTER_PORTAL_VAULT:
    case DNGN_EXIT_PORTAL_VAULT:
        return TILE_DNGN_PORTAL;
    case DNGN_TEMP_PORTAL:
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
    case DNGN_FOUNTAIN_BLUE:
        return TILE_DNGN_BLUE_FOUNTAIN;
    case DNGN_FOUNTAIN_SPARKLING:
        return TILE_DNGN_SPARKLING_FOUNTAIN;
    case DNGN_FOUNTAIN_BLOOD:
        return TILE_DNGN_BLOOD_FOUNTAIN;
    case DNGN_DRY_FOUNTAIN_BLUE:
    case DNGN_DRY_FOUNTAIN_SPARKLING:
    case DNGN_DRY_FOUNTAIN_BLOOD:
    case DNGN_PERMADRY_FOUNTAIN:
        return TILE_DNGN_DRY_FOUNTAIN;
    default:
        return TILE_DNGN_ERROR;
    }
}

tileidx_t tileidx_feature(const coord_def &gc)
{
    dungeon_feature_type feat = env.grid(gc);

    tileidx_t override = env.tile_flv(gc).feat;
    bool can_override = !feat_is_door(grd(gc))
                        && feat != DNGN_FLOOR
                        && feat != DNGN_UNSEEN;
    if (override && can_override)
        return (override);

    if (feature_mimic_at(gc))
        feat = get_mimic_feat(monster_at(gc));

    // Any grid-specific tiles.
    switch (feat)
    {
    case DNGN_SECRET_DOOR:
    case DNGN_DETECTED_SECRET_DOOR:
        {
            coord_def door;
            dungeon_feature_type door_feat;
            find_secret_door_info(gc, &door_feat, &door);

            // If surrounding tiles from a secret door are using tile
            // overrides, then use that tile for the secret door.
            if (env.tile_flv(door).feat)
                return (env.tile_flv(door).feat);
            else
                return (_tileidx_feature_base(door_feat));
        }
    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_NATURAL:
        return (_tileidx_trap(get_trap_type(gc)));
    case DNGN_ENTER_SHOP:
        return (_tileidx_shop(gc));
    case DNGN_DEEP_WATER:
        if (env.grid_colours(gc) == GREEN
            || env.grid_colours(gc) == LIGHTGREEN)
        {
            return (TILE_DNGN_DEEP_WATER_MURKY);
        }
        else if (player_in_branch(BRANCH_SHOALS))
            return (TILE_SHOALS_DEEP_WATER);

        return (TILE_DNGN_DEEP_WATER);
    case DNGN_SHALLOW_WATER:
        {
            tileidx_t t = TILE_DNGN_SHALLOW_WATER;
            if (env.grid_colours(gc) == GREEN
                || env.grid_colours(gc) == LIGHTGREEN)
            {
                t = TILE_DNGN_SHALLOW_WATER_MURKY;
            }
            else if (player_in_branch(BRANCH_SHOALS))
                t = TILE_SHOALS_SHALLOW_WATER;

            monster* mon = monster_at(gc);
            if (mon)
            {
                // Add disturbance to tile.
                if (mon->submerged())
                    t += tile_dngn_count(t);
            }

            return (t);
        }
    default:
        return (_tileidx_feature_base(feat));
    }
}


tileidx_t tileidx_out_of_bounds(int branch)
{
    if (branch == BRANCH_SHOALS)
        return (TILE_DNGN_OPEN_SEA | TILE_FLAG_UNSEEN);
    else
        return (TILE_DNGN_UNSEEN | TILE_FLAG_UNSEEN);
}

void tileidx_from_map_cell(tileidx_t *fg, tileidx_t *bg, const map_cell &cell)
{
    *bg = _tileidx_feature_base(cell.feat());

    switch (get_cell_show_class(cell))
    {
    default:
    case SH_NOTHING:
    case SH_FEATURE:
        *fg = 0;
        break;
    case SH_ITEM:
        *fg = tileidx_item(*cell.item());
        break;
    case SH_CLOUD:
        *fg = TILE_CLOUD_GREY_SMOKE;
        break;
    case SH_INVIS_EXPOSED:
        *fg = TILE_UNSEEN_MONSTER;
        break;
    case SH_MONSTER:
        *fg = _tileidx_monster_base(cell.monster());
        break;
    }
}

void tileidx_out_of_los(tileidx_t *fg, tileidx_t *bg, const coord_def& gc)
{
    // Player memory.
    tileidx_t mem_fg = env.tile_bk_fg(gc);
    tileidx_t mem_bg = env.tile_bk_bg(gc);

    // Detected info is just stored in map_knowledge and doesn't get
    // written to what the player remembers.  We'll feather that in here.

    const map_cell &cell = env.map_knowledge(gc);

    // Override terrain for magic mapping.
    if (!cell.seen() && env.map_knowledge(gc).mapped())
        *bg = _tileidx_feature_base(cell.feat());
    else
        *bg = mem_bg;
    *bg |= tileidx_unseen_flag(gc);

    // Override foreground for monsters/items
    if (env.map_knowledge(gc).detected_monster())
        *fg = _tileidx_monster_base(cell.monster());
    else if (env.map_knowledge(gc).detected_item())
        *fg = tileidx_item(*cell.item());
    else
        *fg = mem_fg;
}

static bool _is_skeleton(const int z_type)
{
    return (z_type == MONS_SKELETON_SMALL || z_type == MONS_SKELETON_LARGE);
}

static tileidx_t _tileidx_monster_zombified(const monster* mon)
{
    const int z_type = mon->type;

    // TODO: Add tiles and code for these as well.
    switch (z_type)
    {
    case MONS_SIMULACRUM_SMALL: return TILEP_MONS_SIMULACRUM_SMALL;
    case MONS_SIMULACRUM_LARGE: return TILEP_MONS_SIMULACRUM_LARGE;
    }

    const int subtype = (int) mons_zombie_base(mon);
    const int z_size = mons_zombie_size(subtype);

    tileidx_t z_tile;
    switch (get_mon_shape(mon))
    {
    case MON_SHAPE_HUMANOID:
    case MON_SHAPE_HUMANOID_WINGED:
    case MON_SHAPE_HUMANOID_TAILED:
    case MON_SHAPE_HUMANOID_WINGED_TAILED:
        if (z_type == MONS_SKELETON_SMALL)
            return TILEP_MONS_SKELETON_SMALL;
        else if (z_type == MONS_SKELETON_LARGE)
            return TILEP_MONS_SKELETON_LARGE;

        z_tile = (z_size == Z_SMALL ? TILEP_MONS_ZOMBIE_SMALL
                                    : TILEP_MONS_ZOMBIE_LARGE);
        break;
    case MON_SHAPE_CENTAUR:
        if (_is_skeleton(z_type))
            return TILEP_MONS_SKELETON_CENTAUR;

        z_tile = TILEP_MONS_ZOMBIE_CENTAUR;
        break;
    case MON_SHAPE_NAGA:
        if (_is_skeleton(z_type))
            return TILEP_MONS_SKELETON_NAGA;

        z_tile = TILEP_MONS_ZOMBIE_NAGA;
        break;
    case MON_SHAPE_QUADRUPED_WINGED:
        if (mons_genus(subtype) == MONS_DRAGON)
        {
            if (_is_skeleton(z_type))
                return TILEP_MONS_SKELETON_DRAGON;

            z_tile = TILEP_MONS_ZOMBIE_DRAGON;
            break;
        }
        // else fall-through
    case MON_SHAPE_QUADRUPED:
        if (mons_genus(subtype) == MONS_HYDRA)
        {
            if (_is_skeleton(z_type))
            {
                return TILEP_MONS_SKELETON_HYDRA
                       + std::min((int)mon->number, 5) - 1;
            }

            z_tile = TILEP_MONS_ZOMBIE_HYDRA
                     + std::min((int)mon->number, 5) - 1;
            break;
        }
        // else fall-through
    case MON_SHAPE_QUADRUPED_TAILLESS:
        if (z_type == MONS_SKELETON_SMALL)
            return TILEP_MONS_SKELETON_QUADRUPED_SMALL;
        else if (z_type == MONS_SKELETON_LARGE)
            return TILEP_MONS_SKELETON_QUADRUPED_LARGE;

        z_tile = (z_size == Z_SMALL ? TILEP_MONS_ZOMBIE_QUADRUPED_SMALL
                                    : TILEP_MONS_ZOMBIE_QUADRUPED_LARGE);
        break;
    case MON_SHAPE_BAT:
        if (_is_skeleton(z_type))
            return TILEP_MONS_SKELETON_BAT;

        z_tile = TILEP_MONS_ZOMBIE_BAT;
        break;
    case MON_SHAPE_SNAIL:
    case MON_SHAPE_SNAKE:
        if (_is_skeleton(z_type))
            return TILEP_MONS_SKELETON_SNAKE;

        z_tile = TILEP_MONS_ZOMBIE_SNAKE;
        break;
    case MON_SHAPE_FISH:
        if (_is_skeleton(z_type))
            return TILEP_MONS_SKELETON_FISH;

        z_tile = TILEP_MONS_ZOMBIE_FISH;
        break;
    case MON_SHAPE_CENTIPEDE:
    case MON_SHAPE_INSECT:
        z_tile = TILEP_MONS_ZOMBIE_BEETLE;
        break;
    case MON_SHAPE_INSECT_WINGED:
        z_tile = TILEP_MONS_ZOMBIE_BEE;
        break;
    case MON_SHAPE_ARACHNID:
        z_tile = TILEP_MONS_ZOMBIE_SPIDER;
        break;
    default:
        z_tile = TILEP_ERROR;
    }

    if (z_type == MONS_SPECTRAL_THING)
        z_tile += (TILEP_MONS_SPECTRAL_SMALL - TILEP_MONS_ZOMBIE_SMALL);

    return (z_tile);
}

// Special case for *taurs which have a different tile
// for when they have a bow.
static int _bow_offset(const monster* mon)
{
    int mon_wep = mon->inv[MSLOT_WEAPON];
    if (mon_wep == NON_ITEM)
        return (1);

    switch (mitm[mon_wep].sub_type)
    {
    case WPN_BOW:
    case WPN_LONGBOW:
    case WPN_CROSSBOW:
        return (0);
    default:
        return (1);
    }
}

static tileidx_t _mon_mod(tileidx_t tile, int offset)
{
    int count = tile_player_count(tile);
    return (tile + offset % count);
}

static tileidx_t _mon_clamp(tileidx_t tile, int offset)
{
    int count = tile_player_count(tile);
    return (tile + std::min(std::max(offset, 0), count - 1));
}

static tileidx_t _mon_random(tileidx_t tile)
{
    int count = tile_player_count(tile);
    return (tile + random2(count));
}

// This function allows for getting a monster from "just" the type.
// To avoid needless duplication of a cases in tileidx_monster, some
// extra parameters that have reasonable defaults for monsters where
// only the type is known are pushed here.
static tileidx_t _tileidx_monster_base(int type, bool in_water, int colour,
                                       int number, int tile_num_prop)
{
    switch (type)
    {
    // program bug
    case MONS_PROGRAM_BUG:
        return TILEP_MONS_PROGRAM_BUG;
    case MONS_SENSED:
        return TILE_UNSEEN_MONSTER;

    // insects ('a')
    case MONS_GIANT_COCKROACH:
        return TILEP_MONS_GIANT_COCKROACH;
    case MONS_GIANT_ANT:
        return TILEP_MONS_GIANT_ANT;
    case MONS_SOLDIER_ANT:
        return TILEP_MONS_SOLDIER_ANT;
    case MONS_QUEEN_ANT:
        return TILEP_MONS_QUEEN_ANT;

    // batty monsters ('b')
    case MONS_GIANT_BAT:
        return TILEP_MONS_GIANT_BAT;
    case MONS_BUTTERFLY:
        return _mon_mod(TILEP_MONS_BUTTERFLY, colour);

    // centaurs ('c')
    case MONS_CENTAUR:
        return TILEP_MONS_CENTAUR;
    case MONS_CENTAUR_WARRIOR:
        return TILEP_MONS_CENTAUR_WARRIOR;
    case MONS_YAKTAUR:
        return TILEP_MONS_YAKTAUR;
    case MONS_YAKTAUR_CAPTAIN:
        return TILEP_MONS_YAKTAUR_CAPTAIN;

    // draconians ('d'):
    case MONS_DRACONIAN:
        return TILEP_DRACO_BASE;

    // elves ('e')
    case MONS_ELF:
        return TILEP_MONS_ELF;
    case MONS_DEEP_ELF_SOLDIER:
        return TILEP_MONS_DEEP_ELF_SOLDIER;
    case MONS_DEEP_ELF_FIGHTER:
        return TILEP_MONS_DEEP_ELF_FIGHTER;
    case MONS_DEEP_ELF_KNIGHT:
        return TILEP_MONS_DEEP_ELF_KNIGHT;
    case MONS_DEEP_ELF_MAGE:
        return TILEP_MONS_DEEP_ELF_MAGE;
    case MONS_DEEP_ELF_SUMMONER:
        return TILEP_MONS_DEEP_ELF_SUMMONER;
    case MONS_DEEP_ELF_CONJURER:
        return TILEP_MONS_DEEP_ELF_CONJURER;
    case MONS_DEEP_ELF_PRIEST:
        return TILEP_MONS_DEEP_ELF_PRIEST;
    case MONS_DEEP_ELF_HIGH_PRIEST:
        return TILEP_MONS_DEEP_ELF_HIGH_PRIEST;
    case MONS_DEEP_ELF_DEMONOLOGIST:
        return TILEP_MONS_DEEP_ELF_DEMONOLOGIST;
    case MONS_DEEP_ELF_ANNIHILATOR:
        return TILEP_MONS_DEEP_ELF_ANNIHILATOR;
    case MONS_DEEP_ELF_SORCERER:
        return TILEP_MONS_DEEP_ELF_SORCERER;
    case MONS_DEEP_ELF_DEATH_MAGE:
        return TILEP_MONS_DEEP_ELF_DEATH_MAGE;
    case MONS_DEEP_ELF_BLADEMASTER:
        return TILEP_MONS_DEEP_ELF_BLADEMASTER;
    case MONS_DEEP_ELF_MASTER_ARCHER:
        return TILEP_MONS_DEEP_ELF_MASTER_ARCHER;

    // fungi ('f')
    case MONS_BALLISTOMYCETE:
        return TILEP_MONS_BALLISTOMYCETE_INACTIVE;
    case MONS_TOADSTOOL:
        return _mon_mod(TILEP_MONS_TOADSTOOL, tile_num_prop);
    case MONS_FUNGUS:
        return TILEP_MONS_FUNGUS;
    case MONS_WANDERING_MUSHROOM:
        return TILEP_MONS_WANDERING_MUSHROOM;

    // goblins ('g')
    case MONS_GOBLIN:
        return TILEP_MONS_GOBLIN;
    case MONS_HOBGOBLIN:
        return TILEP_MONS_HOBGOBLIN;
    case MONS_GNOLL:
        return TILEP_MONS_GNOLL;
    case MONS_BOGGART:
        return TILEP_MONS_BOGGART;

    // hounds and hogs ('h')
    case MONS_JACKAL:
        return TILEP_MONS_JACKAL;
    case MONS_HOUND:
        return TILEP_MONS_HOUND;
    case MONS_WARG:
        return TILEP_MONS_WARG;
    case MONS_WOLF:
        return TILEP_MONS_WOLF;
    case MONS_WAR_DOG:
        return TILEP_MONS_WAR_DOG;
    case MONS_HOG:
        return TILEP_MONS_HOG;
    case MONS_HELL_HOUND:
        return TILEP_MONS_HELL_HOUND;
    case MONS_HELL_HOG:
        return TILEP_MONS_HELL_HOG;
    case MONS_FELID:
        return TILEP_MONS_FELID;

    // slugs ('j')
    case MONS_ELEPHANT_SLUG:
        return TILEP_MONS_ELEPHANT_SLUG;
    case MONS_GIANT_SLUG:
        return TILEP_MONS_GIANT_SLUG;
    case MONS_AGATE_SNAIL:
        return TILEP_MONS_AGATE_SNAIL;

    // killer bees ('k')
    case MONS_KILLER_BEE:
        return TILEP_MONS_KILLER_BEE;
    case MONS_BUMBLEBEE:
        return TILEP_MONS_BUMBLEBEE;
    case MONS_QUEEN_BEE:
        return TILEP_MONS_QUEEN_BEE;
    case MONS_FIREFLY:
        return TILEP_MONS_FIREFLY;

    // lizards ('l')
    case MONS_GIANT_NEWT:
        return TILEP_MONS_GIANT_NEWT;
    case MONS_GIANT_GECKO:
        return TILEP_MONS_GIANT_GECKO;
    case MONS_IGUANA:
        return TILEP_MONS_IGUANA;
    case MONS_GILA_MONSTER:
        return TILEP_MONS_GILA_MONSTER;
    case MONS_KOMODO_DRAGON:
        return TILEP_MONS_KOMODO_DRAGON;

    // drakes (also 'l', but dragon type)
    case MONS_SWAMP_DRAKE:
        return TILEP_MONS_SWAMP_DRAKE;
    case MONS_FIRE_DRAKE:
        return TILEP_MONS_FIRE_DRAKE;
    case MONS_LINDWURM:
        return TILEP_MONS_LINDWURM;
    case MONS_DEATH_DRAKE:
        return TILEP_MONS_DEATH_DRAKE;

    // merfolk ('m')
    case MONS_MERFOLK:
        if (in_water)
            return TILEP_MONS_MERFOLK_WATER;
        else
            return TILEP_MONS_MERFOLK;
    case MONS_MERFOLK_IMPALER:
        if (in_water)
            return TILEP_MONS_MERFOLK_IMPALER_WATER;
        else
            return TILEP_MONS_MERFOLK_IMPALER;
    case MONS_MERFOLK_AQUAMANCER:
        if (in_water)
            return TILEP_MONS_MERFOLK_AQUAMANCER_WATER;
        else
            return TILEP_MONS_MERFOLK_AQUAMANCER;
    case MONS_MERFOLK_JAVELINEER:
        if (in_water)
            return TILEP_MONS_MERFOLK_JAVELINEER_WATER;
        else
            return TILEP_MONS_MERFOLK_JAVELINEER;
    case MONS_MERMAID:
        if (in_water)
            return TILEP_MONS_MERMAID_WATER;
        else
            return TILEP_MONS_MERMAID;
    case MONS_SIREN:
        if (in_water)
            return TILEP_MONS_SIREN_WATER;
        else
            return TILEP_MONS_SIREN;

    // rotting monsters ('n')
    case MONS_NECROPHAGE:
        return TILEP_MONS_NECROPHAGE;
    case MONS_GHOUL:
        return TILEP_MONS_GHOUL;
    case MONS_ROTTING_HULK:
        return TILEP_MONS_ROTTING_HULK;

    // orcs ('o')
    case MONS_ORC:
        return TILEP_MONS_ORC;
    case MONS_ORC_WIZARD:
        return TILEP_MONS_ORC_WIZARD;
    case MONS_ORC_PRIEST:
        return TILEP_MONS_ORC_PRIEST;
    case MONS_ORC_WARRIOR:
        return TILEP_MONS_ORC_WARRIOR;
    case MONS_ORC_KNIGHT:
        return TILEP_MONS_ORC_KNIGHT;
    case MONS_ORC_WARLORD:
        return TILEP_MONS_ORC_WARLORD;
    case MONS_ORC_SORCERER:
        return TILEP_MONS_ORC_SORCERER;
    case MONS_ORC_HIGH_PRIEST:
        return TILEP_MONS_ORC_HIGH_PRIEST;

    // phantoms and ghosts ('p')
    case MONS_PHANTOM:
        return TILEP_MONS_PHANTOM;
    case MONS_HUNGRY_GHOST:
        return TILEP_MONS_HUNGRY_GHOST;
    case MONS_FLAYED_GHOST:
        return TILEP_MONS_FLAYED_GHOST;
    case MONS_PLAYER_GHOST:
    case MONS_PLAYER_ILLUSION:
        return TILEP_MONS_PLAYER_GHOST;
    case MONS_INSUBSTANTIAL_WISP:
        return TILEP_MONS_INSUBSTANTIAL_WISP;
    case MONS_SILENT_SPECTRE:
        return TILEP_MONS_SILENT_SPECTRE;
    case MONS_SPIRIT:
        return TILEP_MONS_SPIRIT;

    // rodents ('r')
    case MONS_RAT:
        return TILEP_MONS_RAT;
    case MONS_QUOKKA:
        return TILEP_MONS_QUOKKA;
    case MONS_GREY_RAT:
        return TILEP_MONS_GREY_RAT;
    case MONS_GREEN_RAT:
        return TILEP_MONS_GREEN_RAT;
    case MONS_ORANGE_RAT:
        return TILEP_MONS_ORANGE_RAT;

    // spiders and insects ('s')
    case MONS_GIANT_MITE:
        return TILEP_MONS_GIANT_MITE;
    case MONS_GIANT_CENTIPEDE:
        return TILEP_MONS_GIANT_CENTIPEDE;
    case MONS_SCORPION:
        return TILEP_MONS_SCORPION;
    case MONS_EMPEROR_SCORPION:
        return TILEP_MONS_EMPEROR_SCORPION;
    case MONS_SPIDER:
        return TILEP_MONS_SPIDER;
    case MONS_TARANTELLA:
        return TILEP_MONS_TARANTELLA;
    case MONS_JUMPING_SPIDER:
        return TILEP_MONS_JUMPING_SPIDER;
    case MONS_WOLF_SPIDER:
        return TILEP_MONS_WOLF_SPIDER;
    case MONS_TRAPDOOR_SPIDER:
        return TILEP_MONS_TRAPDOOR_SPIDER;
    case MONS_REDBACK:
        return TILEP_MONS_REDBACK;
    case MONS_DEMONIC_CRAWLER:
        return TILEP_MONS_DEMONIC_CRAWLER;

    // turtles and crocodiles ('t')
    case MONS_CROCODILE:
        return TILEP_MONS_CROCODILE;
    case MONS_BABY_ALLIGATOR:
        return TILEP_MONS_BABY_ALLIGATOR;
    case MONS_ALLIGATOR:
        return TILEP_MONS_ALLIGATOR;
    case MONS_SNAPPING_TURTLE:
        return TILEP_MONS_SNAPPING_TURTLE;
    case MONS_ALLIGATOR_SNAPPING_TURTLE:
        return TILEP_MONS_ALLIGATOR_SNAPPING_TURTLE;

    // ugly things ('u')
    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
    {
        const tileidx_t ugly_tile = (type == MONS_VERY_UGLY_THING) ?
            TILEP_MONS_VERY_UGLY_THING : TILEP_MONS_UGLY_THING;
        int colour_offset = ugly_thing_colour_offset(colour);
        return _mon_clamp(ugly_tile, colour_offset);
    }

    // vortices ('v')
    case MONS_FIRE_VORTEX:
        return TILEP_MONS_FIRE_VORTEX;
    case MONS_SPATIAL_VORTEX:
        return TILEP_MONS_SPATIAL_VORTEX;

    // elementals (different symbols)
    case MONS_AIR_ELEMENTAL:
        return TILEP_MONS_AIR_ELEMENTAL;
    case MONS_EARTH_ELEMENTAL:
        return TILEP_MONS_EARTH_ELEMENTAL;
    case MONS_FIRE_ELEMENTAL:
        return TILEP_MONS_FIRE_ELEMENTAL;
    case MONS_WATER_ELEMENTAL:
        return TILEP_MONS_WATER_ELEMENTAL;
    case MONS_IRON_ELEMENTAL:
        return TILEP_MONS_IRON_ELEMENTAL;

    // worms and larvae ('w')
    case MONS_KILLER_BEE_LARVA:
        return TILEP_MONS_KILLER_BEE_LARVA;
    case MONS_WORM:
        return TILEP_MONS_WORM;
    case MONS_ANT_LARVA:
        return TILEP_MONS_ANT_LARVA;
    case MONS_BRAIN_WORM:
        return TILEP_MONS_BRAIN_WORM;
    case MONS_SWAMP_WORM:
        return TILEP_MONS_SWAMP_WORM;
    case MONS_GIANT_LEECH:
        return TILEP_MONS_GIANT_LEECH;
    case MONS_SPINY_WORM:
        return TILEP_MONS_SPINY_WORM;

    // small abominations ('x')
    case MONS_UNSEEN_HORROR:
        return TILEP_MONS_UNSEEN_HORROR;
    case MONS_ABOMINATION_SMALL:
        return TILEP_MONS_ABOMINATION_SMALL;

    // flying insects ('y')
    case MONS_YELLOW_WASP:
        return TILEP_MONS_YELLOW_WASP;
    case MONS_GIANT_MOSQUITO:
        return TILEP_MONS_GIANT_MOSQUITO;
    case MONS_GIANT_BLOWFLY:
        return TILEP_MONS_GIANT_BLOWFLY;
    case MONS_RED_WASP:
        return TILEP_MONS_RED_WASP;
    case MONS_GHOST_MOTH:
        return TILEP_MONS_GHOST_MOTH;
    case MONS_MOTH_OF_WRATH:
        return TILEP_MONS_MOTH_OF_WRATH;

    // small zombies, etc. ('z')
    case MONS_ZOMBIE_SMALL:
        return TILEP_MONS_ZOMBIE_SMALL;
    case MONS_SIMULACRUM_SMALL:
        return TILEP_MONS_SIMULACRUM_SMALL;
    case MONS_SKELETON_SMALL:
        return TILEP_MONS_SKELETON_SMALL;
    case MONS_WIGHT:
        return TILEP_MONS_WIGHT;
    case MONS_SKELETAL_WARRIOR:
        return TILEP_MONS_SKELETAL_WARRIOR;
    case MONS_FLYING_SKULL:
        return TILEP_MONS_FLYING_SKULL;
    case MONS_FLAMING_CORPSE:
        return TILEP_MONS_FLAMING_CORPSE;
    case MONS_CURSE_SKULL:
        return TILEP_MONS_CURSE_SKULL;
    case MONS_CURSE_TOE:
        return TILEP_MONS_CURSE_TOE;

    // angelic beings ('A')
    case MONS_ANGEL:
        return TILEP_MONS_ANGEL;
    case MONS_CHERUB:
        return TILEP_MONS_CHERUB;
    case MONS_DAEVA:
        return TILEP_MONS_DAEVA;
    //TODO
    case MONS_MENNAS:
        return TILEP_MONS_ANGEL;

    // beetles ('B')
    case MONS_GIANT_BEETLE:
        return TILEP_MONS_GIANT_BEETLE;
    case MONS_BOULDER_BEETLE:
        return TILEP_MONS_BOULDER_BEETLE;
    case MONS_BORING_BEETLE:
        return TILEP_MONS_BORING_BEETLE;

    // cyclopes and giants ('C')
    case MONS_HILL_GIANT:
        return TILEP_MONS_HILL_GIANT;
    case MONS_ETTIN:
        return TILEP_MONS_ETTIN;
    case MONS_CYCLOPS:
        return TILEP_MONS_CYCLOPS;
    case MONS_FIRE_GIANT:
        return TILEP_MONS_FIRE_GIANT;
    case MONS_FROST_GIANT:
        return TILEP_MONS_FROST_GIANT;
    case MONS_STONE_GIANT:
        return TILEP_MONS_STONE_GIANT;
    case MONS_TITAN:
        return TILEP_MONS_TITAN;

    // dragons ('D')
    case MONS_WYVERN:
        return TILEP_MONS_WYVERN;
    case MONS_DRAGON:
        return TILEP_MONS_DRAGON;
    case MONS_HYDRA:
        // Number of heads
        return _mon_clamp(TILEP_MONS_HYDRA, number - 1);
    case MONS_ICE_DRAGON:
        return TILEP_MONS_ICE_DRAGON;
    case MONS_STEAM_DRAGON:
        return TILEP_MONS_STEAM_DRAGON;
    case MONS_SWAMP_DRAGON:
        return TILEP_MONS_SWAMP_DRAGON;
    case MONS_MOTTLED_DRAGON:
        return TILEP_MONS_MOTTLED_DRAGON;
    case MONS_QUICKSILVER_DRAGON:
        return TILEP_MONS_QUICKSILVER_DRAGON;
    case MONS_IRON_DRAGON:
        return TILEP_MONS_IRON_DRAGON;
    case MONS_STORM_DRAGON:
        return TILEP_MONS_STORM_DRAGON;
    case MONS_GOLDEN_DRAGON:
        return TILEP_MONS_GOLDEN_DRAGON;
    case MONS_SHADOW_DRAGON:
        return TILEP_MONS_SHADOW_DRAGON;
    case MONS_BONE_DRAGON:
        return TILEP_MONS_BONE_DRAGON;
    case MONS_SERPENT_OF_HELL:
        return TILEP_MONS_SERPENT_OF_HELL;
    case MONS_HOLY_DRAGON:
        return TILEP_MONS_HOLY_DRAGON;

    // efreet ('E')
    case MONS_EFREET:
        return TILEP_MONS_EFREET;

    // frogs ('F')
    case MONS_GIANT_FROG:
        return TILEP_MONS_GIANT_FROG;
    case MONS_GIANT_TOAD:
        return TILEP_MONS_GIANT_TOAD;
    case MONS_SPINY_FROG:
        return TILEP_MONS_SPINY_FROG;
    case MONS_BLINK_FROG:
        return TILEP_MONS_BLINK_FROG;

    // eyes and spores ('G')
    case MONS_GIANT_SPORE:
        return TILEP_MONS_GIANT_SPORE;
    case MONS_GIANT_EYEBALL:
        return TILEP_MONS_GIANT_EYEBALL;
    case MONS_EYE_OF_DRAINING:
        return TILEP_MONS_EYE_OF_DRAINING;
    case MONS_GIANT_ORANGE_BRAIN:
        return TILEP_MONS_GIANT_ORANGE_BRAIN;
    case MONS_GREAT_ORB_OF_EYES:
        return TILEP_MONS_GREAT_ORB_OF_EYES;
    case MONS_SHINING_EYE:
        return TILEP_MONS_SHINING_EYE;
    case MONS_EYE_OF_DEVASTATION:
        return TILEP_MONS_EYE_OF_DEVASTATION;
    case MONS_GOLDEN_EYE:
        return TILEP_MONS_GOLDEN_EYE;
    case MONS_OPHAN:
        return TILEP_MONS_OPHAN;

    // hybrids ('H')
    case MONS_HIPPOGRIFF:
        return TILEP_MONS_HIPPOGRIFF;
    case MONS_MANTICORE:
        return TILEP_MONS_MANTICORE;
    case MONS_GRIFFON:
        return TILEP_MONS_GRIFFON;
    case MONS_SPHINX:
        return TILEP_MONS_SPHINX;
    case MONS_HARPY:
        return TILEP_MONS_HARPY;
    case MONS_MINOTAUR:
        return TILEP_MONS_MINOTAUR;
    case MONS_SHEDU:
        return TILEP_MONS_SHEDU;
    case MONS_KENKU:
        return TILEP_MONS_KENKU;

    // ice beast ('I')
    case MONS_ICE_BEAST:
        return TILEP_MONS_ICE_BEAST;
    case MONS_SKY_BEAST:
        return TILEP_MONS_SKY_BEAST;

    // jellies ('J')
    case MONS_OOZE:
        return TILEP_MONS_OOZE;
    case MONS_JELLY:
        return TILEP_MONS_JELLY;
    case MONS_SLIME_CREATURE:
    case MONS_MERGED_SLIME_CREATURE:
        return _mon_clamp(TILEP_MONS_SLIME_CREATURE, number - 1);
    case MONS_PULSATING_LUMP:
        return TILEP_MONS_PULSATING_LUMP;
    case MONS_GIANT_AMOEBA:
        return TILEP_MONS_GIANT_AMOEBA;
    case MONS_BROWN_OOZE:
        return TILEP_MONS_BROWN_OOZE;
    case MONS_AZURE_JELLY:
        return TILEP_MONS_AZURE_JELLY;
    case MONS_DEATH_OOZE:
        return TILEP_MONS_DEATH_OOZE;
    case MONS_ACID_BLOB:
        return TILEP_MONS_ACID_BLOB;
    case MONS_ROYAL_JELLY:
        return TILEP_MONS_ROYAL_JELLY;

    // kobolds ('K')
    case MONS_KOBOLD:
        return TILEP_MONS_KOBOLD;
    case MONS_BIG_KOBOLD:
        return TILEP_MONS_BIG_KOBOLD;
    case MONS_KOBOLD_DEMONOLOGIST:
        return TILEP_MONS_KOBOLD_DEMONOLOGIST;

    // liches ('L')
    case MONS_LICH:
        return TILEP_MONS_LICH;
    case MONS_ANCIENT_LICH:
        return TILEP_MONS_ANCIENT_LICH;

    // mummies ('M')
    case MONS_MUMMY:
        return TILEP_MONS_MUMMY;
    case MONS_GUARDIAN_MUMMY:
        return TILEP_MONS_GUARDIAN_MUMMY;
    case MONS_GREATER_MUMMY:
        return TILEP_MONS_GREATER_MUMMY;
    case MONS_MUMMY_PRIEST:
        return TILEP_MONS_MUMMY_PRIEST;

    // nagas ('N')
    case MONS_NAGA:
        return TILEP_MONS_NAGA;
    case MONS_GUARDIAN_SERPENT:
        return TILEP_MONS_GUARDIAN_SERPENT;
    case MONS_NAGA_MAGE:
        return TILEP_MONS_NAGA_MAGE;
    case MONS_NAGA_WARRIOR:
        return TILEP_MONS_NAGA_WARRIOR;
    case MONS_GREATER_NAGA:
        return TILEP_MONS_GREATER_NAGA;

    // ogres ('O')
    case MONS_OGRE:
        return TILEP_MONS_OGRE;
    case MONS_TWO_HEADED_OGRE:
        return TILEP_MONS_TWO_HEADED_OGRE;
    case MONS_OGRE_MAGE:
        return TILEP_MONS_OGRE_MAGE;

    // plants ('P')
    case MONS_PLANT:
        return TILEP_MONS_PLANT;
    case MONS_BUSH:
        return TILEP_MONS_BUSH;
    case MONS_BURNING_BUSH:
        return TILEP_MONS_BUSH_BURNING;
    case MONS_OKLOB_SAPLING:
    case MONS_OKLOB_PLANT:
        return TILEP_MONS_OKLOB_PLANT;

    // rakshasa ('R')
    case MONS_RAKSHASA:
        return TILEP_MONS_RAKSHASA;
    case MONS_RAKSHASA_FAKE:
        return TILEP_MONS_RAKSHASA_FAKE;

    // snakes ('S')
    case MONS_SMALL_SNAKE:
        return TILEP_MONS_SMALL_SNAKE;
    case MONS_SNAKE:
        return TILEP_MONS_SNAKE;
    case MONS_WATER_MOCCASIN:
        return TILEP_MONS_WATER_MOCCASIN;
    case MONS_BLACK_MAMBA:
        return TILEP_MONS_BLACK_MAMBA;
    case MONS_VIPER:
        return TILEP_MONS_VIPER;
    case MONS_ANACONDA:
        return TILEP_MONS_ANACONDA;
    case MONS_SEA_SNAKE:
        return TILEP_MONS_SEA_SNAKE;

    // trolls ('T')
    case MONS_TROLL:
        return TILEP_MONS_TROLL;
    case MONS_ROCK_TROLL:
        return TILEP_MONS_ROCK_TROLL;
    case MONS_IRON_TROLL:
        return TILEP_MONS_IRON_TROLL;
    case MONS_DEEP_TROLL:
        return TILEP_MONS_DEEP_TROLL;

    // bears ('U')
    case MONS_BEAR:
        return TILEP_MONS_BEAR;
    case MONS_GRIZZLY_BEAR:
        return TILEP_MONS_GRIZZLY_BEAR;
    case MONS_POLAR_BEAR:
        return TILEP_MONS_POLAR_BEAR;
    case MONS_BLACK_BEAR:
        return TILEP_MONS_BLACK_BEAR;

    // vampires ('V')
    case MONS_VAMPIRE:
        return TILEP_MONS_VAMPIRE;
    case MONS_VAMPIRE_KNIGHT:
        return TILEP_MONS_VAMPIRE_KNIGHT;
    case MONS_VAMPIRE_MAGE:
        return TILEP_MONS_VAMPIRE_MAGE;

    // wraiths ('W')
    case MONS_WRAITH:
        return TILEP_MONS_WRAITH;
    case MONS_SHADOW_WRAITH:
        return TILEP_MONS_SHADOW_WRAITH;
    case MONS_FREEZING_WRAITH:
        return TILEP_MONS_FREEZING_WRAITH;
    case MONS_PHANTASMAL_WARRIOR:
        return TILEP_MONS_PHANTASMAL_WARRIOR;
    case MONS_SPECTRAL_THING:
        return TILEP_MONS_SPECTRAL_LARGE;
    case MONS_EIDOLON:
        return TILEP_MONS_EIDOLON;

    // large abominations ('X')
    case MONS_ABOMINATION_LARGE:
        return _mon_mod(TILEP_MONS_ABOMINATION_LARGE, colour);
    case MONS_TENTACLED_MONSTROSITY:
        return TILEP_MONS_TENTACLED_MONSTROSITY;
    case MONS_ORB_GUARDIAN:
        return TILEP_MONS_ORB_GUARDIAN;
    case MONS_TEST_SPAWNER:
        return TILEP_MONS_TEST_SPAWNER;

    // yaks, sheep and elephants ('Y')
    case MONS_SHEEP:
        return TILEP_MONS_SHEEP;
    case MONS_YAK:
        return TILEP_MONS_YAK;
    case MONS_DEATH_YAK:
        return TILEP_MONS_DEATH_YAK;
    case MONS_ELEPHANT:
        return TILEP_MONS_ELEPHANT;
    case MONS_DIRE_ELEPHANT:
        return TILEP_MONS_DIRE_ELEPHANT;
    case MONS_HELLEPHANT:
        return TILEP_MONS_HELLEPHANT;
    case MONS_APIS:
        return TILEP_MONS_APIS;

    // large zombies, etc. ('Z')
    case MONS_ZOMBIE_LARGE:
        return TILEP_MONS_ZOMBIE_LARGE;
    case MONS_SKELETON_LARGE:
        return TILEP_MONS_SKELETON_LARGE;
    case MONS_SIMULACRUM_LARGE:
        return TILEP_MONS_SIMULACRUM_LARGE;

    // water monsters
    case MONS_BIG_FISH:
        return TILEP_MONS_BIG_FISH;
    case MONS_GIANT_GOLDFISH:
        return TILEP_MONS_GIANT_GOLDFISH;
    case MONS_ELECTRIC_EEL:
        return TILEP_MONS_ELECTRIC_EEL;
    case MONS_SHARK:
        return TILEP_MONS_SHARK;
    case MONS_JELLYFISH:
        return TILEP_MONS_JELLYFISH;
    case MONS_KRAKEN:
        return TILEP_MONS_KRAKEN_HEAD;

    // lava monsters
    case MONS_LAVA_WORM:
        return TILEP_MONS_LAVA_WORM;
    case MONS_LAVA_FISH:
        return TILEP_MONS_LAVA_FISH;
    case MONS_LAVA_SNAKE:
        return TILEP_MONS_LAVA_SNAKE;
    case MONS_SALAMANDER:
        return TILEP_MONS_SALAMANDER;

    // monsters moving through rock
    case MONS_ROCK_WORM:
        return TILEP_MONS_ROCK_WORM;

    // humans ('@')
    case MONS_HUMAN:
        return TILEP_MONS_HUMAN;
    case MONS_HELL_KNIGHT:
        return TILEP_MONS_HELL_KNIGHT;
    case MONS_NECROMANCER:
        return TILEP_MONS_NECROMANCER;
    case MONS_WIZARD:
        return TILEP_MONS_WIZARD;
    case MONS_VAULT_GUARD:
        return TILEP_MONS_VAULT_GUARD;
    case MONS_SHAPESHIFTER:
        return TILEP_MONS_SHAPESHIFTER;
    case MONS_GLOWING_SHAPESHIFTER:
        return TILEP_MONS_GLOWING_SHAPESHIFTER;
    case MONS_KILLER_KLOWN:
        return TILEP_MONS_KILLER_KLOWN;
    case MONS_SLAVE:
        return _mon_mod(TILEP_MONS_SLAVE, tile_num_prop);
    case MONS_DEMONSPAWN:
        return TILEP_MONS_DEMONSPAWN;
    case MONS_DEMIGOD:
        return TILEP_MONS_DEMIGOD;
    case MONS_HALFLING:
        return TILEP_MONS_HALFLING;
    case MONS_PALADIN:
        return TILEP_MONS_PALADIN;

    // mimics
    case MONS_GOLD_MIMIC:
        return TILE_UNSEEN_GOLD;
    case MONS_WEAPON_MIMIC:
        return TILE_UNSEEN_WEAPON;
    case MONS_ARMOUR_MIMIC:
        return TILE_UNSEEN_ARMOUR;
    case MONS_SCROLL_MIMIC:
        return TILE_UNSEEN_SCROLL;
    case MONS_POTION_MIMIC:
        return TILE_UNSEEN_POTION;
    case MONS_DANCING_WEAPON:
        return TILE_UNSEEN_WEAPON;

    // Feature mimics actually get drawn with the dungeon code.
    // See tileidx_feature.
    case MONS_DOOR_MIMIC:
    case MONS_PORTAL_MIMIC:
    case MONS_TRAP_MIMIC:
    case MONS_STAIR_MIMIC:
    case MONS_SHOP_MIMIC:
    case MONS_FOUNTAIN_MIMIC:
        return 0;

    // '5' demons
    case MONS_IMP:
        return TILEP_MONS_IMP;
    case MONS_QUASIT:
        return TILEP_MONS_QUASIT;
    case MONS_WHITE_IMP:
        return TILEP_MONS_WHITE_IMP;
    case MONS_LEMURE:
        return TILEP_MONS_LEMURE;
    case MONS_UFETUBUS:
        return TILEP_MONS_UFETUBUS;
    case MONS_IRON_IMP:
        return TILEP_MONS_IRON_IMP;
    case MONS_MIDGE:
        return TILEP_MONS_MIDGE;
    case MONS_SHADOW_IMP:
        return TILEP_MONS_SHADOW_IMP;

    // '4' demons
    case MONS_RED_DEVIL:
        return TILEP_MONS_RED_DEVIL;
    case MONS_HAIRY_DEVIL:
        return TILEP_MONS_HAIRY_DEVIL;
    case MONS_ROTTING_DEVIL:
        return TILEP_MONS_ROTTING_DEVIL;
    case MONS_SMOKE_DEMON:
        return TILEP_MONS_SMOKE_DEMON;
    case MONS_SIXFIRHY:
        return TILEP_MONS_SIXFIRHY;
    case MONS_HELLWING:
        return TILEP_MONS_HELLWING;

    // '3' demons
    case MONS_HELLION:
        return TILEP_MONS_HELLION;
    case MONS_TORMENTOR:
        return TILEP_MONS_TORMENTOR;
    case MONS_BLUE_DEVIL:
        return TILEP_MONS_BLUE_DEVIL;
    case MONS_IRON_DEVIL:
        return TILEP_MONS_IRON_DEVIL;
    case MONS_NEQOXEC:
        return TILEP_MONS_NEQOXEC;
    case MONS_ORANGE_DEMON:
        return TILEP_MONS_ORANGE_DEMON;
    case MONS_YNOXINUL:
        return TILEP_MONS_YNOXINUL;
    case MONS_SHADOW_DEMON:
        return TILEP_MONS_SHADOW_DEMON;
    case MONS_CHAOS_SPAWN:
        return TILEP_MONS_CHAOS_SPAWN;

    // '2' demon
    case MONS_BEAST:
        return TILEP_MONS_BEAST;
    case MONS_SUN_DEMON:
        return TILEP_MONS_SUN_DEMON;
    case MONS_REAPER:
        return TILEP_MONS_REAPER;
    case MONS_SOUL_EATER:
        return TILEP_MONS_SOUL_EATER;
    case MONS_ICE_DEVIL:
        return TILEP_MONS_ICE_DEVIL;
    case MONS_LOROCYPROCA:
        return TILEP_MONS_LOROCYPROCA;

    // '1' demons
    case MONS_FIEND:
        return TILEP_MONS_FIEND;
    case MONS_ICE_FIEND:
        return TILEP_MONS_ICE_FIEND;
    case MONS_SHADOW_FIEND:
        return TILEP_MONS_SHADOW_FIEND;
    case MONS_PIT_FIEND:
        return TILEP_MONS_PIT_FIEND;
    case MONS_EXECUTIONER:
        return TILEP_MONS_EXECUTIONER;
    case MONS_GREEN_DEATH:
        return TILEP_MONS_GREEN_DEATH;
    case MONS_BLUE_DEATH:
        return TILEP_MONS_BLUE_DEATH;
    case MONS_BALRUG:
        return TILEP_MONS_BALRUG;
    case MONS_CACODEMON:
        return TILEP_MONS_CACODEMON;

    // non-living creatures
    // golems ('8')
    case MONS_CLAY_GOLEM:
        return TILEP_MONS_CLAY_GOLEM;
    case MONS_WOOD_GOLEM:
        return TILEP_MONS_WOOD_GOLEM;
    case MONS_IRON_GOLEM:
        return TILEP_MONS_IRON_GOLEM;
    case MONS_STONE_GOLEM:
        return TILEP_MONS_STONE_GOLEM;
    case MONS_CRYSTAL_GOLEM:
        return TILEP_MONS_CRYSTAL_GOLEM;
    case MONS_TOENAIL_GOLEM:
        return TILEP_MONS_TOENAIL_GOLEM;
    case MONS_ELECTRIC_GOLEM:
        return TILEP_MONS_ELECTRIC_GOLEM;

    // statues (also '8')
    case MONS_ICE_STATUE:
        return TILEP_MONS_ICE_STATUE;
    case MONS_SILVER_STATUE:
        return TILEP_MONS_SILVER_STATUE;
    case MONS_ORANGE_STATUE:
        return TILEP_MONS_ORANGE_STATUE;

    // gargoyles ('9')
    case MONS_GARGOYLE:
        return TILEP_MONS_GARGOYLE;
    case MONS_METAL_GARGOYLE:
        return TILEP_MONS_METAL_GARGOYLE;
    case MONS_MOLTEN_GARGOYLE:
        return TILEP_MONS_MOLTEN_GARGOYLE;

    // major demons ('&')
    case MONS_PANDEMONIUM_DEMON:
        return TILEP_MONS_PANDEMONIUM_DEMON;

    // ball lightning / orb of fire ('*')
    case MONS_BALL_LIGHTNING:
        return TILEP_MONS_BALL_LIGHTNING;
    case MONS_ORB_OF_FIRE:
        return TILEP_MONS_ORB_OF_FIRE;
    case MONS_ORB_OF_DESTRUCTION:
        return _mon_random(TILEP_MONS_ORB_OF_DESTRUCTION);
    case MONS_BLESSED_TOE:
        return TILEP_MONS_BLESSED_TOE;

    // other symbols
    case MONS_VAPOUR:
        return TILEP_MONS_VAPOUR;
    case MONS_SHADOW:
        return TILEP_MONS_SHADOW;
    case MONS_DEATH_COB:
        return TILEP_MONS_DEATH_COB;

    // -------------------------------------
    // non-human uniques, sorted by glyph, then difficulty
    // -------------------------------------

    // centaur ('c')
    case MONS_NESSOS:
        return TILEP_MONS_NESSOS;

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

        return (TILEP_MONS_TIAMAT + offset);
    }

    // elves ('e')
    case MONS_DOWAN:
        return TILEP_MONS_DOWAN;
    case MONS_DUVESSA:
        return TILEP_MONS_DUVESSA;

    // goblins and gnolls ('g')
    case MONS_IJYB:
        return TILEP_MONS_IJYB;
    case MONS_CRAZY_YIUF:
        return TILEP_MONS_CRAZY_YIUF;
    case MONS_GRUM:
        return TILEP_MONS_GRUM;

    // spriggans ('i')
    case MONS_SPRIGGAN:
        return TILEP_MONS_SPRIGGAN;
    case MONS_SPRIGGAN_RIDER:
        return TILEP_MONS_SPRIGGAN_RIDER;
    case MONS_SPRIGGAN_DRUID:
        return TILEP_MONS_SPRIGGAN_DRUID;
    case MONS_SPRIGGAN_BERSERKER:
        return TILEP_MONS_SPRIGGAN_BERSERKER;
    case MONS_SPRIGGAN_DEFENDER:
        return TILEP_MONS_SPRIGGAN_DEFENDER;
    case MONS_SPRIGGAN_ASSASSIN:
        return TILEP_MONS_SPRIGGAN_ASSASSIN;
    case MONS_THE_ENCHANTRESS:
        return TILEP_MONS_THE_ENCHANTRESS;
    case MONS_AGNES:
        return TILEP_MONS_AGNES;

    // slug ('j')
    case MONS_GASTRONOK:
        return TILEP_MONS_GASTRONOK;

    // merfolk ('m')
    case MONS_ILSUIW:
        if (in_water)
            return TILEP_MONS_ILSUIW_WATER;
        else
            return TILEP_MONS_ILSUIW;

    // orcs ('o')
    case MONS_BLORK_THE_ORC:
        return TILEP_MONS_BLORK_THE_ORC;
    case MONS_URUG:
        return TILEP_MONS_URUG;
    case MONS_NERGALLE:
        return TILEP_MONS_NERGALLE;
    case MONS_SAINT_ROKA:
        return TILEP_MONS_SAINT_ROKA;

    // dwarves ('q')
    case MONS_DWARF:
        return TILEP_MONS_DWARF;
    case MONS_GNOME:
        return TILEP_MONS_GNOME;

    // curse skull ('z')
    case MONS_MURRAY:
        return TILEP_MONS_MURRAY;

    // cyclopes and giants ('C')
    case MONS_POLYPHEMUS:
        return TILEP_MONS_POLYPHEMUS;
    case MONS_CHUCK:
        return TILEP_MONS_CHUCK;
    case MONS_ANTAEUS:
        return TILEP_MONS_ANTAEUS;
    // TODO
    case MONS_IRON_GIANT:
        return TILEP_MONS_STONE_GIANT;

    // dragons and hydras ('D')
    case MONS_LERNAEAN_HYDRA:
        return TILEP_MONS_LERNAEAN_HYDRA;
    case MONS_XTAHUA:
        return TILEP_MONS_XTAHUA;

    // efreet ('E')
    case MONS_AZRAEL:
        return TILEP_MONS_AZRAEL;

    // frog ('F')
    case MONS_PRINCE_RIBBIT:
        return TILEP_MONS_PRINCE_RIBBIT;

    // jelly ('J')
    case MONS_DISSOLUTION:
        return TILEP_MONS_DISSOLUTION;

    // kobolds ('K')
    case MONS_SONJA:
        return TILEP_MONS_SONJA;
    case MONS_PIKEL:
        return TILEP_MONS_PIKEL;

    // lich ('L')
    case MONS_BORIS:
        return TILEP_MONS_BORIS;

    // mummies ('M')
    case MONS_MENKAURE:
        return TILEP_MONS_MENKAURE;
    case MONS_KHUFU:
        return TILEP_MONS_KHUFU;

    // guardian serpent ('N')
    case MONS_AIZUL:
        return TILEP_MONS_AIZUL;

    // ogre ('O')
    case MONS_EROLCHA:
        return TILEP_MONS_EROLCHA;

    // rakshasas ('R')
    case MONS_MARA:
        return TILEP_MONS_MARA;
    case MONS_MARA_FAKE:
        return TILEP_MONS_MARA_FAKE;

    // trolls ('T')
    case MONS_PURGY:
        return TILEP_MONS_PURGY;
    case MONS_SNORG:
        return TILEP_MONS_SNORG;

    // elephants etc
    case MONS_NELLIE:
        return TILEP_MONS_NELLIE;

    // imps ('5')
    case MONS_GRINDER:
        return TILEP_MONS_GRINDER;

    // statue ('8')
    case MONS_ROXANNE:
        return TILEP_MONS_ROXANNE;

    // -------------------------------------
    // non-human uniques ('@')
    // -------------------------------------

    case MONS_TERENCE:
        return TILEP_MONS_TERENCE;
    case MONS_JESSICA:
        return TILEP_MONS_JESSICA;
    case MONS_SIGMUND:
        return TILEP_MONS_SIGMUND;
    case MONS_EDMUND:
        return TILEP_MONS_EDMUND;
    case MONS_PSYCHE:
        return TILEP_MONS_PSYCHE;
    case MONS_DONALD:
        return TILEP_MONS_DONALD;
    case MONS_JOSEPH:
        return TILEP_MONS_JOSEPH;
    case MONS_ERICA:
        return TILEP_MONS_ERICA;
    case MONS_JOSEPHINE:
        return TILEP_MONS_JOSEPHINE;
    case MONS_HAROLD:
        return TILEP_MONS_HAROLD;
    case MONS_JOZEF:
        return TILEP_MONS_JOZEF;
    case MONS_MAUD:
        return TILEP_MONS_MAUD;
    case MONS_LOUISE:
        return TILEP_MONS_LOUISE;
    case MONS_FRANCES:
        return TILEP_MONS_FRANCES;
    case MONS_RUPERT:
        return TILEP_MONS_RUPERT;
    case MONS_WIGLAF:
        return TILEP_MONS_WIGLAF;
    case MONS_NORRIS:
        return TILEP_MONS_NORRIS;
    case MONS_FREDERICK:
        return TILEP_MONS_FREDERICK;
    case MONS_MARGERY:
        return TILEP_MONS_MARGERY;
    case MONS_EUSTACHIO:
        return TILEP_MONS_EUSTACHIO;
    case MONS_KIRKE:
        return TILEP_MONS_KIRKE;
    case MONS_NIKOLA:
        return TILEP_MONS_NIKOLA;
    case MONS_MAURICE:
        return TILEP_MONS_MAURICE;

    // unique major demons ('&')
    case MONS_MNOLEG:
        return TILEP_MONS_MNOLEG;
    case MONS_LOM_LOBON:
        return TILEP_MONS_LOM_LOBON;
    case MONS_CEREBOV:
        return TILEP_MONS_CEREBOV;
    case MONS_GLOORX_VLOQ:
        return TILEP_MONS_GLOORX_VLOQ;
    case MONS_GERYON:
        return TILEP_MONS_GERYON;
    case MONS_DISPATER:
        return TILEP_MONS_DISPATER;
    case MONS_ASMODEUS:
        return TILEP_MONS_ASMODEUS;
    case MONS_ERESHKIGAL:
        return TILEP_MONS_ERESHKIGAL;
    }

    return TILEP_MONS_PROGRAM_BUG;
}

static tileidx_t _tileidx_tentacle(const monster *mon)
{
	ASSERT(mon->type == MONS_KRAKEN_TENTACLE
		   || mon->type == MONS_KRAKEN_TENTACLE_SEGMENT);
	// No point to drawing submerged monsters.
	ASSERT(!mon->submerged());

	// Get the parent tentacle.
	ASSERT(mon->props.exists("inwards"));
	const int h_idx = mon->props["inwards"].get_int();
	ASSERT(!invalid_monster_index(h_idx));
	const monster head = menv[h_idx];

    // Get head and tentacle positions.
	const coord_def t_pos = mon->pos();  // tentacle position
	const coord_def h_pos = head.pos();  // head position
	ASSERT(adjacent(t_pos, h_pos));

	const bool head_in_water =
					(head.type == MONS_KRAKEN || head.submerged());

	// Tentacle end only requires checking of head position.
	if (mon->type == MONS_KRAKEN_TENTACLE)
	{
		if (head_in_water)
			return _mon_random(TILEP_MONS_KRAKEN_TENTACLE_WATER);

		ASSERT(head.type == MONS_KRAKEN_TENTACLE_SEGMENT);

		// Different handling according to relative positions.
		if (h_pos.x == t_pos.x)
		{
			if (h_pos.y < t_pos.y)
				return TILEP_MONS_KRAKEN_TENTACLE_N;
			else
				return TILEP_MONS_KRAKEN_TENTACLE_S;
		}
		else if (h_pos.y == t_pos.y)
		{
			if (h_pos.x < t_pos.x)
				return TILEP_MONS_KRAKEN_TENTACLE_W;
			else
				return TILEP_MONS_KRAKEN_TENTACLE_E;
		}
		else if (h_pos.x < t_pos.x)
		{
			if (h_pos.y < t_pos.y)
				return TILEP_MONS_KRAKEN_TENTACLE_NW;
			else
				return TILEP_MONS_KRAKEN_TENTACLE_SW;
		}
		else if (h_pos.x > t_pos.x)
		{
			if (h_pos.y < t_pos.y)
				return TILEP_MONS_KRAKEN_TENTACLE_NE;
			else
				return TILEP_MONS_KRAKEN_TENTACLE_SE;
		}
		ASSERT(false);
	}

	// Only tentacle segments from now on.
	ASSERT(mon->type == MONS_KRAKEN_TENTACLE_SEGMENT);

	// For segments, we also need the next segment (or end piece).
	ASSERT(mon->props.exists("outwards"));
	const int n_idx = mon->props["outwards"].get_int();
	ASSERT(!invalid_monster_index(n_idx));
	const monster next = menv[n_idx];

	const coord_def n_pos = next.pos();  // next position
	if (head_in_water && next.submerged())
	{
		// Both head and next are submerged.
		return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_WATER;
	}

	if (head_in_water || next.submerged())
	{
		// One segment end goes into water, the other
		// into the direction of head or next.
		const coord_def s_pos = (head_in_water ? n_pos : h_pos);

		if (s_pos.x == t_pos.x)
		{
			if (s_pos.y < t_pos.y)
				return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N;
			else
				return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S;
		}
		else if (s_pos.y == t_pos.y)
		{
			if (s_pos.x < t_pos.x)
				return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W;
			else
				return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E;
		}
		else if (s_pos.x < t_pos.x)
		{
			if (s_pos.y < t_pos.y)
				return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW;
			else
				return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SW;
		}
		else if (s_pos.x > t_pos.x)
		{
			if (s_pos.y < t_pos.y)
				return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE;
			else
				return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE;
		}
		ASSERT(false);
	}

	// Okay, neither head nor next are submerged.
	// Compare all three positions.

    return TILEP_MONS_PROGRAM_BUG;
}

static tileidx_t _tileidx_monster_no_props(const monster* mon)
{
    bool in_water = feat_is_water(grd(mon->pos()));
    const bool misled = (!crawl_state.game_is_arena() && you.misled());

    // Show only base class for detected monsters.
    if (!misled && mons_is_zombified(mon))
        return _tileidx_monster_zombified(mon);
    else if (mon->props.exists("monster_tile"))
    {
        tileidx_t t = mon->props["monster_tile"].get_short();
        if (t == TILEP_MONS_STATUE_GUARDIAN)
            return _mon_random(t);
        else
            return t;
    }
    else
    {
        int tile_num = 0;
        if (mon->props.exists("tile_num"))
            tile_num = mon->props["tile_num"].get_short();

        int type = mon->type;
        if (misled)
            type = mon->get_mislead_type();

        switch (type)
        {
        case MONS_CENTAUR:
            return TILEP_MONS_CENTAUR + _bow_offset(mon);
        case MONS_CENTAUR_WARRIOR:
            return TILEP_MONS_CENTAUR_WARRIOR + _bow_offset(mon);
        case MONS_YAKTAUR:
            return TILEP_MONS_YAKTAUR + _bow_offset(mon);
        case MONS_YAKTAUR_CAPTAIN:
            return TILEP_MONS_YAKTAUR_CAPTAIN + _bow_offset(mon);
        case MONS_BUSH:
            if (cloud_type_at(mon->pos()) == CLOUD_FIRE)
                return TILEP_MONS_BUSH_BURNING;
            else
                return TILEP_MONS_BUSH;
        case MONS_BALLISTOMYCETE:
            if (mon->has_ench(ENCH_SPORE_PRODUCTION))
                return TILEP_MONS_BALLISTOMYCETE_ACTIVE;
            else
                return TILEP_MONS_BALLISTOMYCETE_INACTIVE;
            break;
        case MONS_HYPERACTIVE_BALLISTOMYCETE:
            return TILEP_MONS_HYPERACTIVE_BALLISTOMYCETE;

        case MONS_GOLD_MIMIC:
        case MONS_WEAPON_MIMIC:
        case MONS_ARMOUR_MIMIC:
        case MONS_SCROLL_MIMIC:
        case MONS_POTION_MIMIC:
            {
                tileidx_t t = tileidx_item(get_mimic_item(mon));
                if (mons_is_known_mimic(mon))
                    t |= TILE_FLAG_MIMIC;
                return t;
            }

        // Feature mimics get drawn with the dungeon, see tileidx_feature.
        case MONS_SHOP_MIMIC:
        case MONS_TRAP_MIMIC:
        case MONS_PORTAL_MIMIC:
        case MONS_DOOR_MIMIC:
        case MONS_STAIR_MIMIC:
        case MONS_FOUNTAIN_MIMIC:
            if (mons_is_known_mimic(mon))
                return TILE_FLAG_MIMIC;
            return 0;

        case MONS_DANCING_WEAPON:
            {
                // Use item tile.
                item_def item = mitm[mon->inv[MSLOT_WEAPON]];
                return tileidx_item(item) | TILE_FLAG_ANIM_WEP;
            }

		case MONS_KRAKEN_TENTACLE:
		case MONS_KRAKEN_TENTACLE_SEGMENT:
			return _tileidx_tentacle(mon);

        default:
            return _tileidx_monster_base(type, in_water, mon->colour,
                                           mon->number, tile_num);
        }
    }
}

tileidx_t tileidx_monster(const monster* mons)
{
    tileidx_t ch = _tileidx_monster_no_props(mons);

    if (mons_flies(mons))
        ch |= TILE_FLAG_FLYING;
    if (mons->has_ench(ENCH_HELD))
        ch |= TILE_FLAG_NET;
    if (mons->has_ench(ENCH_POISON))
        ch |= TILE_FLAG_POISON;
    if (mons->has_ench(ENCH_STICKY_FLAME))
        ch |= TILE_FLAG_FLAME;
    if (mons->berserk())
        ch |= TILE_FLAG_BERSERK;

    if (mons->friendly())
        ch |= TILE_FLAG_PET;
    else if (mons->good_neutral())
        ch |= TILE_FLAG_GD_NEUTRAL;
    else if (mons->neutral())
        ch |= TILE_FLAG_NEUTRAL;
    else if (mons_looks_stabbable(mons))
        ch |= TILE_FLAG_STAB;
    else if (mons_looks_distracted(mons))
        ch |= TILE_FLAG_MAY_STAB;

    mon_dam_level_type damage_level = mons_get_damage_level(mons);

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

    if (Options.tile_show_demon_tier)
    {
        switch (mons_base_char(mons->type))
        {
        case '1':
            ch |= TILE_FLAG_DEMON_1;
            break;
        case '2':
            ch |= TILE_FLAG_DEMON_2;
            break;
        case '3':
            ch |= TILE_FLAG_DEMON_3;
            break;
        case '4':
            ch |= TILE_FLAG_DEMON_4;
            break;
        case '5':
            ch |= TILE_FLAG_DEMON_5;
            break;
        }
    }

    return ch;
}

tileidx_t tileidx_draco_base(const monster* mon)
{
    int draco = draco_subspecies(mon);
    int colour = 0;

    switch (draco)
    {
    default:
    case MONS_DRACONIAN:        colour = 0; break;
    case MONS_BLACK_DRACONIAN:  colour = 1; break;
    case MONS_YELLOW_DRACONIAN: colour = 2; break;
    case MONS_GREEN_DRACONIAN:  colour = 3; break;
    case MONS_GREY_DRACONIAN:   colour = 4; break;
    case MONS_MOTTLED_DRACONIAN:colour = 5; break;
    case MONS_PALE_DRACONIAN:   colour = 6; break;
    case MONS_PURPLE_DRACONIAN: colour = 7; break;
    case MONS_RED_DRACONIAN:    colour = 8; break;
    case MONS_WHITE_DRACONIAN:  colour = 9; break;
    }

    return (TILEP_DRACO_BASE + colour);
}

tileidx_t tileidx_draco_job(const monster* mon)
{

    switch (mon->type)
    {
        case MONS_DRACONIAN_CALLER:      return (TILEP_DRACO_CALLER);
        case MONS_DRACONIAN_MONK:        return (TILEP_DRACO_MONK);
        case MONS_DRACONIAN_ZEALOT:      return (TILEP_DRACO_ZEALOT);
        case MONS_DRACONIAN_SHIFTER:     return (TILEP_DRACO_SHIFTER);
        case MONS_DRACONIAN_ANNIHILATOR: return (TILEP_DRACO_ANNIHILATOR);
        case MONS_DRACONIAN_KNIGHT:      return (TILEP_DRACO_KNIGHT);
        case MONS_DRACONIAN_SCORCHER:    return (TILEP_DRACO_SCORCHER);
        default:                         return (0);
    }
}

static tileidx_t _tileidx_unrand_artefact(int idx)
{
    const tileidx_t tile = unrandart_to_tile(idx);
    return (tile ? tile : TILE_TODO);
}

static tileidx_t _tileidx_weapon_base(const item_def &item)
{
    int race  = item.flags & ISFLAG_RACIAL_MASK;

    switch (item.sub_type)
    {
    case WPN_KNIFE:
        return TILE_WPN_KNIFE;

    case WPN_DAGGER:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_DAGGER_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_DAGGER_ELF;
        return TILE_WPN_DAGGER;

    case WPN_SHORT_SWORD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_SHORT_SWORD_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_SHORT_SWORD_ELF;
        return TILE_WPN_SHORT_SWORD;

    case WPN_QUICK_BLADE:
        return TILE_WPN_QUICK_BLADE;

    case WPN_SABRE:
        return TILE_WPN_SABRE;

    case WPN_FALCHION:
        return TILE_WPN_FALCHION;

    case WPN_KATANA:
        return TILE_WPN_KATANA;

    case WPN_LONG_SWORD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_LONG_SWORD_ORC;
        return TILE_WPN_LONG_SWORD;

    case WPN_GREAT_SWORD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_GREAT_SWORD_ORC;
        return TILE_WPN_GREAT_SWORD;

    case WPN_SCIMITAR:
        return TILE_WPN_SCIMITAR;

    case WPN_DOUBLE_SWORD:
        return TILE_WPN_DOUBLE_SWORD;

    case WPN_TRIPLE_SWORD:
        return TILE_WPN_TRIPLE_SWORD;

    case WPN_HAND_AXE:
        return TILE_WPN_HAND_AXE;

    case WPN_WAR_AXE:
        return TILE_WPN_WAR_AXE;

    case WPN_BROAD_AXE:
        return TILE_WPN_BROAD_AXE;

    case WPN_BATTLEAXE:
        return TILE_WPN_BATTLEAXE;

    case WPN_EXECUTIONERS_AXE:
        return TILE_WPN_EXECUTIONERS_AXE;

    case WPN_BLOWGUN:
        return TILE_WPN_BLOWGUN;

    case WPN_SLING:
        return TILE_WPN_SLING;

    case WPN_BOW:
        return TILE_WPN_BOW;

    case WPN_CROSSBOW:
        return TILE_WPN_CROSSBOW;

    case WPN_SPEAR:
        return TILE_WPN_SPEAR;

    case WPN_TRIDENT:
        return TILE_WPN_TRIDENT;

    case WPN_HALBERD:
        return TILE_WPN_HALBERD;

    case WPN_SCYTHE:
        return TILE_WPN_SCYTHE;

    case WPN_GLAIVE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_GLAIVE_ORC;
        return TILE_WPN_GLAIVE;

    case WPN_QUARTERSTAFF:
        return TILE_WPN_QUARTERSTAFF;

    case WPN_CLUB:
        return TILE_WPN_CLUB;

    case WPN_HAMMER:
        return TILE_WPN_HAMMER;

    case WPN_MACE:
        return TILE_WPN_MACE;

    case WPN_FLAIL:
        return TILE_WPN_FLAIL;

    case WPN_SPIKED_FLAIL:
        return TILE_WPN_SPIKED_FLAIL;

    case WPN_GREAT_MACE:
        return TILE_WPN_GREAT_MACE;

    case WPN_DIRE_FLAIL:
        return TILE_WPN_GREAT_FLAIL;

    case WPN_MORNINGSTAR:
        return TILE_WPN_MORNINGSTAR;

    case WPN_EVENINGSTAR:
        return TILE_WPN_EVENINGSTAR;

    case WPN_GIANT_CLUB:
        return TILE_WPN_GIANT_CLUB;

    case WPN_GIANT_SPIKED_CLUB:
        return TILE_WPN_GIANT_SPIKED_CLUB;

    case WPN_ANKUS:
        return TILE_WPN_ANKUS;

    case WPN_WHIP:
        return TILE_WPN_WHIP;

    case WPN_DEMON_BLADE:
        return TILE_WPN_DEMON_BLADE;

    case WPN_EUDEMON_BLADE:
        return TILE_WPN_BLESSED_BLADE;

    case WPN_DEMON_WHIP:
        return TILE_WPN_DEMON_WHIP;

    case WPN_SACRED_SCOURGE:
        return TILE_WPN_SACRED_SCOURGE;

    case WPN_DEMON_TRIDENT:
        return TILE_WPN_DEMON_TRIDENT;

    case WPN_TRISHULA:
        return TILE_WPN_TRISHULA;

    case WPN_LONGBOW:
        return TILE_WPN_LONGBOW;

    case WPN_LAJATANG:
        return TILE_WPN_LAJATANG;

    case WPN_BARDICHE:
        return TILE_WPN_BARDICHE;

    case WPN_BLESSED_FALCHION:
        return TILE_WPN_FALCHION;

    case WPN_BLESSED_LONG_SWORD:
        return TILE_WPN_LONG_SWORD;

    case WPN_BLESSED_SCIMITAR:
        return TILE_WPN_SCIMITAR;

    case WPN_BLESSED_KATANA:
        return TILE_WPN_KATANA;

    case WPN_BLESSED_GREAT_SWORD:
        return TILE_WPN_GREAT_SWORD;

    case WPN_BLESSED_DOUBLE_SWORD:
        return TILE_WPN_DOUBLE_SWORD;

    case WPN_BLESSED_TRIPLE_SWORD:
        return TILE_WPN_TRIPLE_SWORD;
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
    int brand = item.special;
    switch (item.sub_type)
    {
    case MI_STONE:        return TILE_MI_STONE;
    case MI_LARGE_ROCK:   return TILE_MI_LARGE_ROCK;
    case MI_THROWING_NET: return TILE_MI_THROWING_NET;


    case MI_DART:
        switch (brand)
        {
        default:             return TILE_MI_DART;
        case SPMSL_POISONED: return TILE_MI_DART_POISONED;
        case SPMSL_STEEL:    return TILE_MI_DART_STEEL;
        case SPMSL_SILVER:   return TILE_MI_DART_SILVER;
        }

    case MI_NEEDLE:
        if (brand == SPMSL_POISONED)
            return TILE_MI_NEEDLE_P;
        return TILE_MI_NEEDLE;

    case MI_ARROW:
        switch (brand)
        {
        default:             return TILE_MI_ARROW;
        case SPMSL_STEEL:    return TILE_MI_ARROW_STEEL;
        case SPMSL_SILVER:   return TILE_MI_ARROW_SILVER;
        }

    case MI_BOLT:
        switch (brand)
        {
        default:             return TILE_MI_BOLT;
        case SPMSL_STEEL:    return TILE_MI_BOLT_STEEL;
        case SPMSL_SILVER:   return TILE_MI_BOLT_SILVER;
        }

    case MI_SLING_BULLET:
        switch (brand)
        {
        default:             return TILE_MI_SLING_BULLET;
        case SPMSL_STEEL:    return TILE_MI_SLING_BULLET_STEEL;
        case SPMSL_SILVER:   return TILE_MI_SLING_BULLET_SILVER;
        }

    case MI_JAVELIN:
        switch (brand)
        {
        default:             return TILE_MI_JAVELIN;
        case SPMSL_STEEL:    return TILE_MI_JAVELIN_STEEL;
        case SPMSL_SILVER:   return TILE_MI_JAVELIN_SILVER;
        }
  }

  return TILE_ERROR;
}

static tileidx_t _tileidx_missile(const item_def &item)
{
    int tile = _tileidx_missile_base(item);
    return (tileidx_enchant_equ(item, tile));
}

static tileidx_t _tileidx_armour_base(const item_def &item)
{
    int race  = item.flags & ISFLAG_RACIAL_MASK;
    int type  = item.sub_type;
    switch (type)
    {
    case ARM_ROBE:
        return TILE_ARM_ROBE;

    case ARM_LEATHER_ARMOUR:
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_LEATHER_ARMOUR_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_LEATHER_ARMOUR_ELF;
        return TILE_ARM_LEATHER_ARMOUR;

    case ARM_RING_MAIL:
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_RING_MAIL_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_RING_MAIL_ELF;
        if (race == ISFLAG_DWARVEN)
            return TILE_ARM_RING_MAIL_DWA;
        return TILE_ARM_RING_MAIL;

    case ARM_SCALE_MAIL:
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_SCALE_MAIL_ELF;
        return TILE_ARM_SCALE_MAIL;

    case ARM_CHAIN_MAIL:
        if (race == ISFLAG_ELVEN)
            return TILE_ARM_CHAIN_MAIL_ELF;
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_CHAIN_MAIL_ORC;
        return TILE_ARM_CHAIN_MAIL;

    case ARM_SPLINT_MAIL:
        return TILE_ARM_SPLINT_MAIL;

    case ARM_BANDED_MAIL:
        return TILE_ARM_BANDED_MAIL;

    case ARM_PLATE_MAIL:
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_PLATE_MAIL_ORC;
        return TILE_ARM_PLATE_MAIL;

    case ARM_CRYSTAL_PLATE_MAIL:
        return TILE_ARM_CRYSTAL_PLATE_MAIL;

    case ARM_SHIELD:
        return TILE_ARM_SHIELD;

    case ARM_CLOAK:
        return TILE_ARM_CLOAK;

    case ARM_WIZARD_HAT:
        return TILE_THELM_WIZARD_HAT;

    case ARM_CAP:
        return TILE_THELM_CAP;

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

    case ARM_DRAGON_HIDE:
        return TILE_ARM_DRAGON_HIDE;

    case ARM_DRAGON_ARMOUR:
        return TILE_ARM_DRAGON_ARMOUR;

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

    case ARM_STORM_DRAGON_HIDE:
        return TILE_ARM_STORM_DRAGON_HIDE;

    case ARM_STORM_DRAGON_ARMOUR:
        return TILE_ARM_STORM_DRAGON_ARMOUR;

    case ARM_GOLD_DRAGON_HIDE:
        return TILE_ARM_GOLD_DRAGON_HIDE;

    case ARM_GOLD_DRAGON_ARMOUR:
        return TILE_ARM_GOLD_DRAGON_ARMOUR;

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
    if (food_is_rotten(item))
    {
        if (!is_inedible(item))
        {
            if (is_poisonous(item))
                return TILE_FOOD_CHUNK_ROTTEN_POISONED;

            if (is_mutagenic(item))
                return TILE_FOOD_CHUNK_ROTTEN_MUTAGENIC;

            if (causes_rot(item))
                return TILE_FOOD_CHUNK_ROTTEN_ROTTING;

            if (is_forbidden_food(item))
                return TILE_FOOD_CHUNK_ROTTEN_FORBIDDEN;

            if (is_contaminated(item))
                return TILE_FOOD_CHUNK_ROTTEN_CONTAMINATED;
        }
        return TILE_FOOD_CHUNK_ROTTEN;
    }

    if (is_inedible(item))
        return TILE_FOOD_CHUNK;

    if (is_poisonous(item))
        return TILE_FOOD_CHUNK_POISONED;

    if (is_mutagenic(item))
        return TILE_FOOD_CHUNK_MUTAGENIC;

    if (causes_rot(item))
        return TILE_FOOD_CHUNK_ROTTING;

    if (is_forbidden_food(item))
        return TILE_FOOD_CHUNK_FORBIDDEN;

    if (is_contaminated(item))
        return TILE_FOOD_CHUNK_CONTAMINATED;

    return TILE_FOOD_CHUNK;
}

static tileidx_t _tileidx_food(const item_def &item)
{
    switch (item.sub_type)
    {
    case FOOD_MEAT_RATION:  return TILE_FOOD_MEAT_RATION;
    case FOOD_BREAD_RATION: return TILE_FOOD_BREAD_RATION;
    case FOOD_PEAR:         return TILE_FOOD_PEAR;
    case FOOD_APPLE:        return TILE_FOOD_APPLE;
    case FOOD_CHOKO:        return TILE_FOOD_CHOKO;
    case FOOD_HONEYCOMB:    return TILE_FOOD_HONEYCOMB;
    case FOOD_ROYAL_JELLY:  return TILE_FOOD_ROYAL_JELLY;
    case FOOD_SNOZZCUMBER:  return TILE_FOOD_SNOZZCUMBER;
    case FOOD_PIZZA:        return TILE_FOOD_PIZZA;
    case FOOD_APRICOT:      return TILE_FOOD_APRICOT;
    case FOOD_ORANGE:       return TILE_FOOD_ORANGE;
    case FOOD_BANANA:       return TILE_FOOD_BANANA;
    case FOOD_STRAWBERRY:   return TILE_FOOD_STRAWBERRY;
    case FOOD_RAMBUTAN:     return TILE_FOOD_RAMBUTAN;
    case FOOD_LEMON:        return TILE_FOOD_LEMON;
    case FOOD_GRAPE:        return TILE_FOOD_GRAPE;
    case FOOD_SULTANA:      return TILE_FOOD_SULTANA;
    case FOOD_LYCHEE:       return TILE_FOOD_LYCHEE;
    case FOOD_BEEF_JERKY:   return TILE_FOOD_BEEF_JERKY;
    case FOOD_CHEESE:       return TILE_FOOD_CHEESE;
    case FOOD_SAUSAGE:      return TILE_FOOD_SAUSAGE;
    case FOOD_AMBROSIA:     return TILE_FOOD_AMBROSIA;
    case FOOD_CHUNK:        return _tileidx_chunk(item);
    }

    return TILE_ERROR;
}


// Returns index of corpse tiles.
// Parameter item holds the corpse.
static tileidx_t _tileidx_corpse(const item_def &item)
{
    const int type = item.plus;

    switch (type)
    {
    // insects ('a')
    case MONS_GIANT_COCKROACH:
        return TILE_CORPSE_GIANT_COCKROACH;
    case MONS_GIANT_ANT:
        return TILE_CORPSE_GIANT_ANT;
    case MONS_SOLDIER_ANT:
        return TILE_CORPSE_SOLDIER_ANT;
    case MONS_QUEEN_ANT:
        return TILE_CORPSE_QUEEN_ANT;

    // batty monsters ('b')
    case MONS_GIANT_BAT:
        return TILE_CORPSE_GIANT_BAT;
    case MONS_BUTTERFLY:
        return TILE_CORPSE_BUTTERFLY;

    // centaurs ('c')
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
        return TILE_CORPSE_CENTAUR;
    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        return TILE_CORPSE_YAKTAUR;

    // draconians ('d')
    case MONS_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_BROWN;
    case MONS_BLACK_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_BLACK;
    case MONS_YELLOW_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_YELLOW;
    case MONS_PALE_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_PALE;
    case MONS_GREEN_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_GREEN;
    case MONS_PURPLE_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_PURPLE;
    case MONS_RED_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_RED;
    case MONS_WHITE_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_WHITE;
    case MONS_MOTTLED_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_MOTTLED;
    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_KNIGHT:
    case MONS_DRACONIAN_SCORCHER:
        return TILE_CORPSE_DRACONIAN_BROWN;

    // elves ('e')
    case MONS_ELF:
    case MONS_DEEP_ELF_SOLDIER:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_BLADEMASTER:
    case MONS_DEEP_ELF_MASTER_ARCHER:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_SUMMONER:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_DEEP_ELF_DEATH_MAGE:
        return TILE_CORPSE_ELF;

    // goblins ('g')
    case MONS_GOBLIN:
        return TILE_CORPSE_GOBLIN;
    case MONS_HOBGOBLIN:
        return TILE_CORPSE_HOBGOBLIN;
    case MONS_GNOLL:
        return TILE_CORPSE_GNOLL;

    // hounds and hogs ('h')
    case MONS_JACKAL:
        return TILE_CORPSE_JACKAL;
    case MONS_HOUND:
        return TILE_CORPSE_HOUND;
    case MONS_WARG:
        return TILE_CORPSE_WARG;
    case MONS_WOLF:
        return TILE_CORPSE_WOLF;
    case MONS_WAR_DOG:
        return TILE_CORPSE_WAR_DOG;
    case MONS_HOG:
        return TILE_CORPSE_HOG;
    case MONS_HELL_HOUND:
        return TILE_CORPSE_HELL_HOUND;
    case MONS_HELL_HOG:
        return TILE_CORPSE_HELL_HOG;
    case MONS_FELID:
        return TILE_CORPSE_FELID;

    // spriggans ('i')
    case MONS_SPRIGGAN:
        return TILE_CORPSE_SPRIGGAN;

    // slugs ('j')
    case MONS_ELEPHANT_SLUG:
        return TILE_CORPSE_ELEPHANT_SLUG;
    case MONS_GIANT_SLUG:
        return TILE_CORPSE_GIANT_SLUG;
    case MONS_AGATE_SNAIL:
        return TILE_CORPSE_AGATE_SNAIL;

    // bees ('k')
    case MONS_KILLER_BEE:
        return TILE_CORPSE_KILLER_BEE;
    case MONS_BUMBLEBEE:
        return TILE_CORPSE_BUMBLEBEE;
    case MONS_QUEEN_BEE:
        return TILE_CORPSE_QUEEN_BEE;
    case MONS_FIREFLY:
        return TILE_CORPSE_FIREFLY;

    // lizards ('l')
    case MONS_GIANT_NEWT:
        return TILE_CORPSE_GIANT_NEWT;
    case MONS_GIANT_GECKO:
        return TILE_CORPSE_GIANT_GECKO;
    case MONS_IGUANA:
        return TILE_CORPSE_IGUANA;
    case MONS_GILA_MONSTER:
        return TILE_CORPSE_GILA_MONSTER;
    case MONS_KOMODO_DRAGON:
        return TILE_CORPSE_KOMODO_DRAGON;

    // drakes (also 'l')
    case MONS_SWAMP_DRAKE:
        return TILE_CORPSE_SWAMP_DRAKE;
    case MONS_FIRE_DRAKE:
        return TILE_CORPSE_FIRE_DRAKE;
    case MONS_LINDWURM:
        return TILE_CORPSE_LINDWURM;
    case MONS_DEATH_DRAKE:
        return TILE_CORPSE_DEATH_DRAKE;

    // merfolk ('m')
    case MONS_MERFOLK:
        return TILE_CORPSE_MERFOLK;
    case MONS_MERMAID:
        return TILE_CORPSE_MERMAID;
    case MONS_SIREN:
        return TILE_CORPSE_SIREN;

    // rotting monsters ('n')
    case MONS_NECROPHAGE:
        return TILE_CORPSE_NECROPHAGE;
    case MONS_GHOUL:
        return TILE_CORPSE_GHOUL;
    case MONS_ROTTING_HULK:
        return TILE_CORPSE_ROTTING_HULK;

    // orcs ('o')
    case MONS_ORC:
    case MONS_ORC_WIZARD:
    case MONS_ORC_PRIEST:
    case MONS_ORC_WARRIOR:
    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARLORD:
    case MONS_ORC_SORCERER:
    case MONS_ORC_HIGH_PRIEST:
        return TILE_CORPSE_ORC;

    // dwarves ('q')
    case MONS_DWARF:
    case MONS_DEEP_DWARF:
        return TILE_CORPSE_DWARF;
    case MONS_GNOME:
        return TILE_CORPSE_GNOME;

    // rodents ('r')
    case MONS_RAT:
        return TILE_CORPSE_RAT;
    case MONS_QUOKKA:
        return TILE_CORPSE_QUOKKA;
    case MONS_GREY_RAT:
        return TILE_CORPSE_GREY_RAT;
    case MONS_GREEN_RAT:
        return TILE_CORPSE_GREEN_RAT;
    case MONS_ORANGE_RAT:
        return TILE_CORPSE_ORANGE_RAT;

    // spiders and insects ('s')
    case MONS_GIANT_MITE:
        return TILE_CORPSE_GIANT_MITE;
    case MONS_GIANT_CENTIPEDE:
        return TILE_CORPSE_GIANT_CENTIPEDE;
    case MONS_SCORPION:
        return TILE_CORPSE_SCORPION;
    case MONS_EMPEROR_SCORPION:
        return TILE_CORPSE_EMPEROR_SCORPION;
    case MONS_SPIDER:
        return TILE_CORPSE_SPIDER;
    case MONS_TARANTELLA:
        return TILE_CORPSE_TARANTELLA;
    case MONS_JUMPING_SPIDER:
        return TILE_CORPSE_JUMPING_SPIDER;
    case MONS_WOLF_SPIDER:
        return TILE_CORPSE_WOLF_SPIDER;
    case MONS_TRAPDOOR_SPIDER:
        return TILE_CORPSE_TRAPDOOR_SPIDER;
    case MONS_REDBACK:
        return TILE_CORPSE_REDBACK;
    case MONS_DEMONIC_CRAWLER:
        return TILE_CORPSE_DEMONIC_CRAWLER;

    // turtles and crocodiles ('t')
    case MONS_CROCODILE:
        return TILE_CORPSE_CROCODILE;
    case MONS_BABY_ALLIGATOR:
        return TILE_CORPSE_BABY_ALLIGATOR;
    case MONS_ALLIGATOR:
        return TILE_CORPSE_ALLIGATOR;
    case MONS_SNAPPING_TURTLE:
        return TILE_CORPSE_SNAPPING_TURTLE;
    case MONS_ALLIGATOR_SNAPPING_TURTLE:
        return TILE_CORPSE_ALLIGATOR_SNAPPING_TURTLE;

    // ugly things ('u')
    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
    {
        const tileidx_t ugly_corpse_tile = (type == MONS_VERY_UGLY_THING) ?
            TILE_CORPSE_VERY_UGLY_THING : TILE_CORPSE_UGLY_THING;
        int colour_offset = ugly_thing_colour_offset(item.colour);

        if (colour_offset == -1)
            colour_offset = 0;

        return (ugly_corpse_tile + colour_offset);
    }

    // worms ('w')
    case MONS_KILLER_BEE_LARVA:
        return TILE_CORPSE_KILLER_BEE_LARVA;
    case MONS_ANT_LARVA:
        return TILE_CORPSE_ANT_LARVA;
    case MONS_WORM:
        return TILE_CORPSE_WORM;
    case MONS_BRAIN_WORM:
        return TILE_CORPSE_BRAIN_WORM;
    case MONS_SWAMP_WORM:
        return TILE_CORPSE_SWAMP_WORM;
    case MONS_GIANT_LEECH:
        return TILE_CORPSE_GIANT_LEECH;
    case MONS_ROCK_WORM:
        return TILE_CORPSE_ROCK_WORM;
    case MONS_SPINY_WORM:
        return TILE_CORPSE_SPINY_WORM;

    // flying insects ('y')
    case MONS_GIANT_MOSQUITO:
        return TILE_CORPSE_GIANT_MOSQUITO;
    case MONS_GIANT_BLOWFLY:
        return TILE_CORPSE_GIANT_BLOWFLY;
    case MONS_YELLOW_WASP:
        return TILE_CORPSE_YELLOW_WASP;
    case MONS_RED_WASP:
        return TILE_CORPSE_RED_WASP;
    case MONS_GHOST_MOTH:
        return TILE_CORPSE_GHOST_MOTH;
    case MONS_MOTH_OF_WRATH:
        return TILE_CORPSE_MOTH_OF_WRATH;

    // beetles ('B')
    case MONS_GIANT_BEETLE:
        return TILE_CORPSE_GIANT_BEETLE;
    case MONS_BOULDER_BEETLE:
        return TILE_CORPSE_BOULDER_BEETLE;
    case MONS_BORING_BEETLE:
        return TILE_CORPSE_BORING_BEETLE;

    // giants ('C')
    case MONS_HILL_GIANT:
        return TILE_CORPSE_HILL_GIANT;
    case MONS_ETTIN:
        return TILE_CORPSE_ETTIN;
    case MONS_CYCLOPS:
        return TILE_CORPSE_CYCLOPS;
    case MONS_FIRE_GIANT:
        return TILE_CORPSE_FIRE_GIANT;
    case MONS_FROST_GIANT:
        return TILE_CORPSE_FROST_GIANT;
    case MONS_STONE_GIANT:
        return TILE_CORPSE_STONE_GIANT;
    case MONS_TITAN:
        return TILE_CORPSE_TITAN;

    // dragons ('D')
    case MONS_WYVERN:
        return TILE_CORPSE_WYVERN;
    case MONS_DRAGON:
        return TILE_CORPSE_DRAGON;
    case MONS_HYDRA:
        return TILE_CORPSE_HYDRA;
    case MONS_ICE_DRAGON:
        return TILE_CORPSE_ICE_DRAGON;
    case MONS_IRON_DRAGON:
        return TILE_CORPSE_IRON_DRAGON;
    case MONS_QUICKSILVER_DRAGON:
        return TILE_CORPSE_QUICKSILVER_DRAGON;
    case MONS_STEAM_DRAGON:
        return TILE_CORPSE_STEAM_DRAGON;
    case MONS_SWAMP_DRAGON:
        return TILE_CORPSE_SWAMP_DRAGON;
    case MONS_MOTTLED_DRAGON:
        return TILE_CORPSE_MOTTLED_DRAGON;
    case MONS_STORM_DRAGON:
        return TILE_CORPSE_STORM_DRAGON;
    case MONS_GOLDEN_DRAGON:
        return TILE_CORPSE_GOLDEN_DRAGON;
    case MONS_SHADOW_DRAGON:
        return TILE_CORPSE_SHADOW_DRAGON;

    // frogs ('F')
    case MONS_GIANT_FROG:
        return TILE_CORPSE_GIANT_FROG;
    case MONS_GIANT_TOAD:
        return TILE_CORPSE_GIANT_TOAD;
    case MONS_SPINY_FROG:
        return TILE_CORPSE_SPINY_FROG;
    case MONS_BLINK_FROG:
        return TILE_CORPSE_BLINK_FROG;

    // eyes ('G')
    case MONS_GIANT_EYEBALL:
        return TILE_CORPSE_GIANT_EYEBALL;
    case MONS_EYE_OF_DEVASTATION:
        return TILE_CORPSE_EYE_OF_DEVASTATION;
    case MONS_EYE_OF_DRAINING:
        return TILE_CORPSE_EYE_OF_DRAINING;
    case MONS_GIANT_ORANGE_BRAIN:
        return TILE_CORPSE_GIANT_ORANGE_BRAIN;
    case MONS_GREAT_ORB_OF_EYES:
        return TILE_CORPSE_GREAT_ORB_OF_EYES;
    case MONS_SHINING_EYE:
        return TILE_CORPSE_SHINING_EYE;

    // hybrids ('H')
    case MONS_HIPPOGRIFF:
        return TILE_CORPSE_HIPPOGRIFF;
    case MONS_MANTICORE:
        return TILE_CORPSE_MANTICORE;
    case MONS_GRIFFON:
        return TILE_CORPSE_GRIFFON;
    case MONS_HARPY:
        return TILE_CORPSE_HARPY;
    case MONS_MINOTAUR:
        return TILE_CORPSE_MINOTAUR;
    case MONS_KENKU:
        return TILE_CORPSE_KENKU;
    case MONS_SPHINX:
        return TILE_CORPSE_SPHINX;

    // jellies ('J')
    case MONS_GIANT_AMOEBA:
        return TILE_CORPSE_GIANT_AMOEBA;

    // kobolds ('K')
    case MONS_KOBOLD:
        return TILE_CORPSE_KOBOLD;
    case MONS_BIG_KOBOLD:
        return TILE_CORPSE_BIG_KOBOLD;

    // nagas ('N')
    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_GREATER_NAGA:
        return TILE_CORPSE_NAGA;
    case MONS_GUARDIAN_SERPENT:
        return TILE_CORPSE_GUARDIAN_SERPENT;

    // ogres ('O')
    case MONS_OGRE:
    case MONS_OGRE_MAGE:
        return TILE_CORPSE_OGRE;
    case MONS_TWO_HEADED_OGRE:
        return TILE_CORPSE_TWO_HEADED_OGRE;

    // snakes ('S')
    case MONS_SMALL_SNAKE:
        return TILE_CORPSE_SMALL_SNAKE;
    case MONS_SNAKE:
        return TILE_CORPSE_SNAKE;
    case MONS_ANACONDA:
        return TILE_CORPSE_ANACONDA;
    case MONS_WATER_MOCCASIN:
        return TILE_CORPSE_WATER_MOCCASIN;
    case MONS_BLACK_MAMBA:
        return TILE_CORPSE_BLACK_MAMBA;
    case MONS_VIPER:
        return TILE_CORPSE_VIPER;
    case MONS_SEA_SNAKE:
        return TILE_CORPSE_SEA_SNAKE;

    // trolls ('T')
    case MONS_TROLL:
        return TILE_CORPSE_TROLL;
    case MONS_ROCK_TROLL:
        return TILE_CORPSE_ROCK_TROLL;
    case MONS_IRON_TROLL:
        return TILE_CORPSE_IRON_TROLL;
    case MONS_DEEP_TROLL:
        return TILE_CORPSE_DEEP_TROLL;

    // bears ('U')
    case MONS_BEAR:
        return TILE_CORPSE_BEAR;
    case MONS_GRIZZLY_BEAR:
        return TILE_CORPSE_GRIZZLY_BEAR;
    case MONS_POLAR_BEAR:
        return TILE_CORPSE_POLAR_BEAR;
    case MONS_BLACK_BEAR:
        return TILE_CORPSE_BLACK_BEAR;

    // yaks, sheep and elephants ('Y')
    case MONS_SHEEP:
        return TILE_CORPSE_SHEEP;
    case MONS_YAK:
        return TILE_CORPSE_YAK;
    case MONS_DEATH_YAK:
        return TILE_CORPSE_DEATH_YAK;
    case MONS_ELEPHANT:
        return TILE_CORPSE_ELEPHANT;
    case MONS_DIRE_ELEPHANT:
        return TILE_CORPSE_DIRE_ELEPHANT;
    case MONS_HELLEPHANT:
        return TILE_CORPSE_HELLEPHANT;

    // water monsters
    case MONS_BIG_FISH:
        return TILE_CORPSE_BIG_FISH;
    case MONS_GIANT_GOLDFISH:
        return TILE_CORPSE_GIANT_GOLDFISH;
    case MONS_ELECTRIC_EEL:
        return TILE_CORPSE_ELECTRIC_EEL;
    case MONS_SHARK:
        return TILE_CORPSE_SHARK;
    case MONS_KRAKEN:
        return TILE_CORPSE_KRAKEN;
    case MONS_JELLYFISH:
        return TILE_CORPSE_JELLYFISH;

    // humans ('@')
    case MONS_HUMAN:
    case MONS_HELL_KNIGHT:
    case MONS_NECROMANCER:
    case MONS_WIZARD:
    case MONS_DEMIGOD: // haloed corpse looks abysmal
        return TILE_CORPSE_HUMAN;
    case MONS_DEMONSPAWN:
        return TILE_CORPSE_DEMONSPAWN;
    case MONS_HALFLING:
        return TILE_CORPSE_HALFLING;
    case MONS_SHAPESHIFTER:
        return TILE_CORPSE_SHAPESHIFTER;
    case MONS_GLOWING_SHAPESHIFTER:
        return TILE_CORPSE_GLOWING_SHAPESHIFTER;

    default:
        return TILE_ERROR;
    }
}

static tileidx_t _tileidx_rune(const item_def &item)
{
    switch (item.plus)
    {
    // the hell runes:
    case RUNE_DIS:         return TILE_MISC_RUNE_DIS;
    case RUNE_GEHENNA:     return TILE_MISC_RUNE_GEHENNA;
    case RUNE_COCYTUS:     return TILE_MISC_RUNE_COCYTUS;
    case RUNE_TARTARUS:    return TILE_MISC_RUNE_TARTARUS;

    // special pandemonium runes:
    case RUNE_MNOLEG:      return TILE_MISC_RUNE_MNOLEG;
    case RUNE_LOM_LOBON:   return TILE_MISC_RUNE_LOM_LOBON;
    case RUNE_CEREBOV:     return TILE_MISC_RUNE_CEREBOV;
    case RUNE_GLOORX_VLOQ: return TILE_MISC_RUNE_GLOORX_VLOQ;

    case RUNE_SNAKE_PIT:   return TILE_MISC_RUNE_SNAKE;

    // Most others use the default rune for now.
    case RUNE_SLIME_PITS:
    case RUNE_VAULTS:
    case RUNE_ELVEN_HALLS:
    case RUNE_TOMB:
    case RUNE_SWAMP:
    case RUNE_SHOALS:

    // pandemonium and abyss runes:
    case RUNE_DEMONIC:
    case RUNE_ABYSSAL:
    default:               return TILE_MISC_RUNE_OF_ZOT;
    }
}

static tileidx_t _tileidx_misc(const item_def &item)
{
    switch (item.sub_type)
    {
    case MISC_BOTTLED_EFREET:
        return TILE_MISC_BOTTLED_EFREET;

    case MISC_CRYSTAL_BALL_OF_SEEING:
        return TILE_MISC_CRYSTAL_BALL_OF_SEEING;

    case MISC_AIR_ELEMENTAL_FAN:
        return TILE_MISC_AIR_ELEMENTAL_FAN;

    case MISC_LAMP_OF_FIRE:
        return TILE_MISC_LAMP_OF_FIRE;

    case MISC_STONE_OF_EARTH_ELEMENTALS:
        return TILE_MISC_STONE_OF_EARTH_ELEMENTALS;

    case MISC_LANTERN_OF_SHADOWS:
        return TILE_MISC_LANTERN_OF_SHADOWS;

    case MISC_HORN_OF_GERYON:
        return TILE_MISC_HORN_OF_GERYON;

    case MISC_BOX_OF_BEASTS:
        return TILE_MISC_BOX_OF_BEASTS;

    case MISC_CRYSTAL_BALL_OF_ENERGY:
        return TILE_MISC_CRYSTAL_BALL_OF_ENERGY;

    case MISC_EMPTY_EBONY_CASKET:
        return TILE_MISC_EMPTY_EBONY_CASKET;

    case MISC_CRYSTAL_BALL_OF_FIXATION:
        return TILE_MISC_CRYSTAL_BALL_OF_FIXATION;

    case MISC_DISC_OF_STORMS:
        return TILE_MISC_DISC_OF_STORMS;

    case MISC_DECK_OF_ESCAPE:
    case MISC_DECK_OF_DESTRUCTION:
    case MISC_DECK_OF_DUNGEONS:
    case MISC_DECK_OF_SUMMONING:
    case MISC_DECK_OF_WONDERS:
    case MISC_DECK_OF_PUNISHMENT:
    case MISC_DECK_OF_WAR:
    case MISC_DECK_OF_CHANGES:
    case MISC_DECK_OF_DEFENCE:
    {
        tileidx_t ch = TILE_ERROR;
        switch (item.special)
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
        if (item.flags & ISFLAG_KNOW_TYPE)
        {
            // NOTE: order of tiles must be identical to order of decks.
            int offset = item.sub_type - MISC_DECK_OF_ESCAPE + 1;
            ch += offset;
        }
        return ch;
    }
    case MISC_RUNE_OF_ZOT:
        return _tileidx_rune(item);

    case MISC_QUAD_DAMAGE:
        return TILE_MISC_QUAD_DAMAGE;
    }

    return TILE_ERROR;
}

tileidx_t tileidx_item(const item_def &item)
{
    int clas    = item.base_type;
    int type    = item.sub_type;
    int special = item.special;
    int colour  = item.colour;

    id_arr& id = get_typeid_array();

    switch (clas)
    {
    case OBJ_WEAPONS:
        if (is_unrandom_artefact(item))
            return _tileidx_unrand_artefact(find_unrandart_index(item));
        else
            return _tileidx_weapon(item);

    case OBJ_MISSILES:
        return _tileidx_missile(item);

    case OBJ_ARMOUR:
        if (is_unrandom_artefact(item))
            return _tileidx_unrand_artefact(find_unrandart_index(item));
        else
            return _tileidx_armour(item);

    case OBJ_WANDS:
        if (id[ IDTYPE_WANDS ][type] == ID_KNOWN_TYPE
            ||  (item.flags &ISFLAG_KNOW_TYPE))
        {
            return TILE_WAND_ID_FIRST + type;
        }
        else
            return TILE_WAND_OFFSET + special % NDSC_WAND_PRI;

    case OBJ_FOOD:
        return _tileidx_food(item);

    case OBJ_SCROLLS:
        if (id[ IDTYPE_SCROLLS ][type] == ID_KNOWN_TYPE
            ||  (item.flags &ISFLAG_KNOW_TYPE))
        {
            return TILE_SCR_ID_FIRST + type;
        }
        return TILE_SCROLL;

    case OBJ_GOLD:
        return TILE_GOLD;

    case OBJ_JEWELLERY:
        if (is_unrandom_artefact(item))
            return _tileidx_unrand_artefact(find_unrandart_index(item));
        else if (type < NUM_RINGS)
        {
            if (is_random_artefact(item))
                return TILE_RING_RANDOM_OFFSET + colour - 1;
            else if (id[ IDTYPE_JEWELLERY][type] == ID_KNOWN_TYPE
                     || (item.flags & ISFLAG_KNOW_TYPE))
            {
                return TILE_RING_ID_FIRST + type - RING_FIRST_RING;
            }
            else
            {
                return TILE_RING_NORMAL_OFFSET + special % NDSC_JEWEL_PRI;
            }
        }
        else
        {
            if (is_random_artefact(item))
                return TILE_AMU_RANDOM_OFFSET + colour - 1;
            else if (id[ IDTYPE_JEWELLERY][type] == ID_KNOWN_TYPE
                     || (item.flags & ISFLAG_KNOW_TYPE))
            {
                return TILE_AMU_ID_FIRST + type - AMU_FIRST_AMULET;
            }
            else
            {
                return TILE_AMU_NORMAL_OFFSET + special % NDSC_JEWEL_PRI;
            }
        }

    case OBJ_POTIONS:
        if (id[ IDTYPE_POTIONS ][type] == ID_KNOWN_TYPE
            ||  (item.flags &ISFLAG_KNOW_TYPE))
        {
            return TILE_POT_ID_FIRST + type;
        }
        else
        {
            return TILE_POTION_OFFSET + item.plus % NDSC_POT_PRI;
        }

    case OBJ_BOOKS:
        if (is_random_artefact(item))
        {
            int offset = special % tile_main_count(TILE_BOOK_RANDART_OFFSET);
            return TILE_BOOK_RANDART_OFFSET + offset;
        }

        switch (special % NDSC_BOOK_PRI)
        {
        default:
        case 0:
        case 1:
            return TILE_BOOK_PAPER_OFFSET + colour;
        case 2:
            return TILE_BOOK_LEATHER_OFFSET + special / NDSC_BOOK_PRI;
        case 3:
            return TILE_BOOK_METAL_OFFSET + special / NDSC_BOOK_PRI;
        case 4:
            return TILE_BOOK_PAPYRUS;
        }

    case OBJ_STAVES:
        if (item_is_rod(item))
        {
            if (id[IDTYPE_STAVES][type] == ID_KNOWN_TYPE
                ||  (item.flags & ISFLAG_KNOW_TYPE))
            {
                return TILE_ROD_ID_FIRST + type - STAFF_SMITING;
            }

            int desc = (special / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;
            return TILE_ROD_OFFSET + desc;
        }
        else
        {
            if (id[IDTYPE_STAVES][type] == ID_KNOWN_TYPE
                ||  (item.flags & ISFLAG_KNOW_TYPE))
            {
                return TILE_STAFF_ID_FIRST + type;
            }

            int orig_spec = you.item_description[IDESC_STAVES][item.sub_type];
            int desc = (orig_spec/ NDSC_STAVE_PRI) % NDSC_STAVE_SEC;
            return TILE_STAFF_OFFSET + desc;
        }

    case OBJ_CORPSES:
        if (item.sub_type == CORPSE_SKELETON)
            return TILE_FOOD_BONE;
        else
            return _tileidx_corpse(item);

    case OBJ_ORBS:
        return TILE_ORB + random2(tile_main_count(TILE_ORB));

    case OBJ_MISCELLANY:
        return _tileidx_misc(item);

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
            case MI_DART:
                ch = TILE_MI_DART0;
                break;
            case MI_NEEDLE:
                ch = TILE_MI_NEEDLE0;
                break;
            case MI_JAVELIN:
                ch = TILE_MI_JAVELIN0;
                break;
            case MI_THROWING_NET:
                ch = TILE_MI_THROWING_NET0;
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
                switch (item.special)
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
            return (tileidx_enchant_equ(item, ch));
    }

    // If not a special case, just return the default tile.
    return tileidx_item(item);
}

// For items with randomized descriptions, only the overlay label is
// placed in the tile page.  This function looks up what the base item
// is based on the randomized description.  It returns 0 if there is none.
tileidx_t tileidx_known_base_item(tileidx_t label)
{
    if (label >= TILE_POT_ID_FIRST && label <= TILE_POT_ID_LAST)
    {
        int type = label - TILE_POT_ID_FIRST;
        int desc = you.item_description[IDESC_POTIONS][type] % NDSC_POT_PRI;
        return (TILE_POTION_OFFSET + desc);
    }

    if (label >= TILE_RING_ID_FIRST && label <= TILE_RING_ID_LAST)
    {
        int type = label - TILE_RING_ID_FIRST + RING_FIRST_RING;
        int desc = you.item_description[IDESC_RINGS][type] % NDSC_JEWEL_PRI;
        return (TILE_RING_NORMAL_OFFSET + desc);
    }

    if (label >= TILE_AMU_ID_FIRST && label <= TILE_AMU_ID_LAST)
    {
        int type = label - TILE_AMU_ID_FIRST + AMU_FIRST_AMULET;
        int desc = you.item_description[IDESC_RINGS][type] % NDSC_JEWEL_PRI;
        return (TILE_AMU_NORMAL_OFFSET + desc);
    }

    if (label >= TILE_SCR_ID_FIRST && label <= TILE_SCR_ID_LAST)
    {
        return (TILE_SCROLL);
    }

    if (label >= TILE_WAND_ID_FIRST && label <= TILE_WAND_ID_LAST)
    {
        int type = label - TILE_WAND_ID_FIRST;
        int desc = you.item_description[IDESC_WANDS][type] % NDSC_WAND_PRI;
        return (TILE_WAND_OFFSET + desc);
    }

    if (label >= TILE_STAFF_ID_FIRST && label <= TILE_STAFF_ID_LAST)
    {
        int type = label - TILE_STAFF_ID_FIRST;
        int desc = you.item_description[IDESC_STAVES][type];
        desc = (desc / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;
        return (TILE_STAFF_OFFSET + desc);
    }

    if (label >= TILE_ROD_ID_FIRST && label <= TILE_ROD_ID_LAST)
    {
        int type = label - TILE_ROD_ID_FIRST + STAFF_FIRST_ROD;
        int desc = you.item_description[IDESC_STAVES][type];
        desc = (desc / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;
        return (TILE_ROD_OFFSET + desc);
    }

    return (0);
}

tileidx_t tileidx_cloud(const cloud_struct &cl, bool disturbance)
{
    int type  = cl.type;
    int decay = cl.decay;
    std::string override = cl.tile;
    int colour = cl.colour;

    tileidx_t ch  = TILE_ERROR;
    int dur = decay/20;
    if (dur < 0)
        dur = 0;
    else if (dur > 2)
        dur = 2;

    if (!override.empty())
    {
        tileidx_t index;
        if (!tile_main_index(override.c_str(), &index))
        {
            mprf(MSGCH_ERROR, "Invalid tile requested for cloud: '%s'.", override.c_str());
        }
        else
        {
            int offset = tile_main_count(index);
            ch = index + offset;
        }
    }
    else
    {
        switch (type)
        {
            case CLOUD_FIRE:
                ch = TILE_CLOUD_FIRE_0 + dur;
                break;

            case CLOUD_COLD:
                ch = TILE_CLOUD_COLD_0 + dur;
                break;

            case CLOUD_STINK:
            case CLOUD_POISON:
                ch = TILE_CLOUD_POISON_0 + dur;
                break;

            case CLOUD_BLUE_SMOKE:
                ch = TILE_CLOUD_BLUE_SMOKE;
                break;

            case CLOUD_PURPLE_SMOKE:
            case CLOUD_TLOC_ENERGY:
                ch = TILE_CLOUD_TLOC_ENERGY;
                break;

            case CLOUD_MIASMA:
                ch = TILE_CLOUD_MIASMA;
                break;

            case CLOUD_BLACK_SMOKE:
                ch = TILE_CLOUD_BLACK_SMOKE;
                break;

            case CLOUD_MUTAGENIC:
                ch = (dur == 0 ? TILE_CLOUD_MUTAGENIC_0 :
                      dur == 1 ? TILE_CLOUD_MUTAGENIC_1
                               : TILE_CLOUD_MUTAGENIC_2);
                ch += random2(tile_main_count(ch));
                break;

            case CLOUD_MIST:
                ch = TILE_CLOUD_MIST;
                break;

            case CLOUD_RAIN:
                ch = TILE_CLOUD_RAIN + random2(tile_main_count(TILE_CLOUD_RAIN));
                break;

            case CLOUD_MAGIC_TRAIL:
                if (decay/20 > 2)
                    dur = 3;
                ch = TILE_CLOUD_MAGIC_TRAIL_0 + dur;
                break;

            case CLOUD_INK:
                ch = TILE_CLOUD_INK;
                break;

            case CLOUD_GLOOM:
                ch = TILE_CLOUD_GLOOM;
                break;

            default:
                ch = TILE_CLOUD_GREY_SMOKE;
                break;
        }
    }

    if (colour != -1)
        ch = tile_main_coloured(ch, colour);

    // The following clouds are supposed to be opaque, but I didn't make any
    // disturbance tile for them.
    // CLOUD_FOREST_FIRE and CLOUD_HOLY_FLAMES: are not in the above switch.
    // CLOUD_GLOOM: is this one used? Its tile doesn't look like a could.
    // CLOUD_INK: special cloud with a specific check in tileview.cc.
    if (disturbance && type != CLOUD_FOREST_FIRE  && type != CLOUD_GLOOM
        && type != CLOUD_INK && type != CLOUD_HOLY_FLAMES)
    {
        ch += tile_main_count(ch);
    }

    return (ch | TILE_FLAG_FLYING);
}

tileidx_t tileidx_bolt(const bolt &bolt)
{
    const int col = bolt.colour;
    return tileidx_zap(col);
}

tileidx_t tileidx_zap(int colour)
{
    int col = (colour == ETC_MAGIC ? element_colour(ETC_MAGIC) : colour);

    if (col > 8)
        col -= 8;
    if (col < 1)
        col = 7;
    return (TILE_SYM_BOLT_OFS - 1 + col);
}

tileidx_t tileidx_spell(spell_type spell)
{
    switch (spell)
    {
    case SPELL_NO_SPELL:
    case SPELL_DEBUGGING_RAY:
        return TILEG_ERROR;

    case NUM_SPELLS: // XXX: Hack!
        return TILEG_MEMORISE;

    // Air
    case SPELL_SHOCK:                    return TILEG_SHOCK;
    case SPELL_SWIFTNESS:                return TILEG_SWIFTNESS;
    case SPELL_LEVITATION:               return TILEG_LEVITATION;
    case SPELL_REPEL_MISSILES:           return TILEG_REPEL_MISSILES;
    case SPELL_MEPHITIC_CLOUD:           return TILEG_MEPHITIC_CLOUD;
    case SPELL_DISCHARGE:                return TILEG_STATIC_DISCHARGE;
    case SPELL_FLY:                      return TILEG_FLIGHT;
    case SPELL_INSULATION:               return TILEG_INSULATION;
    case SPELL_LIGHTNING_BOLT:           return TILEG_LIGHTNING_BOLT;
    case SPELL_AIRSTRIKE:                return TILEG_AIRSTRIKE;
    case SPELL_SILENCE:                  return TILEG_SILENCE;
    case SPELL_DEFLECT_MISSILES:         return TILEG_DEFLECT_MISSILES;
    case SPELL_CONJURE_BALL_LIGHTNING:   return TILEG_CONJURE_BALL_LIGHTNING;
    case SPELL_CHAIN_LIGHTNING:          return TILEG_CHAIN_LIGHTNING;
    case SPELL_TORNADO:                  return TILEG_TORNADO;

    // Earth
    case SPELL_SANDBLAST:                return TILEG_SANDBLAST;
    case SPELL_MAXWELLS_SILVER_HAMMER:   return TILEG_MAXWELLS_SILVER_HAMMER;
    case SPELL_STONESKIN:                return TILEG_STONESKIN;
    case SPELL_PASSWALL:                 return TILEG_PASSWALL;
    case SPELL_STONE_ARROW:              return TILEG_STONE_ARROW;
    case SPELL_DIG:                      return TILEG_DIG;
    case SPELL_BOLT_OF_MAGMA:            return TILEG_BOLT_OF_MAGMA;
    case SPELL_FRAGMENTATION:            return TILEG_LEES_RAPID_DECONSTRUCTION;
    case SPELL_IRON_SHOT:                return TILEG_IRON_SHOT;
    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:   return TILEG_LEHUDIBS_CRYSTAL_SPEAR;
    case SPELL_SHATTER:                  return TILEG_SHATTER;

    // Fire
    case SPELL_FLAME_TONGUE:             return TILEG_FLAME_TONGUE;
    case SPELL_EVAPORATE:                return TILEG_EVAPORATE;
    case SPELL_FIRE_BRAND:               return TILEG_FIRE_BRAND;
    case SPELL_THROW_FLAME:              return TILEG_THROW_FLAME;
    case SPELL_CONJURE_FLAME:            return TILEG_CONJURE_FLAME;
    case SPELL_STICKY_FLAME:             return TILEG_STICKY_FLAME;
    case SPELL_BOLT_OF_FIRE:             return TILEG_BOLT_OF_FIRE;
    case SPELL_IGNITE_POISON:            return TILEG_IGNITE_POISON;
    case SPELL_FIREBALL:                 return TILEG_FIREBALL;
    case SPELL_DELAYED_FIREBALL:         return TILEG_DELAYED_FIREBALL;
    case SPELL_RING_OF_FLAMES:           return TILEG_RING_OF_FLAMES;
    case SPELL_FIRE_STORM:               return TILEG_FIRE_STORM;

    // Ice
    case SPELL_FREEZE:                   return TILEG_FREEZE;
    case SPELL_THROW_FROST:              return TILEG_THROW_FROST;
    case SPELL_FREEZING_AURA:            return TILEG_FREEZING_AURA;
    case SPELL_HIBERNATION:              return TILEG_ENSORCELLED_HIBERNATION;
    case SPELL_OZOCUBUS_ARMOUR:          return TILEG_OZOCUBUS_ARMOUR;
    case SPELL_THROW_ICICLE:             return TILEG_THROW_ICICLE;
    case SPELL_CONDENSATION_SHIELD:      return TILEG_CONDENSATION_SHIELD;
    case SPELL_OZOCUBUS_REFRIGERATION:   return TILEG_OZOCUBUS_REFRIGERATION;
    case SPELL_BOLT_OF_COLD:             return TILEG_BOLT_OF_COLD;
    case SPELL_FREEZING_CLOUD:           return TILEG_FREEZING_CLOUD;
    case SPELL_ENGLACIATION:             return TILEG_METABOLIC_ENGLACIATION;
    case SPELL_SIMULACRUM:               return TILEG_SIMULACRUM;
    case SPELL_ICE_STORM:                return TILEG_ICE_STORM;

    // Poison
    case SPELL_STING:                    return TILEG_STING;
    case SPELL_CURE_POISON:              return TILEG_CURE_POISON;
    case SPELL_POISON_WEAPON:            return TILEG_POISON_BRAND;
    case SPELL_INTOXICATE:               return TILEG_ALISTAIRS_INTOXICATION;
    case SPELL_OLGREBS_TOXIC_RADIANCE:   return TILEG_OLGREBS_TOXIC_RADIANCE;
    case SPELL_RESIST_POISON:            return TILEG_RESIST_POISON;
    case SPELL_VENOM_BOLT:               return TILEG_VENOM_BOLT;
    case SPELL_POISON_ARROW:             return TILEG_POISON_ARROW;
    case SPELL_POISONOUS_CLOUD:          return TILEG_POISONOUS_CLOUD;

    // Enchantment
    case SPELL_CONFUSING_TOUCH:          return TILEG_CONFUSING_TOUCH;
    case SPELL_CORONA:                   return TILEG_CORONA;
    case SPELL_PROJECTED_NOISE:          return TILEG_PROJECTED_NOISE;
    case SPELL_SURE_BLADE:               return TILEG_SURE_BLADE;
    case SPELL_TUKIMAS_VORPAL_BLADE:     return TILEG_TUKIMAS_VORPAL_BLADE;
    case SPELL_BERSERKER_RAGE:           return TILEG_BERSERKER_RAGE;
    case SPELL_CONFUSE:                  return TILEG_CONFUSE;
    case SPELL_SLOW:                     return TILEG_SLOW;
    case SPELL_TUKIMAS_DANCE:            return TILEG_TUKIMAS_DANCE;
    case SPELL_ENSLAVEMENT:              return TILEG_ENSLAVEMENT;
    case SPELL_SEE_INVISIBLE:            return TILEG_SEE_INVISIBLE;
    case SPELL_PETRIFY:                  return TILEG_PETRIFY;
    case SPELL_CAUSE_FEAR:               return TILEG_CAUSE_FEAR;
    case SPELL_TAME_BEASTS:              return TILEG_TAME_BEASTS;
    case SPELL_EXTENSION:                return TILEG_EXTENSION;
    case SPELL_HASTE:                    return TILEG_HASTE;
    case SPELL_INVISIBILITY:             return TILEG_INVISIBILITY;
    case SPELL_MASS_CONFUSION:           return TILEG_MASS_CONFUSION;

    // Translocation
    case SPELL_APPORTATION:              return TILEG_APPORTATION;
    case SPELL_PORTAL_PROJECTILE:        return TILEG_PORTAL_PROJECTILE;
    case SPELL_BLINK:                    return TILEG_BLINK;
    case SPELL_BANISHMENT:               return TILEG_BANISHMENT;
    case SPELL_CONTROL_TELEPORT:         return TILEG_CONTROLLED_TELEPORT;
    case SPELL_TELEPORT_OTHER:           return TILEG_TELEPORT_OTHER;
    case SPELL_TELEPORT_SELF:            return TILEG_TELEPORT;
    case SPELL_PHASE_SHIFT:              return TILEG_PHASE_SHIFT;
    case SPELL_CONTROLLED_BLINK:         return TILEG_CONTROLLED_BLINK;
    case SPELL_WARP_BRAND:               return TILEG_WARP_WEAPON;
    case SPELL_DISPERSAL:                return TILEG_DISPERSAL;
    case SPELL_PORTAL:                   return TILEG_PORTAL;

    // Summoning
    case SPELL_SUMMON_BUTTERFLIES:       return TILEG_SUMMON_BUTTERFLIES;
    case SPELL_SUMMON_SMALL_MAMMALS:     return TILEG_SUMMON_SMALL_MAMMALS;
    case SPELL_RECALL:                   return TILEG_RECALL;
    case SPELL_CALL_CANINE_FAMILIAR:     return TILEG_CALL_CANINE_FAMILIAR;
    case SPELL_CALL_IMP:                 return TILEG_CALL_IMP;
    case SPELL_ABJURATION:               return TILEG_ABJURATION;
    case SPELL_SUMMON_SCORPIONS:         return TILEG_SUMMON_SCORPIONS;
    case SPELL_SUMMON_ELEMENTAL:         return TILEG_SUMMON_ELEMENTAL;
    case SPELL_SUMMON_DEMON:             return TILEG_SUMMON_DEMON;
    case SPELL_SUMMON_UGLY_THING:        return TILEG_SUMMON_UGLY_THING;
    case SPELL_SHADOW_CREATURES:         return TILEG_SUMMON_SHADOW_CREATURES;
    case SPELL_SUMMON_ICE_BEAST:         return TILEG_SUMMON_ICE_BEAST;
    case SPELL_DEMONIC_HORDE:            return TILEG_DEMONIC_HORDE;
    case SPELL_SUMMON_GREATER_DEMON:     return TILEG_SUMMON_GREATER_DEMON;
    case SPELL_SUMMON_HORRIBLE_THINGS:   return TILEG_SUMMON_HORRIBLE_THINGS;

    // Necromancy
    case SPELL_ANIMATE_SKELETON:         return TILEG_ANIMATE_SKELETON;
    case SPELL_PAIN:                     return TILEG_PAIN;
    case SPELL_FULSOME_DISTILLATION:     return TILEG_FULSOME_DISTILLATION;
    case SPELL_CORPSE_ROT:               return TILEG_CORPSE_ROT;
    case SPELL_LETHAL_INFUSION:          return TILEG_LETHAL_INFUSION;
    case SPELL_SUBLIMATION_OF_BLOOD:     return TILEG_SUBLIMATION_OF_BLOOD;
    case SPELL_BONE_SHARDS:              return TILEG_BONE_SHARDS;
    case SPELL_VAMPIRIC_DRAINING:        return TILEG_VAMPIRIC_DRAINING;
    case SPELL_REGENERATION:             return TILEG_REGENERATION;
    case SPELL_ANIMATE_DEAD:             return TILEG_ANIMATE_DEAD;
    case SPELL_DISPEL_UNDEAD:            return TILEG_DISPEL_UNDEAD;
    case SPELL_HAUNT:                    return TILEG_HAUNT;
    case SPELL_BORGNJORS_REVIVIFICATION: return TILEG_BORGNJORS_REVIVIFICATION;
    case SPELL_CIGOTUVIS_DEGENERATION:   return TILEG_CIGOTUVIS_DEGENERATION;
    case SPELL_AGONY:                    return TILEG_AGONY;
    case SPELL_TWISTED_RESURRECTION:     return TILEG_TWISTED_RESURRECTION;
    case SPELL_EXCRUCIATING_WOUNDS:      return TILEG_EXCRUCIATING_WOUNDS;
    case SPELL_CONTROL_UNDEAD:           return TILEG_CONTROL_UNDEAD;
    case SPELL_BOLT_OF_DRAINING:         return TILEG_BOLT_OF_DRAINING;
    case SPELL_SYMBOL_OF_TORMENT:        return TILEG_SYMBOL_OF_TORMENT;
    case SPELL_DEATHS_DOOR:              return TILEG_DEATHS_DOOR;
    case SPELL_DEATH_CHANNEL:            return TILEG_DEATH_CHANNEL;

    // Transmutation
    case SPELL_STICKS_TO_SNAKES:         return TILEG_STICKS_TO_SNAKES;
    case SPELL_SPIDER_FORM:              return TILEG_SPIDER_FORM;
    case SPELL_ICE_FORM:                 return TILEG_ICE_FORM;
    case SPELL_BLADE_HANDS:              return TILEG_BLADE_HANDS;
    case SPELL_POLYMORPH_OTHER:          return TILEG_POLYMORPH_OTHER;
    case SPELL_STATUE_FORM:              return TILEG_STATUE_FORM;
    case SPELL_ALTER_SELF:               return TILEG_ALTER_SELF;
    case SPELL_DRAGON_FORM:              return TILEG_DRAGON_FORM;
    case SPELL_NECROMUTATION:            return TILEG_NECROMUTATION;

    // pure Conjuration
    case SPELL_MAGIC_DART:               return TILEG_MAGIC_DART;
    case SPELL_ISKENDERUNS_MYSTIC_BLAST: return TILEG_ISKENDERUNS_MYSTIC_BLAST;
    case SPELL_IOOD:                     return TILEG_IOOD;

    // Divination (soon to be obsolete, or moved to abilities)
    case SPELL_DETECT_SECRET_DOORS:      return TILEG_DETECT_SECRET_DOORS;
    case SPELL_DETECT_TRAPS:             return TILEG_DETECT_TRAPS;
    case SPELL_DETECT_ITEMS:             return TILEG_DETECT_ITEMS;
    case SPELL_DETECT_CREATURES:         return TILEG_DETECT_CREATURES;

    // --------------------------------------------
    // Rods and abilities (tiles needed for later)
    // Abilities
    case SPELL_SMITING:       // Beogh power
    case SPELL_MINOR_HEALING: // Ely power
    case SPELL_MAJOR_HEALING: // Ely power
    case SPELL_HELLFIRE:      // Demonspawn ability

    // Rod-only spells
    case SPELL_PARALYSE:      // we could reuse Petrify for this
    case SPELL_STRIKING:
    case SPELL_BOLT_OF_INACCURACY:
    case SPELL_SUMMON_SWARM:
        return TILEG_TODO;

    // --------------------------------------------
    // Spells that don't need icons:
    case SPELL_STONEMAIL:     // Xom only?
    case SPELL_SUMMON_DRAGON: // Xom
    case SPELL_DISINTEGRATE:  // wand and card

    // Monster spells (mostly?)
    case SPELL_HELLFIRE_BURST:
    case SPELL_VAMPIRE_SUMMON:
    case SPELL_BRAIN_FEED:
    case SPELL_FAKE_RAKSHASA_SUMMON:
    case SPELL_STEAM_BALL:
    case SPELL_SUMMON_UFETUBUS:
    case SPELL_SUMMON_BEAST:
    case SPELL_ENERGY_BOLT:
    case SPELL_POISON_SPLASH:
    case SPELL_SUMMON_UNDEAD:
    case SPELL_CANTRIP:
    case SPELL_QUICKSILVER_BOLT:
    case SPELL_METAL_SPLINTERS:
    case SPELL_MIASMA:
    case SPELL_SUMMON_DRAKES:
    case SPELL_BLINK_OTHER:
    case SPELL_SUMMON_MUSHROOMS:
    case SPELL_ACID_SPLASH:
    case SPELL_STICKY_FLAME_SPLASH:
    case SPELL_FIRE_BREATH:
    case SPELL_COLD_BREATH:
    case SPELL_DRACONIAN_BREATH:
    case SPELL_WATER_ELEMENTALS:
    case SPELL_PORKALATOR:
    default:
        return TILEG_ERROR;
    }
}

tileidx_t tileidx_skill(skill_type skill, bool active)
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
    case SK_STABBING:       ch = TILEG_STABBING_ON; break;
    case SK_SHIELDS:        ch = TILEG_SHIELDS_ON; break;
    case SK_TRAPS_DOORS:    ch = TILEG_TRAPS_DOORS_ON; break;
    case SK_UNARMED_COMBAT: ch = TILEG_UNARMED_COMBAT_ON; break;
    case SK_SPELLCASTING:   ch = TILEG_SPELLCASTING_ON; break;
    case SK_CONJURATIONS:   ch = TILEG_CONJURATIONS_ON; break;
    case SK_ENCHANTMENTS:   ch = TILEG_ENCHANTMENTS_ON; break;
    case SK_SUMMONINGS:     ch = TILEG_SUMMONINGS_ON; break;
    case SK_NECROMANCY:     ch = TILEG_NECROMANCY_ON; break;
    case SK_TRANSLOCATIONS: ch = TILEG_TRANSLOCATIONS_ON; break;
    case SK_TRANSMUTATIONS: ch = TILEG_TRANSMUTATIONS_ON; break;
    case SK_FIRE_MAGIC:     ch = TILEG_FIRE_MAGIC_ON; break;
    case SK_ICE_MAGIC:      ch = TILEG_ICE_MAGIC_ON; break;
    case SK_AIR_MAGIC:      ch = TILEG_AIR_MAGIC_ON; break;
    case SK_EARTH_MAGIC:    ch = TILEG_EARTH_MAGIC_ON; break;
    case SK_POISON_MAGIC:   ch = TILEG_POISON_MAGIC_ON; break;
    case SK_INVOCATIONS:    ch = TILEG_INVOCATIONS_ON; break;
    case SK_EVOCATIONS:     ch = TILEG_EVOCATIONS_ON; break;
    default:                return TILEG_TODO;
    }

    if (!active)
        ch++;

    return ch;
}

tileidx_t tileidx_known_brand(const item_def &item)
{
    if (!item_type_known(item))
        return 0;

    if (item.base_type == OBJ_WEAPONS)
    {
        const int brand = get_weapon_brand(item);
        if (brand != SPWPN_NORMAL)
            return (TILE_BRAND_FLAMING + get_weapon_brand(item) - 1);
    }
    else if (item.base_type == OBJ_ARMOUR)
    {
        const int brand = get_armour_ego_type(item);
        if (brand != SPARM_NORMAL)
            return (TILE_BRAND_ARM_RUNNING + get_armour_ego_type(item) - 1);
        else if (is_artefact(item)
                 && artefact_wpn_property(item, ARTP_PONDEROUS))
        {
            return (TILE_BRAND_ARM_PONDEROUSNESS);
        }
    }
    else if (item.base_type == OBJ_MISSILES)
    {
        switch (get_ammo_brand(item))
        {
        case SPMSL_FLAME:
            return TILE_BRAND_FLAME;
        case SPMSL_FROST:
            return TILE_BRAND_FROST;
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
        case SPMSL_REAPING:
            return TILE_BRAND_REAPING;
        case SPMSL_DISPERSAL:
            return TILE_BRAND_DISPERSAL;
        case SPMSL_EXPLODING:
            return TILE_BRAND_EXPLOSION;
        case SPMSL_CONFUSION:
            return TILE_BRAND_CONFUSION;
        case SPMSL_PARALYSIS:
            return TILE_BRAND_PARALYSIS;
        case SPMSL_SLOW:
            return TILE_BRAND_SLOWING;
        case SPMSL_SICKNESS:
            return TILE_BRAND_SICKNESS;
        case SPMSL_RAGE:
            return TILE_BRAND_RAGE;
        case SPMSL_SLEEP:
            return TILE_BRAND_SLEEP;
        default:
            break;
        }
    }
    return 0;
}

tileidx_t tileidx_corpse_brand(const item_def &item)
{
    if (item.base_type != OBJ_CORPSES || item.sub_type != CORPSE_BODY)
        return (0);

    const bool fulsome_dist = you.has_spell(SPELL_FULSOME_DISTILLATION);
    const bool rotten       = food_is_rotten(item);
    const bool saprovorous  = player_mutation_level(MUT_SAPROVOROUS);

    // Brands are mostly meaningless to herbivores.
    // Could still be interesting for Fulsome Distillation, though.
    if (fulsome_dist && player_mutation_level(MUT_HERBIVOROUS) == 3)
        return (0);

    // Vampires are only interested in fresh blood.
    if (you.species == SP_VAMPIRE
        && (rotten || !mons_has_blood(item.plus)))
    {
        return TILE_FOOD_INEDIBLE;
    }

    // Rotten corpses' chunk effects are meaningless if we are neither
    // saprovorous nor have the Fulsome Distillation spell.
    if (rotten && !saprovorous && !fulsome_dist)
        return TILE_FOOD_INEDIBLE;

    // Harmful chunk effects > religious rules > chance of sickness.
    if (is_poisonous(item))
        return TILE_FOOD_POISONED;

    if (is_mutagenic(item))
        return TILE_FOOD_MUTAGENIC;

    if (causes_rot(item))
        return TILE_FOOD_ROTTING;

    if (is_forbidden_food(item))
        return TILE_FOOD_FORBIDDEN;

    if (is_contaminated(item))
        return TILE_FOOD_CONTAMINATED;

    // If no special chunk effects, mark corpse as inedible
    // unless saprovorous.
    if (rotten && !saprovorous)
        return TILE_FOOD_INEDIBLE;

    return 0;
}

// FIXME: Needs to be updated whenever the order of clouds or monsters changes.
tileidx_t get_clean_map_idx(tileidx_t tile_idx)
{
    tileidx_t idx = tile_idx & TILE_FLAG_MASK;
    if (idx >= TILE_CLOUD_FIRE_0 && idx <= TILE_CLOUD_TLOC_ENERGY
        || idx >= TILEP_MONS_PANDEMONIUM_DEMON && idx <= TILEP_MONS_TEST_SPAWNER
        || idx >= TILEP_MCACHE_START)
    {
        return 0;
    }

    return tile_idx;
}

tileidx_t tileidx_unseen_flag(const coord_def &gc)
{
    if (!map_bounds(gc))
        return (TILE_FLAG_UNSEEN);
    else if (env.map_knowledge(gc).known()
                && !env.map_knowledge(gc).seen()
             || env.map_knowledge(gc).detected_item()
             || env.map_knowledge(gc).detected_monster()
           )
    {
        return (TILE_FLAG_MM_UNSEEN);
    }
    else
        return (TILE_FLAG_UNSEEN);
}

int enchant_to_int(const item_def &item)
{
    switch (item.flags & ISFLAG_COSMETIC_MASK)
    {
        case ISFLAG_EMBROIDERED_SHINY:
            return 1;
        case ISFLAG_RUNED:
            return 2;
        case ISFLAG_GLOWING:
            return 3;
        default:
            return (is_random_artefact(item) ? 4 : 0);
    }
}

tileidx_t tileidx_enchant_equ(const item_def &item, tileidx_t tile)
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
    const int idx   = tile_main_count(tile) - 1;
    ASSERT(idx < 5);

    tile += etable[idx][etype];

    return (tile);
}

std::string tile_debug_string(tileidx_t fg, tileidx_t bg, char prefix)
{
    tileidx_t fg_idx = fg & TILE_FLAG_MASK;
    tileidx_t bg_idx = bg & TILE_FLAG_MASK;

    std::string fg_name;
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
        {
            fg_name += "[not found]";
        }
    }

    std::string tile_string = make_stringf(
        "%cFG: %4d | 0x%8x (%s)\n"
        "%cBG: %4d | 0x%8x (%s)\n",
        prefix,
        fg_idx,
        fg & ~TILE_FLAG_MASK,
        fg_name.c_str(),
        prefix,
        bg_idx,
        bg & ~TILE_FLAG_MASK,
        tile_dngn_name(bg_idx));

    return (tile_string);
}

void tile_init_props(monster* mon)
{
    // Not necessary.
    if (mon->props.exists("monster_tile") || mon->props.exists("tile_num"))
        return;

    // Special-case for monsters masquerading as items.
    if (mon->type == MONS_DANCING_WEAPON || mons_is_mimic(mon->type))
        return;

    // Only add the property for tiles that have several variants.
    const int base_tile = _tileidx_monster_base(mon->type);
    if (tile_player_count(base_tile) > 1)
        mon->props["tile_num"] = short(random2(256));
}

#endif
