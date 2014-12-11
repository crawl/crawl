/**
 * @file
 * @brief Functions for x11
**/

// TODO enne - slowly morph this into tilesdl.h

#ifndef LIBGUI_H
#define LIBGUI_H
#ifdef USE_TILE

#include <cstdio>

#include "libconsole.h"

struct coord_def;
struct crawl_view_geometry;

void gui_init_view_params(crawl_view_geometry &geom);

// If mouse on dungeon map, returns true and sets gc.
// Otherwise, it just returns false.
bool gui_get_mouse_grid_pos(coord_def &gc);

#ifndef USE_TILE_WEB
static inline constexpr bool is_tiles() { return true; }
#endif

#endif // USE_TILE
#endif // LIBGUI_H
