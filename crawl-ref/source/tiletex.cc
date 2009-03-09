#include "AppHdr.h"
REVISION("$Rev$");

#include "files.h"
#include "tiles.h"
#include "tiletex.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>

GenericTexture::GenericTexture() :
    m_handle(0),
    m_width(0),
    m_height(0)
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

    glDeleteTextures(1, (GLuint*)&m_handle);
}

bool GenericTexture::load_texture(const char *filename,
                                  GenericTexture::MipMapOptions mip_opt,
                                  tex_proc_func proc)
{
    char acBuffer[512];

    std::string tex_path = datafile_path(filename);

    if (tex_path.c_str()[0] == 0)
    {
        fprintf(stderr, "Couldn't find texture '%s'.\n", filename);
        return (false);
    }

    SDL_Surface *img = IMG_Load(tex_path.c_str());

    if (!img)
    {
        fprintf(stderr, "Couldn't load texture '%s'.\n", tex_path.c_str());
        return (false);
    }

    unsigned int bpp = img->format->BytesPerPixel;
    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);

    // Determine texture format
    unsigned char *pixels = (unsigned char*)img->pixels;
    int new_width = 1;
    while (new_width < img->w)
        new_width *= 2;

    int new_height = 1;
    while (new_height < img->h)
        new_height *= 2;

    GLenum texture_format;
    if (bpp == 4)
    {
        if (new_width != img->w || new_height != img->h)
        {
            SDL_LockSurface(img);
            pixels = new unsigned char[4 * new_width * new_height];
            memset(pixels, 0, 4 * new_width * new_height);

            int dest = 0;
            for (int y = 0; y < img->h; y++)
            {
                for (int x = 0; x < img->w; x++)
                {
                    unsigned char *p = ((unsigned char*)img->pixels
                                      + y * img->pitch + x * bpp);
                    unsigned int pixel = *(unsigned int*)p;
                    SDL_GetRGBA(pixel, img->format, &pixels[dest],
                                &pixels[dest+1], &pixels[dest+2],
                                &pixels[dest+3]);
                    dest += 4;
                }
                dest += 4 * (new_width - img->w);
            }

            SDL_UnlockSurface(img);
        }
        texture_format = GL_RGBA;
    }
    else if (bpp == 3)
    {
        if (new_width != img->w || new_height != img->h)
        {
            SDL_LockSurface(img);
            pixels = new unsigned char[4 * new_width * new_height];
            memset(pixels, 0, 4 * new_width * new_height);

            int dest = 0;
            for (int y = 0; y < img->h; y++)
            {
                for (int x = 0; x < img->w; x++)
                {
                    unsigned char *p = ((unsigned char*)img->pixels
                                       + y * img->pitch + x * bpp);
                    unsigned int pixel;
                    if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                        pixel = p[0] << 16 | p[1] << 8 | p[2];
                    else
                        pixel = p[0] | p[1] << 8 | p[2];
                    SDL_GetRGBA(pixel, img->format, &pixels[dest],
                                &pixels[dest+1], &pixels[dest+2],
                                &pixels[dest+3]);
                    dest += 4;
                }
                dest += 4 * (new_width - img->w);
            }

            SDL_UnlockSurface(img);
        }
        texture_format = GL_RGBA;
    }
    else if (bpp == 1)
    {
        // need to depalettize
        SDL_LockSurface(img);

        pixels = new unsigned char[4 * new_width * new_height];

        SDL_Palette* pal = img->format->palette;
        ASSERT(pal);
        ASSERT(pal->colors);

        int src = 0;
        int dest = 0;
        for (int y = 0; y < img->h; y++)
        {
            int x;
            for (x = 0; x < img->w; x++)
            {
                unsigned int index = ((unsigned char*)img->pixels)[src++];
                pixels[dest*4    ] = pal->colors[index].r;
                pixels[dest*4 + 1] = pal->colors[index].g;
                pixels[dest*4 + 2] = pal->colors[index].b;
                pixels[dest*4 + 3] = (index != img->format->colorkey ? 255 : 0);
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

        SDL_UnlockSurface(img);

        bpp = 4;
        texture_format = GL_RGBA;
    }
    else
    {
        printf("Warning: unsupported format, bpp = %d for '%s'\n",
               bpp, acBuffer);
        return (false);
    }

    bool success = false;
    if (!proc || proc(pixels, new_width, new_height))
    {
        success |= load_texture(pixels, new_width, new_height, mip_opt);
    }

    // If conversion has occurred, delete converted data.
    if (pixels != img->pixels)
        delete pixels;

    SDL_FreeSurface(img);

    return (success);
}

bool GenericTexture::load_texture(unsigned char *pixels, unsigned int new_width,
                                  unsigned int new_height,
                                  GenericTexture::MipMapOptions mip_opt)
{
    if (!pixels || !new_width || !new_height)
        return (false);

    // Assumptions...
    const unsigned int bpp = 4;
    const GLenum texture_format = GL_RGBA;
    const GLenum format = GL_UNSIGNED_BYTE;

    m_width = new_width;
    m_height = new_height;

    glGenTextures(1, (GLuint*)&m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    if (mip_opt == MIPMAP_CREATE)
    {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gluBuild2DMipmaps(GL_TEXTURE_2D, bpp, m_width, m_height,
                          texture_format, format, pixels);
    }
    else
    {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, bpp, m_width, m_height, 0,
                     texture_format, format, pixels);
    }

    return (true);
}

void GenericTexture::bind() const
{
    ASSERT(m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);
}

TilesTexture::TilesTexture() :
    GenericTexture(), m_tile_max(0), m_info_func(NULL)
{

}

void TilesTexture::set_info(int tile_max, tile_info_func *info_func)
{
    m_tile_max = tile_max;
    m_info_func = info_func;
}

