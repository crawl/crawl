/**
 * @file
 * @brief Tiles interface, either for SDL or web tiles. See `tiles.h`
 * for everything that will be imported for all builds.
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

#ifndef USE_TILE_LOCAL
// in view.cc
void flash_monster_colour(const monster* mon, colour_t fmc_colour,
                          int fmc_delay);
#endif
