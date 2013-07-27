/**
 * @file
 * @brief Tiles interface, either for SDL or web tiles
**/

#ifndef TILES_H
#define TILES_H

// The different texture types.
enum TextureID
{
    TEX_FLOOR,   // floor.png
    TEX_WALL,    // wall.png
    TEX_FEAT,    // feat.png
    TEX_PLAYER,  // player.png
    TEX_DEFAULT, // main.png
    TEX_GUI,     // gui.png
    TEX_ICONS,   // icons.png
    TEX_MAX
};

struct tile_def
{
    tile_def(tileidx_t _tile, TextureID _tex, int _ymax = TILE_Y)
            : tile(_tile), tex(_tex), ymax(_ymax) {}

    tileidx_t tile;
    TextureID tex;
    int ymax;
};

TextureID get_dngn_tex(tileidx_t idx);

#ifdef USE_TILE_LOCAL
 #include "tilesdl.h"
#elif defined(USE_TILE_WEB)
 #include "tileweb.h"
#endif

#endif
