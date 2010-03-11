#include "AppHdr.h"

#ifdef USE_TILE

#include "files.h"
#include "tiletex.h"

#include "uiwrapper.h"
#include "cgcontext.h"
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

    GLStateManager::delete_textures(1, &m_handle);
}

bool GenericTexture::load_texture(const char *filename,
                                  MipMapOptions mip_opt,
                                  tex_proc_func proc,
                                  bool force_power_of_two)
{
    char acBuffer[512];

    std::string tex_path = datafile_path(filename);

    if (tex_path.c_str()[0] == 0)
    {
        fprintf(stderr, "Couldn't find texture '%s'.\n", filename);
        return (false);
    }

    GraphicsContext *img = new GraphicsContext();
    if ( !img )
    {
        fprintf(stderr, "Could not create context for texture '%s'.\n",
            tex_path.c_str());
        return (false);
    }

    if ( img->load_image(tex_path.c_str()) )
    {
        fprintf(stderr, "Couldn't load texture '%s'.\n", tex_path.c_str());
        return (false);
    }

    unsigned int bpp = img->bytes_per_pixel();
    GLStateManager::pixelstore_unpack_alignment(bpp);

    // Determine texture format
    unsigned char *pixels = (unsigned char*)img->pixels();

    int new_width;
    int new_height;
    if (force_power_of_two)
    {
        new_width = 1;
        while (new_width < img->width())
            new_width *= 2;

        new_height = 1;
        while (new_height < img->height())
            new_height *= 2;
    }
    else
    {
        new_width = img->width();
        new_height = img->height();
    }

    if (bpp == 4)
    {
        // Even if the size is the same, still go through
        // SDL_get_rgba to put the image in the right format.
        img->lock();
        pixels = new unsigned char[4 * new_width * new_height];
        memset(pixels, 0, 4 * new_width * new_height);

        int dest = 0;
        for (int y = 0; y < img->height(); y++)
        {
            for (int x = 0; x < img->width(); x++)
            {
                unsigned char *p = ((unsigned char*)img->pixels()
                                  + y * img->pitch() + x * bpp);
                unsigned int pixel = *(unsigned int*)p;
                img->get_rgba(pixel, &pixels[dest], &pixels[dest+1],
                                    &pixels[dest+2], &pixels[dest+3]);
                dest += 4;
            }
            dest += 4 * (new_width - img->width());
        }

        img->unlock();
    }
    else if (bpp == 3)
    {
        if (new_width != img->width() || new_height != img->height())
        {
            img->lock();
            pixels = new unsigned char[4 * new_width * new_height];
            memset(pixels, 0, 4 * new_width * new_height);

            int dest = 0;
            for (int y = 0; y < img->height(); y++)
            {
                for (int x = 0; x < img->width(); x++)
                {
                    unsigned char *p = ((unsigned char*)img->pixels()
                                       + y * img->pitch() + x * bpp);
                    unsigned int pixel;
                    if (wrapper.byte_order() == UI_BIG_ENDIAN)
                        pixel = p[0] << 16 | p[1] << 8 | p[2];
                    else
                        pixel = p[0] | p[1] << 8 | p[2];
                    img->get_rgba(pixel, &pixels[dest], &pixels[dest+1],
                                        &pixels[dest+2], &pixels[dest+3]);
                    dest += 4;
                }
                dest += 4 * (new_width - img->width());
            }

            img->unlock();
        }
    }
    else if (bpp == 1)
    {
        // need to depalettize
        img->lock();

        pixels = new unsigned char[4 * new_width * new_height];

        ui_palette* pal = img->palette;
        ASSERT(pal);
        ASSERT(pal->colors);

        int src = 0;
        int dest = 0;
        for (int y = 0; y < img->height(); y++)
        {
            int x;
            for (x = 0; x < img->width(); x++)
            {
                unsigned int index = ((unsigned char*)img->pixels())[src++];
                pixels[dest*4    ] = pal->colors[index].r;
                pixels[dest*4 + 1] = pal->colors[index].g;
                pixels[dest*4 + 2] = pal->colors[index].b;
                pixels[dest*4 + 3] = (index != img->color_key() ? 255 : 0);
                dest++;
            }
            while (x++ < new_width)
            {
                // Extend to the right with transparent pixels
                pixels[dest*4    ] = 0;
                pixels[dest*4 + 1] = 0;
                pixels[dest*4 + 2] = 0;
                pixels[dest*4 + 3] = 0;
                dest++;
            }
        }
        while (dest < new_width * new_height)
        {
            // Extend down with transparent pixels
            pixels[dest*4    ] = 0;
            pixels[dest*4 + 1] = 0;
            pixels[dest*4 + 2] = 0;
            pixels[dest*4 + 3] = 0;
            dest++;
        }

        img->unlock();

        bpp = 4;
    }
    else
    {
        printf("Warning: unsupported format, bpp = %d for '%s'\n",
               bpp, acBuffer);
        return (false);
    }

    bool success = false;
    if (!proc || proc(pixels, new_width, new_height))
        success |= load_texture(pixels, new_width, new_height, mip_opt);

    // If conversion has occurred, delete converted data.
    if (pixels != img->pixels())
        delete[] pixels;

    m_orig_width  = img->width();
    m_orig_height = img->height();

    delete img;

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
    
    GLStateManager::generate_textures(1, &m_handle);
    GLStateManager::bind_texture(m_handle);
    GLStateManager::load_texture(pixels, m_width, m_height, mip_opt);

    return (true);
}

void GenericTexture::bind() const
{
    ASSERT(m_handle);
    GLStateManager::bind_texture(m_handle);
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
