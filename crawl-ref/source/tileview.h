/*
 *  File:       tileview.h
 *  Summary:    Tile functions that modify the tiles or flavours in
                the crawl_environment that are shown to the player.
 */

#ifndef TILEDRAW_H
#define TILEDRAW_H

#ifdef USE_TILE

#include "tiledef_defines.h"

struct cloud_struct;
class coord_def;
class dolls_data;
class item_def;
class monster;
class tile_flavour;

// Initialize the flavour and the tile env when changing or creating levels.
void tile_new_level(bool first_time);

// Tile flavour

// Set the default type of walls and floors.
void tile_init_default_flavour();
// Get the default types of walls and floors
void tile_default_flv(level_area_type lev, branch_type br, tile_flavour &flv);
// Clear the per-cell wall and floor flavours.
void tile_clear_flavour();
// Initialise types of walls and floors of the entire level using defaults.
void tile_init_flavour();
// Init the flavour of a single cell.
void tile_init_flavour(const coord_def &gc);
// Draw a halo using 'tile' (which has 9 variations) around any features
// that match target.
void tile_floor_halo(dungeon_feature_type target, tileidx_t tile);


// Tile view related
void tile_draw_floor();
void tile_place_item(const coord_def &gc, const item_def &item);
void tile_place_item_marker(const coord_def &gc, const item_def &item);
void tile_place_monster(const coord_def &gc, const monster* mons);
void tile_place_cloud(const coord_def &gc, const cloud_struct &cl);
void tile_place_ray(const coord_def &gc, bool in_range);
void tile_draw_rays(bool reset_count);
void tile_wizmap_terrain(const coord_def &gc);

void tile_apply_animations(tileidx_t bg, tile_flavour *flv);
void tile_apply_properties(const coord_def &gc, tileidx_t *fg,
                           tileidx_t *bg);

void tile_clear_map(const coord_def &gc);
void tile_forget_map(const coord_def &gc);

#endif
#endif
