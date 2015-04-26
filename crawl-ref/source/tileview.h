/**
 * @file
 * @brief Tile functions that modify the tiles or flavours in
 *        the crawl_environment that are shown to the player.
**/

#ifndef TILEVIEW_H
#define TILEVIEW_H

#include "target.h"

struct cloud_info;
struct coord_def;
struct dolls_data;
struct item_def;
class monster;
struct tile_flavour;
struct packed_cell;

tileidx_t pick_dngn_tile(tileidx_t idx, int value);

// Initialize the flavour and the tile env when changing or creating levels.
void tile_new_level(bool first_time, bool init_unseen);

static inline void tile_new_level(bool first_time)
{
    return tile_new_level(first_time, first_time);
}

// Tile flavour

// Set the default type of walls and floors.
void tile_init_default_flavour();
// Get the default types of walls and floors
void tile_default_flv(branch_type br, int depth, tile_flavour &flv);
// Clear the per-cell wall and floor flavours.
void tile_clear_flavour(const coord_def &p);
void tile_clear_flavour();
// Initialise types of walls and floors of the entire level using defaults.
void tile_init_flavour();
// Init the flavour of a single cell.
void tile_init_flavour(const coord_def &gc, const int domino = -1);
// Draw a halo using 'tile' (which has 9 variations) around any features
// that match target.
void tile_floor_halo(dungeon_feature_type target, tileidx_t tile);

// Tile view related
void tile_draw_floor();
void tile_reset_fg(const coord_def &gc);
void tile_reset_feat(const coord_def &gc);
void tile_place_ray(const coord_def &gc, aff_type in_range);
void tile_draw_rays(bool reset_count);
void tile_draw_map_cell(const coord_def &gc, bool foreground_only = false);
void tile_wizmap_terrain(const coord_def &gc);

void tile_apply_animations(tileidx_t bg, tile_flavour *flv);
void tile_apply_properties(const coord_def &gc, packed_cell &cell);
void apply_variations(const tile_flavour &flv, tileidx_t *bg,
                      const coord_def &gc, const unsigned int idx);

void tile_clear_map(const coord_def &gc);
void tile_forget_map(const coord_def &gc);

#endif
