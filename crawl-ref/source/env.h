#ifndef ENV_H
#define ENV_H

#include "map_knowledge.h"
#include "monster.h"
#include "show.h"
#include "trap_def.h"
#include <set>

typedef FixedArray<short, GXM, GYM> grid_heightmap;
typedef uint32_t terrain_property_t;

typedef std::set<std::string> string_set;

struct vault_placement;
typedef std::vector<vault_placement*> vault_placement_refv;

struct crawl_environment
{
    uint8_t rock_colour;
    uint8_t floor_colour;

    FixedVector< item_def, MAX_ITEMS >       item;  // item list
    FixedVector< monsters, MAX_MONSTERS >    mons;  // monster list

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

    std::string                              level_build_method;
    std::string                              level_layout_type;

    vault_placement_refv                     level_vaults;

    std::auto_ptr<grid_heightmap>            heightmap;

    // Player-remembered terrain and LOS
    FixedArray< map_cell, GXM, GYM >         map_knowledge;
    std::set<coord_def> visible;

#ifdef USE_TILE
    // indexed by grid coords
    FixedArray<tile_fg_store, GXM, GYM> tile_bk_fg;
    FixedArray<tileidx_t, GXM, GYM> tile_bk_bg;
    FixedArray<tile_flavour, GXM, GYM> tile_flv;
    // indexed by (show-1) coords
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tile_fg;
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tile_bg;
    tile_flavour tile_default;
#endif

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
    int sanctuary_time;
    int forest_awoken_until;
};

#ifdef DEBUG_GLOBALS
#define env (*real_env)
#endif
extern struct crawl_environment env;

#endif
