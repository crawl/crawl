#ifndef ENV_H
#define ENV_H

#include "map_knowledge.h"
#include "monster.h"
#include "trap_def.h"
#include <set>

typedef FixedArray<short, GXM, GYM> grid_heightmap;
typedef uint32_t terrain_property_t;

typedef set<string> string_set;

struct vault_placement;
typedef vector<vault_placement*> vault_placement_refv;

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
    FixedArray< unsigned short, GXM, GYM >   cgrid; // cloud grid
    FixedArray< unsigned short, GXM, GYM >   grid_colours; // colour overrides
    FixedArray< unsigned short, GXM, GYM >   tgrid; // traps, shops

    map_mask                                 level_map_mask;
    map_mask                                 level_map_ids;

    string_set                               level_uniq_maps;
    string_set                               level_uniq_map_tags;
    string_set                               level_layout_types;

    string                                   level_build_method;

    vault_placement_refv                     level_vaults;

    Unique_ptr<grid_heightmap>               heightmap;

    map_bitmask                              map_seen;
    // Player-remembered terrain and LOS
    MapKnowledge                             map_knowledge;
    // Forgotten map knowledge (X^F)
    Unique_ptr<MapKnowledge>                 map_forgotten;
    set<coord_def> visible;

    vector<coord_def>                        travel_trail;

    // indexed by grid coords
#ifdef USE_TILE
    FixedArray<tile_fg_store, GXM, GYM> tile_bk_fg;
    FixedArray<tileidx_t, GXM, GYM> tile_bk_bg;
    FixedArray<tileidx_t, GXM, GYM> tile_bk_cloud;
#endif
    FixedArray<tile_flavour, GXM, GYM> tile_flv;
    // indexed by (show-1) coords
#ifdef USE_TILE
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tile_fg;
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tile_bg;
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tile_cloud;
#endif
    tile_flavour tile_default;
    vector<string> tile_names;

    FixedVector< cloud_struct, MAX_CLOUDS >  cloud; // cloud list
    short cloud_no;

    FixedVector< shop_struct, MAX_SHOPS >    shop;  // shop list
    FixedVector< trap_def, MAX_TRAPS >       trap;  // trap list

    FixedVector< monster_type, MAX_MONS_ALLOC > mons_alloc;
    map_markers                              markers;

    // Place to associate arbitrary data with a particular level.
    // Sort of like player::attribute
    CrawlHashTable properties;

    // Rate at which random monsters spawn, with lower numbers making
    // them spawn more often (5 or less causes one to spawn about every
    // 5 turns).  Set to 0 to stop random generation.
    int spawn_random_rate;

    // Time when level was saved (hence we write out you.elapsed_time
    // (but load it back to env.elapsed_time); used during level load
    int elapsed_time;

    // Which point did the player leave the level from?
    coord_def old_player_pos;

    // Number of turns the player has spent on this level.
    int turns_on_level;

    // Flags for things like preventing teleport control; see
    // level_flag_type in enum.h
    uint32_t level_flags;

    // Index into the delayed actions array.
    unsigned int dactions_done;

    coord_def sanctuary_pos;
    coord_def orb_pos;
    int sanctuary_time;
    int forest_awoken_until;
    int density;
    int absdepth0;
    vector<pair<coord_def, int> > sunlight;

    //------------------------------------------------------------------------

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

#endif
