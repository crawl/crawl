/*
 *  File:       tile2.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
 */

#include "AppHdr.h"

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

static int wall_flavours    = 0;
static int floor_flavours   = 0;
static int special_flavours = 0;
static int wall_tile_idx    = 0;
static int floor_tile_idx   = 0;
static int special_tile_idx = 0;

int get_num_wall_flavours()
{
    return wall_flavours;
}

int get_num_floor_flavours()
{
    return floor_flavours;
}

int get_num_floor_special_flavours()
{
    return special_flavours;
}

int get_wall_tile_idx()
{
    return wall_tile_idx;
}

int get_floor_tile_idx()
{
    return floor_tile_idx;
}

int get_floor_special_tile_idx()
{
    return special_tile_idx;
}

// TODO: Add this sort of determinism to the lua maps,
//       at least for the portal vaults.
void WallIdx(int &wall, int &floor, int &special)
{
    // Note: This function must be deterministic.

    special = -1;

    if (you.level_type == LEVEL_PANDEMONIUM)
    {
        switch (env.rock_colour)
        {
        case BLUE:
        case LIGHTBLUE:
            wall  = TILE_WALL_ZOT_BLUE;
            floor = TILE_FLOOR_TOMB;
            break;

        case RED:
        case LIGHTRED:
            wall  = TILE_WALL_ZOT_RED;
            floor = TILE_FLOOR_TOMB;
            break;

        case MAGENTA:
        case LIGHTMAGENTA:
            wall  = TILE_WALL_ZOT_MAGENTA;
            floor = TILE_FLOOR_TOMB;
            break;

        case GREEN:
        case LIGHTGREEN:
            wall  = TILE_WALL_ZOT_GREEN;
            floor = TILE_FLOOR_TOMB;
            break;

        case CYAN:
        case LIGHTCYAN:
            wall  = TILE_WALL_ZOT_CYAN;
            floor = TILE_FLOOR_TOMB;
            break;

        case BROWN:
        case YELLOW:
            wall  = TILE_WALL_ZOT_YELLOW;
            floor = TILE_FLOOR_TOMB;
            break;

        case BLACK:
        case WHITE:
        default:
            wall  = TILE_WALL_ZOT_GRAY;
            floor = TILE_FLOOR_TOMB;
            break;
        }

        unsigned int seen = you.get_place_info(LEVEL_PANDEMONIUM).levels_seen;

        if ((seen + env.rock_colour) % 3 == 1)
            wall = TILE_WALL_FLESH;
        if ((seen + env.floor_colour) % 3 == 1)
            floor = TILE_FLOOR_NERVES;

        return;
    }
    else if (you.level_type == LEVEL_ABYSS)
    {
        switch (env.rock_colour)
        {
        case YELLOW:
        case BROWN:
            wall = TILE_WALL_HIVE;
            break;
        case RED:
        case LIGHTRED:
            wall = TILE_WALL_PEBBLE_RED;
            break;
        case GREEN:
        case LIGHTGREEN:
            wall = TILE_WALL_SLIME;
            break;
        case BLUE:
            wall = TILE_WALL_ICE;
            break;
        case LIGHTGREY:
        case WHITE:
            wall = TILE_WALL_HALL;
            break;
        default:
            wall = TILE_WALL_UNDEAD;
            break;
        }
        floor = TILE_FLOOR_NERVES;
        return;
    }
    else if (you.level_type == LEVEL_LABYRINTH)
    {
        wall  = TILE_WALL_UNDEAD;
        floor = TILE_FLOOR_TOMB;
        return;
    }
    else if (you.level_type == LEVEL_PORTAL_VAULT)
    {
        if (you.level_type_name == "bazaar")
        {
            // Bazaar tiles here lean towards grass and dirt to emphasize
            // both the non-hostile and other-wordly aspects of the
            // portal vaults (i.e. they aren't in the dungeon, necessarily.)
            int colour = env.floor_colour;
            switch (colour)
            {
                case BLUE:
                    wall    = TILE_WALL_BAZAAR_GRAY;
                    floor   = TILE_FLOOR_BAZAAR_GRASS;
                    special = TILE_FLOOR_BAZAAR_GRASS1_SPECIAL;
                    return;

                case RED:
                    // Reds often have lava, which looks ridiculous
                    // next to grass or dirt, so we'll use existing
                    // floor and wall tiles here.
                    wall    = TILE_WALL_PEBBLE_RED;
                    floor   = TILE_FLOOR_VAULT;
                    special = TILE_FLOOR_BAZAAR_VAULT_SPECIAL;
                    return;

                case LIGHTBLUE:
                    wall    = TILE_WALL_HIVE;
                    floor   = TILE_FLOOR_BAZAAR_GRASS;
                    special = TILE_FLOOR_BAZAAR_GRASS2_SPECIAL;
                    return;

                case GREEN:
                    wall    = TILE_WALL_BAZAAR_STONE;
                    floor   = TILE_FLOOR_BAZAAR_GRASS;
                    special = TILE_FLOOR_BAZAAR_GRASS1_SPECIAL;
                    return;

                case MAGENTA:
                    wall    = TILE_WALL_BAZAAR_STONE;
                    floor   = TILE_FLOOR_BAZAAR_DIRT;
                    special = TILE_FLOOR_BAZAAR_DIRT_SPECIAL;
                    return;

                default:
                    wall    = TILE_WALL_VAULT;
                    floor   = TILE_FLOOR_VAULT;
                    special = TILE_FLOOR_BAZAAR_VAULT_SPECIAL;
                    return;
            }
        }
        else if (you.level_type_name == "sewer")
        {
            wall  = TILE_WALL_SLIME;
            floor = TILE_FLOOR_SLIME;
            return;
        }
        else if (you.level_type_name == "ice cave")
        {
            wall  = TILE_WALL_ICE;
            floor = TILE_FLOOR_ICE;
            return;
        }
    }

    int depth        = player_branch_depth();
    int branch_depth = your_branch().depth;

    switch (you.where_are_you)
    {
        case BRANCH_MAIN_DUNGEON:
            wall  = TILE_WALL_NORMAL;
            floor = TILE_FLOOR_NORMAL;
            return;

        case BRANCH_HIVE:
            wall  = TILE_WALL_HIVE;
            floor = TILE_FLOOR_HIVE;
            return;

        case BRANCH_VAULTS:
            wall  = TILE_WALL_VAULT;
            floor = TILE_FLOOR_VAULT;
            return;

        case BRANCH_ECUMENICAL_TEMPLE:
            wall  = TILE_WALL_VINES;
            floor = TILE_FLOOR_VINES;
            return;

        case BRANCH_ELVEN_HALLS:
        case BRANCH_HALL_OF_BLADES:
            wall  = TILE_WALL_HALL;
            floor = TILE_FLOOR_HALL;
            return;

        case BRANCH_TARTARUS:
        case BRANCH_CRYPT:
        case BRANCH_VESTIBULE_OF_HELL:
            wall  = TILE_WALL_UNDEAD;
            floor = TILE_FLOOR_TOMB;
            return;

        case BRANCH_TOMB:
            wall  = TILE_WALL_TOMB;
            floor = TILE_FLOOR_TOMB;
            return;

        case BRANCH_DIS:
            wall  = TILE_WALL_ZOT_CYAN;
            floor = TILE_FLOOR_TOMB;
            return;

        case BRANCH_GEHENNA:
            wall  = TILE_WALL_ZOT_RED;
            floor = TILE_FLOOR_ROUGH_RED;
            return;

        case BRANCH_COCYTUS:
            wall  = TILE_WALL_ICE;
            floor = TILE_FLOOR_ICE;
            return;

        case BRANCH_ORCISH_MINES:
            wall  = TILE_WALL_ORC;
            floor = TILE_FLOOR_ORC;
            return;

        case BRANCH_LAIR:
            wall  = TILE_WALL_LAIR;
            floor = TILE_FLOOR_LAIR;
            return;

        case BRANCH_SLIME_PITS:
            wall  = TILE_WALL_SLIME;
            floor = TILE_FLOOR_SLIME;
            return;

        case BRANCH_SNAKE_PIT:
            wall  = TILE_WALL_SNAKE;
            floor = TILE_FLOOR_SNAKE;
            return;

        case BRANCH_SWAMP:
            wall  = TILE_WALL_SWAMP;
            floor = TILE_FLOOR_SWAMP;
            return;

        case BRANCH_SHOALS:
            if (depth == branch_depth)
                wall = TILE_WALL_VINES;
            else
                wall = TILE_WALL_YELLOW_ROCK;

            floor = TILE_FLOOR_SAND_STONE;
            return;

        case BRANCH_HALL_OF_ZOT:
            if (you.your_level - you.branch_stairs[7] <= 1)
            {
                wall  = TILE_WALL_ZOT_YELLOW;
                floor = TILE_FLOOR_TOMB;
                return;
            }

            switch (you.your_level - you.branch_stairs[7])
            {
                case 2:
                    wall  = TILE_WALL_ZOT_GREEN;
                    floor = TILE_FLOOR_TOMB;
                    return;
                case 3:
                    wall  = TILE_WALL_ZOT_CYAN;
                    floor = TILE_FLOOR_TOMB;
                    return;
                case 4:
                    wall  = TILE_WALL_ZOT_BLUE;
                    floor = TILE_FLOOR_TOMB;
                    return;
                case 5:
                default:
                    wall  = TILE_WALL_ZOT_MAGENTA;
                    floor = TILE_FLOOR_TOMB;
                    return;
            }

        case BRANCH_INFERNO:
        case BRANCH_THE_PIT:
        case BRANCH_CAVERNS:
        case NUM_BRANCHES:
            break;

        // NOTE: there is no default case in this switch statement so that
        // the compiler will issue a warning when new branches are created.
    }

    wall  = TILE_WALL_NORMAL;
    floor = TILE_FLOOR_NORMAL;
}

void TileLoadWall(bool wizard)
{
    WallIdx(wall_tile_idx, floor_tile_idx, special_tile_idx);

    // Number of flavours are generated automatically...
    floor_flavours = tile_dngn_count(floor_tile_idx);
    wall_flavours  = tile_dngn_count(wall_tile_idx);

    if (special_tile_idx != -1)
        special_flavours = tile_dngn_count(special_tile_idx);
    else
        special_flavours = 0;
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

void TileDrawTitle()
{
#if 0
    img_type TitleImg = ImgLoadFileSimple("title");
    if (!TitleImg)
        return;

    int winx = win_main->wx;
    int winy = win_main->wy;

    TileRegionClass title(winx, winy, 1, 1);
    win_main->placeRegion(&title, 0, 0, 0, 0, 0, 0, 0);
    title.init_backbuf();
    img_type pBuf = title.backbuf;

    int tx  = ImgWidth(TitleImg);
    int ty  = ImgHeight(TitleImg);

    int x, y;

    if (tx > winx)
    {
        x = 0;
        tx = winx;
    }
    else
        x = (winx - tx)/2;

    if (ty > winy)
    {
        y = 0;
        ty = winy;
    }
    else
        y = (winy - ty)/2;

    ImgCopy(TitleImg, 0, 0, tx, ty, pBuf, x, y, 1);
    title.make_active();
    title.redraw();
    ImgDestroy(TitleImg);

    getch();
    clrscr();

    win_main->removeRegion(&title);
#endif
}

#endif
