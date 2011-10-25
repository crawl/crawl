/**
 * @file
 * @brief Tiles interface, either for SDL or web tiles
**/

#ifndef TILES_H
#define TILES_H

#ifdef USE_TILE_LOCAL
 #include "tilesdl.h"
#elif defined(USE_TILE_WEB)
 #include "tileweb.h"
#endif

#endif
