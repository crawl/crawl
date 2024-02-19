/**
 * @file
 * @brief Tiles interface, either for SDL or web tiles, or neither. See `tiles.h`
 * for everything that will be imported for all builds.
 *
 * This is always safe to import, but the more it is imported, the more switching
 * targets forces rebuilding.
**/

#pragma once

#ifdef USE_TILE_LOCAL
static inline bool is_tiles() { return true; }
# include "tilesdl.h"
#elif defined(USE_TILE_WEB)
bool is_tiles(); // in tilesdl.cc
# include "tileweb.h"
#else
static inline bool is_tiles() { return false; }
#endif

#ifdef USE_TILE
VColour str_to_tile_colour(string colour); // in colour.cc

// in item-use.cc
void tile_item_pickup(int idx, bool part);
void tile_item_drop(int idx, bool partdrop);
void tile_item_use(int idx);
void tile_item_use_secondary(int idx);
#endif
