/**
 * @file
 * @brief Tiles interface, either for SDL or web tiles
**/

#pragma once

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

struct VColour
{
    VColour() {}
    VColour(unsigned char _r, unsigned char _g, unsigned char _b,
            unsigned char _a = 255) : r(_r), g(_g), b(_b), a(_a) {}
    VColour(const VColour &vc) : r(vc.r), g(vc.g), b(vc.b), a(vc.a) {}

    inline void set(const VColour &in)
    {
        r = in.r;
        g = in.g;
        b = in.b;
        a = in.a;
    }

    bool operator==(const VColour &vc) const;
    bool operator!=(const VColour &vc) const;

    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;

    static VColour white;
    static VColour black;
    static VColour transparent;
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

