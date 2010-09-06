#include "AppHdr.h"

#ifdef USE_TILE
#include "tileview.h"

#include "areas.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "fprop.h"
#include "items.h"
#include "kills.h"
#include "libutil.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "showsymb.h"
#include "stuff.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tilemcache.h"
#include "tilepick.h"
#include "traps.h"
#include "travel.h"
#include "viewgeom.h"

void tile_new_level(bool first_time)
{
    tiles.clear_minimap();

    for (unsigned int x = 0; x < GXM; x++)
        for (unsigned int y = 0; y < GYM; y++)
            tiles.update_minimap(coord_def(x, y));

    if (first_time)
        tile_init_flavour();

    if (!player_in_mappable_area() || first_time)
    {
        for (unsigned int x = 0; x < GXM; x++)
            for (unsigned int y = 0; y < GYM; y++)
            {
                env.tile_bk_fg[x][y] = 0;
                env.tile_bk_bg[x][y] = TILE_DNGN_UNSEEN;
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
}

void tile_init_default_flavour()
{
    tile_default_flv(you.level_type, you.where_are_you, env.tile_default);
}


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
        case 6: flv.wall = TILE_WALL_ZOT_WHITE; break;
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
        flv.wall    = TILE_WALL_NORMAL;
        flv.floor   = TILE_FLOOR_NORMAL;
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
        flv.wall  = TILE_WALL_YELLOW_ROCK;
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

void tile_clear_flavour()
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        env.tile_flv(*ri).floor   = 0;
        env.tile_flv(*ri).wall    = 0;
        env.tile_flv(*ri).special = 0;
        env.tile_flv(*ri).feat    = 0;
    }
}

// For floors and walls that have not already been set to a particular tile,
// set them to a random instance of the default floor and wall tileset.
void tile_init_flavour()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        tile_init_flavour(*ri);
}

static tileidx_t _pick_random_dngn_tile(tileidx_t idx, int value = -1)
{
    ASSERT(idx >= 0 && idx < TILE_DNGN_MAX);
    const int count = tile_dngn_count(idx);
    if (count == 1)
        return (idx);

    const int total = tile_dngn_probs(idx + count - 1);
    const int rand  = (value == -1 ? random2(total) : value % total);

    for (int i = 0; i < count; ++i)
    {
        tileidx_t curr = idx + i;
        if (rand < tile_dngn_probs(curr))
            return (curr);
    }

    return (idx);
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
        tileidx_t wall_base = env.tile_default.wall;
        int colour = env.grid_colours(gc);
        if (colour)
            wall_base = tile_dngn_coloured(wall_base, colour);
        env.tile_flv(gc).wall = _pick_random_dngn_tile(wall_base);
    }

    if (feat_is_door(grd(gc)))
    {
        // Check for horizontal gates.

        const coord_def left(gc.x - 1, gc.y);
        const coord_def right(gc.x + 1, gc.y);

        bool door_left  = (grd(left) == grd(gc));
        bool door_right = (grd(right) == grd(gc));

        if (door_left || door_right)
        {
            tileidx_t target;
            if (door_left && door_right)
                target = TILE_DNGN_GATE_CLOSED_MIDDLE;
            else if (door_left)
                target = TILE_DNGN_GATE_CLOSED_RIGHT;
            else
                target = TILE_DNGN_GATE_CLOSED_LEFT;

            // NOTE: This requires that closed gates and open gates
            // are positioned in the tile set relative to their
            // door counterpart.
            env.tile_flv(gc).special = target - TILE_DNGN_CLOSED_DOOR;
        }
        else
            env.tile_flv(gc).special = 0;
    }
    else if (feat_is_secret_door(grd(gc)))
        env.tile_flv(gc).special = 0;
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
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
        {
            if (!map_bounds(x+i, y+j))
                continue;
            if (grd[x+i][y+j] == target)
                return (true);
        }

    return (false);
}

void tile_floor_halo(dungeon_feature_type target, tileidx_t tile)
{
    for (int x = 0; x < GXM; x++)
    {
        for (int y = 0; y < GYM; y++)
        {
            if (grd[x][y] < DNGN_FLOOR_MIN)
                continue;
            if (!_adjacent_target(target, x, y))
                continue;

            bool l_flr = (x > 0) ? grd[x-1][y] >= DNGN_FLOOR_MIN : false;
            bool r_flr = (x < GXM - 1) ? grd[x+1][y] >= DNGN_FLOOR_MIN : false;
            bool u_flr = (y > 0) ? grd[x][y-1] >= DNGN_FLOOR_MIN : false;
            bool d_flr = (y < GYM - 1) ? grd[x][y+1] >= DNGN_FLOOR_MIN : false;

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
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_SE;
            }
            else if (u_spc && r_spc)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_SW;
            }
            else if (d_spc && l_spc)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_NE;
            }
            else if (d_spc && r_spc)
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_NW;
            }
            else
            {
                env.tile_flv[x][y].floor = tile + SPECIAL_FULL;
            }
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

void tile_draw_floor()
{
    for (int cy = 0; cy < env.tile_fg.height(); cy++)
        for (int cx = 0; cx < env.tile_fg.width(); cx++)
        {
            const coord_def ep(cx, cy);
            const coord_def gc = show2grid(ep);

            tileidx_t bg = TILE_DNGN_UNSEEN | tileidx_unseen_flag(gc);

            if (you.see_cell(gc))
            {
                bg = tileidx_feature(gc);

                dungeon_feature_type feat = grid_appearance(gc);
                if (feat == DNGN_DETECTED_SECRET_DOOR)
                     bg |= TILE_FLAG_WAS_SECRET;
                else if (is_unknown_stair(gc))
                     bg |= TILE_FLAG_NEW_STAIR;
            }


            // init tiles
            env.tile_bg(ep) = bg;
            env.tile_fg(ep) = 0;
        }
}

// Called from item() in view.cc
void tile_place_item(const coord_def &gc, const item_def &item)
{
    tileidx_t t = tileidx_item(item);
    if (item.link != NON_ITEM)
        t |= TILE_FLAG_S_UNDER;

    env.tile_fg(gc) = t;

    if (item_needs_autopickup(item))
        env.tile_bg(gc) |= TILE_FLAG_CURSOR3;
}

// Called from item() in view.cc
void tile_place_item_marker(const coord_def &gc, const item_def &item)
{
    env.tile_fg(gc) |= TILE_FLAG_S_UNDER;

    if (item_needs_autopickup(item))
        env.tile_bg(gc) |= TILE_FLAG_CURSOR3;
}

// Called from show_def::_update_monster() in show.cc
void tile_place_monster(const coord_def &gc, const monsters *mon)
{
    if (!mon)
        return;

    const coord_def ep = view2show(grid2view(gc));

    tileidx_t t = tileidx_monster(mon);
    tileidx_t t0   = t & TILE_FLAG_MASK;
    tileidx_t flag = t & (~TILE_FLAG_MASK);

    if (mons_is_mimic(mon->type))
    {
        if (!mons_is_known_mimic(mon))
        {
            // If necessary add item brand.
            if (you.visible_igrd(gc) != NON_ITEM)
                t |= TILE_FLAG_S_UNDER;

            if (item_needs_autopickup(get_mimic_item(mon)))
                env.tile_bg(ep) |= TILE_FLAG_CURSOR3;
        }
    }
    else if (mons_is_stationary(mon))
    {
        // If necessary add item brand.
        if (you.visible_igrd(gc) != NON_ITEM)
            t |= TILE_FLAG_S_UNDER;
    }
    else
    {
        tileidx_t mcache_idx = mcache.register_monster(mon);
        t = flag | (mcache_idx ? mcache_idx : t0);
    }

    // Add name tags.
    env.tile_fg(ep) = t;

    if (!mon->visible_to(&you)
        || mons_is_lurking(mon)
        || mons_is_unknown_mimic(mon)
        || mons_class_flag(mon->type, M_NO_EXP_GAIN))
    {
        return;
    }

    const tag_pref pref = Options.tile_tag_pref;
    if (pref == TAGPREF_NONE)
        return;
    else if (pref == TAGPREF_TUTORIAL)
    {
        const long kills = you.kills->num_kills(mon);
        const int limit  = 0;

        if (!mon->is_named() && kills > limit)
            return;
    }
    else if (!mon->is_named())
        return;

    if (pref != TAGPREF_NAMED && mon->friendly())
        return;

    // HACK.  Names cover up pan demons in a weird way.
    if (mon->type == MONS_PANDEMONIUM_DEMON)
        return;

    tiles.add_text_tag(TAG_NAMED_MONSTER, mon);
}

void tile_place_cloud(const coord_def &gc, const cloud_struct &cl)
{
    const coord_def ep = view2show(grid2view(gc));
    // In the Shoals, ink is handled differently. (jpeg)
    // I'm not sure it is even possible anywhere else, but just to be safe...
    if (cl.type != CLOUD_INK || !player_in_branch(BRANCH_SHOALS))
        env.tile_fg(ep) = tileidx_cloud(cl);
}

unsigned int num_tile_rays = 0;
struct tile_ray
{
    coord_def ep;
    bool in_range;
};
FixedVector<tile_ray, 30> tile_ray_vec;

void tile_place_ray(const coord_def &gc, bool in_range)
{
    // Record rays for later.  The curses version just applies
    // rays directly to the screen.  The tiles version doesn't have
    // (nor want) such direct access.  So, it batches up all of the
    // rays and applies them in viewwindow(...).
    if (num_tile_rays < tile_ray_vec.size() - 1)
    {
        tile_ray_vec[num_tile_rays].in_range = in_range;
        tile_ray_vec[num_tile_rays++].ep = view2show(grid2view(gc));
    }
}

void tile_draw_rays(bool reset_count)
{
    for (unsigned int i = 0; i < num_tile_rays; i++)
    {
        tileidx_t flag = tile_ray_vec[i].in_range ? TILE_FLAG_RAY
                                                  : TILE_FLAG_RAY_OOR;
        env.tile_bg(tile_ray_vec[i].ep) |= flag;
    }

    if (reset_count)
        num_tile_rays = 0;
}

void tile_wizmap_terrain(const coord_def &gc)
{
    env.tile_bk_bg(gc) = tileidx_feature(gc);
}

// Updates the "flavour" of tiles that are animated.
// Unfortunately, these are all hard-coded for now.
void tile_apply_animations(tileidx_t bg, tile_flavour *flv)
{
    tileidx_t bg_idx = bg & TILE_FLAG_MASK;
    if (bg_idx >= TILE_DNGN_LAVA && bg_idx < TILE_BLOOD)
    {
        flv->special = random2(256);
    }
    else if (bg_idx == TILE_DNGN_PORTAL_WIZARD_LAB
             || bg_idx == TILE_DNGN_ALTAR_CHEIBRIADOS)
    {
        flv->special = (flv->special + 1) % tile_dngn_count(bg_idx);
    }
    else if (bg_idx == TILE_WALL_NORMAL
             && flv->wall >= TILE_WALL_BRICK_TORCH_START
             && flv->wall <= TILE_WALL_BRICK_TORCH_END)
    {
        flv->wall += 1;
        if (flv->wall > TILE_WALL_BRICK_TORCH_END)
            flv->wall = TILE_WALL_BRICK_TORCH_START;
    }
}

static bool _suppress_blood(const coord_def pos)
{
    if (!you.see_cell(pos))
        return (true);

    const dungeon_feature_type feat = grd(pos);
    if (feat == DNGN_TREE)
        return (true);

    if (feat >= DNGN_FOUNTAIN_BLUE && feat <= DNGN_PERMADRY_FOUNTAIN)
        return (true);

    if (feat_is_altar(feat))
        return (true);

    if (feat_stair_direction(feat) != CMD_NO_CMD)
        return (true);

    const trap_def *trap = find_trap(pos);
    if (trap && trap->type == TRAP_SHAFT && trap->is_known())
        return (true);

    return (false);
}

// Specifically for vault-overwritten doors. We have three "sets" of tiles that
// can be dealt with. The tile sets should be 2, 3, 8 and 9 respectively. They
// are:
//  2. Closed, open.
//  3. Detected, closed, open.
//  8. Closed, open, gate left closed, gate middle closed, gate right closed,
//     gate left open, gate middle open, gate right open.
//  9. Detected, closed, open, gate left closed, gate middle closed, gate right
//     closed, gate left open, gate middle open, gate right open.
static int _get_door_offset (tileidx_t base_tile, bool opened = false,
                             bool detected = false, int gateway_type = 0)
{
    int count = tile_dngn_count(base_tile);
    if (count == 1)
        return 0;

    // The location of the default "closed" tile.
    int offset = 0;

    switch (count)
    {
    case 2:
        return ((opened) ? 1: 0);
    case 3:
        if (opened)
            return 2;
        else if (detected)
            return 0;
        else
            return 1;
    case 8:
        // But is BASE_TILE for others.
        offset = 0;
        break;
    case 9:
        // It's located at BASE_TILE+1 for tile sets with detected doors
        offset = 1;
        break;
    default:
        // Passed a non-door tile base, pig out now.
        ASSERT(false);
    }

    // If we've reached this point, we're dealing with a gate.
    // Don't believe gateways deal differently with detection.
    if (detected)
        return 0;

    if (!opened && !detected && gateway_type == 0)
        return 0;

    return offset + gateway_type;
}

static inline void _apply_variations(const tile_flavour &flv, tileidx_t *bg)
{
    tileidx_t orig = (*bg) & TILE_FLAG_MASK;
    tileidx_t flag = (*bg) & (~TILE_FLAG_MASK);

    // TODO enne - expose this as an option, so ziggurat can use it too.
    // Alternatively, allow the stone type to be set.
    //
    // Hack: Swap rock/stone in crypt and tomb, because there are
    //       only stone walls.
    if ((you.where_are_you == BRANCH_CRYPT || you.where_are_you == BRANCH_TOMB)
        && orig == TILE_DNGN_STONE_WALL)
    {
        orig = TILE_WALL_NORMAL;
    }

    if (orig == TILE_FLOOR_NORMAL)
        *bg = flv.floor;
    else if (orig == TILE_WALL_NORMAL)
        *bg = flv.wall;
    else if (orig == TILE_DNGN_CLOSED_DOOR || orig == TILE_DNGN_OPEN_DOOR)
    {
        tileidx_t override = flv.feat;
        // Setting an override on a door specifically for undetected secret
        // doors causes issues if there are a number of variants for that tile.
        // In these instances, append "last_tile" and have the tile specifier
        // for the door in question on its own line, and it should bypass any
        // asserts or weird visual issues. Somewhat hackish. The following code
        // assumes that if there is an override on a door location and that
        // has no variations, that the override is not actually a door tile but
        // the aforementioned secret door thing. {due}
        if (override && tile_dngn_count(override) > 1)
        {
            // XXX: This doesn't deal properly with detected doors.
            bool opened = (orig == TILE_DNGN_OPEN_DOOR);
            int offset = _get_door_offset(override, opened, false, flv.special);
            *bg = override + offset;
        }
        else
            *bg = orig + std::min((int)flv.special, 3);
    }
    else if (orig == TILE_DNGN_PORTAL_WIZARD_LAB
             || orig == TILE_DNGN_ALTAR_CHEIBRIADOS)
    {
        *bg = orig + flv.special % tile_dngn_count(orig);
    }
    else if (orig < TILE_DNGN_MAX)
    {
        *bg = _pick_random_dngn_tile(orig, flv.special);
    }

    *bg |= flag;
}

void tile_apply_properties(const coord_def &gc, tileidx_t *fg,
                           tileidx_t *bg)
{
    if (is_excluded(gc))
    {
        if (is_exclude_root(gc))
            *bg |= TILE_FLAG_EXCL_CTR;
        else
            *bg |= TILE_FLAG_TRAV_EXCL;
    }

    if (!map_bounds(gc))
        return;

    _apply_variations(env.tile_flv(gc), bg);

    bool print_blood = true;
    if (haloed(gc))
    {
        monsters *mon = monster_at(gc);
        if (you.see_cell(gc) && mon)
        {
            if (!mons_class_flag(mon->type, M_NO_EXP_GAIN)
                 && (!mons_is_mimic(mon->type)
                     || testbits(mon->flags, MF_KNOWN_MIMIC)))
            {
                *bg |= TILE_FLAG_HALO;
                print_blood = false;
            }
        }
    }

    if (print_blood && _suppress_blood(gc))
        print_blood = false;

    // Mold has the same restrictions as blood
    // but mold takes precendence over blood.
    if (print_blood)
    {
        if (is_moldy(gc))
            *bg |= TILE_FLAG_MOLD;
        else if (is_bloodcovered(gc))
            *bg |= TILE_FLAG_BLOOD;
    }

    const dungeon_feature_type feat = grd(gc);
    if (feat_is_water(feat) || feat == DNGN_LAVA)
        *bg |= TILE_FLAG_WATER;

    if (is_sanctuary(gc))
        *bg |= TILE_FLAG_SANCTUARY;

    if (silenced(gc))
        *bg |= TILE_FLAG_SILENCED;
}

void tile_clear_map(const coord_def& gc)
{
    env.tile_bk_fg(gc) = 0;
    tiles.update_minimap(gc);
}

void tile_forget_map(const coord_def &gc)
{
    env.tile_bk_fg(gc) = 0;
    env.tile_bk_bg(gc) = 0;
    tiles.update_minimap(gc);
}

#endif
