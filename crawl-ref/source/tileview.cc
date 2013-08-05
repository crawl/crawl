#include "AppHdr.h"

#include "tileview.h"

#include "areas.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "dungeon.h"
#include "env.h"
#include "fprop.h"
#include "items.h"
#include "kills.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "showsymb.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tiledef-player.h"
#include "tilemcache.h"
#include "tilepick.h"
#include "traps.h"
#include "travel.h"
#include "viewgeom.h"

void tile_new_level(bool first_time, bool init_unseen)
{
    if (first_time)
        tile_init_flavour();

#ifdef USE_TILE
    if (init_unseen)
    {
        for (unsigned int x = 0; x < GXM; x++)
            for (unsigned int y = 0; y < GYM; y++)
            {
                env.tile_bk_fg[x][y] = 0;
                env.tile_bk_bg[x][y] = TILE_DNGN_UNSEEN;
                env.tile_bk_cloud[x][y] = 0;
            }
    }

    // Fix up stair markers.  The travel information isn't hooked up
    // until after we change levels.  So, look through all of the stairs
    // on this level and check if they still need the stair flag.
    for (unsigned int x = 0; x < GXM; x++)
        for (unsigned int y = 0; y < GYM; y++)
        {
            unsigned int tile = env.tile_bk_bg[x][y];
            if (!(tile & TILE_FLAG_NEW_STAIR))
                continue;
            if (!is_unknown_stair(coord_def(x,y)))
                env.tile_bk_bg[x][y] &= ~TILE_FLAG_NEW_STAIR;
        }

    tiles.clear_minimap();

    for (unsigned int x = 0; x < GXM; x++)
        for (unsigned int y = 0; y < GYM; y++)
            tiles.update_minimap(coord_def(x, y));
#endif
}

void tile_init_default_flavour()
{
    tile_default_flv(you.where_are_you, env.tile_default);
}

void tile_default_flv(branch_type br, tile_flavour &flv)
{
    flv.wall    = TILE_WALL_NORMAL;
    flv.floor   = TILE_FLOOR_NORMAL;
    flv.special = 0;

    flv.wall_idx = 0;
    flv.floor_idx = 0;

    switch (br)
    {
    case BRANCH_MAIN_DUNGEON:
        flv.wall  = TILE_WALL_NORMAL;
        flv.floor = TILE_FLOOR_NORMAL;
        return;

    case BRANCH_VAULTS:
        flv.wall  = TILE_WALL_VAULT;
        flv.floor = TILE_FLOOR_VAULT;
        return;

    case BRANCH_ECUMENICAL_TEMPLE:
        flv.wall  = TILE_WALL_VINES;
        flv.floor = TILE_FLOOR_VINES;
        return;

#if TAG_MAJOR_VERSION == 34
    case BRANCH_DWARVEN_HALL:
        flv.wall  = TILE_WALL_HALL;
        flv.floor = TILE_FLOOR_LIMESTONE;
        return;
#endif

    case BRANCH_ELVEN_HALLS:
    case BRANCH_HALL_OF_BLADES:
        flv.wall  = TILE_WALL_HALL;
        flv.floor = TILE_FLOOR_HALL;
        return;

    case BRANCH_TARTARUS:
        flv.wall  = TILE_WALL_COBALT_ROCK;
        flv.floor = TILE_FLOOR_BLACK_COBALT;
        return;

    case BRANCH_CRYPT:
        flv.wall  = TILE_WALL_BRICK_GRAY;
        flv.floor = TILE_FLOOR_CRYPT;
        return;

    case BRANCH_TOMB:
        flv.wall  = TILE_WALL_UNDEAD;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case BRANCH_VESTIBULE_OF_HELL:
        flv.wall  = TILE_WALL_HELL;
        flv.floor = TILE_FLOOR_CAGE;
        return;

    case BRANCH_DIS:
        flv.wall  = TILE_WALL_ZOT_CYAN;
        flv.floor = TILE_FLOOR_IRON;
        return;

    case BRANCH_GEHENNA:
        flv.wall  = TILE_WALL_ZOT_RED;
        flv.floor = TILE_FLOOR_ROUGH_RED;
        return;

    case BRANCH_COCYTUS:
        flv.wall  = TILE_WALL_ICE;
        flv.floor = TILE_FLOOR_FROZEN;
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
        flv.floor = random_choose(TILE_FLOOR_SNAKE_A,
                                  TILE_FLOOR_SNAKE_C,
                                  TILE_FLOOR_SNAKE_D,
                                  -1);
        return;

    case BRANCH_SWAMP:
        flv.wall  = TILE_WALL_SWAMP;
        flv.floor = TILE_FLOOR_SWAMP;
        return;

    case BRANCH_SHOALS:
        flv.wall  = TILE_WALL_SHOALS;
        flv.floor = TILE_FLOOR_SAND;
        return;

    case BRANCH_SPIDER_NEST:
        flv.wall  = TILE_WALL_SPIDER;
        flv.floor = TILE_FLOOR_SPIDER;
        return;

    case BRANCH_HALL_OF_ZOT:
        flv.wall  = TILE_WALL_ZOT_YELLOW;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case BRANCH_FOREST:
        flv.wall  = TILE_WALL_LAIR;
        flv.floor = TILE_FLOOR_GRASS;
        return;

    case BRANCH_ABYSS:
        flv.floor = tile_dngn_coloured(TILE_FLOOR_NERVES, env.floor_colour);
        switch (random2(6))
        {
        default:
        case 0:
        case 1:
        case 2:
            flv.wall = tile_dngn_coloured(TILE_WALL_ABYSS, env.rock_colour);
            break;
        case 3:
            flv.wall = tile_dngn_coloured(TILE_WALL_PEBBLE, env.rock_colour);
            break;
        case 4:
            flv.wall = tile_dngn_coloured(TILE_WALL_HALL, env.rock_colour);
            break;
        case 5:
            flv.wall = tile_dngn_coloured(TILE_WALL_UNDEAD, env.rock_colour);
            break;
        }
        return;

    case BRANCH_PANDEMONIUM:
        switch (random2(9))
        {
            default:
            case 0: flv.wall = TILE_WALL_BARS_RED; break;
            case 1: flv.wall = TILE_WALL_BARS_BLUE; break;
            case 2: flv.wall = TILE_WALL_BARS_CYAN; break;
            case 3: flv.wall = TILE_WALL_BARS_GREEN; break;
            case 4: flv.wall = TILE_WALL_BARS_MAGENTA; break;
            case 5: flv.wall = TILE_WALL_BARS_BROWN; break;
            case 6: flv.wall = TILE_WALL_BARS_LIGHTGRAY; break;
            case 7: flv.wall = TILE_WALL_BARS_DARKGRAY; break;
            // Wall_flesh used to have a 1/3 chance
            case 8: flv.wall = TILE_WALL_FLESH; break;
        }

        switch (random2(8))
        {
            default:
            case 0: flv.floor = TILE_FLOOR_DEMONIC_RED; break;
            case 1: flv.floor = TILE_FLOOR_DEMONIC_BLUE; break;
            case 2: flv.floor = TILE_FLOOR_DEMONIC_GREEN; break;
            case 3: flv.floor = TILE_FLOOR_DEMONIC_CYAN; break;
            case 4: flv.floor = TILE_FLOOR_DEMONIC_MAGENTA; break;
            case 5: flv.floor = TILE_FLOOR_DEMONIC_BROWN; break;
            case 6: flv.floor = TILE_FLOOR_DEMONIC_LIGHTGRAY; break;
            case 7: flv.floor = TILE_FLOOR_DEMONIC_DARKGRAY; break;
        }
        break;

    case BRANCH_ZIGGURAT:
    case BRANCH_BAZAAR:
    case BRANCH_TROVE:
        flv.wall  = TILE_WALL_VAULT;
        flv.floor = TILE_FLOOR_VAULT;
        return;

    case BRANCH_LABYRINTH:
        flv.wall  = TILE_WALL_LAB_ROCK;
        flv.floor = TILE_FLOOR_LABYRINTH;
        return;

    case BRANCH_SEWER:
        flv.wall  = TILE_WALL_SLIME;
        flv.floor = TILE_FLOOR_SLIME;
        return;

    case BRANCH_OSSUARY:
        flv.wall  = TILE_WALL_SANDSTONE;
        flv.floor = TILE_FLOOR_SANDSTONE;
        return;

    case BRANCH_BAILEY:
        flv.wall  = TILE_WALL_BRICK_BROWN;
        flv.floor = TILE_FLOOR_COBBLE_BLOOD;
        return;

    case BRANCH_ICE_CAVE:
        flv.wall  = TILE_WALL_ZOT_CYAN;
        flv.floor = TILE_FLOOR_ICE;
        return;

    case BRANCH_VOLCANO:
        flv.wall  = TILE_WALL_VOLCANIC;
        flv.floor = TILE_FLOOR_ROUGH_RED;
        return;

    case BRANCH_WIZLAB:
        flv.wall  = TILE_WALL_NORMAL;
        flv.floor = TILE_FLOOR_NORMAL;
        return;

#if TAG_MAJOR_VERSION == 34
    case BRANCH_UNUSED:
#endif
    case NUM_BRANCHES:
        break;
    }
}

void tile_clear_flavour(const coord_def &p)
{
    env.tile_flv(p).floor     = 0;
    env.tile_flv(p).wall      = 0;
    env.tile_flv(p).feat      = 0;
    env.tile_flv(p).floor_idx = 0;
    env.tile_flv(p).wall_idx  = 0;
    env.tile_flv(p).feat_idx  = 0;
    env.tile_flv(p).special   = 0;
}

void tile_clear_flavour()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        tile_clear_flavour(*ri);
}

// For floors and walls that have not already been set to a particular tile,
// set them to a random instance of the default floor and wall tileset.
void tile_init_flavour()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        tile_init_flavour(*ri);
}

static void _get_dungeon_wall_tiles_by_depth(int depth, vector<tileidx_t>& t)
{
    if (crawl_state.game_is_sprint() || crawl_state.game_is_zotdef() || crawl_state.game_is_arena())
    {
        t.push_back(TILE_WALL_CATACOMBS);
        return;
    }
    if (depth <= 6)
        t.push_back(TILE_WALL_BRICK_DARK_1);
    if (depth > 3 && depth <= 9)
    {
        t.push_back(TILE_WALL_BRICK_DARK_2);
        t.push_back(TILE_WALL_BRICK_DARK_2_TORCH);
    }
    if (depth > 6 && depth <= 14)
        t.push_back(TILE_WALL_BRICK_DARK_3);
    if (depth > 9 && depth <= 20)
    {
        t.push_back(TILE_WALL_BRICK_DARK_4);
        t.push_back(TILE_WALL_BRICK_DARK_4_TORCH);
    }
    if (depth > 14)
        t.push_back(TILE_WALL_BRICK_DARK_5);
    if (depth > 20)
    {
        t.push_back(TILE_WALL_BRICK_DARK_6);
        t.push_back(TILE_WALL_BRICK_DARK_6_TORCH);
    }
}

static tileidx_t _pick_random_dngn_tile(tileidx_t idx, int value = -1)
{
    ASSERT_RANGE(idx, 0, TILE_DNGN_MAX);
    const int count = tile_dngn_count(idx);
    if (count == 1)
        return idx;

    const int total = tile_dngn_probs(idx + count - 1);
    const int rand  = (value == -1 ? random2(total) : value % total);

    for (int i = 0; i < count; ++i)
    {
        tileidx_t curr = idx + i;
        if (rand < tile_dngn_probs(curr))
            return curr;
    }

    return idx;
}

static tileidx_t _pick_random_dngn_tile_multi(vector<tileidx_t> candidates, int value = -1)
{
    ASSERT(candidates.size() > 0);

    int total = 0;
    for (unsigned int i = 0; i < candidates.size(); ++i)
    {
        const unsigned int count = tile_dngn_count(candidates[i]);
        total += tile_dngn_probs(candidates[i] + count - 1);
    }
    int rand = (value == -1 ? random2(total) : value % total);

    for (unsigned int i = 0; i < candidates.size(); ++i)
    {
        const unsigned int count = tile_dngn_count(candidates[i]);
        for (unsigned int j = 0; j < count; ++j)
        {
            if (rand < tile_dngn_probs(candidates[i] + j))
                return candidates[i] + j;
        }
        rand -= tile_dngn_probs(candidates[i] + count - 1);
    }

    // Should never reach this place
    ASSERT(false);
}

static bool _same_door_at(dungeon_feature_type feat, const coord_def &gc)
{
    const dungeon_feature_type door = grd(gc);
    return feat_is_closed_door(door) && feat == DNGN_SEALED_DOOR
           || door == DNGN_SEALED_DOOR && feat_is_closed_door(feat)
           || door == feat
           || map_masked(gc, MMT_WAS_DOOR_MIMIC);
}

void tile_init_flavour(const coord_def &gc)
{
    if (!map_bounds(gc))
        return;

    if (!env.tile_flv(gc).floor)
    {
        tileidx_t floor_base = env.tile_default.floor;
        int colour = env.grid_colours(gc);
        if (colour)
            floor_base = tile_dngn_coloured(floor_base, colour);
        env.tile_flv(gc).floor = _pick_random_dngn_tile(floor_base);
    }

    if (!env.tile_flv(gc).wall)
    {
        if (player_in_branch(BRANCH_MAIN_DUNGEON) && env.tile_default.wall == TILE_WALL_NORMAL)
        {
            vector<tileidx_t> tile_candidates;
            _get_dungeon_wall_tiles_by_depth(you.depth, tile_candidates);
            env.tile_flv(gc).wall = _pick_random_dngn_tile_multi(tile_candidates);
        }
        else
        {
            tileidx_t wall_base = env.tile_default.wall;
            int colour = env.grid_colours(gc);
            if (colour)
                wall_base = tile_dngn_coloured(wall_base, colour);
            env.tile_flv(gc).wall = _pick_random_dngn_tile(wall_base);
        }
    }

    if (feat_is_stone_stair(grd(gc)) && player_in_branch(BRANCH_SHOALS))
    {
        const bool up = feat_stair_direction(grd(gc)) == CMD_GO_UPSTAIRS;
        env.tile_flv(gc).feat = up ? TILE_DNGN_SHOALS_STAIRS_UP
                                   : TILE_DNGN_SHOALS_STAIRS_DOWN;
    }

    if (feat_is_door(grd(gc)))
    {
        // Check for gates.
        bool door_left  = _same_door_at(grd(gc), coord_def(gc.x - 1, gc.y));
        bool door_right = _same_door_at(grd(gc), coord_def(gc.x + 1, gc.y));
        bool door_up    = _same_door_at(grd(gc), coord_def(gc.x, gc.y - 1));
        bool door_down  = _same_door_at(grd(gc), coord_def(gc.x, gc.y + 1));

        if (door_left || door_right || door_up || door_down)
        {
            tileidx_t target;
            if (door_left && door_right)
                target = TILE_DNGN_GATE_CLOSED_MIDDLE;
            else if (door_up && door_down)
                target = TILE_DNGN_VGATE_CLOSED_MIDDLE;
            else if (door_left)
                target = TILE_DNGN_GATE_CLOSED_RIGHT;
            else if (door_right)
                target = TILE_DNGN_GATE_CLOSED_LEFT;
            else if (door_up)
                target = TILE_DNGN_VGATE_CLOSED_DOWN;
            else
                target = TILE_DNGN_VGATE_CLOSED_UP;

            // NOTE: This requires that closed gates and open gates
            // are positioned in the tile set relative to their
            // door counterpart.
            env.tile_flv(gc).special = target - TILE_DNGN_CLOSED_DOOR;
        }
        else
            env.tile_flv(gc).special = 0;
    }
    else if (!env.tile_flv(gc).special)
        env.tile_flv(gc).special = random2(256);
}

enum SpecialIdx
{
    SPECIAL_N    = 0,
    SPECIAL_NE   = 1,
    SPECIAL_E    = 2,
    SPECIAL_SE   = 3,
    SPECIAL_S    = 4,
    SPECIAL_SW   = 5,
    SPECIAL_W    = 6,
    SPECIAL_NW   = 7,
    SPECIAL_FULL = 8,
};

static int _jitter(SpecialIdx i)
{
    return (i + random_range(-1, 1) + 8) % 8;
}

static bool _adjacent_target(dungeon_feature_type target, int x, int y)
{
    for (adjacent_iterator ai(coord_def(x, y), false); ai; ++ai)
    {
        if (!map_bounds(*ai))
            continue;
        if (grd(*ai) == target)
            return true;
    }

    return false;
}

void tile_floor_halo(dungeon_feature_type target, tileidx_t tile)
{
    for (int x = 0; x < GXM; x++)
    {
        for (int y = 0; y < GYM; y++)
        {
            if (grd[x][y] < DNGN_FLOOR)
                continue;
            if (!_adjacent_target(target, x, y))
                continue;

            bool l_flr = (x > 0 && grd[x-1][y] >= DNGN_FLOOR);
            bool r_flr = (x < GXM - 1 && grd[x+1][y] >= DNGN_FLOOR);
            bool u_flr = (y > 0 && grd[x][y-1] >= DNGN_FLOOR);
            bool d_flr = (y < GYM - 1 && grd[x][y+1] >= DNGN_FLOOR);

            bool l_target = _adjacent_target(target, x-1, y);
            bool r_target = _adjacent_target(target, x+1, y);
            bool u_target = _adjacent_target(target, x, y-1);
            bool d_target = _adjacent_target(target, x, y+1);

            // The special tiles contains part floor and part special, so
            // if there are adjacent floor or special tiles, we should
            // do our best to "connect" them appropriately.  If there are
            // are other tiles there (walls, doors, whatever...) then it
            // doesn't matter.
            bool l_nrm = (l_flr && !l_target);
            bool r_nrm = (r_flr && !r_target);
            bool u_nrm = (u_flr && !u_target);
            bool d_nrm = (d_flr && !d_target);

            bool l_spc = (l_flr && l_target);
            bool r_spc = (r_flr && r_target);
            bool u_spc = (u_flr && u_target);
            bool d_spc = (d_flr && d_target);

            if (l_nrm && r_nrm || u_nrm && d_nrm)
            {
                // Not much to do here...
                env.tile_flv[x][y].floor = tile + SPECIAL_FULL;
            }
            else if (l_nrm)
            {
                if (u_nrm)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NW;
                else if (d_nrm)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SW;
                else if (u_spc && d_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_W;
                else if (u_spc && r_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SW;
                else if (d_spc && r_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NW;
                else if (u_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_W : SPECIAL_SW);
                }
                else if (d_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_W : SPECIAL_NW);
                }
                else
                    env.tile_flv[x][y].floor = tile + _jitter(SPECIAL_W);
            }
            else if (r_nrm)
            {
                if (u_nrm)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NE;
                else if (d_nrm)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SE;
                else if (u_spc && d_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_E;
                else if (u_spc && l_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SE;
                else if (d_spc && l_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NE;
                else if (u_spc)
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_E : SPECIAL_SE);
                else if (d_spc)
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_E : SPECIAL_NE);
                else
                    env.tile_flv[x][y].floor = tile + _jitter(SPECIAL_E);
            }
            else if (u_nrm)
            {
                if (r_spc && l_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_N;
                else if (r_spc && d_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NW;
                else if (l_spc && d_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_NE;
                else if (r_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_N : SPECIAL_NW);
                }
                else if (l_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_N : SPECIAL_NE);
                }
                else
                    env.tile_flv[x][y].floor = tile + _jitter(SPECIAL_N);
            }
            else if (d_nrm)
            {
                if (r_spc && l_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_S;
                else if (r_spc && u_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SW;
                else if (l_spc && u_spc)
                    env.tile_flv[x][y].floor = tile + SPECIAL_SE;
                else if (r_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_S : SPECIAL_SW);
                }
                else if (l_spc)
                {
                    env.tile_flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_S : SPECIAL_SE);
                }
                else
                    env.tile_flv[x][y].floor = tile + _jitter(SPECIAL_S);
            }
            else if (u_spc && d_spc)
            {
                // We know this value is already initialised and
                // is necessarily in bounds.
                tileidx_t t = env.tile_flv[x][y-1].floor - tile;
                if (t == SPECIAL_NE || t == SPECIAL_E)
                    env.tile_flv[x][y].floor = tile + SPECIAL_E;
                else if (t == SPECIAL_NW || t == SPECIAL_W)
                    env.tile_flv[x][y].floor = tile + SPECIAL_W;
                else
                    env.tile_flv[x][y].floor = tile + SPECIAL_FULL;
            }
            else if (r_spc && l_spc)
            {
                // We know this value is already initialised and
                // is necessarily in bounds.
                tileidx_t t = env.tile_flv[x-1][y].floor - tile;
                if (t == SPECIAL_NW || t == SPECIAL_N)
                    env.tile_flv[x][y].floor = tile + SPECIAL_N;
                else if (t == SPECIAL_SW || t == SPECIAL_S)
                    env.tile_flv[x][y].floor = tile + SPECIAL_S;
                else
                    env.tile_flv[x][y].floor = tile + SPECIAL_FULL;
            }
            else if (u_spc && l_spc)
                env.tile_flv[x][y].floor = tile + SPECIAL_SE;
            else if (u_spc && r_spc)
                env.tile_flv[x][y].floor = tile + SPECIAL_SW;
            else if (d_spc && l_spc)
                env.tile_flv[x][y].floor = tile + SPECIAL_NE;
            else if (d_spc && r_spc)
                env.tile_flv[x][y].floor = tile + SPECIAL_NW;
            else
                env.tile_flv[x][y].floor = tile + SPECIAL_FULL;
        }
    }

    // Second pass for clean up.  The only bad part about the above
    // algorithm is that it could turn a block of floor like this:
    //
    // N4NN
    // 3125
    // NN6N
    //
    // (KEY: N = normal floor, # = special floor)
    //
    // Into these flavours:
    // 1 - SPECIAL_S
    // 2 - SPECIAL_N
    // 3-6, not important
    //
    // Generally the tiles don't fit with a north to the right or left
    // of a south tile.  What we really want to do is to separate the
    // two regions, by making 1 a SPECIAL_SE and 2 a SPECIAL_NW tile.
    for (int y = 0; y < GYM - 1; ++y)
        for (int x = 0; x < GXM - 1; ++x)
        {
            int this_spc = env.tile_flv[x][y].floor - tile;
            if (this_spc < 0 || this_spc > 8)
                continue;

            if (this_spc != SPECIAL_N && this_spc != SPECIAL_S
                && this_spc != SPECIAL_E && this_spc != SPECIAL_W)
            {
                continue;
            }

            int right_spc = x < GXM - 1 ? env.tile_flv[x+1][y].floor - tile
                                        : SPECIAL_FULL;
            int down_spc  = y < GYM - 1 ? env.tile_flv[x][y+1].floor - tile
                                        : SPECIAL_FULL;

            if (this_spc == SPECIAL_N && right_spc == SPECIAL_S)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_NE;
                env.tile_flv[x+1][y].floor = tile + SPECIAL_SW;
            }
            else if (this_spc == SPECIAL_S && right_spc == SPECIAL_N)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_SE;
                env.tile_flv[x+1][y].floor = tile + SPECIAL_NW;
            }
            else if (this_spc == SPECIAL_E && down_spc == SPECIAL_W)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_SE;
                env.tile_flv[x][y+1].floor = tile + SPECIAL_NW;
            }
            else if (this_spc == SPECIAL_W && down_spc == SPECIAL_E)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_NE;
                env.tile_flv[x][y+1].floor = tile + SPECIAL_SW;
            }
        }
}

#ifdef USE_TILE
static tileidx_t _get_floor_bg(const coord_def& gc)
{
    tileidx_t bg = TILE_DNGN_UNSEEN | tileidx_unseen_flag(gc);

    if (map_bounds(gc))
    {
        bg = tileidx_feature(gc);

        if (is_unknown_stair(gc))
            bg |= TILE_FLAG_NEW_STAIR;
    }

    return bg;
}

void tile_draw_floor()
{
    for (int cy = 0; cy < env.tile_fg.height(); cy++)
        for (int cx = 0; cx < env.tile_fg.width(); cx++)
        {
            const coord_def ep(cx, cy);
            const coord_def gc = show2grid(ep);

            tileidx_t bg = _get_floor_bg(gc);

            // init tiles
            env.tile_bg(ep) = bg;
            env.tile_fg(ep) = 0;
            env.tile_cloud(ep) = 0;
        }
}

void tile_clear_map(const coord_def& gc)
{
    env.tile_bk_fg(gc) = 0;
    env.tile_bk_cloud(gc) = 0;
    tiles.update_minimap(gc);
}

void tile_forget_map(const coord_def &gc)
{
    env.tile_bk_fg(gc) = 0;
    env.tile_bk_bg(gc) = 0;
    env.tile_bk_cloud(gc) = 0;
    tiles.update_minimap(gc);
}

static void _tile_place_item(const coord_def &gc, const item_info &item,
                             bool more_items)
{
    tileidx_t t = tileidx_item(item);
    if (more_items)
        t |= TILE_FLAG_S_UNDER;

    if (you.see_cell(gc))
    {
        const coord_def ep = crawl_view.grid2show(gc);
        if (env.tile_fg(ep))
            return;

        env.tile_fg(ep) = t;

        if (item_needs_autopickup(item))
            env.tile_bg(ep) |= TILE_FLAG_CURSOR3;
    }
    else
    {
        env.tile_bk_fg(gc) = t;

        if (item_needs_autopickup(item))
            env.tile_bk_bg(gc) |= TILE_FLAG_CURSOR3;
    }
}

static void _tile_place_item_marker(const coord_def &gc, const item_def &item)
{
    if (you.see_cell(gc))
    {
        const coord_def ep = crawl_view.grid2show(gc);
        env.tile_fg(ep) |= TILE_FLAG_S_UNDER;

        if (item_needs_autopickup(item))
            env.tile_bg(ep) |= TILE_FLAG_CURSOR3;
    }
    else
    {
        env.tile_bk_fg(gc) = ((tileidx_t) env.tile_bk_fg(gc)) | TILE_FLAG_S_UNDER;

        if (item_needs_autopickup(item))
            env.tile_bk_bg(gc) |= TILE_FLAG_CURSOR3;
    }
}

/**
 * Place the tile for an unseen monster's disturbance.
 *
 * @param gc    The disturbance's map position.
**/
static void _tile_place_invisible_monster(const coord_def &gc)
{
    const coord_def ep = grid2show(gc);

    // Shallow water has its own modified tile for disturbances
    // see tileidx_feature
    if (env.map_knowledge(gc).feat() != DNGN_SHALLOW_WATER)
    {
        if (you.see_cell(gc))
            env.tile_fg(ep) = TILE_UNSEEN_MONSTER;
        else
            env.tile_bk_fg(gc) = TILE_UNSEEN_MONSTER;
    }

    if (env.map_knowledge(gc).item())
        _tile_place_item_marker(gc, *env.map_knowledge(gc).item());
}

static void _tile_place_monster(const coord_def &gc, const monster_info& mon)
{
    const coord_def ep = grid2show(gc);

    tileidx_t t    = tileidx_monster(mon);
    tileidx_t t0   = t & TILE_FLAG_MASK;
    tileidx_t flag = t & (~TILE_FLAG_MASK);

    if ((mons_class_is_stationary(mon.type)
         || mon.is(MB_WITHDRAWN))
        && mon.type != MONS_TRAINING_DUMMY)
    {
        // If necessary add item brand.
        if (env.map_knowledge(gc).item())
        {
            t |= TILE_FLAG_S_UNDER;

            if (item_needs_autopickup(*env.map_knowledge(gc).item()))
            {
                if (you.see_cell(gc))
                    env.tile_bg(ep) |= TILE_FLAG_CURSOR3;
                else
                    env.tile_bk_bg(gc) |= TILE_FLAG_CURSOR3;
            }
        }
    }
    else
    {
        tileidx_t mcache_idx = mcache.register_monster(mon);
        t = flag | (mcache_idx ? mcache_idx : t0);
    }

    if (!you.see_cell(gc))
    {
        env.tile_bk_fg(gc) = t;
        return;
    }
    env.tile_fg(ep) = t;

    // Add name tags.
    if (mons_class_flag(mon.type, M_NO_EXP_GAIN))
        return;

    const tag_pref pref = Options.tile_tag_pref;
    if (pref == TAGPREF_NONE)
        return;
    else if (pref == TAGPREF_TUTORIAL)
    {
        const int kills = you.kills->num_kills(mon);
        const int limit  = 0;

        if (!mon.is_named() && kills > limit)
            return;
    }
    else if (!mon.is_named())
        return;

    if (pref != TAGPREF_NAMED && mon.attitude == ATT_FRIENDLY)
        return;

    tiles.add_text_tag(TAG_NAMED_MONSTER, mon);
}

void tile_reset_fg(const coord_def &gc)
{
    // remove autopickup cursor, it will be added back if necessary
    env.tile_bk_bg(gc) &= ~TILE_FLAG_CURSOR3;
    tile_draw_map_cell(gc, true);
    tiles.update_minimap(gc);
}

void tile_reset_feat(const coord_def &gc)
{
    env.tile_bk_bg(gc) = tileidx_feature(gc);
}

static void _tile_place_cloud(const coord_def &gc, const cloud_info &cl)
{
    // In the Shoals, ink is handled differently. (jpeg)
    // I'm not sure it is even possible anywhere else, but just to be safe...
    if (cl.type == CLOUD_INK && player_in_branch(BRANCH_SHOALS))
        return;

    bool disturbance = false;

    if (env.map_knowledge(gc).invisible_monster())
        disturbance = true;

    if (you.see_cell(gc))
    {
        const coord_def ep = grid2show(gc);
        env.tile_cloud(ep) = tileidx_cloud(cl, disturbance);
    }
    else
        env.tile_bk_cloud(gc) = tileidx_cloud(cl, disturbance);
}

unsigned int num_tile_rays = 0;
struct tile_ray
{
    coord_def ep;
    aff_type in_range;
};
FixedVector<tile_ray, 40> tile_ray_vec;

void tile_place_ray(const coord_def &gc, aff_type in_range)
{
    // Record rays for later.  The curses version just applies
    // rays directly to the screen.  The tiles version doesn't have
    // (nor want) such direct access.  So, it batches up all of the
    // rays and applies them in viewwindow(...).
    if (num_tile_rays < tile_ray_vec.size() - 1)
    {
        tile_ray_vec[num_tile_rays].in_range = in_range;
        tile_ray_vec[num_tile_rays++].ep = grid2show(gc);
    }
}

void tile_draw_rays(bool reset_count)
{
    for (unsigned int i = 0; i < num_tile_rays; i++)
    {
        tileidx_t flag = tile_ray_vec[i].in_range > AFF_MAYBE ? TILE_FLAG_RAY
                                                           : TILE_FLAG_RAY_OOR;
        env.tile_bg(tile_ray_vec[i].ep) |= flag;
    }

    if (reset_count)
        num_tile_rays = 0;
}

void tile_draw_map_cell(const coord_def& gc, bool foreground_only)
{
    if (!foreground_only)
        env.tile_bk_bg(gc) = _get_floor_bg(gc);

    if (you.see_cell(gc))
    {
        env.tile_fg(grid2show(gc)) = 0;
        env.tile_cloud(grid2show(gc)) = 0;
    }

    const map_cell& cell = env.map_knowledge(gc);

    if (cell.invisible_monster())
        _tile_place_invisible_monster(gc);
    else if (cell.monsterinfo())
        _tile_place_monster(gc, *cell.monsterinfo());
    else if (cell.item())
    {
        if (feat_is_stair(cell.feat()))
            _tile_place_item_marker(gc, *cell.item());
        else
            _tile_place_item(gc, *cell.item(), (cell.flags & MAP_MORE_ITEMS) != 0);
    }
    else
        env.tile_bk_fg(gc) = 0;

    // Always place clouds now they have their own layer
    if (cell.cloud() != CLOUD_NONE)
        _tile_place_cloud(gc, *cell.cloudinfo());
    else
        env.tile_bk_cloud(gc) = 0;
}

void tile_wizmap_terrain(const coord_def &gc)
{
    env.tile_bk_bg(gc) = tileidx_feature(gc);
}

static bool _is_torch(tileidx_t basetile)
{
    return (basetile == TILE_WALL_BRICK_DARK_2_TORCH
            || basetile == TILE_WALL_BRICK_DARK_4_TORCH
            || basetile == TILE_WALL_BRICK_DARK_6_TORCH);
}

// Updates the "flavour" of tiles that are animated.
// Unfortunately, these are all hard-coded for now.
void tile_apply_animations(tileidx_t bg, tile_flavour *flv)
{
#ifndef USE_TILE_WEB
    tileidx_t bg_idx = bg & TILE_FLAG_MASK;
    if (bg_idx == TILE_DNGN_PORTAL_WIZARD_LAB)
        flv->special = (flv->special + 1) % tile_dngn_count(bg_idx);
    else if (bg_idx == TILE_DNGN_LAVA)
    {
        // Lava tiles are four sets of four tiles (the second and fourth
        // sets are the same). This cycles between the four sets, picking
        // a random element from each set.
        flv->special = ((flv->special - ((flv->special % 4)))
                        + 4 + random2(4)) % tile_dngn_count(bg_idx);
    }
    else if (bg_idx > TILE_DNGN_LAVA && bg_idx < TILE_BLOOD)
        flv->special = random2(256);
    else if (bg_idx == TILE_WALL_NORMAL)
    {
        tileidx_t basetile = tile_dngn_basetile(flv->wall);
        if (_is_torch(basetile))
            flv->wall = basetile + (flv->wall - basetile + 1) % tile_dngn_count(basetile);
    }
#endif
}

static bool _suppress_blood(const map_cell& mc)
{
    const dungeon_feature_type feat = mc.feat();
    if (feat_is_tree(feat))
        return true;

    if (feat >= DNGN_FOUNTAIN_BLUE && feat <= DNGN_PERMADRY_FOUNTAIN)
        return true;

    if (feat_is_altar(feat))
        return true;

    if (feat_stair_direction(feat) != CMD_NO_CMD)
        return true;

    if (feat == DNGN_MALIGN_GATEWAY)
        return true;

    if (mc.trap() == TRAP_SHAFT)
        return true;

    return false;
}

static bool _suppress_blood(tileidx_t bg_idx)
{
    tileidx_t basetile = tile_dngn_basetile(bg_idx);
    return _is_torch(basetile);
}

// Specifically for vault-overwritten doors. We have three "sets" of tiles that
// can be dealt with. The tile sets should be 2, 3, 8 and 9 respectively. They
// are:
//  2. Closed, open.
//  3. Runed, closed, open.
//  8. Closed, open, gate left closed, gate middle closed, gate right closed,
//     gate left open, gate middle open, gate right open.
//  9. Runed, closed, open, gate left closed, gate middle closed, gate right
//     closed, gate left open, gate middle open, gate right open.
static int _get_door_offset(tileidx_t base_tile, bool opened, bool runed,
                            int gateway_type)
{
    int count = tile_dngn_count(base_tile);
    if (count == 1)
        return 0;

    // The location of the default "closed" tile.
    int offset = 0;

    switch (count)
    {
    case 2:
        ASSERT(!runed);
        return opened ? 1: 0;
    case 3:
        if (opened)
            return 2;
        else if (runed)
            return 0;
        else
            return 1;
    case 8:
        ASSERT(!runed);
        // The closed door is at BASE_TILE for sets without runed doors
        offset = 0;
        break;
    case 9:
        // But is at BASE_TILE+1 for sets with them.
        offset = 1;
        break;
    default:
        // Passed a non-door tile base, pig out now.
        die("non-door tile");
    }

    // If we've reached this point, we're dealing with a gate.
    if (runed)
        return 0;

    if (!opened && !runed && gateway_type == 0)
        return 0;

    return offset + gateway_type;
}

void apply_variations(const tile_flavour &flv, tileidx_t *bg,
                      const coord_def &gc)
{
    tileidx_t orig = (*bg) & TILE_FLAG_MASK;
    tileidx_t flag = (*bg) & (~TILE_FLAG_MASK);

    // TODO: allow the stone type to be set in a cleaner way.
    if (player_in_branch(BRANCH_LABYRINTH))
    {
        if (orig == TILE_DNGN_STONE_WALL)
            orig = TILE_WALL_LAB_STONE;
        else if (orig == TILE_DNGN_METAL_WALL)
            orig = TILE_WALL_LAB_METAL;
    }
    else if (player_in_branch(BRANCH_CRYPT))
    {
        if (orig == TILE_DNGN_STONE_WALL)
            orig = TILE_WALL_CRYPT;
        else if (orig == TILE_DNGN_METAL_WALL)
            orig = TILE_WALL_CRYPT_METAL;
        else if (orig == TILE_DNGN_OPEN_DOOR)
            orig = TILE_DNGN_OPEN_DOOR_CRYPT;
        else if (orig == TILE_DNGN_CLOSED_DOOR)
            orig = TILE_DNGN_CLOSED_DOOR_CRYPT;
    }
    else if (player_in_branch(BRANCH_TOMB))
    {
        if (orig == TILE_DNGN_STONE_WALL)
            orig = TILE_WALL_TOMB;
    }
    else if (player_in_branch(BRANCH_DIS))
    {
        if (orig == TILE_DNGN_METAL_WALL)
            orig = TILE_DNGN_METAL_IRON;
        else if (orig == TILE_DNGN_CRYSTAL)
            orig = TILE_WALL_EMERALD;
    }
    else if (player_in_branch(BRANCH_COCYTUS))
    {
        if (orig == TILE_DNGN_STONE_WALL)
            orig = TILE_WALL_ICY_STONE;
    }
    else if (player_in_branch(BRANCH_TARTARUS))
    {
        if (orig == TILE_DNGN_STONE_WALL)
            orig = TILE_WALL_COBALT_STONE;
        else if (orig == TILE_DNGN_CRYSTAL)
            orig = TILE_WALL_EMERALD;
        else if (orig == TILE_DNGN_METAL_WALL)
            orig = TILE_DNGN_METAL_WALL_DARKGRAY;
    }
    else if (player_in_branch(BRANCH_GEHENNA))
    {
        if (orig == TILE_DNGN_STONE_WALL)
            orig = TILE_DNGN_STONE_WALL_RED;
        if (orig == TILE_DNGN_METAL_WALL)
            orig = TILE_DNGN_METAL_WALL_RED;
    }
    else if (player_in_branch(BRANCH_BAILEY))
    {
        if (orig == TILE_DNGN_STONE_WALL)
            orig = TILE_WALL_STONE_BRICK;
    }
    else if (player_in_branch(BRANCH_OSSUARY))
    {
        if (orig == TILE_DNGN_STONE_WALL)
            orig = TILE_DNGN_STONE_WALL_BROWN;
    }
    else if (player_in_branch(BRANCH_SLIME_PITS))
    {
        if (orig == TILE_DNGN_STONE_WALL)
            orig = TILE_STONE_WALL_SLIME;
    }

    const bool mimic = monster_at(gc) && mons_is_feat_mimic(monster_at(gc)->type);

    if (orig == TILE_FLOOR_NORMAL)
        *bg = flv.floor;
    else if (orig == TILE_WALL_NORMAL)
        *bg = flv.wall;
    else if (orig == TILE_DNGN_STONE_WALL)
    {
        *bg = _pick_random_dngn_tile(tile_dngn_coloured(orig,
                                                        env.grid_colours(gc)),
                                     flv.special);
    }
    else if (is_door_tile(orig) && !mimic)
    {
        tileidx_t override = flv.feat;
        /*
          Was: secret doors.  Is it ever needed anymore?
         */
        if (is_door_tile(override))
        {
            bool opened = (orig == TILE_DNGN_OPEN_DOOR);
            bool runed = (orig == TILE_DNGN_RUNED_DOOR);
            int offset = _get_door_offset(override, opened, runed, flv.special);
            *bg = override + offset;
        }
        else
            *bg = orig + min((int)flv.special, 6);
    }
    else if (orig == TILE_DNGN_PORTAL_WIZARD_LAB)
        *bg = orig + flv.special % tile_dngn_count(orig);
    else if (orig < TILE_DNGN_MAX)
        *bg = _pick_random_dngn_tile(orig, flv.special);

    *bg |= flag;
}

// If the top tile is a corpse, don't draw blood underneath.
static bool _top_item_is_corpse(const map_cell& mc)
{
    const item_info* item = mc.item();
    return (item
            && item->base_type == OBJ_CORPSES
            && item->sub_type == CORPSE_BODY);
}

static uint8_t _get_direction_index(const coord_def& delta)
{
    if (delta.x ==  0 && delta.y ==  1) return 1;
    if (delta.x == -1 && delta.y ==  1) return 2;
    if (delta.x == -1 && delta.y ==  0) return 3;
    if (delta.x == -1 && delta.y == -1) return 4;
    if (delta.x ==  0 && delta.y == -1) return 5;
    if (delta.x ==  1 && delta.y == -1) return 6;
    if (delta.x ==  1 && delta.y ==  0) return 7;
    if (delta.x ==  1 && delta.y ==  1) return 8;
    return 0;
}

void tile_apply_properties(const coord_def &gc, packed_cell &cell)
{
    if (is_excluded(gc))
    {
        if (is_exclude_root(gc))
            cell.bg |= TILE_FLAG_EXCL_CTR;
        else
            cell.bg |= TILE_FLAG_TRAV_EXCL;
    }

    if (!map_bounds(gc))
        return;

    apply_variations(env.tile_flv(gc), &cell.bg, gc);

    const map_cell& mc = env.map_knowledge(gc);

    bool print_blood = true;
    if (mc.flags & MAP_UMBRAED)
        cell.halo = HALO_UMBRA;
    else if (mc.flags & MAP_HALOED)
    {
        monster_info* mon = mc.monsterinfo();
        if (mon && !mons_class_flag(mon->type, M_NO_EXP_GAIN))
        {
            cell.halo = HALO_MONSTER;
            print_blood = false;
        }
        else
            cell.halo = HALO_RANGE;
    }
    else
        cell.halo = HALO_NONE;

    if (mc.flags & MAP_LIQUEFIED)
        cell.is_liquefied = true;
    else if (print_blood && (_suppress_blood(mc)
                             || _suppress_blood((cell.bg) & TILE_FLAG_MASK)))
    {
        print_blood = false;
    }

    // Mold has the same restrictions as blood but takes precedence.
    if (print_blood)
    {
        if (mc.flags & MAP_GLOWING_MOLDY)
            cell.glowing_mold = true;
        else if (mc.flags & MAP_MOLDY)
            cell.is_moldy = true;
        // Corpses have a blood puddle of their own.
        else if (mc.flags & MAP_BLOODY && !_top_item_is_corpse(mc))
        {
            cell.is_bloody = true;
            cell.blood_rotation = blood_rotation(gc);
            cell.old_blood = env.pgrid(gc) & FPROP_OLD_BLOOD;
        }
    }

    const dungeon_feature_type feat = mc.feat();
    if (feat_is_water(feat) || feat == DNGN_LAVA)
        cell.bg |= TILE_FLAG_WATER;

    if ((mc.flags & MAP_SANCTUARY_1) || (mc.flags & MAP_SANCTUARY_2))
        cell.is_sanctuary = true;

    if (mc.flags & MAP_SILENCED)
        cell.is_silenced = true;

    if (mc.flags & MAP_SUPPRESSED)
        cell.is_suppressed = true;

    if (feat == DNGN_MANGROVE)
        cell.mangrove_water = true;

    if (mc.flags & MAP_ORB_HALOED)
        cell.orb_glow = get_orb_phase(gc) ? 2 : 1;

    if (mc.flags & MAP_HOT)
        cell.heat_aura = 1 + random2(3);

    if (mc.flags & MAP_QUAD_HALOED)
        cell.quad_glow = true;

    if (mc.flags & MAP_DISJUNCT)
        cell.disjunct = get_disjunct_phase(gc);

    if (Options.show_travel_trail)
    {
        int tt_idx = travel_trail_index(gc);
        if (tt_idx >= 0 && tt_idx < (int) env.travel_trail.size() - 1)
        {
            if (tt_idx > 0)
            {
                coord_def delta = gc - env.travel_trail[tt_idx-1];
                cell.travel_trail = _get_direction_index(delta);
            }
            if (tt_idx < (int) env.travel_trail.size() - 1)
            {
                coord_def delta = gc - env.travel_trail[tt_idx+1];
                cell.travel_trail |= _get_direction_index(delta) << 4;
            }
        }
    }
}
#endif
