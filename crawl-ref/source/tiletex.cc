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

    // sequence of events:
    // 1. if needed, the glmanager generates a texture name at m_handle.
    //    the amount of abstraction is a bit obnoxious, but really m_handle
    //    is a cast to GLuint, where texture names are ints of this type.
    // 2. we bind the texture name for writing
    // 3. load the texture with the actual tile sheet (or other texture data)
    //    in question
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


const tile_info &TilesTexture::get_info(tileidx_t idx) const
{
    ASSERT(m_info_func);
    return m_info_func(idx);
}

// XX write a coherent docstring for this, should be done by someone who really
// understands this code
bool TilesTexture::get_coords(tileidx_t idx,
    int ofs_x, int ofs_y,              // input: offset in output space
    float &pos_sx, float &pos_sy,      // output: start x,y of tile in output space
    float &pos_ex, float &pos_ey,      // output: end x,y of tile in output space
    float &tex_sx, float &tex_sy,      // output: start x,y of tile in tile sheet
    float &tex_ex, float &tex_ey,      // output: end x,y of tile in tile sheet
    bool centre, int ymin, int ymax,   // input: centring, allow y trimming
    float tile_x, float tile_y) const  // input: x,y scaling
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
        sy = max(ymin, sy);

    int ey = pos_ey_adjust;
    if (ymax >= 0)
        ey = min(ymax, ey);

    // at this point we have calculated the y bounds and y output adjustments(?)
    if (sy >= ey)
        return false; // Nothing to draw.

    pos_sx += (ofs_x + inf.offset_x + size_ox) / tile_x;
    pos_sy += sy / tile_y;
    pos_ex = pos_sx + (inf.ex - inf.sx) / tile_x;
    pos_ey = pos_sy + (ey - sy) / tile_y;

    tex_sx = inf.sx / fwidth;
    tex_sy = (inf.sy + sy - pos_sy_adjust) / fheight;
    tex_ex = inf.ex / fwidth;
    tex_ey = (inf.ey + ey - pos_ey_adjust) / fheight;

    return true;
}

static const vector<string> &get_texture_filenames()
{
    // This array should correspond to the TEX_ enum.
    static vector<string> _filenames =
    {
        "floor.png",
        "wall.png",
        "feat.png",
        "player.png",
        "main.png",
        "gui.png",
        "icons.png",
    };
    ASSERT(_filenames.size() == TEX_MAX);
    return _filenames;
}

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

    int i = 0;
    for (const auto &f : get_texture_filenames())
        if (!m_textures[i++].load_texture(f.c_str(), mip))
            return false;

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

const TilesTexture &ImageManager::get_texture(TextureID t) const
{
    return m_textures[t];
}

FontWrapper *ImageManager::get_glyph_font() const
{
    return tiles.get_glyph_font();
}

#endif
