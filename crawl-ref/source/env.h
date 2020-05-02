#pragma once

#include <set>
#include <memory> // unique_ptr

#include "cloud.h"
#include "coord.h"
#include "fprop.h"
#include "map-cell.h"
#include "mapmark.h"
#include "monster.h"
#include "shopping.h"
#include "trap-def.h"

typedef FixedArray<short, GXM, GYM> grid_heightmap;

typedef set<string> string_set;

struct vault_placement;
typedef vector<unique_ptr<vault_placement>> vault_placement_refv;

typedef FixedArray< map_cell, GXM, GYM > MapKnowledge;

class final_effect;
struct crawl_environment
{
    colour_t rock_colour;
    colour_t floor_colour;

    FixedVector< item_def, MAX_ITEMS >       item;  // item list
    FixedVector< monster, MAX_MONSTERS+2 >   mons;  // monster list, plus anon

    feature_grid                             grid;  // terrain grid
    FixedArray<terrain_property_t, GXM, GYM> pgrid; // terrain properties
    FixedArray< unsigned short, GXM, GYM >   mgrid; // monster grid
    FixedArray< int, GXM, GYM >              igrid; // item grid
    FixedArray< unsigned short, GXM, GYM >   grid_colours; // colour overrides

    map_mask                                 level_map_mask;
    map_mask                                 level_map_ids;

    string_set                               level_uniq_maps;
    string_set                               level_uniq_map_tags;
    string_set                               level_layout_types;

    string                                   level_build_method;

    vault_placement_refv                     level_vaults;

    unique_ptr<grid_heightmap>               heightmap;

    map_bitmask                              map_seen;
    // Player-remembered terrain and LOS
    MapKnowledge                             map_knowledge;
    // Forgotten map knowledge (X^F)
    unique_ptr<MapKnowledge>                 map_forgotten;
    set<coord_def> visible;

    vector<coord_def>                        travel_trail;

    // indexed by grid coords
#ifdef USE_TILE // TODO: separate out this stuff from crawl_environment
    FixedArray<tile_fg_store, GXM, GYM> tile_bk_fg;
    FixedArray<tileidx_t, GXM, GYM> tile_bk_bg;
    FixedArray<tileidx_t, GXM, GYM> tile_bk_cloud;
#endif
    FixedArray<tile_flavour, GXM, GYM> tile_flv;
    // indexed by (show-1) coords
#ifdef USE_TILE // TODO: separate out this stuff from crawl_environment
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tile_fg;
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tile_bg;
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tile_cloud;
#endif
    tile_flavour tile_default;
    vector<string> tile_names;

    map<coord_def, cloud_struct> cloud;

    map<coord_def, shop_struct> shop; // shop list
    map<coord_def, trap_def> trap; // trap list

    FixedVector< monster_type, MAX_MONS_ALLOC > mons_alloc;
    map_markers                              markers;

    // Place to associate arbitrary data with a particular level.
    // Sort of like player::attribute
    CrawlHashTable properties;

    // Rate at which random monsters spawn, with lower numbers making
    // them spawn more often (5 or less causes one to spawn about every
    // 5 turns). Set to 0 to stop random generation.
    int spawn_random_rate;

    // Time when level was saved (hence we write out you.elapsed_time
    // (but load it back to env.elapsed_time); used during level load
    int elapsed_time;

    // Which point did the player leave the level from?
    coord_def old_player_pos;

    // Number of turns the player has spent on this level.
    int turns_on_level;

    // Index into the delayed actions array.
    unsigned int dactions_done;

    coord_def sanctuary_pos;
    coord_def orb_pos;
    int sanctuary_time;
    int forest_awoken_until;
    int density;
    int absdepth0;

    // Remaining fields not marshalled:

    // Volatile level flags, not saved.
    uint32_t level_state;

    // Mapping mid->mindex until the transition is finished.
    map<mid_t, unsigned short> mid_cache;

    // Things to happen when the current attack/etc finishes.
    vector<final_effect *> final_effects;

    // A stack that accumulates subvaults being placed. A failure may pop a
    // part of the stack before retrying.
    vector<string> new_subvault_names, new_subvault_tags;
    // A set of the unique subvaults being placed. These are considered used
    // for the purposes of placing additional subvaults.
    string_set new_used_subvault_names;
    // A set of uniq_ or luniq_ map tags being placed.
    string_set new_used_subvault_tags;

    // Vault currently being placed, for crash dump purposes.
    string placing_vault;
};

#ifdef DEBUG_GLOBALS
#define env (*real_env)
#endif
extern struct crawl_environment env;

/**
 * Range proxy to iterate over only "real" menv slots, skipping anon slots.
 *
 * Use as the range expression in a for loop:
 *     for (auto &mons : menv_real)
 */
static const struct menv_range_proxy
{
    menv_range_proxy() {}
    monster *begin() const { return &menv[0]; }
    monster *end()   const { return &menv[MAX_MONSTERS]; }
} menv_real;

/**
 * Look up a property of a coordinate in the player's map_knowledge grid.
 *
 * @tparam T The type of the property being queried.
 * @tparam F A callable type taking const map_cell& and returning T.
 *
 * @param default_value The value to return if pos is out of bounds.
 * @param pos The position to query.
 * @param f A function that will be passed a map_cell& representing what the
 *     player knows about the map at the given position. Will only be called
 *     if pos is in-bounds.
 *
 * @return Either the default value, or the result of f(env.map_knowledge(pos)).
 */
template<typename T, typename F>
T query_map_knowledge(T default_value, const coord_def& pos, F f)
{
    if (!map_bounds(pos))
        return default_value;
    return f(env.map_knowledge(pos));
}
