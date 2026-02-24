#include "AppHdr.h"

#include "tileview.h"

#include "areas.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "domino.h"
#include "domino-data.h"
#include "dungeon.h"
#include "env.h"
#include "tile-env.h"
#include "fprop.h"
#include "items.h"
#include "kills.h"
#include "level-state-type.h"
#include "mon-util.h"
#include "movement.h"
#include "options.h"
#include "pcg.h"
#include "player.h"
#include "state.h"
#include "tag-version.h"
#include "terrain.h"
#include "tile-flags.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-player.h"
#include "tilemcache.h"
#include "tilepick.h"
#include "tiles-build-specific.h"
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
                tile_env.bk_fg[x][y] = 0;
                tile_env.bk_bg[x][y] = TILE_DNGN_UNSEEN;
                tile_env.bk_cloud[x][y] = 0;
            }
    }

    // Fix up stair markers. The travel information isn't hooked up
    // until after we change levels. So, look through all of the stairs
    // on this level and check if they still need the stair flag.
    for (unsigned int x = 0; x < GXM; x++)
        for (unsigned int y = 0; y < GYM; y++)
        {
            unsigned int tile = tile_env.bk_bg[x][y];
            if ((tile & TILE_FLAG_NEW_STAIR)
                && !is_unknown_stair(coord_def(x,y)))
            {
                tile_env.bk_bg[x][y] &= ~TILE_FLAG_NEW_STAIR;
            }
            else if ((tile & TILE_FLAG_NEW_TRANSPORTER)
                     && !is_unknown_transporter(coord_def(x,y)))
                tile_env.bk_bg[x][y] &= ~TILE_FLAG_NEW_TRANSPORTER;
        }

    tiles.clear_minimap();

    for (unsigned int x = 0; x < GXM; x++)
        for (unsigned int y = 0; y < GYM; y++)
            tiles.update_minimap(coord_def(x, y));
#else
    UNUSED(init_unseen);
#endif
}

void tile_init_default_flavour()
{
    tile_default_flv(you.where_are_you, tile_env.default_flavour);
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
    case BRANCH_DUNGEON:
        flv.wall  = TILE_WALL_NORMAL;
        flv.floor = TILE_FLOOR_NORMAL;
        return;

    case BRANCH_DEPTHS:
        flv.wall  = TILE_WALL_NORMAL;
        flv.floor = TILE_FLOOR_GREY_DIRT_B;
        return;

    case BRANCH_VAULTS:
        flv.wall  = TILE_WALL_VAULT;
        flv.floor = TILE_FLOOR_VAULT;
        return;

    case BRANCH_TEMPLE:
        flv.wall  = TILE_WALL_VINES;
        flv.floor = TILE_FLOOR_VINES;
        return;

#if TAG_MAJOR_VERSION == 34
    case BRANCH_DWARF:
        flv.wall  = TILE_WALL_HALL;
        flv.floor = TILE_FLOOR_LIMESTONE;
        return;
#endif

#if TAG_MAJOR_VERSION == 34
    case BRANCH_BLADE:
#endif
    case BRANCH_ELF:
        flv.wall  = TILE_WALL_HALL;
        flv.floor = TILE_FLOOR_HALL;
        return;

    case BRANCH_TARTARUS:
        flv.wall  = TILE_WALL_COBALT_ROCK;
        flv.floor = TILE_FLOOR_BLACK_COBALT;
        return;

    case BRANCH_CRYPT:
        flv.wall  = TILE_ROCK_WALL_CRYPT;
        flv.floor = TILE_FLOOR_CRYPT;
        return;

    case BRANCH_TOMB:
        flv.wall  = TILE_WALL_UNDEAD;
        flv.floor = TILE_FLOOR_TOMB;
        return;

    case BRANCH_VESTIBULE:
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

    case BRANCH_ORC:
        flv.wall  = TILE_WALL_ORC;
        flv.floor = TILE_FLOOR_ORC;
        return;

    case BRANCH_LAIR:
        flv.wall  = TILE_WALL_LAIR;
        flv.floor = TILE_FLOOR_LAIR;
        return;

    case BRANCH_SLIME:
        flv.wall  = TILE_WALL_SLIME;
        flv.floor = TILE_FLOOR_SLIME;
        return;

    case BRANCH_SNAKE:
        flv.wall  = TILE_WALL_SNAKE;
        flv.floor = TILE_FLOOR_MOSAIC;
        return;

    case BRANCH_SWAMP:
        flv.wall  = TILE_WALL_SWAMP;
        flv.floor = TILE_FLOOR_SWAMP;
        return;

    case BRANCH_SHOALS:
        flv.wall  = TILE_WALL_SHOALS;
        flv.floor = TILE_FLOOR_SAND;
        return;

    case BRANCH_SPIDER:
        flv.wall  = TILE_WALL_SPIDER;
        flv.floor = TILE_FLOOR_SPIDER;
        return;

    case BRANCH_ZOT:
        flv.wall  = TILE_WALL_ZOT_YELLOW;
        flv.floor = TILE_FLOOR_TOMB;
        return;

#if TAG_MAJOR_VERSION == 34
    case BRANCH_FOREST:
        flv.wall  = TILE_WALL_LAIR;
        flv.floor = TILE_FLOOR_GRASS;
        return;
#endif
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
        flv.floor = tile_dngn_coloured(TILE_FLOOR_DEMONIC, env.floor_colour);
        if (env.rock_colour == LIGHTRED)
            flv.wall = TILE_WALL_FLESH;
        else
            flv.wall = tile_dngn_coloured(TILE_WALL_BARS, env.rock_colour);
        break;

    case BRANCH_ZIGGURAT:
    case BRANCH_BAZAAR:
    case BRANCH_TROVE:
        flv.wall  = TILE_WALL_VAULT;
        flv.floor = TILE_FLOOR_VAULT;
        return;

    case BRANCH_NECROPOLIS:
        flv.wall  = TILE_WALL_CATACOMBS;
        flv.floor = TILE_FLOOR_NECROPOLIS_SQUARES;
        return;

#if TAG_MAJOR_VERSION == 34
    case BRANCH_LABYRINTH:
#endif
    case BRANCH_GAUNTLET:
        flv.wall  = TILE_WALL_LAB_ROCK;
        flv.floor = TILE_FLOOR_GAUNTLET;
        return;

    case BRANCH_SEWER:
        flv.wall  = TILE_WALL_OOZING;
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
        flv.wall  = TILE_WALL_ICE_BLOCK;
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

    case BRANCH_DESOLATION:
        flv.floor = TILE_FLOOR_SALT;
        flv.wall = TILE_WALL_DESOLATION;
        return;

    case BRANCH_ARENA:
        flv.wall  = TILE_WALL_NORMAL;
        flv.floor = TILE_FLOOR_NORMAL;
        return;

    case BRANCH_CRUCIBLE:
        flv.wall  = TILE_WALL_NORMAL;
        flv.floor = TILE_FLOOR_NORMAL;
        return;

    case NUM_BRANCHES:
    case GLOBAL_BRANCH_INFO:
        break;
    }
}

void tile_clear_flavour(const coord_def &p)
{
    tile_env.flv(p).floor     = 0;
    tile_env.flv(p).wall      = 0;
    tile_env.flv(p).feat      = 0;
    tile_env.flv(p).floor_idx = 0;
    tile_env.flv(p).wall_idx  = 0;
    tile_env.flv(p).feat_idx  = 0;
    tile_env.flv(p).special   = 0;
}

void tile_clear_flavour()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        tile_clear_flavour(*ri);
}

static bool _level_uses_dominoes()
{
    return you.where_are_you == BRANCH_CRYPT;
}

// For floors and walls that have not already been set to a particular tile,
// set them to a random instance of the default floor and wall tileset.
void tile_init_flavour()
{
    if (_level_uses_dominoes())
    {
        vector<unsigned int> output;

        {
            rng::subgenerator sub_rng(
                static_cast<uint64_t>(you.where_are_you ^ you.game_seed),
                static_cast<uint64_t>(you.depth));
            output.reserve(X_WIDTH * Y_WIDTH);
            domino::DominoSet<domino::EdgeDomino> dominoes(domino::cohen_set, 8);
            // TODO: don't pass a PcgRNG object
            dominoes.Generate(X_WIDTH, Y_WIDTH, output,
                                                    rng::current_generator());
        }

        for (rectangle_iterator ri(0); ri; ++ri)
            tile_init_flavour(*ri, output[ri->x + ri->y * GXM]);
    }
    else
        for (rectangle_iterator ri(0); ri; ++ri)
            tile_init_flavour(*ri, 0);
}

// 11111333333   55555555
//   222222444444   6666666666
static void _get_dungeon_wall_tiles_by_depth(int depth,
                                             vector<pair<tileidx_t, int>>& t)
{
    if (crawl_state.game_is_sprint() || crawl_state.game_is_arena())
    {
        t.emplace_back(TILE_WALL_CATACOMBS, 1);
        return;
    }
    if (depth <= 5)
        t.emplace_back(TILE_WALL_BRICK_DARK_1, 460);
    if (depth > 2 && depth <= 8)
    {
        t.emplace_back(TILE_WALL_BRICK_DARK_2, 440);
        t.emplace_back(TILE_WALL_BRICK_DARK_2_TORCH, 20);
    }
    if (depth > 5 && depth <= 11)
        t.emplace_back(TILE_WALL_BRICK_DARK_3, 464);
    int torch_4_weight = 0;
    if (depth > 8)
    {
        t.emplace_back(TILE_WALL_BRICK_DARK_4, 452);
        torch_4_weight += 40;
    }
    // Torches are more common on D:$
    if (depth == brdepth[BRANCH_DUNGEON])
        torch_4_weight += 40;

    if (torch_4_weight)
        t.emplace_back(TILE_WALL_BRICK_DARK_4_TORCH, torch_4_weight);
}

static void _get_depths_wall_tiles_by_depth(int depth,
                                            vector<pair<tileidx_t, int>>& t)
{
    if (depth <= 3)
        t.emplace_back(TILE_WALL_BRICK_DARK_5, 476);
    if (depth > 3)
        t.emplace_back(TILE_WALL_BRICK_DARK_6, 464);

    int torch_weight = 60;
    // Torches are more common on Depths:$
    if (depth == brdepth[BRANCH_DEPTHS])
        torch_weight += 60;
    t.emplace_back(TILE_WALL_BRICK_DARK_6_TORCH, torch_weight);
}

static int _find_variants(tileidx_t idx, int variant, vector<int> &out)
{
    const int count = tile_dngn_count(idx);
    out.reserve(count);
    if (count == 1)
    {
        out.push_back(1);
        return 1;
    }

    int total = 0;
    int curr_prob = 0;
    for (int i = 0; i < count; ++i)
    {
        int last_prob = curr_prob;
        curr_prob = tile_dngn_probs(idx + i);
        if (tile_dngn_dominoes(idx + i) == variant)
        {
            int weight = curr_prob - last_prob;
            total += weight;
            out.push_back(weight);
        }
        else
            out.push_back(0);
    }
    if (!total)
    {
        out.clear();
        out.push_back(tile_dngn_probs(idx));
        for (int i = 1; i < count; ++i)
            out.push_back(tile_dngn_probs(idx + i) - tile_dngn_probs(idx + i - 1));
        return tile_dngn_probs(idx + count - 1);
    }
    return total;
}

tileidx_t pick_dngn_tile(tileidx_t idx, int value, int domino)
{
    ASSERT_LESS(idx, TILE_DNGN_MAX);
    static vector<int> weights;
    weights.clear();

    int total = _find_variants(idx, domino, weights);
    if (weights.size() == 1)
        return idx;
    int rand = value % total;

    for (size_t i = 0; i < weights.size(); ++i)
    {
        rand -= weights[i];
        if (rand < 0)
            return idx + i;
    }

    return idx;
}

static tileidx_t _pick_dngn_tile_multi(
                                const vector<pair<tileidx_t, int>>& candidates,
                                int rand)
{
    int total = 0;
    for (const pair<tileidx_t, int>& candidate : candidates)
        total += candidate.second;

    int rand1 = rand % total;
    int rand2 = rand / total;

    for (const pair<tileidx_t, int>& candidate : candidates)
    {
        if (rand1 < candidate.second)
        {
            // XXX: this should be for any animated tile
            if (is_torch_tile(candidate.first))
                return candidate.first;
            return pick_dngn_tile(candidate.first, rand2, -1);
        }
        rand1 -= candidate.second;
    }

    // Should never reach this place
    die("couldn't find tile");
}

static bool _same_door_at(dungeon_feature_type feat, const coord_def &gc)
{
    const dungeon_feature_type door = env.grid(gc);

    return door == feat
#if TAG_MAJOR_VERSION == 34
        || map_masked(gc, MMT_WAS_DOOR_MIMIC)
#endif
        || feat_is_closed_door(door)
           && feat_is_opaque(feat) == feat_is_opaque(door)
           && (feat_is_sealed(feat) || feat_is_sealed(door));
}

void tile_init_flavour(const coord_def &gc, const int domino)
{
    if (!map_bounds(gc))
        return;

    uint32_t seed = you.birth_time + you.where_are_you +
        (you.depth << 8) + (gc.x << 16) + (gc.y << 24);

    int rand1 = hash_with_seed(INT_MAX, seed, 0);
    int rand2 = hash_with_seed(INT_MAX, seed, 1);

    if (!tile_env.flv(gc).floor)
    {
        tileidx_t floor_base = tile_env.default_flavour.floor;
        int colour = env.grid_colours(gc);
        if (colour)
            floor_base = tile_dngn_coloured(floor_base, colour);
        tile_env.flv(gc).floor = pick_dngn_tile(floor_base, rand1, domino);
    }
    else if (tile_env.flv(gc).floor != TILE_HALO_GRASS
             && tile_env.flv(gc).floor != TILE_HALO_GRASS2
             && tile_env.flv(gc).floor != TILE_HALO_VAULT
             && tile_env.flv(gc).floor != TILE_HALO_DIRT)
    {
        tile_env.flv(gc).floor = pick_dngn_tile(tile_env.flv(gc).floor, rand1);
    }

    if (!tile_env.flv(gc).wall)
    {
        if ((player_in_branch(BRANCH_DUNGEON) || player_in_branch(BRANCH_DEPTHS))
            && tile_env.default_flavour.wall == TILE_WALL_NORMAL)
        {
            vector<pair<tileidx_t, int>> tile_candidates;
            if (player_in_branch(BRANCH_DEPTHS))
                _get_depths_wall_tiles_by_depth(you.depth, tile_candidates);
            else
                _get_dungeon_wall_tiles_by_depth(you.depth, tile_candidates);
            tile_env.flv(gc).wall = _pick_dngn_tile_multi(tile_candidates, rand2);
        }
        else
        {
            tileidx_t wall_base = tile_env.default_flavour.wall;
            int colour = env.grid_colours(gc);
            if (colour)
                wall_base = tile_dngn_coloured(wall_base, colour);
            tile_env.flv(gc).wall = pick_dngn_tile(wall_base, rand2);
        }
    }
    else
        tile_env.flv(gc).wall = pick_dngn_tile(tile_env.flv(gc).wall, rand2);

    if (feat_is_stone_stair(env.grid(gc)))
    {
        const bool up = feat_stair_direction(env.grid(gc)) == CMD_GO_UPSTAIRS;
        if (player_in_branch(BRANCH_SHOALS))
        {
            tile_env.flv(gc).feat = up ? TILE_DNGN_SHOALS_STAIRS_UP
                                       : TILE_DNGN_SHOALS_STAIRS_DOWN;
        }
        else if (player_in_branch(BRANCH_VAULTS))
        {
            if (you.depth == branches[BRANCH_VAULTS].numlevels - 1 && !up)
                tile_env.flv(gc).feat = TILE_DNGN_METAL_STAIRS_DOWN;
            else if (you.depth == branches[BRANCH_VAULTS].numlevels && up)
                tile_env.flv(gc).feat = TILE_DNGN_METAL_STAIRS_UP;
        }
        else if (player_in_branch(BRANCH_ZOT))
        {
            if (you.depth == branches[BRANCH_VAULTS].numlevels - 1 && !up)
                tile_env.flv(gc).feat = TILE_DNGN_ZOT_STAIRS_DOWN;
            else if (you.depth == branches[BRANCH_VAULTS].numlevels && up)
                tile_env.flv(gc).feat = TILE_DNGN_ZOT_STAIRS_UP;
        }
        else if (player_in_branch(BRANCH_SLIME) && !you.royal_jelly_dead)
        {
            if (up)
                tile_env.flv(gc).feat = TILE_DNGN_SLIMY_STAIRS_UP;
            else
                tile_env.flv(gc).feat = TILE_DNGN_SLIMY_STAIRS_DOWN;
        }
    }

    if (feat_is_escape_hatch(env.grid(gc)) && player_in_branch(BRANCH_TOMB))
    {
        const bool up = feat_stair_direction(env.grid(gc)) == CMD_GO_UPSTAIRS;
        tile_env.flv(gc).feat = up ? TILE_DNGN_ONE_WAY_STAIRS_UP
                                   : TILE_DNGN_ONE_WAY_STAIRS_DOWN;
    }

    if (feat_is_door(env.grid(gc)))
    {
        // Check for gates.
        bool door_left  = _same_door_at(env.grid(gc), coord_def(gc.x - 1, gc.y));
        bool door_right = _same_door_at(env.grid(gc), coord_def(gc.x + 1, gc.y));
        bool door_up    = _same_door_at(env.grid(gc), coord_def(gc.x, gc.y - 1));
        bool door_down  = _same_door_at(env.grid(gc), coord_def(gc.x, gc.y + 1));

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
            tile_env.flv(gc).special = target - TILE_DNGN_CLOSED_DOOR;
        }
        else
            tile_env.flv(gc).special = 0;
    }
    else if (!tile_env.flv(gc).special)
        tile_env.flv(gc).special = hash_with_seed(256, seed, 10);
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
        if (env.grid(*ai) == target)
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
            if (!feat_has_dry_floor(env.grid[x][y]))
                continue;
            if (!_adjacent_target(target, x, y))
                continue;

            bool l_flr = (x > 0 && feat_has_dry_floor(env.grid[x-1][y]));
            bool r_flr = (x < GXM - 1 && feat_has_dry_floor(env.grid[x+1][y]));
            bool u_flr = (y > 0 && feat_has_dry_floor(env.grid[x][y-1]));
            bool d_flr = (y < GYM - 1 && feat_has_dry_floor(env.grid[x][y+1]));

            bool l_target = _adjacent_target(target, x-1, y);
            bool r_target = _adjacent_target(target, x+1, y);
            bool u_target = _adjacent_target(target, x, y-1);
            bool d_target = _adjacent_target(target, x, y+1);

            // The special tiles contains part floor and part special, so
            // if there are adjacent floor or special tiles, we should
            // do our best to "connect" them appropriately. If there are
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
                tile_env.flv[x][y].floor = tile + SPECIAL_FULL;
            }
            else if (l_nrm)
            {
                if (u_nrm)
                    tile_env.flv[x][y].floor = tile + SPECIAL_NW;
                else if (d_nrm)
                    tile_env.flv[x][y].floor = tile + SPECIAL_SW;
                else if (u_spc && d_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_W;
                else if (u_spc && r_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_SW;
                else if (d_spc && r_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_NW;
                else if (u_spc)
                {
                    tile_env.flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_W : SPECIAL_SW);
                }
                else if (d_spc)
                {
                    tile_env.flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_W : SPECIAL_NW);
                }
                else
                    tile_env.flv[x][y].floor = tile + _jitter(SPECIAL_W);
            }
            else if (r_nrm)
            {
                if (u_nrm)
                    tile_env.flv[x][y].floor = tile + SPECIAL_NE;
                else if (d_nrm)
                    tile_env.flv[x][y].floor = tile + SPECIAL_SE;
                else if (u_spc && d_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_E;
                else if (u_spc && l_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_SE;
                else if (d_spc && l_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_NE;
                else if (u_spc)
                    tile_env.flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_E : SPECIAL_SE);
                else if (d_spc)
                    tile_env.flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_E : SPECIAL_NE);
                else
                    tile_env.flv[x][y].floor = tile + _jitter(SPECIAL_E);
            }
            else if (u_nrm)
            {
                if (r_spc && l_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_N;
                else if (r_spc && d_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_NW;
                else if (l_spc && d_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_NE;
                else if (r_spc)
                {
                    tile_env.flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_N : SPECIAL_NW);
                }
                else if (l_spc)
                {
                    tile_env.flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_N : SPECIAL_NE);
                }
                else
                    tile_env.flv[x][y].floor = tile + _jitter(SPECIAL_N);
            }
            else if (d_nrm)
            {
                if (r_spc && l_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_S;
                else if (r_spc && u_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_SW;
                else if (l_spc && u_spc)
                    tile_env.flv[x][y].floor = tile + SPECIAL_SE;
                else if (r_spc)
                {
                    tile_env.flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_S : SPECIAL_SW);
                }
                else if (l_spc)
                {
                    tile_env.flv[x][y].floor = tile + (coinflip() ?
                        SPECIAL_S : SPECIAL_SE);
                }
                else
                    tile_env.flv[x][y].floor = tile + _jitter(SPECIAL_S);
            }
            else if (u_spc && d_spc)
            {
                // We know this value is already initialised and
                // is necessarily in bounds.
                tileidx_t t = tile_env.flv[x][y-1].floor - tile;
                if (t == SPECIAL_NE || t == SPECIAL_E)
                    tile_env.flv[x][y].floor = tile + SPECIAL_E;
                else if (t == SPECIAL_NW || t == SPECIAL_W)
                    tile_env.flv[x][y].floor = tile + SPECIAL_W;
                else
                    tile_env.flv[x][y].floor = tile + SPECIAL_FULL;
            }
            else if (r_spc && l_spc)
            {
                // We know this value is already initialised and
                // is necessarily in bounds.
                tileidx_t t = tile_env.flv[x-1][y].floor - tile;
                if (t == SPECIAL_NW || t == SPECIAL_N)
                    tile_env.flv[x][y].floor = tile + SPECIAL_N;
                else if (t == SPECIAL_SW || t == SPECIAL_S)
                    tile_env.flv[x][y].floor = tile + SPECIAL_S;
                else
                    tile_env.flv[x][y].floor = tile + SPECIAL_FULL;
            }
            else if (u_spc && l_spc)
                tile_env.flv[x][y].floor = tile + SPECIAL_SE;
            else if (u_spc && r_spc)
                tile_env.flv[x][y].floor = tile + SPECIAL_SW;
            else if (d_spc && l_spc)
                tile_env.flv[x][y].floor = tile + SPECIAL_NE;
            else if (d_spc && r_spc)
                tile_env.flv[x][y].floor = tile + SPECIAL_NW;
            else
                tile_env.flv[x][y].floor = tile + SPECIAL_FULL;
        }
    }

    // Second pass for clean up. The only bad part about the above
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
    // of a south tile. What we really want to do is to separate the
    // two regions, by making 1 a SPECIAL_SE and 2 a SPECIAL_NW tile.
    for (int y = 0; y < GYM - 1; ++y)
        for (int x = 0; x < GXM - 1; ++x)
        {
            int this_spc = tile_env.flv[x][y].floor - tile;
            if (this_spc < 0 || this_spc > 8)
                continue;

            if (this_spc != SPECIAL_N && this_spc != SPECIAL_S
                && this_spc != SPECIAL_E && this_spc != SPECIAL_W)
            {
                continue;
            }

            // TODO: these conditions are guaranteed?
            int right_spc = x < GXM - 1 ? tile_env.flv[x+1][y].floor - tile
                                        : int{SPECIAL_FULL};
            int down_spc  = y < GYM - 1 ? tile_env.flv[x][y+1].floor - tile
                                        : int{SPECIAL_FULL};

            if (this_spc == SPECIAL_N && right_spc == SPECIAL_S)
            {
                tile_env.flv[x][y].floor = tile + SPECIAL_NE;
                tile_env.flv[x+1][y].floor = tile + SPECIAL_SW;
            }
            else if (this_spc == SPECIAL_S && right_spc == SPECIAL_N)
            {
                tile_env.flv[x][y].floor = tile + SPECIAL_SE;
                tile_env.flv[x+1][y].floor = tile + SPECIAL_NW;
            }
            else if (this_spc == SPECIAL_E && down_spc == SPECIAL_W)
            {
                tile_env.flv[x][y].floor = tile + SPECIAL_SE;
                tile_env.flv[x][y+1].floor = tile + SPECIAL_NW;
            }
            else if (this_spc == SPECIAL_W && down_spc == SPECIAL_E)
            {
                tile_env.flv[x][y].floor = tile + SPECIAL_NE;
                tile_env.flv[x][y+1].floor = tile + SPECIAL_SW;
            }
        }
}

#ifdef USE_TILE

void tile_draw_map_cells()
{
    if (crawl_state.game_is_arena())
        for (rectangle_iterator ri(crawl_view.vgrdc, LOS_MAX_RANGE); ri; ++ri)
        {
            tile_draw_map_cell(*ri, true);
#ifdef USE_TILE_WEB
            tiles.mark_for_redraw(*ri);
#endif
        }
    else
        for (radius_iterator ri(you.pos(), LOS_NONE); ri; ++ri)
        {
            tile_draw_map_cell(*ri, true);
#ifdef USE_TILE_WEB
            tiles.mark_for_redraw(*ri);
#endif
        }
}

static tileidx_t _get_floor_bg(const coord_def& gc)
{
    tileidx_t bg = TILE_DNGN_UNSEEN | tileidx_unseen_flag(gc);

    if (map_bounds(gc))
    {
        bg = tileidx_feature_for_cache(gc);

        if (is_unknown_stair(gc)
            && env.map_knowledge(gc).feat() != DNGN_ENTER_ZOT
            && !feat_is_hell_subbranch_exit(env.map_knowledge(gc).feat()))
        {
            bg |= TILE_FLAG_NEW_STAIR;
        }
        else if (is_unknown_transporter(gc))
            bg |= TILE_FLAG_NEW_TRANSPORTER;
    }

    return bg;
}

void tile_draw_floor()
{
    tile_env.icons.clear();

    for (int cy = 0; cy < ENV_SHOW_DIAMETER; cy++)
        for (int cx = 0; cx < ENV_SHOW_DIAMETER; cx++)
        {
            const coord_def ep(cx, cy);
            const coord_def gc = show2grid(ep);

            if (!you.see_cell(gc))
                continue;

            tileidx_t bg = _get_floor_bg(gc);

            // init tiles
            tile_env.bk_bg(gc) = bg;
            tile_env.bk_fg(gc) = 0;
            tile_env.bk_cloud(gc) = 0;
        }
}

void tile_forget_map(const coord_def &gc)
{
    tile_env.bk_fg(gc) = 0;
    tile_env.bk_bg(gc) = 0;
    tile_env.bk_cloud(gc) = 0;
    // This may have changed the explore horizon, so update adjacent minimap
    // squares as well.
    for (adjacent_iterator ai(gc, false); ai; ++ai)
        tiles.update_minimap(*ai);
}

static void _tile_place_item(const coord_def &gc, const item_def &item,
                             bool more_items)
{
    tileidx_t t = tileidx_item(item);
    if (more_items)
        t |= TILE_FLAG_S_UNDER;

    tile_env.bk_fg(gc) = t;

    if (item_needs_autopickup(item))
        tile_env.bk_bg(gc) |= TILE_FLAG_CURSOR3;
}

static void _tile_place_item_marker(const coord_def &gc, const item_def &item)
{
    tile_env.bk_fg(gc) = ((tileidx_t)tile_env.bk_fg(gc)) | TILE_FLAG_S_UNDER;

    if (item_needs_autopickup(item))
        tile_env.bk_bg(gc) |= TILE_FLAG_CURSOR3;
}

/**
 * Place the tile for an unseen monster's disturbance.
 *
 * @param gc    The disturbance's map position.
**/
static void _tile_place_invisible_monster(const coord_def &gc)
{
    const map_cell& cell = env.map_knowledge(gc);

    // Shallow water has its own modified tile for disturbances
    // see tileidx_feature
    // That tile is hidden by clouds though
    if (cell.feat() != DNGN_SHALLOW_WATER || cell.cloud() != CLOUD_NONE)
        tile_env.bk_fg(gc) = TILE_UNSEEN_MONSTER;
    else
        tile_env.bk_fg(gc) = 0;

    if (env.map_knowledge(gc).item())
        _tile_place_item_marker(gc, *env.map_knowledge(gc).item());
}

static void _tile_place_monster(const coord_def &gc, const monster_info& mon)
{
    tileidx_t t    = tileidx_monster(mon);
    tileidx_t t0   = t & TILE_FLAG_MASK;
    tileidx_t flag = t & (~TILE_FLAG_MASK);

    if (mons_class_is_stationary(mon.type)
        && mon.type != MONS_TRAINING_DUMMY)
    {
        // If necessary add item brand.
        if (env.map_knowledge(gc).item())
        {
            t |= TILE_FLAG_S_UNDER;

            if (item_needs_autopickup(*env.map_knowledge(gc).item()))
                tile_env.bk_bg(gc) |= TILE_FLAG_CURSOR3;
        }
    }
    else
    {
        tileidx_t mcache_idx = mcache.register_monster(mon);
        t = flag | (mcache_idx ? mcache_idx : t0);
    }

    tile_env.bk_fg(gc) = t;
    if (!you.see_cell(gc))
        return;
    set<tileidx_t> status_icons = status_icons_for(mon);
    if (!status_icons.empty())
        tile_env.icons[gc] = std::move(status_icons);

    // Add name tags.
    if (!mons_class_gives_xp(mon.type))
        return;

    const tag_pref pref =
        Options.tile_tag_pref == TAGPREF_AUTO
            ? ((crawl_state.game_is_tutorial() || crawl_state.game_is_hints())
                    ? TAGPREF_TUTORIAL
                    : crawl_state.game_is_arena()
                    ? TAGPREF_NAMED
                    : TAGPREF_ENEMY)
            : Options.tile_tag_pref;
    if (pref == TAGPREF_NONE)
        return;
    else if (pref == TAGPREF_TUTORIAL)
    {
        const int kills = you.kills.num_kills(mon);
        const int limit  = 0;

        if (!mon.is_named() && kills > limit)
            return;
    }
    else if (pref != TAGPREF_ALL && !mon.is_named())
        return;

    if (pref != TAGPREF_NAMED && pref != TAGPREF_ALL && mon.attitude == ATT_FRIENDLY)
        return;

    tiles.add_text_tag(TAG_NAMED_MONSTER, mon);
}

void tile_reset_fg(const coord_def &gc)
{
    // remove autopickup cursor, it will be added back if necessary
    tile_env.bk_bg(gc) &= ~TILE_FLAG_CURSOR3;
    tile_draw_map_cell(gc, true);
    tiles.update_minimap(gc);
}

static void _tile_place_cloud(const coord_def &gc, const cloud_info &cl)
{
    tile_env.bk_cloud(gc) = tileidx_cloud(cl);
}

void tile_draw_map_cell(const coord_def& gc, bool foreground_only)
{
    if (!foreground_only)
        tile_env.bk_bg(gc) = _get_floor_bg(gc);

    if (you.see_cell(gc))
        tile_env.icons.erase(gc);

    const map_cell& cell = env.map_knowledge(gc);

    tile_env.bk_fg(gc) = 0;
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

    // Always place clouds now they have their own layer
    if (cell.cloud() != CLOUD_NONE)
        _tile_place_cloud(gc, *cell.cloudinfo());
    else
        tile_env.bk_cloud(gc) = 0;
}

#ifndef USE_TILE_WEB
static bool _tile_has_cycling_misc_animation(tileidx_t tile)
{
    if (!Options.tile_misc_anim)
        return false;
    // Wizlab entries, conduits, and harlequin traps both have spinning
    // sequential cycle tile animations. The Jiyva altar, meanwhile, drips.
    return tile == TILE_DNGN_PORTAL_WIZARD_LAB
           || tile == TILE_DNGN_EXIT_NECROPOLIS
           || tile == TILE_DNGN_ALTAR_JIYVA
           || tile == TILE_DNGN_TRAP_HARLEQUIN
           || tile >= TILE_ARCANE_CONDUIT && tile < TILE_DNGN_SARCOPHAGUS_SEALED
           || is_torch_tile(tile);
}

static bool _tile_has_random_misc_animation(tileidx_t tile)
{
    if (!Options.tile_misc_anim)
        return false;
    // This includes branch / portal entries and exits, altars, runelights, and
    // fountains in the first range, and some randomly-animated weighted
    // vault statues in the second statues.
    return tile >= TILE_DNGN_ENTER_ZOT_CLOSED && tile < TILE_DNGN_CACHE_OF_FRUIT
           || tile >= TILE_DNGN_SILVER_STATUE && tile < TILE_ARCANE_CONDUIT
           || tile >= TILE_WALL_STONE_CRACKLE_1 && tile <= TILE_WALL_STONE_CRACKLE_4;
}
#endif

// Updates the "flavour" of tiles that are animated.
// Unfortunately, these are all hard-coded for now.
void tile_apply_animations(tileidx_t bg, tile_flavour *flv)
{
#ifndef USE_TILE_WEB
    tileidx_t bg_idx = bg & TILE_FLAG_MASK;

    if (_tile_has_cycling_misc_animation(bg_idx))
        flv->special = (flv->special + 1) % tile_dngn_count(bg_idx);
    else if (bg_idx == TILE_DNGN_LAVA && Options.tile_water_anim)
    {
        // Lava tiles are four sets of four tiles (the second and fourth
        // sets are the same). This cycles between the four sets, picking
        // a random element from each set.
        flv->special = ((flv->special - ((flv->special % 4)))
                        + 4 + random2(4)) % tile_dngn_count(bg_idx);
    }
    else if (bg_idx > TILE_DNGN_LAVA && bg_idx < TILE_FLOOR_MAX
             && Options.tile_water_anim)
    {
        flv->special = random2(256);
    }
    else if (_tile_has_random_misc_animation(bg_idx))
        flv->special = random2(256);
#else
    UNUSED(bg, flv);
#endif
}

static bool _suppress_blood(tileidx_t bg_idx)
{
    tileidx_t basetile = tile_dngn_basetile(bg_idx);
    return is_torch_tile(basetile);
}

// If the top tile is a corpse, don't draw blood underneath.
static bool _top_item_is_corpse(const map_cell& mc)
{
    const item_def* item = mc.item();
    return item && item->is_type(OBJ_CORPSES, CORPSE_BODY);
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

static unsigned int _pick_floor_tile(tileidx_t base_tile, coord_def gc)
{
    unsigned int count = tile_dngn_count(base_tile);
    uint32_t seed = you.where_are_you + (you.depth << 8)
                    + (gc.x << 16) + (gc.y << 24);
    unsigned int offset = hash_with_seed(count, seed, you.birth_time);
    return base_tile + offset;
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

    apply_variations(tile_env.flv(gc), &cell.bg, gc);

    const map_cell& mc = env.map_knowledge(gc);

    bool print_blood = true;
    if (mc.flags & MAP_HALOED)
    {
        if (mc.flags & MAP_UMBRAED)
            cell.halo = HALO_NONE;
        else
            cell.halo = HALO_RANGE;
    }
    else if (mc.flags & MAP_UMBRAED)
    {
        int num = HALO_UMBRA_LAST - HALO_UMBRA_FIRST + 1;
        int variety = hash_with_seed(num, gc.y * GXM + gc.x, you.frame_no);
        cell.halo = (halo_type)(HALO_UMBRA_FIRST + variety);
    }
    else
        cell.halo = HALO_NONE;

    if (mc.flags & MAP_LIQUEFIED)
        cell.is_liquefied = true;
    else if (print_blood && (feat_suppress_blood(mc.feat())
                             || _suppress_blood((cell.bg) & TILE_FLAG_MASK)))
    {
        print_blood = false;
    }

    if (print_blood)
    {
        // Corpses have a blood puddle of their own.
        if (mc.flags & MAP_BLOODY && !_top_item_is_corpse(mc))
        {
            cell.is_bloody = true;
            cell.blood_rotation = mc.blood_rotation();
            cell.old_blood = bool(mc.flags & MAP_OLD_BLOOD);
        }
    }

    const dungeon_feature_type feat = mc.feat();
    if (feat_is_water(feat) || feat == DNGN_LAVA)
        cell.bg |= TILE_FLAG_WATER;

    if ((mc.flags & MAP_SANCTUARY_1) || (mc.flags & MAP_SANCTUARY_2))
        cell.is_sanctuary = true;

    if (mc.flags & MAP_BLASPHEMY)
        cell.is_blasphemy = true;

    if (mc.flags & MAP_SILENCED)
        cell.is_silenced = true;

    if (feat == DNGN_MANGROVE)
        cell.mangrove_water = true;
    cell.awakened_forest = feat_is_tree(feat) && env.forest_awoken_until;

    if (mc.flags & MAP_ORB_HALOED)
        cell.orb_glow = get_orb_phase(gc) ? 2 : 1;

    if (mc.flags & MAP_QUAD_HALOED)
        cell.quad_glow = true;

    if (mc.flags & MAP_DISJUNCT)
        cell.disjunct = get_disjunct_phase(gc);

    if (mc.flags & MAP_BFB_CORPSE)
        cell.has_bfb_corpse = true;

    if (you.on_current_level && you.rampage_hints.count(gc) > 0)
        cell.bg |= TILE_FLAG_RAMPAGE;

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

    cell.flv = tile_env.flv(gc);

    if (mc.flags & MAP_CORRODING && !feat_is_wall(feat))
        cell.flv.floor = _pick_floor_tile(TILE_FLOOR_SLIME_ACIDIC, gc);
    else if (mc.flags & MAP_ICY)
        cell.flv.floor = _pick_floor_tile(TILE_FLOOR_ICY, gc);
    else if ((env.pgrid(gc) & FPROP_SEISMOROCK) && you.see_cell(gc)
             && feat_has_dry_floor(env.grid(gc)))
    {
        // Use the id of the underlying tile to randomize the rock appearance.
        // XXX: This doesn't look great when the underlying tile is animated.
        tileidx_t tile = TILE_FLOOR_SEISMOROCK
                            + cell.bg % tile_dngn_count(TILE_FLOOR_SEISMOROCK);

        cell.add_overlay(tile);
    }

    if (you.did_east_wind && grid_distance(you.pos(), gc) <= 2 && !cell_is_solid(gc))
        cell.add_overlay(TILE_OVERLAY_EAST_WIND);
}
#endif
