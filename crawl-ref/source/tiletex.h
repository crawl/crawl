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
    TEX_MAX
};

struct tile_def
{
    tile_def(int _tile, TextureID _tex) : tile(_tile), tex(_tex) {}

    int tile;
    TextureID tex;
};

class GenericTexture
{
public:
    GenericTexture();
    virtual ~GenericTexture();

    enum MipMapOptions
    {
        MIPMAP_CREATE,
        MIPMAP_NONE,
        MIPMAP_MAX
    };

    // Arbitrary post-load texture processing
    typedef bool(*tex_proc_func)(unsigned char *pixels, unsigned int w,
        unsigned int h);

    bool load_texture(const char *filename, MipMapOptions mip_opt,
                      tex_proc_func proc = NULL);
    bool load_texture(unsigned char *pixels, unsigned int w, unsigned int h,
                      MipMapOptions mip_opt);
    void unload_texture();

    unsigned int width() const { return m_width; }
    unsigned int height() const { return m_height; }
    void bind() const;

protected:
    unsigned int m_handle;
    unsigned int m_width;
    unsigned int m_height;
};

class TilesTexture : public GenericTexture
{
public:
    TilesTexture();

    void set_info(int max, tile_info_func *info);
    inline const tile_info &get_info(int idx) const;
    inline void get_coords(int idx, int ofs_x, int ofs_y,
                           float &pos_sx, float &pos_sy,
                           float &pos_ex, float &pos_ey,
                           float &tex_sx, float &tex_sy,
                           float &tex_ex, float &tex_ey,
                           bool centre = true, int ymax = -1) const;

protected:
    int m_tile_max;
    tile_info_func *m_info_func;
};

inline const tile_info &TilesTexture::get_info(int idx) const
{
    ASSERT(m_info_func);
    return m_info_func(idx);
}

inline void TilesTexture::get_coords(int idx, int ofs_x, int ofs_y,
                                     float &pos_sx, float &pos_sy,
                                     float &pos_ex, float &pos_ey,
                                     float &tex_sx, float &tex_sy,
                                     float &tex_ex, float &tex_ey,
                                     bool centre, int ymax) const
{
    const tile_info &inf = get_info(idx);

    float fwidth = m_width;
    float fheight = m_height;

    // center tiles on x, but allow taller tiles to extend upwards
    int size_ox = centre ? TILE_X / 2 - inf.width / 2 : 0;
    int size_oy = centre ? TILE_Y - inf.height : 0;

    int ey = inf.ey;
    if (ymax > 0)
        ey = std::min(inf.sy + ymax - inf.offset_y, ey);

    pos_sx += (ofs_x + inf.offset_x + size_ox) / (float)TILE_X;
    pos_sy += (ofs_y + inf.offset_y + size_oy) / (float)TILE_Y;
    pos_ex = pos_sx + (inf.ex - inf.sx) / (float)TILE_X;
    pos_ey = pos_sy + (ey - inf.sy) / (float)TILE_Y;

    tex_sx = inf.sx / fwidth;
    tex_sy = inf.sy / fheight;
    tex_ex = inf.ex / fwidth;
    tex_ey = ey / fheight;
}


#endif
