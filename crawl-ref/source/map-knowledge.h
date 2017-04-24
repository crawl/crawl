#pragma once

#include "coord.h"
#include "enum.h"
#include "feature.h"

void set_terrain_mapped(const coord_def c);
void set_terrain_seen(const coord_def c);

void set_terrain_visible(const coord_def c);
void clear_terrain_visibility();

int count_detected_mons();

void update_cloud_knowledge();

/**
 * @brief Clear non-terrain knowledge from the map.
 *
 * Cloud knowledge is always cleared. Item and monster knowledge clears depend
 * on @p clear_items and @p clear_mons, respectively.
 *
 * @param clear_items
 *  Clear item knowledge?
 * @param clear_mons
 *  Clear monster knowledge?
 */
void clear_map(bool clear_items = true, bool clear_mons = true);

/**
 * @brief If a travel trail exists, clear it; otherwise clear the map.
 *
 * When clearing the map, all non-terrain knowledge is wiped.
 *
 * @see clear_map() and clear_travel_trail()
 */
void clear_map_or_travel_trail();

map_feature get_cell_map_feature(const map_cell& cell);

void reautomap_level();
