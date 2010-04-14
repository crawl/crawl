#include "AppHdr.h"

#ifdef USE_TILE

#include "describe.h"
#include "files.h"
#include "glwrapper.h"
#include "tiledef-dngn.h"
#include "tiledef-gui.h"
#include "tiledef-main.h"
#include "tiledef-player.h"
#include "tiletex.h"
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

const char *ImageManager::filenames[TEX_MAX] =
{
    "dngn.png",
    "player.png",
    "main.png",
    "gui.png"
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
    if (!m_textures[TEX_DUNGEON].load_texture(filenames[TEX_DUNGEON], mip))
        return (false);

    if (!m_textures[TEX_PLAYER].load_texture(filenames[TEX_PLAYER], mip))
        return (false);

    if (!m_textures[TEX_GUI].load_texture(filenames[TEX_GUI], mip))
        return (false);

    m_textures[TEX_DUNGEON].set_info(TILE_DNGN_MAX, &tile_dngn_info);
    m_textures[TEX_PLAYER].set_info(TILEP_PLAYER_MAX, &tile_player_info);
    m_textures[TEX_GUI].set_info(TILEG_GUI_MAX, &tile_gui_info);

    return (true);
}

static void _copy_onto(unsigned char *pixels, int width,
                       int height, unsigned char *src,
                       const tile_info &inf, bool blend)
{
    unsigned char *dest = &pixels[4 * (inf.sy * width + inf.sx)];

    size_t dest_row_size = width * 4;
    size_t src_row_size = inf.width * 4;

    if (blend)
    {
        for (int r = 0; r < inf.height; r++)
        {
            for (int c = 0; c < inf.width; c++)
            {
                unsigned char a = src[3];
                unsigned char inv_a = 255 - src[3];
                dest[0] = (src[0] * a + dest[0] * inv_a) / 255;
                dest[1] = (src[1] * a + dest[1] * inv_a) / 255;
                dest[2] = (src[2] * a + dest[2] * inv_a) / 255;
                dest[3] = (src[3] * a + dest[3] * inv_a) / 255;

                dest += 4;
                src += 4;
            }
            dest += dest_row_size - src_row_size;
        }
    }
    else
    {
        for (int r = 0; r < inf.height; r++)
        {
            memcpy(dest, src, src_row_size);

            dest += dest_row_size;
            src += src_row_size;
        }
    }
}

// Copy an image at inf from pixels into dest.
static void _copy_into(unsigned char *dest, unsigned char *pixels,
                       int width, int height, const tile_info &inf,
                       int ofs_x = 0, int ofs_y = 0)
{
    unsigned char *src = &pixels[4 * (inf.sy * width + inf.sx)];

    size_t src_row_size = width * 4;
    size_t dest_row_size = inf.width * 4;

    memset(dest, 0, 4 * inf.width * inf.height);

    int total_ofs_x = inf.offset_x + ofs_x;
    int total_ofs_y = inf.offset_y + ofs_y;
    int src_height  = inf.ey - inf.sy;
    int src_width   = inf.ex - inf.sx;

    if (total_ofs_x < 0)
    {
        src_width += total_ofs_x;
        src -= 4 * total_ofs_x;
        total_ofs_x = 0;
    }
    if (total_ofs_y < 0)
    {
        src_height += total_ofs_y;
        src -= 4 * width * total_ofs_y;
        total_ofs_y = 0;
    }

    dest += total_ofs_x * 4 + total_ofs_y * dest_row_size;

    for (int r = 0; r < src_height; r++)
    {
        memcpy(dest, src, src_width * 4);

        dest += dest_row_size;
        src += src_row_size;
    }
}

// Stores "over" on top of "under" in the location of "over".
static bool _copy_under(unsigned char *pixels, int width,
                        int height, int idx_under, int idx_over,
                        int uofs_x = 0, int uofs_y = 0)
{
    const tile_info &under = tile_main_info(idx_under);
    const tile_info &over  = tile_main_info(idx_over);

    if (over.width != under.width || over.height != under.height)
        return (false);

    if (over.ex - over.sx != over.width || over.ey - over.sy != over.height)
        return (false);

    if (over.offset_x != 0 || over.offset_y != 0)
        return (false);

    size_t image_size = under.width * under.height * 4;

    // Make a copy of the original images.
    unsigned char *under_pixels = new unsigned char[image_size];
    _copy_into(under_pixels, pixels, width, height, under, uofs_x, uofs_y);
    unsigned char *over_pixels = new unsigned char[image_size];
    _copy_into(over_pixels, pixels, width, height, over);

    // Replace the over image with the under image
    _copy_onto(pixels, width, height, under_pixels, over, false);
    // Blend the over image over top.
    _copy_onto(pixels, width, height, over_pixels, over, true);

    delete[] under_pixels;
    delete[] over_pixels;

    return (true);
}

static bool _process_item_image(unsigned char *pixels, unsigned int width,
                                unsigned int height)
{
    bool success = true;
    for (int i = 0; i < NUM_POTIONS; i++)
    {
        int special = you.item_description[IDESC_POTIONS][i];
        int tile0 = TILE_POTION_OFFSET + special % 14;
        int tile1 = TILE_POT_HEALING + i;
        success &= _copy_under(pixels, width, height, tile0, tile1);
    }

    for (int i = 0; i < NUM_WANDS; i++)
    {
        int special = you.item_description[IDESC_WANDS][i];
        int tile0 = TILE_WAND_OFFSET + special % 12;
        int tile1 = TILE_WAND_FLAME + i;
        success &= _copy_under(pixels, width, height, tile0, tile1);
    }

    for (int i = 0; i < STAFF_SMITING; i++)
    {
        int special = you.item_description[IDESC_STAVES][i];
        int tile0 = TILE_STAFF_OFFSET + (special / 4) % 10;
        int tile1 = TILE_STAFF_WIZARDRY + i;
        success &= _copy_under(pixels, width, height, tile0, tile1);
    }
    for (int i = STAFF_SMITING; i < NUM_STAVES; i++)
    {
        int special = you.item_description[IDESC_STAVES][i];
        int tile0 = TILE_ROD_OFFSET + (special / 4) % 10;
        int tile1 = TILE_ROD_SMITING + i - STAFF_SMITING;
        success &= _copy_under(pixels, width, height, tile0, tile1);
    }
    for (int i = RING_FIRST_RING; i < NUM_RINGS; i++)
    {
        int special = you.item_description[IDESC_RINGS][i];
        int tile0 = TILE_RING_NORMAL_OFFSET + special % 13;
        int tile1 = TILE_RING_REGENERATION + i - RING_FIRST_RING;
        success &= _copy_under(pixels, width, height, tile0, tile1, -5, -6);
    }
    for (int i = AMU_FIRST_AMULET; i < NUM_JEWELLERY; i++)
    {
        int special = you.item_description[IDESC_RINGS][i];
        int tile0 = TILE_AMU_NORMAL_OFFSET + special % 13;
        int tile1 = TILE_AMU_RAGE + i - AMU_FIRST_AMULET;
        success &= _copy_under(pixels, width, height, tile0, tile1);
    }

    return (true);
}

bool ImageManager::load_item_texture()
{
    // We need to load images in two passes: one for the title and one
    // for the items.  To handle identifiable items, the texture itself
    // is modified.  So, it cannot be loaded until after the item
    // description table has been initialised.
    MipMapOptions mip = MIPMAP_CREATE;
    const char *fname = filenames[TEX_DEFAULT];
    bool success = m_textures[TEX_DEFAULT].load_texture(fname, mip,
                                                        &_process_item_image);
    m_textures[TEX_DEFAULT].set_info(TILE_MAIN_MAX, tile_main_info);

    return success;
}

void ImageManager::unload_textures()
{
    for (int i = 0; i < TEX_MAX; i++)
        m_textures[i].unload_texture();
}

#endif
