/*
 *  File:       tiletex.h
 *  Summary:    PNG and texture loading functionality
 *  Written by: Enne Walker
 */

#ifndef TILETEX_H
#define TILETEX_H

enum TextureID
{
    TEX_DUNGEON,
    TEX_DEFAULT,
    TEX_DOLL,
    TEX_TITLE,
    TEX_MAX
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
    void bind();

protected:
    unsigned int m_handle;
    unsigned int m_width;
    unsigned int m_height;
};

class TilesTexture : public GenericTexture
{
public:
    void get_texcoord(int idx, float &x, float &y, float &wx, float &wy);

    void get_texcoord_doll(int part, int idx, int ymax, float &x, float &y, float &wx, float &wy, int &wx_pix, int &wy_pix, int &ox, int &oy);
};

#endif
