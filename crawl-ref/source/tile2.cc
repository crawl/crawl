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

static int wall_flavors     = 0;
static int floor_flavors    = 0;
static int special_flavors  = 0;
static int wall_tile_idx    = 0;
static int floor_tile_idx   = 0;
static int special_tile_idx = 0;

int get_num_wall_flavors()
{
    return wall_flavors;
}

int get_num_floor_flavors()
{
    return floor_flavors;
}

int get_num_floor_special_flavors()
{
    return special_flavors;
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

void TileDrawBolt(int x, int y, int fg)
{
    // TODO enne
#if 0
    t1buf[x+1][y+1] = fg | TILE_FLAG_FLYING;
    _update_single_grid(x, y);
#endif
}

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

    // Number of flavors are generated automatically...
    floor_flavors  = tile_dngn_count[floor_tile_idx];
    wall_flavors  = tile_dngn_count[wall_tile_idx];

    if (special_tile_idx != -1)
    {
        special_flavors = tile_dngn_count[special_tile_idx];
    }
    else
    {
        special_flavors = 0;
    }
}

#define DOLLS_MAX 11
#define PARTS_DISP_MAX 11
#define PARTS_ITEMS 12
#define TILEP_SELECT_DOLL 20

static dolls_data current_doll;
static int current_gender = 0;

static void _load_doll_data(const char *fn, dolls_data *dolls, int max,
                            int *mode, int *cur)
{
    char fbuf[1024];
    int cur0  = 0;
    int mode0 = TILEP_M_DEFAULT;
    FILE *fp  = NULL;

    std::string dollsTxtString = datafile_path(fn, false, true);
    const char *dollsTxt = (dollsTxtString.c_str()[0] == 0) ?
                            "dolls.txt" : dollsTxtString.c_str();

    if ( (fp = fopen(dollsTxt, "r")) == NULL )
    {
        // File doesn't exist.  By default, use show equipment defaults.
        // This will be saved out (if possible.)

        for (int i = 0; i < max; i++)
        {
            // Don't take gender too seriously.  -- Enne
            tilep_race_default(you.species, coinflip() ? 1 : 0,
                               you.experience_level, dolls[i].parts);

            dolls[i].parts[TILEP_PART_SHADOW] = TILEP_SHADOW_SHADOW;
            dolls[i].parts[TILEP_PART_CLOAK]  = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_BOOTS]  = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_LEG]    = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_BODY]   = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_ARM]    = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_HAND1]  = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_HAND2]  = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_HAIR]   = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_BEARD]  = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_HELM]   = TILEP_SHOW_EQUIP;
        }

    }
    else
    {
        memset(fbuf, 0, sizeof(fbuf));
        if (fscanf(fp, "%s", fbuf) != EOF)
        {
            if (strcmp(fbuf, "MODE=LOADING") == 0)
                mode0 = TILEP_M_LOADING;
        }
        if (fscanf(fp, "%s", fbuf) != EOF)
        {
            if (strncmp(fbuf, "NUM=", 4) == 0)
            {
                sscanf(fbuf, "NUM=%d",&cur0);
                if ( (cur0 < 0)||(cur0 > 10) )
                    cur0 = 0;
            }
        }
        if (max == 1)
        {
            for (int j = 0; j <= cur0; j++)
            {
                if (fscanf(fp, "%s", fbuf) != EOF)
                    tilep_scan_parts(fbuf, dolls[0].parts);
                else
                    break;
            }
        }
        else
        {
            for (int j = 0; j < max; j++)
            {
                if (fscanf(fp, "%s", fbuf) != EOF)
                    tilep_scan_parts(fbuf, dolls[j].parts);
                else
                    break;
            }
        }

        fclose(fp);
    }
    *mode = mode0;
    *cur = cur0;

    current_gender = dolls[cur0].parts[TILEP_PART_BASE] % 2;
}

void TilePlayerEdit()
{
#if 0
    const int p_lines[PARTS_ITEMS] =
    {
        TILEP_SELECT_DOLL,
        TILEP_PART_BASE,
        TILEP_PART_HELM,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_HAND1,
        TILEP_PART_HAND2,
        TILEP_PART_ARM,
        TILEP_PART_BODY,
        TILEP_PART_LEG,
        TILEP_PART_BOOTS,
        TILEP_PART_CLOAK
    };

    const char *p_names[PARTS_ITEMS] =
    {
        "  Index:",
        "  Gendr:",
        "  Helm :",
        "  Hair :",
        "  Beard:",
        "  Weapn:",
        "  Shild:",
        "  Glove:",
        "  Armor:",
        "  Legs :",
        "  Boots:",
        "  Cloak:"
    };

    const char *gender_name[2] = {
        "Male",
        "Fem "
    };

#define display_parts_idx(part) \
{                               \
    cgotoxy(10 , part + 1, GOTO_STAT);    \
    tilep_part_to_str(dolls[cur_doll].parts[ p_lines[part] ], ibuf);\
    cprintf(ibuf);\
}

    dolls_data dolls[DOLLS_MAX], undo_dolls[DOLLS_MAX];
    int copy_doll[TILEP_PART_MAX];

    //int parts[TILEP_PART_MAX];
    int i, j, x, y, kin, done = 0;
    int cur_doll = 0;
    int pre_part = 0;
    int cur_part = 0;
    int mode = TILEP_M_DEFAULT;
    int  d = 0;
    FILE *fp;
    char fbuf[80], ibuf[8];
    int tile_str, sx, sy;

    img_type PartsImg = ImgCreateSimple(TILE_X * PARTS_DISP_MAX, TILE_Y * 1);

    img_type DollsListImg = ImgCreateSimple( TILE_X * DOLLS_MAX, TILE_Y * 1);

    ImgClear(ScrBufImg);

    memset( copy_doll, 0, sizeof(dolls_data) );
    tilep_race_default(you.species, current_gender,
                       you.experience_level, copy_doll);

    for (j = 0; j < DOLLS_MAX; j++)
    {
        memset( dolls[j].parts, 0, sizeof(dolls_data) );
        memset( undo_dolls[j].parts, 0, sizeof(dolls_data) );
        tilep_race_default(you.species, current_gender, you.experience_level,
                           dolls[j].parts);
        tilep_race_default(you.species, current_gender, you.experience_level,
                           undo_dolls[j].parts);
    }

    // load settings
    _load_doll_data("dolls.txt", dolls, DOLLS_MAX, &mode, &cur_doll);
    for (j = 0; j < DOLLS_MAX; j++)
        undo_dolls[j] = dolls[j];

    clrscr();
    cgotoxy(1,1, GOTO_MSG);
    mpr("Select Part              : [j][k]or[up][down]");
    mpr("Change Part/Page         : [h][l]or[left][right]/[H][L]");
    mpr("Class-Default            : [CTRL]+[D]");
    mpr("Toggle Equipment Mode    : [*]");
    mpr("Take off/Take off All    : [t]/[CTRL]+[T]");
    mpr("Copy/Paste/Undo          : [CTRL]+[C]/[CTRL]+[V]/[CTRL]+[Z]");
    mpr("Randomize                : [CTRL]+[R]");

    cgotoxy(1, 8, GOTO_MSG);
    cprintf("Toggle Startup mode      : [m] (Current mode:%s)",
            (mode == TILEP_M_LOADING ? "Load User's Settings"
                                     : "Class Default" ));
    for (j = 0; j < PARTS_ITEMS; j++)
    {
        cgotoxy(1, 1+j, GOTO_STAT);
        cprintf(p_names[j]);
    }

#define PARTS_Y (TILE_Y*11)

// draw strings "Dolls/Parts" into backbuffer
    tile_str = TILE_TEXT_PARTS_E;
    sx = (tile_str % TILE_PER_ROW) * TILE_X;
    sy = (tile_str / TILE_PER_ROW) * TILE_Y;

    ImgCopy(TileImg, sx, sy, TILE_X, TILE_Y/2,
            ScrBufImg, (TILE_X * 3) - 8,(TILE_Y * 4) - 8-24, 0);
    ImgCopy(TileImg, sx, sy+ TILE_Y/2, TILE_X, TILE_Y/2,
            ScrBufImg, (TILE_X * 4) - 8,(TILE_Y * 4) - 8-24, 0);

    tile_str = TILE_TEXT_DOLLS_E;
    sx = (tile_str % TILE_PER_ROW) * TILE_X;
    sy = (tile_str / TILE_PER_ROW) * TILE_Y;

    ImgCopy(TileImg, sx, sy, TILE_X, TILE_Y/2,
            ScrBufImg, (TILE_X * 3) - 8,PARTS_Y - 8 -24, 0);
    ImgCopy(TileImg, sx, sy+ TILE_Y/2, TILE_X, TILE_Y/2,
            ScrBufImg, (TILE_X * 4) - 8,PARTS_Y - 8 -24, 0);

    ImgClear(DollsListImg);
    for (j = 0; j < DOLLS_MAX; j++)
    {
        _draw_doll(DollCacheImg, &dolls[j], true);
        ImgCopy(DollCacheImg, 0, 0,
        ImgWidth(DollCacheImg), ImgHeight(DollCacheImg),
                 DollsListImg, j * TILE_X, 0, 1);
    }

    done = 0;
    while (!done)
    {
        static int inc = 1;
        static int cur_inc = 1;

        ImgClear(PartsImg);

        region_tile->DrawPanel((TILE_X * 3) - 8, (TILE_Y * 4) - 8,
                   ImgWidth(PartsImg) + 16, ImgHeight(PartsImg) + 16);
        region_tile->DrawPanel((TILE_X * 3) - 8, PARTS_Y - 8,
                   ImgWidth(DollsListImg) + 16, ImgHeight(DollsListImg) + 16);
        region_tile->DrawPanel(8*TILE_X - 8, 8*TILE_Y - 8,
                   TILE_X + 16, TILE_Y + 16);

        if (cur_part == 0)
        {
            // now selecting doll index
            cgotoxy(10 , 1, GOTO_STAT);
            cprintf("%02d", cur_doll);
            cgotoxy(10 , 2, GOTO_STAT);
            current_gender = dolls[cur_doll].parts[TILEP_PART_BASE] % 2;
            cprintf("%s", gender_name[ current_gender ]);

            for (i = 2; i < PARTS_ITEMS; i++)
            {
                cgotoxy(10 , i + 1, GOTO_STAT);
                tilep_part_to_str(dolls[cur_doll].parts[ p_lines[i] ], ibuf);
                cprintf("%s / %03d", ibuf, tilep_parts_total[ p_lines[i] ]);
            }
        }
        else
        {
            for (i = 0; i < PARTS_DISP_MAX; i++)
            {
                int index;
                if (dolls[cur_doll].parts[ p_lines[cur_part] ]
                     == TILEP_SHOW_EQUIP)
                {
                    index = 0;
                }
                else
                {
                    index = i + dolls[cur_doll].parts[ p_lines[cur_part] ]
                      - (PARTS_DISP_MAX / 2);
                }
                if (index < 0)
                    index = index + tilep_parts_total[ p_lines[cur_part] ] + 1;
                if (index > tilep_parts_total[ p_lines[cur_part] ])
                    index = index - tilep_parts_total[ p_lines[cur_part] ] - 1;

                _tcache_overlay_player(PartsImg, i * TILE_X, 0,
                p_lines[cur_part], index, TILE_Y, &d);
            }
            ImgCopy(PartsImg, 0, 0, ImgWidth(PartsImg), ImgHeight(PartsImg),
                    ScrBufImg, 3 * TILE_X, 4 * TILE_Y, 0);
            _ImgCopyFromTileImg( TILE_CURSOR, ScrBufImg,
                                (3 + PARTS_DISP_MAX / 2) * TILE_X,
                                4 * TILE_Y, 0);
        }
        _draw_doll(DollCacheImg, &dolls[cur_doll], true);

        ImgCopy(DollCacheImg, 0, 0, TILE_X, TILE_Y,
                DollsListImg, cur_doll*TILE_X, 0, 1);

        _ImgCopyToTileImg(TILE_PLAYER, DollCacheImg, 0, 0, 1);
        _ImgCopyFromTileImg(TILE_PLAYER, ScrBufImg, 8*TILE_X, 8*TILE_Y, 0);
        ImgCopy(DollsListImg, 0, 0,
                ImgWidth(DollsListImg), ImgHeight(DollsListImg),
                ScrBufImg, 3 * TILE_X, PARTS_Y, 0);
        _ImgCopyFromTileImg( TILE_CURSOR, ScrBufImg,
                             (3 + cur_doll) * TILE_X, PARTS_Y, 0);
        region_tile->redraw();

        cgotoxy(10 , cur_part + 1, GOTO_STAT);

        if (cur_part == 1)
        {
            current_gender = dolls[cur_doll].parts[p_lines[cur_part] ]%2;
            cprintf("%s", gender_name[ current_gender ]);
        }
        else if (cur_part != 0)
        {
            tilep_part_to_str(dolls[cur_doll].parts[ p_lines[cur_part] ], ibuf);
            cprintf("%s", ibuf);
        }

        cgotoxy(1, pre_part + 1, GOTO_STAT);cprintf("  ");
        cgotoxy(1, cur_part + 1, GOTO_STAT);cprintf("->");
        pre_part = cur_part;

        /* Get a key */
        kin = getch();

        inc = cur_inc = 0;

        /* Analyze the key */
        switch (kin)
        {
            case 0x1B: //ESC
                done = 1;
                break;

            case 'h':
                inc = -1;
                break;

            case 'l':
                inc = +1;
                break;

            case 'H':
                inc = -PARTS_DISP_MAX;
                break;

            case 'L':
                inc = +PARTS_DISP_MAX;
                break;

            case 'j':
                if (cur_part < (PARTS_ITEMS-1) )
                    cur_part += 1;
                else
                    cur_part = 0;
                break;

            case 'k':
                if (cur_part > 0)
                    cur_part -= 1;
                else
                    cur_part = (PARTS_ITEMS-1);
                break;

            case 'm':
                if (mode == TILEP_M_LOADING)
                    mode = TILEP_M_DEFAULT;
                else
                    mode = TILEP_M_LOADING;

                cgotoxy(1, 8, GOTO_MSG);
                cprintf("Toggle Startup mode      : [m] (Current mode:%s)",
                        ( mode == TILEP_M_LOADING ? "Load User's Settings"
                                                  : "Class Default" ));
                break;

            case 't':
                if (cur_part >= 2)
                {
                    dolls[cur_doll].parts[ p_lines[cur_part] ] = 0;
                    display_parts_idx(cur_part);
                }
                break;

            case CONTROL('D'):
                tilep_job_default(you.char_class, current_gender,
                                  dolls[cur_doll].parts);

                for (i = 2; i < PARTS_ITEMS; i++)
                     display_parts_idx(i);
                break;

            case CONTROL('T'):
                for (i = 2; i < PARTS_ITEMS; i++)
                {
                    undo_dolls[cur_doll].parts[ p_lines[i] ]
                           = dolls[cur_doll].parts[ p_lines[i] ];
                    dolls[cur_doll].parts[ p_lines[i] ] = 0;
                    display_parts_idx(i);
                }
                break;

            case CONTROL('C'):
                for (i = 2; i < PARTS_ITEMS; i++)
                {
                     copy_doll[ p_lines[i] ]
                           = dolls[cur_doll].parts[ p_lines[i] ];
                }
                break;

            case CONTROL('V'):
                for (i = 2; i < PARTS_ITEMS; i++)
                {
                    undo_dolls[cur_doll].parts[ p_lines[i] ]
                           = dolls[cur_doll].parts[ p_lines[i] ];
                    dolls[cur_doll].parts[ p_lines[i] ]
                           = copy_doll[ p_lines[i] ];
                    display_parts_idx(i);
                }
                break;

            case CONTROL('Z'):
                for (i = 2; i < PARTS_ITEMS; i++)
                {
                    dolls[cur_doll].parts[ p_lines[i] ]
                           = undo_dolls[cur_doll].parts[ p_lines[i] ];
                    display_parts_idx(i);
                }
                break;

            case CONTROL('R'):
                for (i = 2; i < PARTS_ITEMS; i++)
                {
                    undo_dolls[cur_doll].parts[ p_lines[i] ]
                           = dolls[cur_doll].parts[ p_lines[i] ];
                }
                dolls[cur_doll].parts[TILEP_PART_CLOAK]
                  = coinflip() * (random2(tilep_parts_total[ TILEP_PART_CLOAK ]) + 1);
                dolls[cur_doll].parts[TILEP_PART_BOOTS]
                  = random2(tilep_parts_total[ TILEP_PART_BOOTS ] + 1);
                dolls[cur_doll].parts[TILEP_PART_LEG]
                  = random2(tilep_parts_total[ TILEP_PART_LEG ] + 1);
                dolls[cur_doll].parts[TILEP_PART_BODY]
                  = random2(tilep_parts_total[ TILEP_PART_BODY ] + 1);
                dolls[cur_doll].parts[TILEP_PART_ARM]
                  = coinflip() * ( random2(tilep_parts_total[ TILEP_PART_ARM ]) + 1);
                dolls[cur_doll].parts[TILEP_PART_HAND1]
                  = random2(tilep_parts_total[ TILEP_PART_HAND1 ] + 1);
                dolls[cur_doll].parts[TILEP_PART_HAND2]
                  = coinflip() * ( random2(tilep_parts_total[ TILEP_PART_HAND2 ]) + 1);
                dolls[cur_doll].parts[TILEP_PART_HAIR]
                  = random2(tilep_parts_total[ TILEP_PART_HAIR ] + 1);
                dolls[cur_doll].parts[TILEP_PART_BEARD]
                  = ((dolls[cur_doll].parts[TILEP_PART_BASE] + 1) % 2)
                     * one_chance_in(4)
                     * ( random2(tilep_parts_total[ TILEP_PART_BEARD ]) + 1 );
                dolls[cur_doll].parts[TILEP_PART_HELM]
                  = coinflip() * ( random2(tilep_parts_total[ TILEP_PART_HELM ]) + 1 );

                for (i = 2; i < PARTS_ITEMS; i++)
                    display_parts_idx(i);

                break;

            case '*':
            {
                int part = p_lines[cur_part];
                int *target = &dolls[cur_doll].parts[part];

                if (part == TILEP_PART_BASE)
                    continue;
                if (*target == TILEP_SHOW_EQUIP)
                    *target = 0;
                else
                    *target = TILEP_SHOW_EQUIP;

                display_parts_idx(cur_part);
                break;
            }
#ifdef WIZARD
            case '1':
                dolls[cur_doll].parts[TILEP_PART_DRCHEAD]++;
                dolls[cur_doll].parts[TILEP_PART_DRCHEAD]
                    %= tilep_parts_total[TILEP_PART_DRCHEAD] - 1;
                break;

            case '2':
                dolls[cur_doll].parts[TILEP_PART_DRCWING]++;
                dolls[cur_doll].parts[TILEP_PART_DRCWING]
                    %= tilep_parts_total[TILEP_PART_DRCWING] - 1;
                break;
#endif
            default:
                break;
        }

        if (inc != 0)
        {
            int *target = &dolls[cur_doll].parts[ p_lines[cur_part] ];

            if (cur_part == 0)
            {
                if (inc > 0)
                    cur_doll++;
                else
                    cur_doll--;

                if (cur_doll < 0)
                    cur_doll = DOLLS_MAX - 1;
                else if (cur_doll == DOLLS_MAX)
                    cur_doll = 0;
            }
            else if (cur_part == 1)
            {
                if (*target % 2)
                    (*target)++;
                else
                    (*target)--;
            }
            else
            {
                if (*target == TILEP_SHOW_EQUIP)
                    continue;

                (*target) += inc;
                (*target) += tilep_parts_total[ p_lines[cur_part] ] + 1;
                (*target) %= tilep_parts_total[ p_lines[cur_part] ] + 1;
            }
        }

        delay(20);
    }

    current_doll = dolls[cur_doll];
    _draw_doll(DollCacheImg, &current_doll);
    _ImgCopyToTileImg(TILE_PLAYER, DollCacheImg, 0, 0, 1);

    std::string dollsTxtString = datafile_path("dolls.txt", false, true);
    const char *dollsTxt = (dollsTxtString.c_str()[0] == 0) ?
                           "dolls.txt" : dollsTxtString.c_str();

    if ( (fp = fopen(dollsTxt, "w+")) != NULL )
    {
        fprintf(fp, "MODE=%s\n", (mode == TILEP_M_LOADING) ? "LOADING"
                                                           : "DEFAULT" );
        fprintf(fp, "NUM=%02d\n", cur_doll);
        for (j = 0; j < DOLLS_MAX; j++)
        {
            tilep_print_parts(fbuf, dolls[j].parts);
            fprintf(fp, "%s\n", fbuf);
        }
        fclose(fp);
    }

    ImgDestroy(PartsImg);
    ImgDestroy(DollsListImg);

    ImgClear(ScrBufImg);
    _redraw_spx_tcache(TILE_PLAYER);

    for (x = 0; x < TILE_DAT_XMAX + 2; x++)
    {
        for (y = 0; y < TILE_DAT_YMAX + 2; y++)
        {
            t1buf[x][y] = 0;
#if 0
            t2buf[x][y] = TILE_DNGN_UNSEEN | TILE_FLAG_UNSEEN;
#endif
        }
    }

    for (x = 0; x < tile_xmax * tile_ymax; x++)
        screen_tcache_idx[x] = -1;

    clrscr();
    redraw_screen();
#endif
}

void TileGhostInit(const struct ghost_demon &ghost)
{
#if 0
    dolls_data doll;
    int x, y;
    unsigned int pseudo_rand = ghost.max_hp * 54321 * 54321;
    char mask[TILE_X*TILE_Y];
    int g_gender = (pseudo_rand >> 8) & 1;

    for (x = 0; x < TILE_X; x++)
    for (y = 0; y < TILE_X; y++)
         mask[x + y*TILE_X] = (x+y)&1;

    for (x = 0; x < TILEP_PART_MAX; x++)
    {
         doll.parts[x] = 0;
         current_parts[x] = 0;
    }
    tilep_race_default(ghost.species, g_gender,
                       ghost.xl, doll.parts);
    tilep_job_default (ghost.job, g_gender,  doll.parts);

    for (x = TILEP_PART_CLOAK; x < TILEP_PART_MAX; x++)
    {
         if (doll.parts[x] == TILEP_SHOW_EQUIP)
         {
             doll.parts[x] = 1 + (pseudo_rand % tilep_parts_total[x]);

             if (x == TILEP_PART_BODY)
             {
                 int p = 0;
                 int ac = ghost.ac;
                 ac *= (5 + (pseudo_rand/11) % 11);
                 ac /= 10;

                 if (ac > 25)
                     p = TILEP_BODY_PLATE_BLACK;
                 else if (ac > 20)
                     p =  TILEP_BODY_BANDED;
                 else if (ac > 15)
                     p = TILEP_BODY_SCALEMAIL;
                 else if (ac > 10)
                     p = TILEP_BODY_CHAINMAIL;
                 else if (ac > 5 )
                     p = TILEP_BODY_LEATHER_HEAVY;
                 else
                     p = TILEP_BODY_ROBE_BLUE;
                doll.parts[x] = p;
            }
        }
    }

    int sk = ghost.best_skill;
    int dam = ghost.damage;
    int p = 0;

    dam *= (5 + pseudo_rand % 11);
    dam /= 10;

    switch (sk)
    {
    case SK_MACES_FLAILS:
        if (dam > 30)
            p = TILEP_HAND1_GREAT_FRAIL;
        else if (dam > 25)
            p = TILEP_HAND1_GREAT_MACE;
        else if (dam > 20)
            p = TILEP_HAND1_SPIKED_FRAIL;
        else if (dam > 15)
            p = TILEP_HAND1_MORNINGSTAR;
        else if (dam > 10)
            p = TILEP_HAND1_FRAIL;
        else if (dam > 5)
            p = TILEP_HAND1_MACE;
        else
            p = TILEP_HAND1_CLUB_SLANT;

        doll.parts[TILEP_PART_HAND1] = p;
        break;

    case SK_SHORT_BLADES:
        if (dam > 20)
            p = TILEP_HAND1_SABRE;
        else if (dam > 10)
            p = TILEP_HAND1_SHORT_SWORD_SLANT;
        else
            p = TILEP_HAND1_DAGGER_SLANT;

        doll.parts[TILEP_PART_HAND1] = p;
        break;

    case SK_LONG_BLADES:
        if (dam > 25)
            p = TILEP_HAND1_GREAT_SWORD_SLANT;
        else if (dam > 20)
            p = TILEP_HAND1_KATANA_SLANT;
        else if (dam > 15)
            p = TILEP_HAND1_SCIMITAR;
        else if (dam > 10)
            p = TILEP_HAND1_LONG_SWORD_SLANT;
        else
            p = TILEP_HAND1_FALCHION;

        doll.parts[TILEP_PART_HAND1] = p;
        break;

    case SK_AXES:
        if (dam > 30)
            p = TILEP_HAND1_EXECUTIONERS_AXE;
        else if (dam > 20)
            p = TILEP_HAND1_BATTLEAXE;
        else if (dam > 15)
            p = TILEP_HAND1_BROAD_AXE;
        else if (dam > 10)
            p = TILEP_HAND1_WAR_AXE;
        else
            p = TILEP_HAND1_HAND_AXE;

        doll.parts[TILEP_PART_HAND1] = p;
        break;

    case SK_POLEARMS:
        if (dam > 30)
            p =  TILEP_HAND1_GLAIVE;
        else if (dam > 20)
            p = TILEP_HAND1_SCYTHE;
        else if (dam > 15)
            p = TILEP_HAND1_HALBERD;
        else if (dam > 10)
            p = TILEP_HAND1_TRIDENT2;
        else if (dam > 10)
            p = TILEP_HAND1_HAMMER;
        else
            p = TILEP_HAND1_SPEAR;

        doll.parts[TILEP_PART_HAND1] = p;
        break;

    case SK_BOWS:
        doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_BOW2;
        break;

    case SK_CROSSBOWS:
        doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_CROSSBOW;
        break;

    case SK_SLINGS:
        doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SLING;
        break;

    case SK_UNARMED_COMBAT:
    default:
        doll.parts[TILEP_PART_HAND1] = doll.parts[TILEP_PART_HAND2] = 0;
        break;
    }

    ImgClear(DollCacheImg);
    // Clear
    _ImgCopyToTileImg(TILE_MONS_PLAYER_GHOST, DollCacheImg, 0, 0, 1);

    _draw_doll(DollCacheImg, &doll);
    _ImgCopyToTileImg(TILE_MONS_PLAYER_GHOST, DollCacheImg, 0, 0, 1, mask, false);
    _redraw_spx_tcache(TILE_MONS_PLAYER_GHOST);
#endif
}

void tile_get_monster_weapon_offset(int mon_tile, int &ofs_x, int &ofs_y)
{
    ofs_x = 0;
    ofs_y = 0;

    switch (mon_tile)
    {
        case TILE_MONS_ORC:
        case TILE_MONS_URUG:
        case TILE_MONS_BLORK_THE_ORC:
        case TILE_MONS_ORC_WARRIOR:
        case TILE_MONS_ORC_KNIGHT:
        case TILE_MONS_ORC_WARLORD:
            ofs_y = 2;
            break;
        case TILE_MONS_GOBLIN:
        case TILE_MONS_IJYB:
            ofs_y = 4;
            break;
        case TILE_MONS_GNOLL:
            ofs_x = -1;
            break;
        case TILE_MONS_BOGGART:
            ofs_y = 2;
            break;
        case TILE_MONS_DEEP_ELF_FIGHTER:
        case TILE_MONS_DEEP_ELF_SOLDIER:
            ofs_y = 2;
            break;
        case TILE_MONS_DEEP_ELF_KNIGHT:
            ofs_y = 1;
            break;
        case TILE_MONS_KOBOLD:
            ofs_x = 3;
            ofs_y = 4;
            break;
        case TILE_MONS_KOBOLD_DEMONOLOGIST:
            ofs_y = -10;
            break;
        case TILE_MONS_BIG_KOBOLD:
            ofs_x = 2;
            ofs_y = 3;
            break;
        case TILE_MONS_MIDGE:
            ofs_y = -2;
            break;
        case TILE_MONS_NAGA:
        case TILE_MONS_GREATER_NAGA:
        case TILE_MONS_NAGA_WARRIOR:
        case TILE_MONS_GUARDIAN_NAGA:
        case TILE_MONS_NAGA_MAGE:
            ofs_y = 1;
            break;
        case TILE_MONS_HELL_KNIGHT:
            ofs_x = -1;
            ofs_y = 3;
            break;
        case TILE_MONS_RED_DEVIL:
            ofs_x = 2;
            ofs_y = -3;
            break;
        case TILE_MONS_WIZARD:
            ofs_x = 2;
            ofs_y = -2;
            break;
        case TILE_MONS_HUMAN:
            ofs_x = 5;
            ofs_y = 2;
            break;
        case TILE_MONS_ELF:
            ofs_y = 1;
            ofs_x = 4;
            break;
        case TILE_MONS_OGRE_MAGE:
            ofs_y = -2;
            ofs_x = -4;
            break;
        case TILE_MONS_DEEP_ELF_MAGE:
        case TILE_MONS_DEEP_ELF_SUMMONER:
        case TILE_MONS_DEEP_ELF_CONJURER:
        case TILE_MONS_DEEP_ELF_PRIEST:
        case TILE_MONS_DEEP_ELF_HIGH_PRIEST:
        case TILE_MONS_DEEP_ELF_DEMONOLOGIST:
        case TILE_MONS_DEEP_ELF_ANNIHILATOR:
        case TILE_MONS_DEEP_ELF_SORCERER:
            ofs_x = -1;
            ofs_y = -2;
            break;
        case TILE_MONS_DEEP_ELF_DEATH_MAGE:
            ofs_x = -1;
            break;
    }
}

int get_clean_map_idx(int tile_idx)
{
    int idx = tile_idx & TILE_FLAG_MASK;
    if (idx >= TILE_CLOUD_FIRE_0 && idx <= TILE_CLOUD_PURP_SMOKE ||
        idx >= TILE_MONS_SHADOW && idx <= TILE_MONS_WATER_ELEMENTAL ||
        idx >= TILE_MCACHE_START)
    {
        return 0;
    }
    else
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
