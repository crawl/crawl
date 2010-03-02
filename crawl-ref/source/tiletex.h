/*
 *  File:       tiletex.h
 *  Summary:    PNG and texture loading functionality
 *  Written by: Enne Walker
 */

#ifndef TILETEX_H
#define TILETEX_H

#include "tiles.h"

// The different texture types.
enum TextureID
{
    TEX_DUNGEON, // dngn.png
    TEX_PLAYER,  // player.png
    TEX_DEFAULT, // main.png
    TEX_GUI,     // gui.png
    TEX_MAX
};

struct tile_def
{
    tile_def(int _tile, TextureID _tex, int _ymax = TILE_Y)
            : tile(_tile), tex(_tex), ymax(_ymax){}

    int tile;
    TextureID tex;
    int ymax;
};

class GenericTexture
{
public:
    GenericTexture();
    virtual ~GenericTexture();

    // Arbitrary post-load texture processing
    typedef bool(*tex_proc_func)(unsigned char *pixels, unsigned int w,
        unsigned int h);

    bool load_texture(const char *filename, MipMapOptions mip_opt,
                      tex_proc_func proc = NULL,
                      bool force_power_of_two = true);
    bool load_texture(unsigned char *pixels, unsigned int w, unsigned int h,
                      MipMapOptions mip_opt);
    void unload_texture();

    unsigned int width() const { return m_width; }
    unsigned int height() const { return m_height; }
    void bind() const;

    unsigned int orig_width() const { return m_orig_width; }
    unsigned int orig_height() const { return m_orig_height; }

protected:
    unsigned int m_handle;
    unsigned int m_width;
    unsigned int m_height;

    unsigned int m_orig_width;
    unsigned int m_orig_height;
};

class TilesTexture : public GenericTexture
{
public:
    TilesTexture();

    void set_info(int max, tile_info_func *info);
    inline const tile_info &get_info(int idx) const;
    inline bool get_coords(int idx, int ofs_x, int ofs_y,
                           float &pos_sx, float &pos_sy,
                           float &pos_ex, float &pos_ey,
                           float &tex_sx, float &tex_sy,
                           float &tex_ex, float &tex_ey,
                           bool centre = true, int ymin = -1, int ymax = -1,
                           float tile_x = TILE_X, float tile_y = TILE_Y) const;

protected:
    int m_tile_max;
    tile_info_func *m_info_func;
};

inline const tile_info &TilesTexture::get_info(int idx) const
{
    ASSERT(m_info_func);
    return m_info_func(idx);
}

inline bool TilesTexture::get_coords(int idx, int ofs_x, int ofs_y,
                                     float &pos_sx, float &pos_sy,
                                     float &pos_ex, float &pos_ey,
                                     float &tex_sx, float &tex_sy,
                                     float &tex_ex, float &tex_ey,
                                     bool centre, int ymin, int ymax,
                                     float tile_x, float tile_y) const
{
    const tile_info &inf = get_info(idx);

    float fwidth  = m_width;
    float fheight = m_height;

    // Centre tiles on x, but allow taller tiles to extend upwards.
    int size_ox = centre ? TILE_X / 2 - inf.width / 2 : 0;
    int size_oy = centre ? TILE_Y - inf.height : 0;

    int pos_sy_adjust = ofs_y + inf.offset_y + size_oy;
    int pos_ey_adjust = pos_sy_adjust + inf.ey - inf.sy;

    int sy = pos_sy_adjust;
    if (ymin >= 0)
        sy = std::max(ymin, sy);

    int ey = pos_ey_adjust;
    if (ymax >= 0)
        ey = std::min(ymax, ey);

    // Nothing to draw.
    if (sy >= ey)
        return (false);

    pos_sx += (ofs_x + inf.offset_x + size_ox) / tile_x;
    pos_sy += sy / tile_y;
    pos_ex = pos_sx + (inf.ex - inf.sx) / tile_x;
    pos_ey = pos_sy + (ey - sy) / tile_y;

    tex_sx = inf.sx / fwidth;
    tex_sy = (inf.sy + sy - pos_sy_adjust) / fheight;
    tex_ex = inf.ex / fwidth;
    tex_ey = (inf.ey + ey - pos_ey_adjust) / fheight;

    return (true);
}

#endif
