#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tiletex.h"

#include "files.h"
#include "glwrapper.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-gui.h"
#include "rltiles/tiledef-icons.h"
#include "rltiles/tiledef-main.h"
#include "rltiles/tiledef-player.h"
#include "windowmanager.h"

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

    return success;
}

bool GenericTexture::load_texture(unsigned char *pixels, unsigned int new_width,
                                  unsigned int new_height,
                                  MipMapOptions mip_opt,
                                  int offsetx, int offsety)
{
    if (!new_width || !new_height)
        return false;

    if (offsetx == -1 && offsety == -1)
    {
        m_width = new_width;
        m_height = new_height;
        glmanager->generate_textures(1, &m_handle);
    }

    bind();
    glmanager->load_texture(pixels, new_width, new_height, mip_opt, offsetx, offsety);

    return true;
}

void GenericTexture::bind() const
{
    ASSERT(m_handle);
    glmanager->bind_texture(m_handle);
}

TilesTexture::TilesTexture() :
    GenericTexture(), m_tile_max(0), m_info_func(nullptr)
{
}

void TilesTexture::set_info(int tile_max, tile_info_func *info_func)
{
    m_tile_max  = tile_max;
    m_info_func = info_func;
}

// This array should correspond to the TEX_ enum.
static const char *_filenames[TEX_MAX] =
{
    "floor.png",
    "wall.png",
    "feat.png",
    "player.png",
    "main.png",
    "gui.png",
    "icons.png",
};

ImageManager::ImageManager()
{
}

ImageManager::~ImageManager()
{
    unload_textures();
}

bool ImageManager::load_textures(bool need_mips)
{
    MipMapOptions mip = need_mips ?
        MIPMAP_CREATE : MIPMAP_NONE;

    for (size_t i = 0; i < ARRAYSZ(_filenames); ++i)
    {
        if (!m_textures[i].load_texture(_filenames[i], mip))
            return false;
    }

    m_textures[TEX_FLOOR].set_info(TILE_FLOOR_MAX, &tile_floor_info);
    m_textures[TEX_WALL].set_info(TILE_DNGN_MAX, &tile_wall_info);
    m_textures[TEX_FEAT].set_info(TILE_DNGN_MAX, &tile_feat_info);
    m_textures[TEX_DEFAULT].set_info(TILEP_PLAYER_MAX, &tile_main_info);
    m_textures[TEX_PLAYER].set_info(TILEP_PLAYER_MAX, &tile_player_info);
    m_textures[TEX_GUI].set_info(TILEG_GUI_MAX, &tile_gui_info);
    m_textures[TEX_ICONS].set_info(TILEI_ICONS_MAX, &tile_icons_info);

    return true;
}

void ImageManager::unload_textures()
{
    for (int i = 0; i < TEX_MAX; i++)
        m_textures[i].unload_texture();
}

#endif
