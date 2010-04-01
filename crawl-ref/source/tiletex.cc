#include "AppHdr.h"

#ifdef USE_TILE

#include "files.h"
#include "tiletex.h"

#include "uiwrapper.h"
#include "glwrapper.h"

GenericTexture::GenericTexture() :
    m_handle(0),
    m_width(0),
    m_height(0),
    m_orig_width(0),
    m_orig_height(0)
{
}

GenericTexture::~GenericTexture()
{
    unload_texture();
}

void GenericTexture::unload_texture()
{
    if (!m_handle)
        return;

    glmanager->delete_textures(1, &m_handle);
}

bool GenericTexture::load_texture(const char *filename,
                                  MipMapOptions mip_opt,
                                  tex_proc_func proc,
                                  bool force_power_of_two)
{
    bool success = false;
    success = wm->load_texture(this, filename, mip_opt, m_orig_width,
        m_orig_height, proc, force_power_of_two);

    return (success);
}

bool GenericTexture::load_texture(unsigned char *pixels, unsigned int new_width,
                                  unsigned int new_height,
                                  MipMapOptions mip_opt)
{
    if (!pixels || !new_width || !new_height)
        return (false);

    m_width = new_width;
    m_height = new_height;
    
    glmanager->generate_textures(1, &m_handle);
    bind();
    glmanager->load_texture(pixels, m_width, m_height, mip_opt);

    return (true);
}

void GenericTexture::bind() const
{
    ASSERT(m_handle);
    glmanager->bind_texture(m_handle);
}

TilesTexture::TilesTexture() :
    GenericTexture(), m_tile_max(0), m_info_func(NULL)
{
    
}

void TilesTexture::set_info(int tile_max, tile_info_func *info_func)
{
    m_tile_max  = tile_max;
    m_info_func = info_func;
}

#endif
