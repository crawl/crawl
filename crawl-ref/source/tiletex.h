/**
 * @file
 * @brief PNG and texture loading functionality
**/

#pragma once

#include "tilepick.h"
#include "debug.h"

enum MipMapOptions
{
    MIPMAP_CREATE,
    MIPMAP_NONE,
    MIPMAP_MAX,
};

// Convenience structure to encapsulate parameters that always go together
// Not all combinations of parameters are valid, so this class's factory methods
// enforce that only valid combinations can be created
class LoadTextureArgs
{
public:
    static constexpr int NO_OFFSET = -1;
    // Fonts aren't being mipmapped.
    // Currently, glwrapper-ogl.cc does not support generating mipmaps for offset textures.
    // It's unclear if it even would be desirable to handle it
    // Also, the pixels aren't provided - presumably because they're already in some texture that's a font atlas?
    // If you understand the surrounding code better, please clarify why this works at all.
    static LoadTextureArgs CreateForFont(unsigned int width, unsigned int height,
                                             int xoffset, int yoffset)
    {
        return LoadTextureArgs(nullptr,
                               width, height,
                               MIPMAP_NONE,
                               xoffset, yoffset);
    }

    // Textures don't have offsets, but may be mipmapped (if mipmaps are enabled globally)
    static LoadTextureArgs CreateForTexture(unsigned char *pixels, unsigned int width,
                                             unsigned int height, MipMapOptions mip_opt)
    {
        return LoadTextureArgs(pixels,
                               width, height,
                               mip_opt,
                               NO_OFFSET, NO_OFFSET);
    }

    const unsigned char *pixels;
    const unsigned int width;
    const unsigned int height;
    const MipMapOptions mip_opt;
    const int xoffset;
    const int yoffset;

    inline bool has_offset() const
    {
        return xoffset != NO_OFFSET || yoffset != NO_OFFSET;
    }
private:
    LoadTextureArgs(unsigned char *my_pixels,
                    unsigned int my_width,
                    unsigned int my_height,
                    MipMapOptions my_mip_opt,
                    int my_xoffset,
                    int my_yoffset):
            pixels(my_pixels),
            width(my_width),
            height(my_height),
            mip_opt(my_mip_opt),
            xoffset(my_xoffset),
            yoffset(my_yoffset)
    {
        // It's valid for xoffset or yoffset to be zero alone.
        // But they should only be the "magic" NO_OFFSET value together
        ASSERT((xoffset == NO_OFFSET) == (yoffset == NO_OFFSET));
        if (this->has_offset())
        {
            // offset with mipmap not supported, this should be impossible to reach
            ASSERT(mip_opt == MIPMAP_NONE);
        }
    }
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
    bool load_texture(LoadTextureArgs texture);
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

    bool load_textures(MipMapOptions mip_opts);
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
