#include "AppHdr.h"

#include "tilepick.h"

#include "art-enum.h"
#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "decks.h"
#include "describe.h"
#include "env.h"
#include "food.h"
#include "ghost.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
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
#include "tileview.h"
#include "traps.h"
#include "viewgeom.h"

#ifdef COMPILE_CHECKS_NEED_FUNCTION
// FIXME: this is too ugly to live, pollutes the executable's debug symbols,
// and stinks of elderberries.  Need a better non-C++11 way to do static
// asserts in GCC 4.8.
void tile_compile_checks(); // shut up -Wmissing-declarations
void tile_compile_checks()
{
#endif

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

// Some tile sets have a corresponding tile for every colour (excepting black).
// If this assert breaks, then it means that either there are tiles that are
// being unused in the tileset or an incorrect type of tile will get used.
COMPILE_CHECK(MAX_TERM_COLOUR - 1
              == TILE_RING_COL_LAST - TILE_RING_COL_FIRST + 1);
COMPILE_CHECK(MAX_TERM_COLOUR - 1
              == TILE_AMU_COL_LAST - TILE_AMU_COL_FIRST + 1);
COMPILE_CHECK(MAX_TERM_COLOUR - 1
              == TILE_BOOK_COL_LAST - TILE_BOOK_COL_FIRST + 1);

#ifdef COMPILE_CHECKS_NEED_FUNCTION
}
#endif

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

#ifdef USE_TILE
static tileidx_t _tileidx_monster_base(int type,
                                       bool in_water = false,
                                       int colour = 0,
                                       int number = 0,
                                       int tile_num_prop = 0);
#endif

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
    const shop_struct *shop = get_shop(where);

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
        case SHOP_MISCELLANY:
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
    case DNGN_ROCK_WALL:
    case DNGN_PERMAROCK_WALL:
        return TILE_WALL_NORMAL;
    case DNGN_SLIMY_WALL:
        return TILE_WALL_SLIME;
    case DNGN_OPEN_SEA:
        return TILE_DNGN_OPEN_SEA;
    case DNGN_RUNED_DOOR:
        return TILE_DNGN_RUNED_DOOR;
    case DNGN_SEALED_DOOR:
        return TILE_DNGN_SEALED_DOOR;
    case DNGN_GRATE:
        return TILE_DNGN_GRATE;
    case DNGN_CLEAR_ROCK_WALL:
        return TILE_DNGN_TRANSPARENT_WALL;
    case DNGN_CLEAR_STONE_WALL:
    case DNGN_CLEAR_PERMAROCK_WALL:
        return TILE_DNGN_TRANSPARENT_STONE;
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
    case DNGN_TREE:
        return TILE_DNGN_TREE;
    case DNGN_MANGROVE:
        return TILE_DNGN_MANGROVE;
    case DNGN_GRANITE_STATUE:
        return TILE_DNGN_GRANITE_STATUE;
    case DNGN_LAVA_SEA: // FIXME
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
        if (player_in_hell())
            return TILE_DNGN_RETURN_VESTIBULE;
        return TILE_DNGN_ENTER_HELL;
    case DNGN_OPEN_DOOR:
        return TILE_DNGN_OPEN_DOOR;
    case DNGN_TRAP_MECHANICAL:
        return TILE_DNGN_TRAP_DART;
    case DNGN_TRAP_MAGICAL:
        return TILE_DNGN_TRAP_ZOT;
    case DNGN_TRAP_NATURAL:
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
        if (you.where_are_you == BRANCH_VESTIBULE_OF_HELL)
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
    case DNGN_ENTER_DWARVEN_HALL:
#endif
    case DNGN_ENTER_HALL_OF_BLADES:
    case DNGN_ENTER_FOREST:
        return TILE_DNGN_ENTER;
    case DNGN_ENTER_TEMPLE:
        return TILE_DNGN_ENTER_TEMPLE;
    case DNGN_ENTER_ORCISH_MINES:
        return TILE_DNGN_ENTER_ORCISH_MINES;
    case DNGN_ENTER_ELVEN_HALLS:
        return TILE_DNGN_ENTER_ELVEN_HALLS;
    case DNGN_ENTER_LAIR:
        return TILE_DNGN_ENTER_LAIR;
    case DNGN_ENTER_SNAKE_PIT:
        return TILE_DNGN_ENTER_SNAKE_PIT;
    case DNGN_ENTER_SWAMP:
        return TILE_DNGN_ENTER_SWAMP;
    case DNGN_ENTER_SPIDER_NEST:
        return TILE_DNGN_ENTER_SPIDER_NEST;
    case DNGN_ENTER_SHOALS:
        return TILE_DNGN_ENTER_SHOALS;
    case DNGN_ENTER_SLIME_PITS:
        return TILE_DNGN_ENTER_SLIME_PITS;
    case DNGN_ENTER_VAULTS:
        return TILE_DNGN_ENTER_VAULTS;
    case DNGN_ENTER_CRYPT:
        return TILE_DNGN_ENTER_CRYPT;
    case DNGN_ENTER_TOMB:
        return TILE_DNGN_ENTER_TOMB;
    case DNGN_ENTER_ZOT:
        if (you.opened_zot)
            return TILE_DNGN_ENTER_ZOT_OPEN;
        return TILE_DNGN_ENTER_ZOT_CLOSED;

    // branch exit stairs
#if TAG_MAJOR_VERSION == 34
    case DNGN_RETURN_FROM_DWARVEN_HALL:
#endif
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
    case DNGN_RETURN_FROM_FOREST:
        return TILE_DNGN_RETURN;
    case DNGN_RETURN_FROM_TEMPLE:
        return TILE_DNGN_EXIT_TEMPLE;
    case DNGN_RETURN_FROM_ORCISH_MINES:
        return TILE_DNGN_EXIT_ORCISH_MINES;
    case DNGN_RETURN_FROM_ELVEN_HALLS:
        return TILE_DNGN_EXIT_ELVEN_HALLS;
    case DNGN_RETURN_FROM_LAIR:
        return TILE_DNGN_EXIT_LAIR;
    case DNGN_RETURN_FROM_SNAKE_PIT:
        return TILE_DNGN_EXIT_SNAKE_PIT;
    case DNGN_RETURN_FROM_SWAMP:
        return TILE_DNGN_EXIT_SWAMP;
    case DNGN_RETURN_FROM_SPIDER_NEST:
        return TILE_DNGN_EXIT_SPIDER_NEST;
    case DNGN_RETURN_FROM_SHOALS:
        return TILE_DNGN_EXIT_SHOALS;
    case DNGN_RETURN_FROM_SLIME_PITS:
        return TILE_DNGN_EXIT_SLIME_PITS;
    case DNGN_RETURN_FROM_VAULTS:
        return TILE_DNGN_EXIT_VAULTS;
    case DNGN_RETURN_FROM_CRYPT:
        return TILE_DNGN_EXIT_CRYPT;
    case DNGN_RETURN_FROM_TOMB:
        return TILE_DNGN_EXIT_TOMB;
    case DNGN_RETURN_FROM_ZOT:
        return TILE_DNGN_RETURN_ZOT;

    case DNGN_ENTER_PORTAL_VAULT:
    case DNGN_EXIT_PORTAL_VAULT:
        return TILE_DNGN_PORTAL;
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
                        && feat != DNGN_UNSEEN;
    if (override && can_override)
        return override;

    const monster_info* mimic = env.map_knowledge(gc).monsterinfo();
    if (mimic && mons_is_feat_mimic(mimic->type))
        return mimic->props["tile_idx"].get_int();

    // Any grid-specific tiles.
    switch (feat)
    {
    case DNGN_FLOOR:
        if (player_in_branch(BRANCH_SLIME_PITS))
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
    {
        unsigned colour = env.map_knowledge(gc).feat_colour();
        if (colour == 0)
        {
            colour = (feat == DNGN_FLOOR)     ? env.floor_colour :
                     (feat == DNGN_ROCK_WALL) ? env.rock_colour
                                              : 0; // meh
        }
        if (colour >= ETC_FIRST)
        {
            tileidx_t idx =
                (feat == DNGN_FLOOR)     ? env.tile_flv(gc).floor :
                (feat == DNGN_ROCK_WALL) ? env.tile_flv(gc).wall
                                         : TILE_DNGN_STONE_WALL;
#ifdef USE_TILE
            if (feat == DNGN_STONE_WALL)
                apply_variations(env.tile_flv(gc), &idx, gc);
#endif
            tileidx_t base = tile_dngn_basetile(idx);
            tileidx_t spec = idx - base;
            unsigned rc = real_colour(colour, gc);
            return tile_dngn_coloured(base, rc) + spec; // XXX
        }
        return _tileidx_feature_base(feat);
    }

    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_NATURAL:
        return (_tileidx_trap(env.map_knowledge(gc).trap()));

    case DNGN_TRAP_WEB:
    {
        /*
        trap_type this_trap_type = get_trap_type(gc);
        // There's room here to have different types of webs (acid? fire? ice? different strengths?)
        if (this_trap_type==TRAP_WEB) {*/

        // Determine web connectivity on all sides
        const coord_def neigh[4] = {
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
        return _tileidx_feature_base(feat);
    }
}

#ifdef USE_TILE
tileidx_t tileidx_out_of_bounds(int branch)
{
    if (branch == BRANCH_SHOALS)
        return (TILE_DNGN_OPEN_SEA | TILE_FLAG_UNSEEN);
    else
        return (TILE_DNGN_UNSEEN | TILE_FLAG_UNSEEN);
}

void tileidx_out_of_los(tileidx_t *fg, tileidx_t *bg, tileidx_t *cloud, const coord_def& gc)
{
    // Player memory.
    tileidx_t mem_fg = env.tile_bk_fg(gc);
    tileidx_t mem_bg = env.tile_bk_bg(gc);
    tileidx_t mem_cloud = env.tile_bk_cloud(gc);

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
    {
        ASSERT(cell.monster() == MONS_SENSED);
        *fg = _tileidx_monster_base(cell.monsterinfo()->base_type);
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
        return TILEP_MONS_SPECTRAL_SMALL;
    case TILEP_MONS_ZOMBIE_LARGE:
    case TILEP_MONS_ZOMBIE_OGRE:
    case TILEP_MONS_ZOMBIE_DRACONIAN:
    case TILEP_MONS_ZOMBIE_TROLL:
        return TILEP_MONS_SPECTRAL_LARGE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_SMALL:
    case TILEP_MONS_ZOMBIE_RAT:
    case TILEP_MONS_ZOMBIE_HOUND:
    case TILEP_MONS_ZOMBIE_CRAB:
    case TILEP_MONS_ZOMBIE_TURTLE:
        return TILEP_MONS_SPECTRAL_QUADRUPED_SMALL;
    case TILEP_MONS_ZOMBIE_QUADRUPED_LARGE:
        return TILEP_MONS_SPECTRAL_QUADRUPED_LARGE;
    case TILEP_MONS_ZOMBIE_TOAD:
        return TILEP_MONS_SPECTRAL_TOAD;
    case TILEP_MONS_ZOMBIE_BAT:
        return TILEP_MONS_SPECTRAL_BAT;
    case TILEP_MONS_ZOMBIE_BEE:
        return TILEP_MONS_SPECTRAL_BEE;
    case TILEP_MONS_ZOMBIE_BEETLE:
        return TILEP_MONS_SPECTRAL_BEETLE;
    case TILEP_MONS_ZOMBIE_FISH:
        return TILEP_MONS_SPECTRAL_FISH;
    case TILEP_MONS_ZOMBIE_CENTAUR:
        return TILEP_MONS_SPECTRAL_CENTAUR;
    case TILEP_MONS_ZOMBIE_NAGA:
        return TILEP_MONS_SPECTRAL_NAGA;
    case TILEP_MONS_ZOMBIE_SNAKE:
    case TILEP_MONS_ZOMBIE_WORM:
        return TILEP_MONS_SPECTRAL_SNAKE;
    case TILEP_MONS_ZOMBIE_LIZARD:
        return TILEP_MONS_SPECTRAL_LIZARD;
    case TILEP_MONS_ZOMBIE_SPIDER:
        return TILEP_MONS_SPECTRAL_SPIDER;
    case TILEP_MONS_ZOMBIE_DRAGON:
        return TILEP_MONS_SPECTRAL_DRAGON;
    case TILEP_MONS_ZOMBIE_DRAKE:
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
        return TILEP_MONS_SIMULACRUM_SMALL;
    case TILEP_MONS_ZOMBIE_LARGE:
    case TILEP_MONS_ZOMBIE_OGRE:
    case TILEP_MONS_ZOMBIE_DRACONIAN:
    case TILEP_MONS_ZOMBIE_TROLL:
        return TILEP_MONS_SIMULACRUM_LARGE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_SMALL:
    case TILEP_MONS_ZOMBIE_RAT:
    case TILEP_MONS_ZOMBIE_HOUND:
    case TILEP_MONS_ZOMBIE_CRAB:
    case TILEP_MONS_ZOMBIE_TURTLE:
        return TILEP_MONS_SIMULACRUM_QUADRUPED_SMALL;
    case TILEP_MONS_ZOMBIE_QUADRUPED_LARGE:
    case TILEP_MONS_ZOMBIE_TOAD:
        return TILEP_MONS_SIMULACRUM_QUADRUPED_LARGE;
    case TILEP_MONS_ZOMBIE_BAT:
        return TILEP_MONS_SIMULACRUM_BAT;
    case TILEP_MONS_ZOMBIE_BEE:
        return TILEP_MONS_SIMULACRUM_BEE;
    case TILEP_MONS_ZOMBIE_BEETLE:
        return TILEP_MONS_SIMULACRUM_BEETLE;
    case TILEP_MONS_ZOMBIE_FISH:
        return TILEP_MONS_SIMULACRUM_FISH;
    case TILEP_MONS_ZOMBIE_CENTAUR:
        return TILEP_MONS_SIMULACRUM_CENTAUR;
    case TILEP_MONS_ZOMBIE_NAGA:
        return TILEP_MONS_SIMULACRUM_NAGA;
    case TILEP_MONS_ZOMBIE_SNAKE:
    case TILEP_MONS_ZOMBIE_WORM:
        return TILEP_MONS_SIMULACRUM_SNAKE;
    case TILEP_MONS_ZOMBIE_LIZARD:
        return TILEP_MONS_SIMULACRUM_LIZARD;
    case TILEP_MONS_ZOMBIE_SPIDER:
        return TILEP_MONS_SIMULACRUM_SPIDER;
    case TILEP_MONS_ZOMBIE_DRAGON:
        return TILEP_MONS_SIMULACRUM_DRAGON;
    case TILEP_MONS_ZOMBIE_DRAKE:
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
        return TILEP_MONS_SKELETON_SMALL;
    case TILEP_MONS_ZOMBIE_TROLL:
    case TILEP_MONS_ZOMBIE_LARGE:
    case TILEP_MONS_ZOMBIE_OGRE:
    case TILEP_MONS_ZOMBIE_DRACONIAN:
        return TILEP_MONS_SKELETON_LARGE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_SMALL:
    case TILEP_MONS_ZOMBIE_RAT:
    case TILEP_MONS_ZOMBIE_HOUND:
    case TILEP_MONS_ZOMBIE_BEETLE:
        return TILEP_MONS_SKELETON_QUADRUPED_SMALL;
    case TILEP_MONS_ZOMBIE_LIZARD:
    case TILEP_MONS_ZOMBIE_CRAB:
        return TILEP_MONS_SKELETON_LIZARD;
    case TILEP_MONS_ZOMBIE_TURTLE:
        return TILEP_MONS_SKELETON_TURTLE;
    case TILEP_MONS_ZOMBIE_QUADRUPED_LARGE:
        return TILEP_MONS_SKELETON_QUADRUPED_LARGE;
    case TILEP_MONS_ZOMBIE_TOAD:
        return TILEP_MONS_SKELETON_TOAD;
    case TILEP_MONS_ZOMBIE_GRIFFON:
    case TILEP_MONS_ZOMBIE_QUADRUPED_WINGED:
        return TILEP_MONS_SKELETON_QUADRUPED_WINGED;
    case TILEP_MONS_ZOMBIE_BAT:
    case TILEP_MONS_ZOMBIE_HARPY:
        return TILEP_MONS_SKELETON_BAT;
    case TILEP_MONS_ZOMBIE_FISH:
        return TILEP_MONS_SKELETON_FISH;
    case TILEP_MONS_ZOMBIE_CENTAUR:
        return TILEP_MONS_SKELETON_CENTAUR;
    case TILEP_MONS_ZOMBIE_NAGA:
        return TILEP_MONS_SKELETON_NAGA;
    case TILEP_MONS_ZOMBIE_SNAKE:
    case TILEP_MONS_ZOMBIE_WORM:
        return TILEP_MONS_SKELETON_SNAKE;
    case TILEP_MONS_ZOMBIE_DRAGON:
        return TILEP_MONS_SKELETON_DRAGON;
    case TILEP_MONS_ZOMBIE_DRAKE:
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

static tileidx_t _tileidx_monster_zombified(const monster_info& mon)
{
    const int z_type = mon.type;
    const monster_type subtype = mon.base_type;

    const int z_size = mons_zombie_size(subtype);

    tileidx_t z_tile;

    switch (get_mon_shape(subtype))
    {
    case MON_SHAPE_HUMANOID:
        if (mons_genus(subtype) == MONS_TROLL)
        {
            z_tile = TILEP_MONS_ZOMBIE_TROLL;
            break;
        }
        if (mons_genus(subtype) == MONS_OGRE)
        {
            z_tile = TILEP_MONS_ZOMBIE_OGRE;
            break;
        }
    case MON_SHAPE_HUMANOID_WINGED:
        if (mons_genus(subtype) == MONS_HARPY)
        {
            z_tile = TILEP_MONS_ZOMBIE_HARPY;
            break;
        }
    case MON_SHAPE_HUMANOID_TAILED:
    case MON_SHAPE_HUMANOID_WINGED_TAILED:
        if (mons_genus(subtype) == MONS_DRACONIAN)
            z_tile = TILEP_MONS_ZOMBIE_DRACONIAN;
        else
        {
            z_tile = (z_size == Z_SMALL ? TILEP_MONS_ZOMBIE_SMALL
                      : TILEP_MONS_ZOMBIE_LARGE);
        }
        break;
    case MON_SHAPE_CENTAUR:
        z_tile = TILEP_MONS_ZOMBIE_CENTAUR;
        break;
    case MON_SHAPE_NAGA:
        z_tile = TILEP_MONS_ZOMBIE_NAGA;
        break;
    case MON_SHAPE_QUADRUPED_WINGED:
        if (mons_genus(subtype) != MONS_DRAGON && mons_genus(subtype) != MONS_WYVERN)
        {
            if (mons_genus(subtype) == MONS_GRIFFON)
                z_tile = TILEP_MONS_ZOMBIE_GRIFFON;
            else
                z_tile = TILEP_MONS_ZOMBIE_QUADRUPED_WINGED;
            break;
        }
        // else fall-through for skeletons & dragons
    case MON_SHAPE_QUADRUPED:
        if (mons_genus(subtype) == MONS_DRAGON
            || mons_genus(subtype) == MONS_WYVERN)
        {
            if (subtype == MONS_MOTTLED_DRAGON
                || subtype == MONS_STEAM_DRAGON)
            {
                z_tile = TILEP_MONS_ZOMBIE_DRAKE;
            }
            else
                z_tile = TILEP_MONS_ZOMBIE_DRAGON;
            break;
        }
        else if (mons_genus(subtype) == MONS_DRAKE)
        {
            z_tile = TILEP_MONS_ZOMBIE_DRAKE;
            break;
        }
        else if (mons_genus(subtype) == MONS_HYDRA)
        {
            z_tile = TILEP_MONS_ZOMBIE_HYDRA
                     + min((int)mon.number, 5) - 1;
            break;
        }
        else if ((mons_genus(subtype) == MONS_GIANT_LIZARD
                  || mons_genus(subtype) == MONS_CROCODILE))
        {
            z_tile = TILEP_MONS_ZOMBIE_LIZARD;
            break;
        }
        else if (mons_genus(subtype) == MONS_RAT)
        {
            z_tile = TILEP_MONS_ZOMBIE_RAT;
            break;
        }
        else if (mons_genus(subtype) == MONS_HOUND)
        {
            z_tile = TILEP_MONS_ZOMBIE_HOUND;
            break;
        }
        // else fall-through
    case MON_SHAPE_QUADRUPED_TAILLESS:
        if (mons_base_char(subtype) == 'F')
            z_tile = TILEP_MONS_ZOMBIE_TOAD;
        else if (subtype == MONS_FIRE_CRAB || subtype == MONS_APOCALYPSE_CRAB)
            z_tile = TILEP_MONS_ZOMBIE_CRAB;
        else if (subtype == MONS_SNAPPING_TURTLE || subtype == MONS_ALLIGATOR_SNAPPING_TURTLE)
            z_tile = TILEP_MONS_ZOMBIE_TURTLE;
        else
        {
            z_tile = (z_size == Z_SMALL ? TILEP_MONS_ZOMBIE_QUADRUPED_SMALL
                      : TILEP_MONS_ZOMBIE_QUADRUPED_LARGE);
        }
        break;
    case MON_SHAPE_BAT:
        z_tile = TILEP_MONS_ZOMBIE_BAT;
        break;
    case MON_SHAPE_SNAIL:
    case MON_SHAPE_SNAKE:
        if (mons_genus(subtype) == MONS_WORM)
            z_tile = TILEP_MONS_ZOMBIE_WORM;
        else
            z_tile = TILEP_MONS_ZOMBIE_SNAKE;
        break;
    case MON_SHAPE_FISH:
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
    case MON_SHAPE_MISC:
        if (subtype == MONS_KRAKEN)
        {
            z_tile = TILEP_MONS_ZOMBIE_KRAKEN;
            break;
        }
        else if (mons_genus(subtype) == MONS_OCTOPODE)
        {
            z_tile = TILEP_MONS_ZOMBIE_OCTOPODE;
            break;
        }
        else if (mons_genus(subtype) == MONS_UGLY_THING)
        {
            z_tile = TILEP_MONS_ZOMBIE_UGLY_THING;
            break;
        }
    default:
        z_tile = TILEP_ERROR;
    }

    if (z_type == MONS_SKELETON)
        z_tile = _zombie_tile_to_skeleton(z_tile);

    if (z_type == MONS_SPECTRAL_THING)
        z_tile = _zombie_tile_to_spectral(z_tile);

    if (z_type == MONS_SIMULACRUM)
        z_tile = _zombie_tile_to_simulacrum(z_tile);

    return z_tile;
}

// Special case for *taurs which have a different tile
// for when they have a bow.
static int _bow_offset(const monster_info& mon)
{
    if (!mon.inv[MSLOT_WEAPON].get())
        return 1;

    switch (mon.inv[MSLOT_WEAPON]->sub_type)
    {
    case WPN_BOW:
    case WPN_LONGBOW:
    case WPN_CROSSBOW:
        return 0;
    default:
        return 1;
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
    return (tile + min(max(offset, 0), count - 1));
}

static tileidx_t _mon_random(tileidx_t tile)
{
    int count = tile_player_count(tile);
    return (tile + random2(count));
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
    return (tile + ((offset + you.frame_no) % count));
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
    case MONS_SENSED_FRIENDLY:
        return TILE_MONS_SENSED_FRIENDLY;
    case MONS_SENSED_TRIVIAL:
        return TILE_MONS_SENSED_TRIVIAL;
    case MONS_SENSED_EASY:
        return TILE_MONS_SENSED_EASY;
    case MONS_SENSED_TOUGH:
        return TILE_MONS_SENSED_TOUGH;
    case MONS_SENSED_NASTY:
        return TILE_MONS_SENSED_NASTY;

    // insects ('a')
    case MONS_GIANT_COCKROACH:
        return TILEP_MONS_GIANT_COCKROACH;
    case MONS_WORKER_ANT:
        return TILEP_MONS_WORKER_ANT;
    case MONS_SOLDIER_ANT:
        return TILEP_MONS_SOLDIER_ANT;
    case MONS_QUEEN_ANT:
        return TILEP_MONS_QUEEN_ANT;

    // bats and birds ('b')
    case MONS_BAT:
        return TILEP_MONS_BAT;
    case MONS_VAMPIRE_BAT:
        return TILEP_MONS_VAMPIRE_BAT;
    case MONS_BUTTERFLY:
        return _mon_mod(TILEP_MONS_BUTTERFLY, colour);
    case MONS_FIRE_BAT:
        return TILEP_MONS_FIRE_BAT;
    case MONS_RAVEN:
        return TILEP_MONS_RAVEN;
    case MONS_PHOENIX:
        return TILEP_MONS_PHOENIX;
    case MONS_CHAOS_BUTTERFLY:
        return _mon_random(TILEP_MONS_CHAOS_BUTTERFLY);

    // centaurs ('c')
    case MONS_CENTAUR:
        return TILEP_MONS_CENTAUR;
    case MONS_CENTAUR_WARRIOR:
        return TILEP_MONS_CENTAUR_WARRIOR;
    case MONS_YAKTAUR:
        return TILEP_MONS_YAKTAUR;
    case MONS_YAKTAUR_CAPTAIN:
        return TILEP_MONS_YAKTAUR_CAPTAIN;
    case MONS_FAUN:
        return TILEP_MONS_FAUN;
    case MONS_SATYR:
        return TILEP_MONS_SATYR;

    // draconians ('d'):
    case MONS_DRACONIAN:
        return TILEP_DRACO_BASE;

    // elves ('e')
    case MONS_ELF:
        return TILEP_MONS_ELF;
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
        return _mon_mod(TILEP_MONS_FUNGUS, tile_num_prop);
    case MONS_WANDERING_MUSHROOM:
        return TILEP_MONS_WANDERING_MUSHROOM;
    case MONS_DEATHCAP:
        return TILEP_MONS_DEATHCAP;

    // goblins ('g')
    case MONS_GOBLIN:
        return TILEP_MONS_GOBLIN;
    case MONS_HOBGOBLIN:
        return TILEP_MONS_HOBGOBLIN;
    case MONS_GNOLL:
        return TILEP_MONS_GNOLL;
    case MONS_GNOLL_SHAMAN:
        return TILEP_MONS_GNOLL_SHAMAN;
    case MONS_GNOLL_SERGEANT:
        return TILEP_MONS_GNOLL_SERGEANT;
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
    case MONS_SPIRIT_WOLF:
        return TILEP_MONS_SPIRIT_WOLF;
    case MONS_HOG:
        return TILEP_MONS_HOG;
    case MONS_HELL_HOUND:
        return TILEP_MONS_HELL_HOUND;
    case MONS_HELL_HOG:
        return TILEP_MONS_HELL_HOG;
    case MONS_HOLY_SWINE:
        return TILEP_MONS_HOLY_SWINE;
    case MONS_FELID:
        return TILEP_MONS_FELID;

    // slugs ('j')
    case MONS_ELEPHANT_SLUG:
        return TILEP_MONS_ELEPHANT_SLUG;
    case MONS_GIANT_SLUG:
        return TILEP_MONS_GIANT_SLUG;

    // killer bees ('k')
    case MONS_KILLER_BEE:
        return TILEP_MONS_KILLER_BEE;
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
    case MONS_BASILISK:
        return TILEP_MONS_BASILISK;
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
    case MONS_WIND_DRAKE:
        return TILEP_MONS_WIND_DRAKE;

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
    case MONS_DRYAD:
        return TILEP_MONS_DRYAD;
    case MONS_WATER_NYMPH:
        return TILEP_MONS_WATER_NYMPH;

    // rotting monsters ('n')
    case MONS_BOG_BODY:
        return TILEP_MONS_BOG_BODY;
    case MONS_NECROPHAGE:
        return TILEP_MONS_NECROPHAGE;
    case MONS_GHOUL:
        return TILEP_MONS_GHOUL;
    case MONS_PLAGUE_SHAMBLER:
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
    case MONS_LOST_SOUL:
        return TILEP_MONS_LOST_SOUL;

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
    case MONS_PORCUPINE:
        return TILEP_MONS_PORCUPINE;

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
    case MONS_ORB_SPIDER:
        return TILEP_MONS_ORB_SPIDER;

    // turtles and crocodiles ('t')
    case MONS_CROCODILE:
        return TILEP_MONS_CROCODILE;
    case MONS_BABY_ALLIGATOR:
        return TILEP_MONS_BABY_ALLIGATOR;
    case MONS_ALLIGATOR:
        return TILEP_MONS_ALLIGATOR;
    case MONS_FIRE_CRAB:
        return TILEP_MONS_FIRE_CRAB;

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
        return _mon_cycle(TILEP_MONS_FIRE_VORTEX, tile_num_prop);
    case MONS_SPATIAL_VORTEX:
        return _mon_cycle(TILEP_MONS_SPATIAL_VORTEX, tile_num_prop);
    case MONS_SPATIAL_MAELSTROM:
        return _mon_cycle(TILEP_MONS_SPATIAL_MAELSTROM, tile_num_prop);
    case MONS_TWISTER:
        return _mon_cycle(TILEP_MONS_TWISTER, tile_num_prop);

    // elementals ('E')
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
    case MONS_ELEMENTAL_WELLSPRING:
        return TILEP_MONS_ELEMENTAL_WELLSPRING;

    // worms ('w')
    case MONS_WORM:
        return TILEP_MONS_WORM;
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
        return _mon_mod(TILEP_MONS_ABOMINATION_SMALL, tile_num_prop);
    case MONS_CRAWLING_CORPSE:
        return TILEP_MONS_CRAWLING_CORPSE;
    case MONS_MACABRE_MASS:
        return TILEP_MONS_MACABRE_MASS;

    // abyssal monsters
    case MONS_LURKING_HORROR:
        return TILEP_MONS_LURKING_HORROR;
    case MONS_ANCIENT_ZYME:
        return TILEP_MONS_ANCIENT_ZYME;
    case MONS_APOCALYPSE_CRAB:
        return TILEP_MONS_APOCALYPSE_CRAB;
    case MONS_STARCURSED_MASS:
        return TILEP_MONS_STARCURSED_MASS;
    case MONS_TENTACLED_STARSPAWN:
        return TILEP_MONS_TENTACLED_STARSPAWN;
    case MONS_THRASHING_HORROR:
        return TILEP_MONS_THRASHING_HORROR;
    case MONS_WRETCHED_STAR:
        return TILEP_MONS_WRETCHED_STAR;

    // flying insects ('y')
    case MONS_YELLOW_WASP:
        return TILEP_MONS_YELLOW_WASP;
    case MONS_VAMPIRE_MOSQUITO:
        return TILEP_MONS_VAMPIRE_MOSQUITO;
    case MONS_RED_WASP:
        return TILEP_MONS_RED_WASP;
    case MONS_GHOST_MOTH:
        return TILEP_MONS_GHOST_MOTH;
    case MONS_MOTH_OF_WRATH:
        return TILEP_MONS_MOTH_OF_WRATH;
    case MONS_MOTH_OF_SUPPRESSION:
        return TILEP_MONS_MOTH_OF_SUPPRESSION;

    // small zombies, etc. ('z')
    case MONS_ZOMBIE:
    case MONS_ZOMBIE_SMALL:
        return TILEP_MONS_ZOMBIE_SMALL;
    case MONS_SIMULACRUM:
    case MONS_SIMULACRUM_SMALL:
        return TILEP_MONS_SIMULACRUM_SMALL;
    case MONS_SKELETON:
    case MONS_SKELETON_SMALL:
        return TILEP_MONS_SKELETON_SMALL;
    case MONS_WIGHT:
        return TILEP_MONS_WIGHT;
    case MONS_SKELETAL_WARRIOR:
        return TILEP_MONS_SKELETAL_WARRIOR;
    case MONS_ANCIENT_CHAMPION:
        return TILEP_MONS_ANCIENT_CHAMPION;
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
    case MONS_SERAPH:
        return TILEP_MONS_SERAPH;
    case MONS_DAEVA:
        return TILEP_MONS_DAEVA;
    case MONS_PROFANE_SERVITOR:
        return TILEP_MONS_PROFANE_SERVITOR;
    case MONS_MENNAS:
        return TILEP_MONS_MENNAS;

    // beetles ('B')
    case MONS_GOLIATH_BEETLE:
        return TILEP_MONS_GOLIATH_BEETLE;
    case MONS_BOULDER_BEETLE:
        return TILEP_MONS_BOULDER_BEETLE;
    case MONS_BORING_BEETLE:
        return TILEP_MONS_BORING_BEETLE;

    // cyclopes and giants ('C')
    case MONS_GIANT:
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
        return TILEP_MONS_SERPENT_OF_HELL_GEHENNA;
    case MONS_SERPENT_OF_HELL_COCYTUS:
        return TILEP_MONS_SERPENT_OF_HELL_COCYTUS;
    case MONS_SERPENT_OF_HELL_DIS:
        return TILEP_MONS_SERPENT_OF_HELL_DIS;
    case MONS_SERPENT_OF_HELL_TARTARUS:
        return TILEP_MONS_SERPENT_OF_HELL_TARTARUS;
    case MONS_PEARL_DRAGON:
        return TILEP_MONS_PEARL_DRAGON;

    // efreet ('E')
    case MONS_EFREET:
        return TILEP_MONS_EFREET;

    // frogs ('F')
    case MONS_GIANT_FROG:
        return TILEP_MONS_GIANT_FROG;
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
    case MONS_TENGU:
        return TILEP_MONS_TENGU;
    case MONS_TENGU_CONJURER:
        return TILEP_MONS_TENGU_CONJURER;
    case MONS_TENGU_WARRIOR:
        return TILEP_MONS_TENGU_WARRIOR;
    case MONS_TENGU_REAVER:
        return TILEP_MONS_TENGU_REAVER;

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
    case MONS_REVENANT:
        return TILEP_MONS_REVENANT;
    case MONS_UNBORN:
        return TILEP_MONS_UNBORN;

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
        return _mon_mod(TILEP_MONS_PLANT, tile_num_prop);
    case MONS_BUSH:
        return _mon_mod(TILEP_MONS_BUSH, tile_num_prop);
    case MONS_BURNING_BUSH:
        return TILEP_MONS_BUSH_BURNING;
    case MONS_OKLOB_SAPLING:
        return TILEP_MONS_OKLOB_SAPLING;
    case MONS_OKLOB_PLANT:
        return TILEP_MONS_OKLOB_PLANT;
    case MONS_THORN_LOTUS:
        return TILEP_MONS_THORN_LOTUS;
    case MONS_THORN_HUNTER:
        return TILEP_MONS_THORN_HUNTER;
    case MONS_TREANT:
        return TILEP_MONS_TREANT;

    // rakshasa ('R')
    case MONS_RAKSHASA:
    case MONS_RAKSHASA_FAKE:
        return TILEP_MONS_RAKSHASA;

    // snakes ('S')
    case MONS_BALL_PYTHON:
        return TILEP_MONS_BALL_PYTHON;
    case MONS_ADDER:
        return TILEP_MONS_ADDER;
    case MONS_WATER_MOCCASIN:
        return TILEP_MONS_WATER_MOCCASIN;
    case MONS_BLACK_MAMBA:
        return TILEP_MONS_BLACK_MAMBA;
    case MONS_ANACONDA:
        return TILEP_MONS_ANACONDA;
    case MONS_SEA_SNAKE:
        return TILEP_MONS_SEA_SNAKE;

    // trolls ('T')
    case MONS_TROLL:
        return TILEP_MONS_TROLL;
#if TAG_MAJOR_VERSION == 34
    case MONS_ROCK_TROLL:
#endif
    case MONS_IRON_TROLL:
        return TILEP_MONS_IRON_TROLL;
    case MONS_DEEP_TROLL:
        return TILEP_MONS_DEEP_TROLL;
    case MONS_DEEP_TROLL_EARTH_MAGE:
        return TILEP_MONS_DEEP_TROLL_EARTH_MAGE;
    case MONS_DEEP_TROLL_SHAMAN:
        return TILEP_MONS_DEEP_TROLL_SHAMAN;

    // bears ('U')
    case MONS_GRIZZLY_BEAR:
        return TILEP_MONS_GRIZZLY_BEAR;
    case MONS_POLAR_BEAR:
        return TILEP_MONS_POLAR_BEAR;
    case MONS_BLACK_BEAR:
        return TILEP_MONS_BLACK_BEAR;
    case MONS_ANCIENT_BEAR:
        return TILEP_MONS_ANCIENT_BEAR;

    // vampires ('V')
    case MONS_VAMPIRE:
        return TILEP_MONS_VAMPIRE;
    case MONS_VAMPIRE_KNIGHT:
        return TILEP_MONS_VAMPIRE_KNIGHT;
    case MONS_VAMPIRE_MAGE:
        return TILEP_MONS_VAMPIRE_MAGE;
    case MONS_JIANGSHI:
        return TILEP_MONS_JIANGSHI;
    case MONS_JORY:
        return TILEP_MONS_JORY;

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
        return _mon_mod(TILEP_MONS_ABOMINATION_LARGE, tile_num_prop);
    case MONS_TENTACLED_MONSTROSITY:
        return TILEP_MONS_TENTACLED_MONSTROSITY;
    case MONS_ORB_GUARDIAN:
        return TILEP_MONS_ORB_GUARDIAN;
    case MONS_OCTOPODE:
        return TILEP_MONS_OCTOPODE;
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
    case MONS_CATOBLEPAS:
        return TILEP_MONS_CATOBLEPAS;

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
    case MONS_HELLBINDER:
        return TILEP_MONS_HELLBINDER;
    case MONS_CLOUD_MAGE:
        return TILEP_MONS_CLOUD_MAGE;
    case MONS_VAULT_GUARD:
        return TILEP_MONS_VAULT_GUARD;
    case MONS_VAULT_WARDEN:
        return TILEP_MONS_VAULT_WARDEN;
    case MONS_VAULT_SENTINEL:
        return TILEP_MONS_VAULT_SENTINEL;
    case MONS_IRONBRAND_CONVOKER:
        return TILEP_MONS_IRONBRAND_CONVOKER;
    case MONS_IRONHEART_PRESERVER:
        return TILEP_MONS_IRONHEART_PRESERVER;
    case MONS_SHAPESHIFTER:
        return TILEP_MONS_SHAPESHIFTER;
    case MONS_GLOWING_SHAPESHIFTER:
        return TILEP_MONS_GLOWING_SHAPESHIFTER;
    case MONS_KILLER_KLOWN:
        return _mon_mod(TILEP_MONS_KILLER_KLOWN, colour);
    case MONS_SLAVE:
        return TILEP_MONS_SLAVE;
    case MONS_DEMONSPAWN:
        return TILEP_MONS_DEMONSPAWN;
    case MONS_DEMIGOD:
        return TILEP_MONS_DEMIGOD;
    case MONS_HALFLING:
        return TILEP_MONS_HALFLING;

    // mimics
    case MONS_ITEM_MIMIC:
        return 0;
    case MONS_DANCING_WEAPON:
        return TILE_UNSEEN_WEAPON;

    // Feature mimics actually get drawn with the dungeon code.
    // See tileidx_feature.
    case MONS_FEATURE_MIMIC:
        return 0;

    // '5' demons
    case MONS_CRIMSON_IMP:
        return TILEP_MONS_CRIMSON_IMP;
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
    case MONS_SHADOW_IMP:
        return TILEP_MONS_SHADOW_IMP;

    // '4' demons
    case MONS_RED_DEVIL:
        return TILEP_MONS_RED_DEVIL;
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
        return _mon_random(TILEP_MONS_CHAOS_SPAWN);

    // '2' demon
    case MONS_HELL_BEAST:
        return TILEP_MONS_HELL_BEAST;
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
    case MONS_BRIMSTONE_FIEND:
        return TILEP_MONS_BRIMSTONE_FIEND;
    case MONS_ICE_FIEND:
        return TILEP_MONS_ICE_FIEND;
    case MONS_SHADOW_FIEND:
        return TILEP_MONS_SHADOW_FIEND;
    case MONS_HELL_SENTINEL:
        return TILEP_MONS_HELL_SENTINEL;
    case MONS_EXECUTIONER:
        return TILEP_MONS_EXECUTIONER;
    case MONS_GREEN_DEATH:
        return TILEP_MONS_GREEN_DEATH;
    case MONS_BLIZZARD_DEMON:
        return TILEP_MONS_BLIZZARD_DEMON;
    case MONS_BALRUG:
        return TILEP_MONS_BALRUG;
    case MONS_CACODEMON:
        return TILEP_MONS_CACODEMON;
    case MONS_IGNACIO:
        return TILEP_MONS_IGNACIO;

    // non-living creatures
    // golems ('8')
    case MONS_CLAY_GOLEM:
        return TILEP_MONS_CLAY_GOLEM;
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

    // statues and statue-like things (also '8')
    case MONS_PILLAR_OF_SALT:
        return TILEP_MONS_PILLAR_OF_SALT;
    case MONS_TRAINING_DUMMY:
        return TILEP_MONS_TRAINING_DUMMY;
    case MONS_ICE_STATUE:
        return TILEP_MONS_ICE_STATUE;
    case MONS_SILVER_STATUE:
        return TILEP_MONS_SILVER_STATUE;
    case MONS_ORANGE_STATUE:
        return TILEP_MONS_ORANGE_STATUE;
    case MONS_DIAMOND_OBELISK:
        return TILEP_MONS_DIAMOND_OBELISK;

    // rods in a coil jutting out of ground ('{')
    case MONS_LIGHTNING_SPIRE:
        return TILEP_MONS_LIGHTNING_SPIRE;

    // gargoyles ('9')
    case MONS_GARGOYLE:
        return TILEP_MONS_GARGOYLE;
    case MONS_METAL_GARGOYLE:
        return TILEP_MONS_METAL_GARGOYLE;
    case MONS_MOLTEN_GARGOYLE:
        return TILEP_MONS_MOLTEN_GARGOYLE;

    // major demons ('&')
    case MONS_PANDEMONIUM_LORD:
        return TILEP_MONS_PANDEMONIUM_LORD;

    // ball lightning / orb of fire ('*')
    case MONS_BALL_LIGHTNING:
        return TILEP_MONS_BALL_LIGHTNING;
    case MONS_ORB_OF_FIRE:
        return TILEP_MONS_ORB_OF_FIRE;
    case MONS_ORB_OF_DESTRUCTION:
        return _mon_random(TILEP_MONS_ORB_OF_DESTRUCTION);
    case MONS_BATTLESPHERE:
        return TILEP_MONS_BATTLESPHERE;
    case MONS_FULMINANT_PRISM:
        return _mon_random(TILEP_MONS_FULMINANT_PRISM);
    case MONS_SILVER_STAR:
        return TILEP_MONS_SILVER_STAR;

    // other symbols
    case MONS_VAPOUR:
        return TILEP_MONS_VAPOUR;
    case MONS_SHADOW:
        return TILEP_MONS_SHADOW;
    case MONS_DEATH_COB:
        return TILEP_MONS_DEATH_COB;
    case MONS_SPECTRAL_WEAPON:
        return TILEP_MONS_SPECTRAL_SBL;

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
    case MONS_FANNAR:
        return TILEP_MONS_FANNAR;

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
    case MONS_SPRIGGAN_AIR_MAGE:
        return TILEP_MONS_SPRIGGAN_AIR_MAGE;
    case MONS_SPRIGGAN_BERSERKER:
        return TILEP_MONS_SPRIGGAN_BERSERKER;
    case MONS_SPRIGGAN_DEFENDER:
        return TILEP_MONS_SPRIGGAN_DEFENDER;
    case MONS_SPRIGGAN_ASSASSIN:
        return TILEP_MONS_SPRIGGAN_ASSASSIN;
    case MONS_SPRIGGAN_ENCHANTER:
        return TILEP_MONS_SPRIGGAN_ENCHANTER;
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
    case MONS_DEEP_DWARF:
        return TILEP_MONS_DEEP_DWARF;
    case MONS_DEEP_DWARF_BERSERKER:
        return TILEP_MONS_DEEP_DWARF_BERSERKER;
    case MONS_DEEP_DWARF_DEATH_KNIGHT:
        return TILEP_MONS_DEEP_DWARF_DEATH_KNIGHT;
    case MONS_JORGRUN:
        return TILEP_MONS_JORGRUN;
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
    case MONS_IRON_GIANT:
        return TILEP_MONS_IRON_GIANT;
    case MONS_ANTAEUS:
        return TILEP_MONS_ANTAEUS;

    // dragons and hydras ('D')
    case MONS_LERNAEAN_HYDRA:
        // Step down the number of heads to get the appropriate tile:
        // For the last five heads, use tiles 1-5, for greater amounts
        // use the next tile for every 5 more heads.
        return _mon_clamp(TILEP_MONS_LERNAEAN_HYDRA,
                          number <= 5 ? number - 1
                                      : 4 + (number - 1)/5);
    case MONS_XTAHUA:
        return TILEP_MONS_XTAHUA;

    // efreet ('E')
    case MONS_AZRAEL:
        return TILEP_MONS_AZRAEL;

    // frog ('F')
    case MONS_PRINCE_RIBBIT:
        return TILEP_MONS_PRINCE_RIBBIT;

    // hybrid ('H')
    case MONS_ARACHNE:
        return TILEP_MONS_ARACHNE;
    case MONS_SOJOBO:
        return TILEP_MONS_SOJOBO;

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
    case MONS_LAMIA:
        return TILEP_MONS_LAMIA;

    // ogre ('O')
    case MONS_EROLCHA:
        return TILEP_MONS_EROLCHA;

    // rakshasas ('R')
    case MONS_MARA:
    case MONS_MARA_FAKE:
        return TILEP_MONS_MARA;

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
    TYPE_STARSPAWN = 2
};

static void _add_tentacle_overlay(const coord_def pos,
                                  const main_dir dir,
                                  tentacle_type type)
{
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
        case NORTH: // SW
            if (type == TYPE_KRAKEN)
                flag = TILE_FLAG_KRAKEN_SW;
            else if (type == TYPE_ELDRITCH)
                flag = TILE_FLAG_ELDRITCH_SW;
            else
                flag = TILE_FLAG_STARSPAWN_SW;
            break;
        case EAST: // NW
            if (type == TYPE_KRAKEN)
                flag = TILE_FLAG_KRAKEN_NW;
            else if (type == TYPE_ELDRITCH)
                flag = TILE_FLAG_ELDRITCH_NW;
            else
                flag = TILE_FLAG_STARSPAWN_NW;
            break;
        case SOUTH: // NE
            if (type == TYPE_KRAKEN)
                flag = TILE_FLAG_KRAKEN_NE;
            else if (type == TYPE_ELDRITCH)
                flag = TILE_FLAG_ELDRITCH_NE;
            else
                flag = TILE_FLAG_STARSPAWN_NE;
            break;
        case WEST: // SE
            if (type == TYPE_KRAKEN)
                flag = TILE_FLAG_KRAKEN_SE;
            else if (type == TYPE_ELDRITCH)
                flag = TILE_FLAG_ELDRITCH_SE;
            else
                flag = TILE_FLAG_STARSPAWN_SE;
            break;
        default:
            die("invalid direction");
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

static bool _mons_is_kraken_tentacle(const int mtype)
{
    return (mtype == MONS_KRAKEN_TENTACLE
            || mtype == MONS_KRAKEN_TENTACLE_SEGMENT);
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
            return TYPE_KRAKEN;

        default:
            ASSERT("Invalid tentacle type!");
            return TYPE_KRAKEN; // Silence a warning
    }
}

static tileidx_t _tileidx_tentacle(const monster_info& mon)
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

    // Tentacle only requires checking of head position.
    if (mons_is_tentacle(mon.type))
    {
        if (no_head_connect)
        {
            if (_mons_is_kraken_tentacle(mon.type)
                || mon.type == MONS_SNAPLASHER_VINE
                || mon.type == MONS_SNAPLASHER_VINE_SEGMENT)
                return _mon_random(TILEP_MONS_KRAKEN_TENTACLE_WATER);
            return _mon_random(TILEP_MONS_ELDRITCH_TENTACLE_PORTAL);
        }

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

    if (no_head_connect || no_next_connect)
    {
        // One segment end goes into water, the other
        // into the direction of head or next.
        const coord_def s_pos = (no_head_connect ? n_pos : h_pos);

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
        die("impossible kraken direction");
    }

    // Okay, neither head nor next are submerged.
    // Compare all three positions.

    // Straight lines first: Vertical.
    if (h_pos.x == t_pos.x && t_pos.x == n_pos.x)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_S;
    // Horizontal.
    if (h_pos.y == t_pos.y && t_pos.y == n_pos.y)
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_W;
    // Diagonals.
    if (h_pos.x < t_pos.x && t_pos.x < n_pos.x
           && h_pos.y < t_pos.y && t_pos.y < n_pos.y
        || n_pos.x < t_pos.x && t_pos.x < h_pos.x
           && n_pos.y < t_pos.y && t_pos.y < h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SE;
    }
    if (n_pos.x < t_pos.x && t_pos.x < h_pos.x
           && h_pos.y < t_pos.y && t_pos.y < n_pos.y
        || h_pos.x < t_pos.x && t_pos.x < n_pos.x
           && n_pos.y < t_pos.y && t_pos.y < h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SW;
    }

    // Curved segments.
    if (h_pos.x > t_pos.x && h_pos.y == t_pos.y
           && t_pos.x == n_pos.x && t_pos.y > n_pos.y
        || n_pos.x > t_pos.x && n_pos.y == t_pos.y
           && t_pos.x == h_pos.x && t_pos.y > h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_N;
    }
    if (h_pos.x > t_pos.x && h_pos.y == t_pos.y
           && t_pos.x == n_pos.x && t_pos.y < n_pos.y
        || n_pos.x > t_pos.x && n_pos.y == t_pos.y
           && t_pos.x == h_pos.x && t_pos.y < h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_S;
    }
    if (h_pos.x < t_pos.x && h_pos.y == t_pos.y
           && t_pos.x == n_pos.x && t_pos.y < n_pos.y
        || n_pos.x < t_pos.x && n_pos.y == t_pos.y
           && t_pos.x == h_pos.x && t_pos.y < h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_W;
    }
    if (h_pos.x < t_pos.x && h_pos.y == t_pos.y
           && t_pos.x == n_pos.x && t_pos.y > n_pos.y
        || n_pos.x < t_pos.x && n_pos.y == t_pos.y
           && t_pos.x == h_pos.x && t_pos.y > h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_W;
    }
    if (h_pos.x < t_pos.x && h_pos.y < t_pos.y
           && t_pos.x < n_pos.x && t_pos.y > n_pos.y
        || n_pos.x < t_pos.x && n_pos.y < t_pos.y
           && t_pos.x < h_pos.x && t_pos.y > h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_NW;
    }
    if (h_pos.x < t_pos.x && h_pos.y > t_pos.y
           && t_pos.x < n_pos.x && t_pos.y < n_pos.y
        || n_pos.x < t_pos.x && n_pos.y > t_pos.y
           && t_pos.x < h_pos.x && t_pos.y < h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_SE_SW;
    }
    if (h_pos.x < t_pos.x && h_pos.y < t_pos.y
           && t_pos.x > n_pos.x && t_pos.y < n_pos.y
        || n_pos.x < t_pos.x && n_pos.y < t_pos.y
           && t_pos.x > h_pos.x && t_pos.y < h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NW_SW;
    }
    if (h_pos.x > t_pos.x && h_pos.y < t_pos.y
           && t_pos.x < n_pos.x && t_pos.y < n_pos.y
        || n_pos.x > t_pos.x && n_pos.y < t_pos.y
           && t_pos.x < h_pos.x && t_pos.y < h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_NE_SE;
    }

    // Connect corners and edges.
    if (h_pos.x == t_pos.x && h_pos.y < t_pos.y
           && t_pos.x > n_pos.x && t_pos.y < n_pos.y
        || n_pos.x == t_pos.x && n_pos.y < t_pos.y
           && t_pos.x > h_pos.x && t_pos.y < h_pos.y)
    {
        _add_tentacle_overlay(t_pos, WEST, _get_tentacle_type(mon.type));
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SW;
    }
    if (h_pos.x == t_pos.x && h_pos.y < t_pos.y
           && t_pos.x < n_pos.x && t_pos.y < n_pos.y
        || n_pos.x == t_pos.x && n_pos.y < t_pos.y
           && t_pos.x < h_pos.x && t_pos.y < h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N_SE;
    }
    if (h_pos.x == t_pos.x && h_pos.y > t_pos.y
           && t_pos.x > n_pos.x && t_pos.y > n_pos.y
        || n_pos.x == t_pos.x && n_pos.y > t_pos.y
           && t_pos.x > h_pos.x && t_pos.y > h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NW;
    }
    if (h_pos.x == t_pos.x && h_pos.y > t_pos.y
           && t_pos.x < n_pos.x && t_pos.y > n_pos.y
        || n_pos.x == t_pos.x && n_pos.y > t_pos.y
           && t_pos.x < h_pos.x && t_pos.y > h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_S_NE;
    }
    if (h_pos.x < t_pos.x && h_pos.y == t_pos.y
           && t_pos.x < n_pos.x && t_pos.y > n_pos.y
        || n_pos.x < t_pos.x && n_pos.y == t_pos.y
           && t_pos.x < h_pos.x && t_pos.y > h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_NE;
    }
    if (h_pos.x < t_pos.x && h_pos.y == t_pos.y
           && t_pos.x < n_pos.x && t_pos.y < n_pos.y
        || n_pos.x < t_pos.x && n_pos.y == t_pos.y
           && t_pos.x < h_pos.x && t_pos.y < h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SE;
    }
    if (h_pos.x > t_pos.x && h_pos.y == t_pos.y
           && t_pos.x > n_pos.x && t_pos.y > n_pos.y
        || n_pos.x > t_pos.x && n_pos.y == t_pos.y
           && t_pos.x > h_pos.x && t_pos.y > h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_NW;
    }
    if (h_pos.x > t_pos.x && h_pos.y == t_pos.y
           && t_pos.x > n_pos.x && t_pos.y < n_pos.y
        || n_pos.x > t_pos.x && n_pos.y == t_pos.y
           && t_pos.x > h_pos.x && t_pos.y < h_pos.y)
    {
        return TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_E_SW;
    }

    return TILEP_MONS_PROGRAM_BUG;
}

static bool _tentacle_tile_not_flying(tileidx_t tile)
{
    // All tiles between these two enums feature tentacles
    // emerging from water.
    return (tile >= TILEP_FIRST_TENTACLE_IN_WATER
            && tile <= TILEP_LAST_TENTACLE_IN_WATER);
}

static tileidx_t _tileidx_monster_no_props(const monster_info& mon)
{
    bool in_water = feat_is_water(env.map_knowledge(mon.pos).feat());

    // Show only base class for detected monsters.
    if (mons_class_is_zombified(mon.type))
        return _tileidx_monster_zombified(mon);
    else if (mon.props.exists("monster_tile"))
    {
        tileidx_t t = mon.props["monster_tile"].get_short();
        if (t == TILEP_MONS_STATUE_GUARDIAN)
            return _mon_random(t);
        else if (t == TILEP_MONS_HELL_WIZARD)
            return _mon_sinus(t);
        else
            return t;
    }
    else
    {
        int tile_num = 0;
        if (mon.props.exists("tile_num"))
            tile_num = mon.props["tile_num"].get_short();

        int type = mon.type;

        if (type == MONS_CHIMERA)
            type = mon.base_type;

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
        case MONS_SLAVE:
            return TILEP_MONS_SLAVE + (mon.mname == "freed slave" ? 1 : 0);
        case MONS_BUSH:
            if (env.map_knowledge(mon.pos).cloud() == CLOUD_FIRE)
                return TILEP_MONS_BUSH_BURNING;
            else
                return _mon_mod(TILEP_MONS_BUSH, tile_num);
        case MONS_BALLISTOMYCETE:
            if (mon.number)
                return TILEP_MONS_BALLISTOMYCETE_ACTIVE;
            else
                return TILEP_MONS_BALLISTOMYCETE_INACTIVE;
            break;
        case MONS_HYPERACTIVE_BALLISTOMYCETE:
            return TILEP_MONS_HYPERACTIVE_BALLISTOMYCETE;

        case MONS_SNAPPING_TURTLE:
            return TILEP_MONS_SNAPPING_TURTLE
                    + (mon.is(MB_WITHDRAWN) ? 1 : 0);
        case MONS_ALLIGATOR_SNAPPING_TURTLE:
            return TILEP_MONS_ALLIGATOR_SNAPPING_TURTLE
                    + (mon.is(MB_WITHDRAWN) ? 1 : 0);
        case MONS_AGATE_SNAIL:
            return TILEP_MONS_AGATE_SNAIL
                    + (mon.is(MB_WITHDRAWN) ? 1 : 0);

        case MONS_BOULDER_BEETLE:
            return (mon.is(MB_ROLLING)
                    ? _mon_random(TILEP_MONS_BOULDER_BEETLE_ROLLING)
                    : TILEP_MONS_BOULDER_BEETLE);

        case MONS_INEPT_ITEM_MIMIC:
            return tileidx_item(*mon.get_mimic_item()) | TILE_FLAG_MIMIC_INEPT;
        case MONS_ITEM_MIMIC:
            return tileidx_item(*mon.get_mimic_item()) | TILE_FLAG_MIMIC;
        case MONS_RAVENOUS_ITEM_MIMIC:
#if TAG_MAJOR_VERSION == 34
        case MONS_MONSTROUS_ITEM_MIMIC:
#endif
            return tileidx_item(*mon.get_mimic_item()) | TILE_FLAG_MIMIC_RAVEN;

        // Feature mimics get drawn with the dungeon, see tileidx_feature.
        case MONS_INEPT_FEATURE_MIMIC:
            return TILE_FLAG_MIMIC_INEPT;
        case MONS_FEATURE_MIMIC:
            return TILE_FLAG_MIMIC;
        case MONS_RAVENOUS_FEATURE_MIMIC:
            return TILE_FLAG_MIMIC_RAVEN;

        case MONS_DANCING_WEAPON:
        {
            // Use item tile.
            const item_def& item = *mon.inv[MSLOT_WEAPON];
            return tileidx_item(item) | TILE_FLAG_ANIM_WEP;
        }

        case MONS_SPECTRAL_WEAPON:
        {
            // Tiles exist for each class of weapon.
            const item_def& item = *mon.inv[MSLOT_WEAPON];
            switch (weapon_skill(item))
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
            tileidx_t tile = _tileidx_tentacle(mon);
            _handle_tentacle_overlay(mon.pos, tile, _get_tentacle_type(mon.type));

            if ((!_mons_is_kraken_tentacle(mon.type)
                 && mon.type != MONS_SNAPLASHER_VINE
                 && mon.type != MONS_SNAPLASHER_VINE_SEGMENT)
                && tile >= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N
                && tile <= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_W_SE)
            {

                tile += TILEP_MONS_ELDRITCH_TENTACLE_PORTAL_N;
                tile -= TILEP_MONS_KRAKEN_TENTACLE_SEGMENT_N;

                if (mon.type == MONS_STARSPAWN_TENTACLE
                    || mon.type == MONS_STARSPAWN_TENTACLE_SEGMENT)
                {
                    tile += TILEP_MONS_STARSPAWN_TENTACLE_N;
                    tile -= TILEP_MONS_ELDRITCH_TENTACLE_N;
                }
            }

            return tile;
        }

        case MONS_ARACHNE:
        {
            // Arachne normally is drawn with her staff wielded two-handed,
            // but will use a regular stance if she picks up a shield
            // (enhancer staves are compatible with those) or Xom does his
            // weapon swap trick.
            const item_def* weapon = mon.inv[MSLOT_WEAPON].get();
            if (!mon.inv[MSLOT_SHIELD].get() && weapon
                && (weapon->base_type == OBJ_STAVES
                      && weapon->sub_type == STAFF_POISON
                    || weapon->base_type == OBJ_WEAPONS
                      && weapon->special == UNRAND_OLGREB))
            {
                return TILEP_MONS_ARACHNE;
            }
            else
                return TILEP_MONS_ARACHNE_STAVELESS;
        }

        case MONS_SENSED:
        {
            // Should be always out of LOS, though...
            const tileidx_t t = _tileidx_monster_base(type, in_water,
                                    mon.colour, mon.number, tile_num);
            if (t == TILEP_MONS_PROGRAM_BUG)
                return TILE_UNSEEN_MONSTER;
            return t;
        }

        default:
            return _tileidx_monster_base(type, in_water, mon.colour,
                                         mon.number, tile_num);
        }
    }
}

tileidx_t tileidx_monster(const monster_info& mons)
{
    tileidx_t ch = _tileidx_monster_no_props(mons);

    if ((!mons.ground_level() && !_tentacle_tile_not_flying(ch))
        || (ch == TILEP_MONS_THORN_LOTUS))
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

    if (mons.attitude == ATT_FRIENDLY)
        ch |= TILE_FLAG_PET;
    else if (mons.attitude == ATT_GOOD_NEUTRAL)
        ch |= TILE_FLAG_GD_NEUTRAL;
    else if (mons.neutral())
        ch |= TILE_FLAG_NEUTRAL;
    else if (mons.is(MB_FLEEING))
        ch |= TILE_FLAG_FLEEING;
    else if (mons.is(MB_STABBABLE))
        ch |= TILE_FLAG_STAB;
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

    if (Options.tile_show_demon_tier)
    {
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
    }

    return ch;
}

tileidx_t tileidx_draco_base(const monster_info& mon)
{
    int draco = mon.draco_subspecies();
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

tileidx_t tileidx_draco_job(const monster_info& mon)
{
    switch (mon.type)
    {
        case MONS_DRACONIAN_CALLER:      return TILEP_DRACO_CALLER;
        case MONS_DRACONIAN_MONK:        return TILEP_DRACO_MONK;
        case MONS_DRACONIAN_ZEALOT:      return TILEP_DRACO_ZEALOT;
        case MONS_DRACONIAN_SHIFTER:     return TILEP_DRACO_SHIFTER;
        case MONS_DRACONIAN_ANNIHILATOR: return TILEP_DRACO_ANNIHILATOR;
        case MONS_DRACONIAN_KNIGHT:      return TILEP_DRACO_KNIGHT;
        case MONS_DRACONIAN_SCORCHER:    return TILEP_DRACO_SCORCHER;
        default:                         return 0;
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
    case WPN_DAGGER:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_DAGGER_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_DAGGER_DWARF;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_DAGGER_ELF;
        return TILE_WPN_DAGGER;

    case WPN_SHORT_SWORD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_SHORT_SWORD_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_SHORT_SWORD_DWARF;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_SHORT_SWORD_ELF;
        return TILE_WPN_SHORT_SWORD;

    case WPN_QUICK_BLADE:
        return TILE_WPN_QUICK_BLADE;

    case WPN_SABRE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_SABRE_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_SABRE_DWARF;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_SABRE_ELF;
        return TILE_WPN_SABRE;

    case WPN_FALCHION:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_FALCHION_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_FALCHION_DWARF;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_FALCHION_ELF;
        return TILE_WPN_FALCHION;

    case WPN_LONG_SWORD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_LONG_SWORD_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_LONG_SWORD_ELF;
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
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_HAND_AXE_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_HAND_AXE_DWARF;
        return TILE_WPN_HAND_AXE;

    case WPN_WAR_AXE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_WAR_AXE_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_WAR_AXE_DWARF;
        return TILE_WPN_WAR_AXE;

    case WPN_BROAD_AXE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_BROAD_AXE_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_BROAD_AXE_DWARF;
        return TILE_WPN_BROAD_AXE;

    case WPN_BATTLEAXE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_BATTLEAXE_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_BATTLEAXE_DWARF;
        return TILE_WPN_BATTLEAXE;

    case WPN_EXECUTIONERS_AXE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_EXECUTIONERS_AXE_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_EXECUTIONERS_AXE_DWARF;
        return TILE_WPN_EXECUTIONERS_AXE;

    case WPN_BLOWGUN:
        return TILE_WPN_BLOWGUN;

    case WPN_SLING:
        return TILE_WPN_SLING;

    case WPN_BOW:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_BOW_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_BOW_ELF;
        return TILE_WPN_BOW;

    case WPN_CROSSBOW:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_CROSSBOW_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_CROSSBOW_DWARF;
        return TILE_WPN_CROSSBOW;

    case WPN_SPEAR:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_SPEAR_ORC;
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_SPEAR_ELF;
        return TILE_WPN_SPEAR;

    case WPN_TRIDENT:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_TRIDENT_ORC;
        return TILE_WPN_TRIDENT;

    case WPN_HALBERD:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_HALBERD_ORC;
        return TILE_WPN_HALBERD;

    case WPN_SCYTHE:
        return TILE_WPN_SCYTHE;

    case WPN_GLAIVE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_GLAIVE_ORC;
        return TILE_WPN_GLAIVE;

#if TAG_MAJOR_VERSION == 34
    case WPN_STAFF:
        return TILE_WPN_STAFF;
#endif
    case WPN_QUARTERSTAFF:
        return TILE_WPN_QUARTERSTAFF;

    case WPN_CLUB:
        return TILE_WPN_CLUB;

    case WPN_HAMMER:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_HAMMER_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_HAMMER_DWARF;
        return TILE_WPN_HAMMER;

    case WPN_MACE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_MACE_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_MACE_DWARF;
        return TILE_WPN_MACE;

    case WPN_FLAIL:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_FLAIL_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_FLAIL_DWARF;
        return TILE_WPN_FLAIL;

    case WPN_GREAT_MACE:
        return TILE_WPN_GREAT_MACE;

    case WPN_DIRE_FLAIL:
        return TILE_WPN_DIRE_FLAIL;

    case WPN_MORNINGSTAR:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_MORNINGSTAR_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_MORNINGSTAR_DWARF;
        return TILE_WPN_MORNINGSTAR;

    case WPN_EVENINGSTAR:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_EVENINGSTAR_ORC;
        if (race == ISFLAG_DWARVEN)
            return TILE_WPN_EVENINGSTAR_DWARF;
        return TILE_WPN_EVENINGSTAR;

    case WPN_GIANT_CLUB:
        return TILE_WPN_GIANT_CLUB;

    case WPN_GIANT_SPIKED_CLUB:
        return TILE_WPN_GIANT_SPIKED_CLUB;

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
        if (race == ISFLAG_ELVEN)
            return TILE_WPN_LONGBOW_ELF;
        return TILE_WPN_LONGBOW;

    case WPN_LAJATANG:
        return TILE_WPN_LAJATANG;

    case WPN_BARDICHE:
        if (race == ISFLAG_ORCISH)
            return TILE_WPN_BARDICHE_ORC;
        return TILE_WPN_BARDICHE;

    case WPN_BLESSED_FALCHION:
        return TILE_WPN_FALCHION;

    case WPN_BLESSED_LONG_SWORD:
        return TILE_WPN_LONG_SWORD;

    case WPN_BLESSED_SCIMITAR:
        return TILE_WPN_SCIMITAR;

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
    case MI_PIE:          return TILE_MI_PIE;

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
    return tileidx_enchant_equ(item, tile);
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

    case ARM_PLATE_ARMOUR:
        if (race == ISFLAG_ORCISH)
            return TILE_ARM_PLATE_ARMOUR_ORC;
        return TILE_ARM_PLATE_ARMOUR;

    case ARM_CRYSTAL_PLATE_ARMOUR:
        return TILE_ARM_CRYSTAL_PLATE_ARMOUR;

    case ARM_SHIELD:
        return TILE_ARM_SHIELD;

    case ARM_CLOAK:
        return TILE_ARM_CLOAK;

    case ARM_WIZARD_HAT:
        return TILE_THELM_WIZARD_HAT;

    case ARM_CAP:
        return TILE_THELM_CAP;

    case ARM_HELMET:
        switch (get_helmet_desc(item))
        {
        case THELM_DESC_HORNED:
            return TILE_THELM_HELM_HORNED;
        case THELM_DESC_SPIKED:
            return TILE_THELM_HELM_SPIKED;
        case THELM_DESC_PLUMED:
            return TILE_THELM_HELM_PLUMED;
        case THELM_DESC_WINGED:
            return TILE_THELM_HELM_WINGED;
        default:
            return TILE_THELM_HELM;
        }

    case ARM_GLOVES:
        return TILE_ARM_GLOVES;

    case ARM_BOOTS:
        return TILE_ARM_BOOTS;

    case ARM_BUCKLER:
        if (race == ISFLAG_DWARVEN)
            return TILE_ARM_BUCKLER_DWARVEN;
        else if (race == ISFLAG_ELVEN)
            return TILE_ARM_BUCKLER_ELVEN;
        else if (race == ISFLAG_ORCISH)
            return TILE_ARM_BUCKLER_ORCISH;
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

    case ARM_STORM_DRAGON_HIDE:
        return TILE_ARM_STORM_DRAGON_HIDE;

    case ARM_STORM_DRAGON_ARMOUR:
        return TILE_ARM_STORM_DRAGON_ARMOUR;

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
    case SIZE_HUGE:
    case SIZE_GIANT:
        cs = 3; break;

    }

    switch (get_mon_shape(item.mon_type))
    {
    case MON_SHAPE_HUMANOID:
    case MON_SHAPE_HUMANOID_TAILED:
    case MON_SHAPE_HUMANOID_WINGED:
    case MON_SHAPE_HUMANOID_WINGED_TAILED:
        return (TILE_FOOD_BONE_HUMANOID + cs);
    default:
        return (TILE_FOOD_BONE + cs);
    }
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
    case MONS_WORKER_ANT:
        return TILE_CORPSE_WORKER_ANT;
    case MONS_SOLDIER_ANT:
        return TILE_CORPSE_SOLDIER_ANT;
    case MONS_QUEEN_ANT:
        return TILE_CORPSE_QUEEN_ANT;

    // bats and birds ('b')
    case MONS_BAT:
        return TILE_CORPSE_BAT;
    case MONS_BUTTERFLY:
        return TILE_CORPSE_BUTTERFLY;
    case MONS_RAVEN:
        return TILE_CORPSE_RAVEN;
    case MONS_PHOENIX:
        return TILE_CORPSE_PHOENIX;

    // centaurs ('c')
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
        return TILE_CORPSE_CENTAUR;
    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        return TILE_CORPSE_YAKTAUR;
    case MONS_FAUN:
        return TILE_CORPSE_FAUN;
    case MONS_SATYR:
        return TILE_CORPSE_SATYR;

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
    case MONS_GREY_DRACONIAN:
        return TILE_CORPSE_DRACONIAN_GREY;
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
    case MONS_GNOLL_SHAMAN:
    case MONS_GNOLL_SERGEANT:
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
    case MONS_HOG:
        return TILE_CORPSE_HOG;
    case MONS_HELL_HOUND:
        return TILE_CORPSE_HELL_HOUND;
    case MONS_HELL_HOG:
        return TILE_CORPSE_HELL_HOG;
    case MONS_HOLY_SWINE:
        return TILE_CORPSE_HOLY_SWINE;
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
    case MONS_BASILISK:
        return TILE_CORPSE_BASILISK;
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
    case MONS_WIND_DRAKE:
        return TILE_CORPSE_WIND_DRAKE;

    // merfolk ('m')
    case MONS_MERFOLK:
        return TILE_CORPSE_MERFOLK;
    case MONS_MERMAID:
        return TILE_CORPSE_MERMAID;
    case MONS_SIREN:
        return TILE_CORPSE_SIREN;
    case MONS_DRYAD:
        return TILE_CORPSE_DRYAD;
    case MONS_WATER_NYMPH:
        return TILE_CORPSE_WATER_NYMPH;

    // rotting monsters ('n')
    case MONS_BOG_BODY:
        return TILE_CORPSE_BOG_BODY;
    case MONS_NECROPHAGE:
        return TILE_CORPSE_NECROPHAGE;
    case MONS_GHOUL:
        return TILE_CORPSE_GHOUL;
    case MONS_PLAGUE_SHAMBLER:
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
        return TILE_CORPSE_DWARF;
    case MONS_DEEP_DWARF:
        return TILE_CORPSE_DEEP_DWARF;
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
    case MONS_PORCUPINE:
        return TILE_CORPSE_PORCUPINE;

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
    case MONS_ORB_SPIDER:
        return TILE_CORPSE_ORB_SPIDER;

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
    case MONS_FIRE_CRAB:
        return TILE_CORPSE_FIRE_CRAB;
    case MONS_APOCALYPSE_CRAB:
        return TILE_CORPSE_APOCALYPSE_CRAB;

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
    case MONS_VAMPIRE_MOSQUITO:
        return TILE_CORPSE_VAMPIRE_MOSQUITO;
    case MONS_YELLOW_WASP:
        return TILE_CORPSE_YELLOW_WASP;
    case MONS_RED_WASP:
        return TILE_CORPSE_RED_WASP;
    case MONS_GHOST_MOTH:
        return TILE_CORPSE_GHOST_MOTH;
    case MONS_MOTH_OF_WRATH:
        return TILE_CORPSE_MOTH_OF_WRATH;
    case MONS_MOTH_OF_SUPPRESSION:
        return TILE_CORPSE_MOTH_OF_SUPPRESSION;

    // beetles ('B')
    case MONS_GOLIATH_BEETLE:
        return TILE_CORPSE_GOLIATH_BEETLE;
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
    case MONS_PEARL_DRAGON:
        return TILE_CORPSE_PEARL_DRAGON;

    // frogs ('F')
    case MONS_GIANT_FROG:
        return TILE_CORPSE_GIANT_FROG;
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
    case MONS_TENGU:
        return TILE_CORPSE_TENGU;
    case MONS_SPHINX:
        return TILE_CORPSE_SPHINX;
    case MONS_SHEDU:
        return TILE_CORPSE_SHEDU;
    case MONS_ARACHNE:
        return TILE_CORPSE_ARACHNE;

    // beasts ('I')
    case MONS_SKY_BEAST:
        return TILE_CORPSE_SKY_BEAST;

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
    case MONS_BALL_PYTHON:
        return TILE_CORPSE_BALL_PYTHON;
    case MONS_ADDER:
        return TILE_CORPSE_ADDER;
    case MONS_ANACONDA:
        return TILE_CORPSE_ANACONDA;
    case MONS_WATER_MOCCASIN:
        return TILE_CORPSE_WATER_MOCCASIN;
    case MONS_BLACK_MAMBA:
        return TILE_CORPSE_BLACK_MAMBA;
    case MONS_SEA_SNAKE:
        return TILE_CORPSE_SEA_SNAKE;

    // trolls ('T')
    case MONS_TROLL:
        return TILE_CORPSE_TROLL;
#if TAG_MAJOR_VERSION == 34
    case MONS_ROCK_TROLL:
#endif
    case MONS_IRON_TROLL:
        return TILE_CORPSE_IRON_TROLL;
    case MONS_DEEP_TROLL:
    case MONS_DEEP_TROLL_SHAMAN:
    case MONS_DEEP_TROLL_EARTH_MAGE:
        return TILE_CORPSE_DEEP_TROLL;

    // bears ('U')
    case MONS_GRIZZLY_BEAR:
        return TILE_CORPSE_GRIZZLY_BEAR;
    case MONS_POLAR_BEAR:
        return TILE_CORPSE_POLAR_BEAR;
    case MONS_BLACK_BEAR:
        return TILE_CORPSE_BLACK_BEAR;
    case MONS_ANCIENT_BEAR:
        return TILE_CORPSE_ANCIENT_BEAR;

    // seafood ('X')
    case MONS_OCTOPODE:
        return TILE_CORPSE_OCTOPODE;
    case MONS_KRAKEN:
        return TILE_CORPSE_KRAKEN;

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
    case MONS_CATOBLEPAS:
        return TILE_CORPSE_CATOBLEPAS;
    case MONS_APIS:
        return TILE_CORPSE_APIS;

    // water monsters
    case MONS_BIG_FISH:
        return TILE_CORPSE_BIG_FISH;
    case MONS_GIANT_GOLDFISH:
        return TILE_CORPSE_GIANT_GOLDFISH;
    case MONS_ELECTRIC_EEL:
        return TILE_CORPSE_ELECTRIC_EEL;
    case MONS_SHARK:
        return TILE_CORPSE_SHARK;
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

    case RUNE_DEMONIC:     return TILE_MISC_RUNE_DEMONIC
        + ((uint32_t)item.special) % tile_main_count(TILE_MISC_RUNE_DEMONIC);
    case RUNE_ABYSSAL:     return TILE_MISC_RUNE_ABYSS;

    case RUNE_SNAKE_PIT:   return TILE_MISC_RUNE_SNAKE;
    case RUNE_SPIDER_NEST: return TILE_MISC_RUNE_SPIDER;
    case RUNE_SLIME_PITS:  return TILE_MISC_RUNE_SLIME;
    case RUNE_VAULTS:      return TILE_MISC_RUNE_VAULTS;
    case RUNE_TOMB:        return TILE_MISC_RUNE_TOMB;
    case RUNE_SWAMP:       return TILE_MISC_RUNE_SWAMP;
    case RUNE_SHOALS:      return TILE_MISC_RUNE_SHOALS;
    case RUNE_ELVEN_HALLS: return TILE_MISC_RUNE_ELVEN;

    case RUNE_FOREST:
    default:               return TILE_MISC_RUNE_OF_ZOT;
    }
}

static tileidx_t _tileidx_misc(const item_def &item)
{
    switch (item.sub_type)
    {
    case MISC_BOTTLED_EFREET:
        return TILE_MISC_BOTTLED_EFREET;

    case MISC_FAN_OF_GALES:
        return TILE_MISC_AIR_ELEMENTAL_FAN;

    case MISC_LAMP_OF_FIRE:
        return TILE_MISC_LAMP_OF_FIRE;

    case MISC_STONE_OF_TREMORS:
        return TILE_MISC_STONE_OF_EARTH_ELEMENTALS;

    case MISC_PHIAL_OF_FLOODS:
        return TILE_MISC_PHIAL_OF_FLOODS;

    case MISC_LANTERN_OF_SHADOWS:
        return TILE_MISC_LANTERN_OF_SHADOWS;

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

    case MISC_DECK_OF_ESCAPE:
    case MISC_DECK_OF_DESTRUCTION:
    case MISC_DECK_OF_DUNGEONS:
    case MISC_DECK_OF_SUMMONING:
    case MISC_DECK_OF_WONDERS:
    case MISC_DECK_OF_PUNISHMENT:
    case MISC_DECK_OF_WAR:
    case MISC_DECK_OF_CHANGES:
    case MISC_DECK_OF_DEFENCE:
    case NUM_MISCELLANY: // indicates unidentified deck
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

    int clas    = item.base_type;
    int type    = item.sub_type;
    int special = item.special;
    int colour  = item.colour;

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
            return TILE_WAND_OFFSET + special % NDSC_WAND_PRI;

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
        else if (type <= NUM_RINGS)
        {
            if (is_artefact(item))
            {
                int offset = item.rnd % tile_main_count(TILE_RING_RANDART_OFFSET);
                return TILE_RING_RANDART_OFFSET + offset;
            }
            else if (item.flags & ISFLAG_KNOW_TYPE)
                return TILE_RING_ID_FIRST + type - RING_FIRST_RING;
            else
                return TILE_RING_NORMAL_OFFSET + special % NDSC_JEWEL_PRI;
        }
        else
        {
            if (is_artefact(item))
            {
                int offset = item.rnd % tile_main_count(TILE_AMU_RANDOM_OFFSET);
                return TILE_AMU_RANDOM_OFFSET + offset;
            }
            else if (item.flags & ISFLAG_KNOW_TYPE)
                return TILE_AMU_ID_FIRST + type - AMU_FIRST_AMULET;
            else
                return TILE_AMU_NORMAL_OFFSET + special % NDSC_JEWEL_PRI;
        }

    case OBJ_POTIONS:
        if (item.flags & ISFLAG_KNOW_TYPE)
            return TILE_POT_ID_FIRST + type;
        else
            return TILE_POTION_OFFSET + item.plus % NDSC_POT_PRI;

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
        if (item.flags & ISFLAG_KNOW_TYPE)
            return TILE_STAFF_ID_FIRST + type;

        return TILE_STAFF_OFFSET + (special/ NDSC_STAVE_PRI) % NDSC_STAVE_SEC;

    case OBJ_RODS:
        return TILE_ROD + item.rnd % tile_main_count(TILE_ROD);

    case OBJ_CORPSES:
        if (item.sub_type == CORPSE_SKELETON)
            return _tileidx_bone(item);
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
                break;
            case MI_PIE:
                ch = TILE_MI_PIE0;
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
            return tileidx_enchant_equ(item, ch);
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

        if (get_ident_type(OBJ_POTIONS, type) != ID_KNOWN_TYPE)
            return TILE_UNSEEN_POTION;
        else
            return (TILE_POTION_OFFSET + desc);
    }

    if (label >= TILE_RING_ID_FIRST && label <= TILE_RING_ID_LAST)
    {
        int type = label - TILE_RING_ID_FIRST + RING_FIRST_RING;
        int desc = you.item_description[IDESC_RINGS][type] % NDSC_JEWEL_PRI;

        if (get_ident_type(OBJ_JEWELLERY, type) != ID_KNOWN_TYPE)
            return TILE_UNSEEN_RING;
        else
            return (TILE_RING_NORMAL_OFFSET + desc);
    }

    if (label >= TILE_AMU_ID_FIRST && label <= TILE_AMU_ID_LAST)
    {
        int type = label - TILE_AMU_ID_FIRST + AMU_FIRST_AMULET;
        int desc = you.item_description[IDESC_RINGS][type] % NDSC_JEWEL_PRI;

        if (get_ident_type(OBJ_JEWELLERY, type) != ID_KNOWN_TYPE)
            return TILE_UNSEEN_AMULET;
        else
            return (TILE_AMU_NORMAL_OFFSET + desc);
    }

    if (label >= TILE_SCR_ID_FIRST && label <= TILE_SCR_ID_LAST)
        return TILE_SCROLL;

    if (label >= TILE_WAND_ID_FIRST && label <= TILE_WAND_ID_LAST)
    {
        int type = label - TILE_WAND_ID_FIRST;
        int desc = you.item_description[IDESC_WANDS][type] % NDSC_WAND_PRI;

        if (get_ident_type(OBJ_WANDS, type) != ID_KNOWN_TYPE)
            return TILE_UNSEEN_WAND;
        else
            return (TILE_WAND_OFFSET + desc);
    }

    if (label >= TILE_STAFF_ID_FIRST && label <= TILE_STAFF_ID_LAST)
    {
        int type = label - TILE_STAFF_ID_FIRST;
        int desc = you.item_description[IDESC_STAVES][type];
        desc = (desc / NDSC_STAVE_PRI) % NDSC_STAVE_SEC;

        if (get_ident_type(OBJ_STAVES, type) != ID_KNOWN_TYPE)
            return TILE_UNSEEN_STAFF;
        else
            return (TILE_STAFF_OFFSET + desc);
    }

    return 0;
}

tileidx_t tileidx_cloud(const cloud_info &cl, bool disturbance)
{
    int type  = cl.type;
    int colour = cl.colour;

    tileidx_t ch = cl.tile;
    int dur = cl.duration;

    if (type != CLOUD_MAGIC_TRAIL && dur > 2)
        dur = 2;

    if (ch == 0)
    {
        switch (type)
        {
            case CLOUD_FIRE:
                ch = TILE_CLOUD_FIRE_0 + dur;
                break;

            case CLOUD_COLD:
                ch = TILE_CLOUD_COLD_0 + dur;
                break;

            case CLOUD_POISON:
                ch = TILE_CLOUD_POISON_0 + dur;
                break;

            case CLOUD_MEPHITIC:
                ch = TILE_CLOUD_MEPHITIC_0 + dur;
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
                ch = TILE_CLOUD_MAGIC_TRAIL_0 + dur;
                break;

            case CLOUD_DUST_TRAIL:
                ch = TILE_CLOUD_DUST_TRAIL_0 + dur;
                break;

            case CLOUD_INK:
                ch = TILE_CLOUD_INK;
                break;

            case CLOUD_GLOOM:
                ch = TILE_CLOUD_GLOOM;
                break;

            case CLOUD_TORNADO:
                ch = get_tornado_phase(cl.pos) ? TILE_CLOUD_RAGING_WINDS_0
                                               : TILE_CLOUD_RAGING_WINDS_1;
                break;

            case CLOUD_HOLY_FLAMES:
                ch = TILE_CLOUD_YELLOW_SMOKE;
                break;

            case CLOUD_PETRIFY:
                ch = TILE_CLOUD_PETRIFY;
                ch += random2(tile_main_count(ch));
                break;

            case CLOUD_CHAOS:
                ch = TILE_CLOUD_CHAOS;
                ch += random2(tile_main_count(ch));
                break;

            case CLOUD_FOREST_FIRE:
                ch = TILE_CLOUD_FOREST_FIRE;
                break;

            case CLOUD_GHOSTLY_FLAME:
                ch = TILE_CLOUD_GHOSTLY_FLAME_0 + dur;
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

    // XXX: Should be no need for TILE_FLAG_FLYING anymore since clouds are
    // drawn in a separate layer but I'll leave it for now in case anything changes --mumra
    return (ch | TILE_FLAG_FLYING);
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
        else if (bolt.name == "burst of hellfire")
            return TILE_BOLT_HELLFIRE;
        break;

    case LIGHTRED:
        if (bolt.name == "hellfire")
            return TILE_BOLT_HELLFIRE;
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
        return tile + random2(tile_main_count(tile));
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

    return (TILE_SYM_BOLT_OFS - 1 + colour);
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
    case SPELL_REPEL_MISSILES:           return TILEG_REPEL_MISSILES;
    case SPELL_MEPHITIC_CLOUD:           return TILEG_MEPHITIC_CLOUD;
    case SPELL_DISCHARGE:                return TILEG_STATIC_DISCHARGE;
    case SPELL_FLY:                      return TILEG_FLIGHT;
    case SPELL_LIGHTNING_BOLT:           return TILEG_LIGHTNING_BOLT;
    case SPELL_AIRSTRIKE:                return TILEG_AIRSTRIKE;
    case SPELL_SILENCE:                  return TILEG_SILENCE;
    case SPELL_DEFLECT_MISSILES:         return TILEG_DEFLECT_MISSILES;
    case SPELL_CONJURE_BALL_LIGHTNING:   return TILEG_CONJURE_BALL_LIGHTNING;
    case SPELL_CHAIN_LIGHTNING:          return TILEG_CHAIN_LIGHTNING;
    case SPELL_TORNADO:                  return TILEG_TORNADO;

    // Earth
    case SPELL_SANDBLAST:                return TILEG_SANDBLAST;
    case SPELL_STONESKIN:                return TILEG_STONESKIN;
    case SPELL_PASSWALL:                 return TILEG_PASSWALL;
    case SPELL_STONE_ARROW:              return TILEG_STONE_ARROW;
    case SPELL_DIG:                      return TILEG_DIG;
    case SPELL_BOLT_OF_MAGMA:            return TILEG_BOLT_OF_MAGMA;
    case SPELL_LRD:                      return TILEG_LEES_RAPID_DECONSTRUCTION;
    case SPELL_IRON_SHOT:                return TILEG_IRON_SHOT;
    case SPELL_LEDAS_LIQUEFACTION:       return TILEG_LEDAS_LIQUEFACTION;
    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:   return TILEG_LEHUDIBS_CRYSTAL_SPEAR;
    case SPELL_SHATTER:                  return TILEG_SHATTER;

    // Fire
    case SPELL_FLAME_TONGUE:             return TILEG_FLAME_TONGUE;
    case SPELL_FIRE_BRAND:               return TILEG_FIRE_BRAND;
    case SPELL_THROW_FLAME:              return TILEG_THROW_FLAME;
    case SPELL_CONJURE_FLAME:            return TILEG_CONJURE_FLAME;
    case SPELL_INNER_FLAME:              return TILEG_INNER_FLAME;
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
    case SPELL_VENOM_BOLT:               return TILEG_VENOM_BOLT;
    case SPELL_POISON_ARROW:             return TILEG_POISON_ARROW;
    case SPELL_POISONOUS_CLOUD:          return TILEG_POISONOUS_CLOUD;

    // Enchantment
    case SPELL_CONFUSING_TOUCH:          return TILEG_CONFUSING_TOUCH;
    case SPELL_CORONA:                   return TILEG_CORONA;
    case SPELL_SURE_BLADE:               return TILEG_SURE_BLADE;
    case SPELL_CONFUSE:                  return TILEG_CONFUSE;
    case SPELL_SLOW:                     return TILEG_SLOW;
    case SPELL_TUKIMAS_DANCE:            return TILEG_TUKIMAS_DANCE;
    case SPELL_ENSLAVEMENT:              return TILEG_ENSLAVEMENT;
    case SPELL_PETRIFY:                  return TILEG_PETRIFY;
    case SPELL_CAUSE_FEAR:               return TILEG_CAUSE_FEAR;
    case SPELL_HASTE:                    return TILEG_HASTE;
    case SPELL_INVISIBILITY:             return TILEG_INVISIBILITY;
    case SPELL_MASS_CONFUSION:           return TILEG_MASS_CONFUSION;
    case SPELL_DARKNESS:                 return TILEG_DARKNESS;
    case SPELL_INFUSION:                 return TILEG_INFUSION;
    case SPELL_SONG_OF_SLAYING:          return TILEG_SONG_OF_SLAYING;
    case SPELL_SPECTRAL_WEAPON:          return TILEG_SPECTRAL_WEAPON;
    case SPELL_SONG_OF_SHIELDING:        return TILEG_SONG_OF_SHIELDING;

    // Translocation
    case SPELL_APPORTATION:              return TILEG_APPORTATION;
    case SPELL_PORTAL_PROJECTILE:        return TILEG_PORTAL_PROJECTILE;
    case SPELL_BLINK:                    return TILEG_BLINK;
    case SPELL_CONTROL_TELEPORT:         return TILEG_CONTROLLED_TELEPORT;
    case SPELL_TELEPORT_OTHER:           return TILEG_TELEPORT_OTHER;
    case SPELL_PHASE_SHIFT:              return TILEG_PHASE_SHIFT;
    case SPELL_CONTROLLED_BLINK:         return TILEG_CONTROLLED_BLINK;
    case SPELL_WARP_BRAND:               return TILEG_WARP_WEAPON;
    case SPELL_DISJUNCTION:              return TILEG_DISJUNCTION;
    case SPELL_DISPERSAL:                return TILEG_DISPERSAL;
    case SPELL_GOLUBRIAS_PASSAGE:        return TILEG_PASSAGE_OF_GOLUBRIA;
    case SPELL_SHROUD_OF_GOLUBRIA:       return TILEG_SHROUD_OF_GOLUBRIA;

    // Summoning
    case SPELL_SUMMON_BUTTERFLIES:       return TILEG_SUMMON_BUTTERFLIES;
    case SPELL_SUMMON_SMALL_MAMMAL:      return TILEG_SUMMON_SMALL_MAMMAL;
    case SPELL_RECALL:                   return TILEG_RECALL;
    case SPELL_CALL_CANINE_FAMILIAR:     return TILEG_CALL_CANINE_FAMILIAR;
    case SPELL_CALL_IMP:                 return TILEG_CALL_IMP;
    case SPELL_ABJURATION:               return TILEG_ABJURATION;
    case SPELL_MASS_ABJURATION:          return TILEG_MASS_ABJURATION;
    case SPELL_SUMMON_SCORPIONS:         return TILEG_SUMMON_SCORPIONS;
    case SPELL_SUMMON_ELEMENTAL:         return TILEG_SUMMON_ELEMENTAL;
    case SPELL_SUMMON_DEMON:             return TILEG_SUMMON_DEMON;
    case SPELL_SUMMON_UGLY_THING:        return TILEG_SUMMON_UGLY_THING;
    case SPELL_SHADOW_CREATURES:         return TILEG_SUMMON_SHADOW_CREATURES;
    case SPELL_SUMMON_ICE_BEAST:         return TILEG_SUMMON_ICE_BEAST;
    case SPELL_DEMONIC_HORDE:            return TILEG_DEMONIC_HORDE;
    case SPELL_SUMMON_GREATER_DEMON:     return TILEG_SUMMON_GREATER_DEMON;
    case SPELL_SUMMON_HORRIBLE_THINGS:   return TILEG_SUMMON_HORRIBLE_THINGS;
    case SPELL_MALIGN_GATEWAY:           return TILEG_MALIGN_GATEWAY;
    case SPELL_SUMMON_DRAGON:            return TILEG_SUMMON_DRAGON;
    case SPELL_SUMMON_HYDRA:             return TILEG_SUMMON_HYDRA;

    // Necromancy
    case SPELL_ANIMATE_SKELETON:         return TILEG_ANIMATE_SKELETON;
    case SPELL_PAIN:                     return TILEG_PAIN;
    case SPELL_CORPSE_ROT:               return TILEG_CORPSE_ROT;
    case SPELL_LETHAL_INFUSION:          return TILEG_LETHAL_INFUSION;
    case SPELL_SUBLIMATION_OF_BLOOD:     return TILEG_SUBLIMATION_OF_BLOOD;
    case SPELL_VAMPIRIC_DRAINING:        return TILEG_VAMPIRIC_DRAINING;
    case SPELL_REGENERATION:             return TILEG_REGENERATION;
    case SPELL_ANIMATE_DEAD:             return TILEG_ANIMATE_DEAD;
    case SPELL_DISPEL_UNDEAD:            return TILEG_DISPEL_UNDEAD;
    case SPELL_HAUNT:                    return TILEG_HAUNT;
    case SPELL_BORGNJORS_REVIVIFICATION: return TILEG_BORGNJORS_REVIVIFICATION;
    case SPELL_AGONY:                    return TILEG_AGONY;
    case SPELL_TWISTED_RESURRECTION:     return TILEG_TWISTED_RESURRECTION;
    case SPELL_EXCRUCIATING_WOUNDS:      return TILEG_EXCRUCIATING_WOUNDS;
    case SPELL_CONTROL_UNDEAD:           return TILEG_CONTROL_UNDEAD;
    case SPELL_BOLT_OF_DRAINING:         return TILEG_BOLT_OF_DRAINING;
    case SPELL_DEATHS_DOOR:              return TILEG_DEATHS_DOOR;
    case SPELL_DEATH_CHANNEL:            return TILEG_DEATH_CHANNEL;

    // Transmutation
    case SPELL_BEASTLY_APPENDAGE:        return TILEG_BEASTLY_APPENDAGE;
    case SPELL_STICKS_TO_SNAKES:         return TILEG_STICKS_TO_SNAKES;
    case SPELL_SPIDER_FORM:              return TILEG_SPIDER_FORM;
    case SPELL_ICE_FORM:                 return TILEG_ICE_FORM;
    case SPELL_BLADE_HANDS:              return TILEG_BLADE_HANDS;
    case SPELL_POLYMORPH:                return TILEG_POLYMORPH;
    case SPELL_STATUE_FORM:              return TILEG_STATUE_FORM;
    case SPELL_DRAGON_FORM:              return TILEG_DRAGON_FORM;
    case SPELL_NECROMUTATION:            return TILEG_NECROMUTATION;

    // Conjurations
    case SPELL_MAGIC_DART:               return TILEG_MAGIC_DART;
    case SPELL_ISKENDERUNS_MYSTIC_BLAST: return TILEG_ISKENDERUNS_MYSTIC_BLAST;
    case SPELL_IOOD:                     return TILEG_IOOD;
    case SPELL_FORCE_LANCE:              return TILEG_FORCE_LANCE;
    case SPELL_BATTLESPHERE:             return TILEG_BATTLESPHERE;
    case SPELL_DAZZLING_SPRAY:           return TILEG_DAZZLING_SPRAY;
    case SPELL_FULMINANT_PRISM:          return TILEG_FULMINANT_PRISM;
    case SPELL_SEARING_RAY:              return TILEG_SEARING_RAY;


    // --------------------------------------------
    // Rods and abilities (tiles needed for later)
    // Abilities
    case SPELL_SMITING:       // Beogh power
    case SPELL_MINOR_HEALING: // Ely power
    case SPELL_MAJOR_HEALING: // Ely power
    case SPELL_HELLFIRE:      // Demonspawn ability

    // Rod-only spells
    case SPELL_PARALYSE:      // we could reuse Petrify for this
    case SPELL_BOLT_OF_INACCURACY:
    case SPELL_SUMMON_SWARM:
        return TILEG_TODO;

    // --------------------------------------------
    // Spells that don't need icons:
    case SPELL_DISINTEGRATE:  // wand and card

    // Monster spells (mostly?)
    case SPELL_HELLFIRE_BURST:
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
    case SPELL_MIASMA_BREATH:
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
    case SPELL_STICKY_FLAME_RANGE:
    default:
        return TILEG_ERROR;
    }
}

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
#if TAG_MAJOR_VERSION == 34
    case SK_STABBING:       ch = TILEG_STABBING_ON; break;
#endif
    case SK_SHIELDS:        ch = TILEG_SHIELDS_ON; break;
#if TAG_MAJOR_VERSION == 34
    case SK_TRAPS:          ch = TILEG_TRAPS_ON; break;
#endif
    case SK_UNARMED_COMBAT: ch = TILEG_UNARMED_COMBAT_ON; break;
    case SK_SPELLCASTING:   ch = TILEG_SPELLCASTING_ON; break;
    case SK_CONJURATIONS:   ch = TILEG_CONJURATIONS_ON; break;
    case SK_HEXES:          ch = TILEG_HEXES_ON; break;
    case SK_CHARMS:         ch = TILEG_CHARMS_ON; break;
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

    switch (train)
    {
    case 0: // disabled
        return ch + TILEG_FIGHTING_OFF - TILEG_FIGHTING_ON;
    case 1: // enabled
        return ch;
    case 2: // focused
        return ch + TILEG_FIGHTING_FOCUS - TILEG_FIGHTING_ON;
    case -1: // mastered
        return ch + TILEG_FIGHTING_MAX - TILEG_FIGHTING_ON;
    }

    die("invalid skill tile type");
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
    case GAME_TYPE_ZOTDEF:
        return TILEG_STARTUP_ZOTDEF;
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
    case ABIL_HELLFIRE:
        return TILEG_ABILITY_HELLFIRE;
    // Tengu, Draconians
    case ABIL_FLY:
        return TILEG_ABILITY_FLIGHT;
    case ABIL_STOP_FLYING:
        return TILEG_ABILITY_FLIGHT_END;
    // Mummies
    case ABIL_MUMMY_RESTORATION:
        return TILEG_ABILITY_MUMMY_RESTORATION;
    // Vampires
    case ABIL_TRAN_BAT:
        return TILEG_ABILITY_BAT_FORM;
    case ABIL_BOTTLE_BLOOD:
        return TILEG_ABILITY_BOTTLE_BLOOD;
    // Deep Dwarves
    case ABIL_RECHARGING:
        return TILEG_ABILITY_RECHARGE;

    // Evoking items.
    case ABIL_EVOKE_BERSERK:
        return TILEG_ABILITY_EVOKE_BERSERK;
    case ABIL_EVOKE_TELEPORTATION:
        return TILEG_ABILITY_EVOKE_TELEPORT;
    case ABIL_EVOKE_TELEPORT_CONTROL:
        return TILEG_ABILITY_EVOKE_TELEPORT_CONTROL;
    case ABIL_EVOKE_BLINK:
        return TILEG_ABILITY_EVOKE_BLINK;
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
    // TSO
    case ABIL_TSO_DIVINE_SHIELD:
        return TILEG_ABILITY_TSO_DIVINE_SHIELD;
    case ABIL_TSO_CLEANSING_FLAME:
        return TILEG_ABILITY_TSO_CLEANSING_FLAME;
    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
        return TILEG_ABILITY_TSO_DIVINE_WARRIOR;
    // Kiku
    case ABIL_KIKU_RECEIVE_CORPSES:
        return TILEG_ABILITY_KIKU_RECEIVE_CORPSES;
    case ABIL_KIKU_TORMENT:
        return TILEG_ABILITY_KIKU_TORMENT;
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
    case ABIL_YRED_ANIMATE_REMAINS_OR_DEAD:
        return TILEG_ABILITY_YRED_ANIMATE_DEAD;
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
    case ABIL_ELYVILON_LESSER_HEALING_SELF:
        return TILEG_ABILITY_ELYVILON_LESSER_SELF_HEALING;
    case ABIL_ELYVILON_LESSER_HEALING_OTHERS:
        return TILEG_ABILITY_ELYVILON_LESSER_HEALING;
    case ABIL_ELYVILON_PURIFICATION:
        return TILEG_ABILITY_ELYVILON_PURIFICATION;
    case ABIL_ELYVILON_GREATER_HEALING_SELF:
        return TILEG_ABILITY_ELYVILON_GREATER_SELF_HEALING;
    case ABIL_ELYVILON_GREATER_HEALING_OTHERS:
        return TILEG_ABILITY_ELYVILON_GREATER_HEALING;
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
    // Nemelex
    case ABIL_NEMELEX_DRAW_ONE:
        return TILEG_ABILITY_NEMELEX_DRAW_ONE;
    case ABIL_NEMELEX_PEEK_TWO:
        return TILEG_ABILITY_NEMELEX_PEEK_AT_TWO;
    case ABIL_NEMELEX_TRIPLE_DRAW:
        return TILEG_ABILITY_NEMELEX_TRIPLE_DRAW;
    case ABIL_NEMELEX_DEAL_FOUR:
        return TILEG_ABILITY_NEMELEX_DEAL_FOUR;
    case ABIL_NEMELEX_STACK_FIVE:
        return TILEG_ABILITY_NEMELEX_STACK_FIVE;
    // Beogh
    case ABIL_BEOGH_SMITING:
        return TILEG_ABILITY_BEOGH_SMITE;
    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
        return TILEG_ABILITY_BEOGH_RECALL;
    case ABIL_CONVERT_TO_BEOGH:
        return TILEG_ABILITY_CONVERT_TO_BEOGH;
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
    case ABIL_ASHENZARI_SCRYING:
        return TILEG_ABILITY_ASHENZARI_SCRY;
    case ABIL_ASHENZARI_TRANSFER_KNOWLEDGE:
        return TILEG_ABILITY_ASHENZARI_TRANSFER_KNOWLEDGE;
    case ABIL_ASHENZARI_END_TRANSFER:
        return TILEG_ABILITY_ASHENZARI_TRANSFER_KNOWLEDGE_END;

    // General divine (pseudo) abilities.
    case ABIL_RENOUNCE_RELIGION:
        return TILEG_ABILITY_RENOUNCE_RELIGION;

    // Zot Defence abilities
    case ABIL_MAKE_FUNGUS:
        return TILEG_ABILITY_ZOTDEF_FUNGUS;
    case ABIL_MAKE_PLANT:
        return TILEG_ABILITY_ZOTDEF_PLANT;
    case ABIL_MAKE_OKLOB_SAPLING:
        return TILEG_ABILITY_ZOTDEF_OKLOB_SAPLING;
    case ABIL_MAKE_DART_TRAP:
        return TILEG_ABILITY_ZOTDEF_DART_TRAP;
    case ABIL_MAKE_ICE_STATUE:
        return TILEG_ABILITY_ZOTDEF_ICE_STATUE;
    case ABIL_MAKE_OCS:
        return TILEG_ABILITY_ZOTDEF_OCS;
    case ABIL_MAKE_SILVER_STATUE:
        return TILEG_ABILITY_ZOTDEF_SILVER_STATUE;
    case ABIL_MAKE_CURSE_SKULL:
        return TILEG_ABILITY_ZOTDEF_CURSE_SKULL;
    case ABIL_MAKE_TELEPORT:
        return TILEG_ABILITY_ZOTDEF_TELEPORT;
    case ABIL_MAKE_ARROW_TRAP:
        return TILEG_ABILITY_ZOTDEF_ARROW_TRAP;
    case ABIL_MAKE_BOLT_TRAP:
        return TILEG_ABILITY_ZOTDEF_BOLT_TRAP;
    case ABIL_MAKE_SPEAR_TRAP:
        return TILEG_ABILITY_ZOTDEF_SPEAR_TRAP;
    case ABIL_MAKE_NEEDLE_TRAP:
        return TILEG_ABILITY_ZOTDEF_NEEDLE_TRAP;
    case ABIL_MAKE_NET_TRAP:
        return TILEG_ABILITY_ZOTDEF_NET_TRAP;
    case ABIL_MAKE_ALARM_TRAP:
        return TILEG_ABILITY_ZOTDEF_ALARM_TRAP;
    case ABIL_MAKE_BLADE_TRAP:
        return TILEG_ABILITY_ZOTDEF_BLADE_TRAP;
    case ABIL_MAKE_OKLOB_CIRCLE:
        return TILEG_ABILITY_ZOTDEF_OKLOB_CIRCLE;
    case ABIL_MAKE_ACQUIRE_GOLD:
        return TILEG_ABILITY_ZOTDEF_GOLD;
    case ABIL_MAKE_ACQUIREMENT:
        return TILEG_ABILITY_ZOTDEF_ACQUIREMENT;
    case ABIL_MAKE_WATER:
        return TILEG_ABILITY_ZOTDEF_WATER;
    case ABIL_MAKE_LIGHTNING_SPIRE:
        return TILEG_ABILITY_ZOTDEF_LIGHTNING_SPIRE;
    case ABIL_MAKE_BAZAAR:
        return TILEG_ABILITY_ZOTDEF_BAZAAR;
    case ABIL_MAKE_ALTAR:
        return TILEG_ABILITY_ZOTDEF_ALTAR;
    case ABIL_MAKE_GRENADES:
        return TILEG_ABILITY_ZOTDEF_SPORE;
    case ABIL_MAKE_SAGE:
        return TILEG_ABILITY_ZOTDEF_SAGE;
    case ABIL_MAKE_OKLOB_PLANT:
        return TILEG_ABILITY_ZOTDEF_OKLOB_PLANT;
    case ABIL_MAKE_BURNING_BUSH:
        return TILEG_ABILITY_ZOTDEF_BURNING_BUSH;
    case ABIL_REMOVE_CURSE:
        return TILEG_ABILITY_ZOTDEF_REMOVE_CURSE;

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
            return (TILE_BRAND_FLAMING + get_weapon_brand(item) - 1);
    }
    else if (item.base_type == OBJ_ARMOUR)
    {
        const int brand = get_armour_ego_type(item);
        if (brand != SPARM_NORMAL)
            return (TILE_BRAND_ARM_RUNNING + get_armour_ego_type(item) - 1);
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
#if TAG_MAJOR_VERSION == 34
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

    const bool rotten       = food_is_rotten(item);
    const bool saprovorous  = player_mutation_level(MUT_SAPROVOROUS);

    // Vampires are only interested in fresh blood.
    if (you.species == SP_VAMPIRE
        && (rotten || !mons_has_blood(item.mon_type)))
    {
        return TILE_FOOD_INEDIBLE;
    }

    // Rotten corpses' chunk effects are meaningless if we are not
    // saprovorous.
    if (rotten && !saprovorous)
        return TILE_FOOD_INEDIBLE;

    // Harmful chunk effects > religious rules > reduced nutrition.
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
tileidx_t get_clean_map_idx(tileidx_t tile_idx, bool mon_only)
{
    tileidx_t idx = tile_idx & TILE_FLAG_MASK;
    if (!mon_only && idx >= TILE_CLOUD_FIRE_0 && idx <= TILE_CLOUD_TLOC_ENERGY)
        return 0;

    if (idx >= TILEP_MONS_PANDEMONIUM_LORD && idx <= TILEP_MONS_TEST_SPAWNER
        || idx >= TILEP_MCACHE_START)
    {
        return 0;
    }

    return tile_idx;
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
    if (mon->type != MONS_TOADSTOOL && mon->type != MONS_SLAVE
        && mon->type != MONS_PLANT && mon->type != MONS_FUNGUS
        && mon->type != MONS_BUSH
        && mon->type != MONS_FIRE_VORTEX && mon->type != MONS_TWISTER
        && mon->type != MONS_SPATIAL_VORTEX && mon->type != MONS_SPATIAL_MAELSTROM
        && mon->type != MONS_ABOMINATION_SMALL
        && mon->type != MONS_ABOMINATION_LARGE)
    {
        return;
    }

    // Already overridden or set.
    if (mon->props.exists("monster_tile") || mon->props.exists("tile_num"))
        return;

    mon->props["tile_num"] = short(random2(256));
}
