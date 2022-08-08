/**
 * @file
 * @brief PNG and texture loading functionality
**/

#pragma once

#include "tilepick.h"

enum MipMapOptions
{
    MIPMAP_CREATE,
    MIPMAP_NONE,
    MIPMAP_MAX,
};

// Arbitrary post-load texture processing
typedef bool(*tex_proc_func)(unsigned char *pixels, unsigned int w,
    unsigned int h);

class GenericTexture
{
public:
    GenericTexture();
    virtual ~GenericTexture();

    bool load_texture(const char *filename, MipMapOptions mip_opt,
                      tex_proc_func proc = nullptr,
                      bool force_power_of_two = true);
    bool load_texture(unsigned char *pixels, unsigned int w, unsigned int h,
                      MipMapOptions mip_opt, int offsetx=-1, int offsety=-1);
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
    const tile_info &get_info(tileidx_t idx) const;
    bool get_coords(tileidx_t idx, int ofs_x, int ofs_y,
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

class FontWrapper;

class ImageManager
{
public:
    ImageManager();
    virtual ~ImageManager();

    bool load_textures(bool need_mips);
    void unload_textures();
    inline const tile_info &tile_def_info(tile_def tile) const;
    const TilesTexture &get_texture(TextureID t) const;
    FontWrapper *get_glyph_font() const;

private:
    // XX just use vector??
    FixedVector<TilesTexture, TEX_MAX> m_textures;
};

// TODO: This function should be moved elsewhere (where?) and called by
// the ImageManager.
inline const tile_info &ImageManager::tile_def_info(tile_def tile) const
{
    const auto tex_id = get_tile_texture(tile.tile);
    return m_textures[tex_id].get_info(tile.tile);
}
