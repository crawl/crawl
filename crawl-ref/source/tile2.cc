/*
 *  File:       tile2.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#ifdef USE_TILE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "branch.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "files.h"
#include "ghost.h"
#include "tilereg.h"
#include "itemprop.h"
#include "it_use2.h"
#include "place.h"
#include "player.h"
#include "spells3.h"
#include "stuff.h"
#include "tiles.h"
#include "transfor.h"

void tile_default_flv(level_area_type lev, branch_type br, tile_flavour &flv)
{
    flv.wall    = TILE_WALL_NORMAL;
    flv.floor   = TILE_FLOOR_NORMAL;
    flv.special = 0;

    if (lev == LEVEL_PANDEMONIUM)
    {
        flv.floor = TILE_FLOOR_TOMB;
        switch (random2(7))
        {
        default:
        case 0: flv.wall = TILE_WALL_ZOT_BLUE; break;
        case 1: flv.wall = TILE_WALL_ZOT_RED; break;
        case 2: flv.wall = TILE_WALL_ZOT_MAGENTA; break;
        case 3: flv.wall = TILE_WALL_ZOT_GREEN; break;
        case 4: flv.wall = TILE_WALL_ZOT_CYAN; break;
        case 5: flv.wall = TILE_WALL_ZOT_YELLOW; break;
        case 6: flv.wall = TILE_WALL_ZOT_GRAY; break;
        }

        if (one_chance_in(3))
            flv.wall = TILE_WALL_FLESH;
        if (one_chance_in(3))
            flv.floor = TILE_FLOOR_NERVES;

        return;
    }
    else if (lev == LEVEL_ABYSS)
    {
        flv.floor = TILE_FLOOR_NERVES;
        switch (random2(6))
        {
        default:
        case 0: flv.wall = TILE_WALL_HIVE; break;
        case 1: flv.wall = TILE_WALL_PEBBLE_RED; break;
        case 2: flv.wall = TILE_WALL_SLIME; break;
        case 3: flv.wall = TILE_WALL_ICE; break;
        case 4: flv.wall = TILE_WALL_HALL; break;
        case 5: flv.wall = TILE_WALL_UNDEAD; break;
        }
        return;
    }
    else if (lev == LEVEL_LABYRINTH)
    {
        flv.wall  = TILE_WALL_UNDEAD;
        flv.floor = TILE_FLOOR_TOMB;
        return;
    }
    else if (lev == LEVEL_PORTAL_VAULT)
    {
        // These should be handled in the respective lua files.
        flv.wall  = TILE_WALL_NORMAL;
        flv.floor = TILE_FLOOR_NORMAL;
        return;
    }

    switch (br)
    {
    case BRANCH_MAIN_DUNGEON:
        flv.wall  = TILE_WALL_NORMAL;
        flv.floor = TILE_FLOOR_NORMAL;
        return;

    case BRANCH_HIVE:
        flv.wall  = TILE_WALL_HIVE;
        flv.floor = TILE_FLOOR_HIVE;
        return;

    case BRANCH_VAULTS:
        flv.wall  = TILE_WALL_VAULT;
        flv.floor = TILE_FLOOR_VAULT;
        return;

    case BRANCH_ECUMENICAL_TEMPLE:
        flv.wall  = TILE_WALL_VINES;
        flv.floor = TILE_FLOOR_VINES;
        return;

    case BRANCH_ELVEN_HALLS:
    case BRANCH_HALL_OF_BLADES:
        flv.wall  = TILE_WALL_HALL;
        flv.floor = TILE_FLOOR_HALL;
        return;

    case BRANCH_TARTARUS:
    case BRANCH_CRYPT:
    case BRANCH_VESTIBULE_OF_HELL:
        flv.wall  = TILE_WALL_UNDEAD;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case BRANCH_TOMB:
        flv.wall  = TILE_WALL_TOMB;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case BRANCH_DIS:
        flv.wall  = TILE_WALL_ZOT_CYAN;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case BRANCH_GEHENNA:
        flv.wall  = TILE_WALL_ZOT_RED;
        flv.floor = TILE_FLOOR_ROUGH_RED;
        return;

    case BRANCH_COCYTUS:
        flv.wall  = TILE_WALL_ICE;
        flv.floor = TILE_FLOOR_ICE;
        return;

    case BRANCH_ORCISH_MINES:
        flv.wall  = TILE_WALL_ORC;
        flv.floor = TILE_FLOOR_ORC;
        return;

    case BRANCH_LAIR:
        flv.wall  = TILE_WALL_LAIR;
        flv.floor = TILE_FLOOR_LAIR;
        return;

    case BRANCH_SLIME_PITS:
        flv.wall  = TILE_WALL_SLIME;
        flv.floor = TILE_FLOOR_SLIME;
        return;

    case BRANCH_SNAKE_PIT:
        flv.wall  = TILE_WALL_SNAKE;
        flv.floor = TILE_FLOOR_SNAKE;
        return;

    case BRANCH_SWAMP:
        flv.wall  = TILE_WALL_SWAMP;
        flv.floor = TILE_FLOOR_SWAMP;
        return;

    case BRANCH_SHOALS:
        flv.wall = TILE_WALL_YELLOW_ROCK;
        flv.floor = TILE_FLOOR_SAND_STONE;
        return;

    case BRANCH_HALL_OF_ZOT:
        flv.wall  = TILE_WALL_ZOT_YELLOW;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case NUM_BRANCHES:
        break;
    }
}

void tile_init_default_flavour()
{
    tile_default_flv(you.level_type, you.where_are_you, env.tile_default);
}

int get_clean_map_idx(int tile_idx)
{
    int idx = tile_idx & TILE_FLAG_MASK;
    if (idx >= TILE_CLOUD_FIRE_0 && idx <= TILE_CLOUD_PURP_SMOKE
        || idx >= TILEP_MONS_SHADOW && idx <= TILEP_MONS_WATER_ELEMENTAL
        || idx >= TILEP_MCACHE_START)
    {
        return 0;
    }

    return tile_idx;
}

#endif
