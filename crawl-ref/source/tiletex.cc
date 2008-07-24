#include "AppHdr.h"

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
        return false;
    }

    SDL_Surface *img = IMG_Load(tex_path.c_str());

    if (!img)
    {
        fprintf(stderr, "Couldn't load texture '%s'.\n", tex_path.c_str());
        return false;
    }

    unsigned int bpp = img->format->BytesPerPixel;
    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);

    // Determine texture format
    unsigned char *pixels = (unsigned char*)img->pixels;
    int new_width = img->w;
    int new_height = img->h;
    GLenum texture_format;
    if (bpp == 4)
    {
        if (img->format->Rmask == 0x000000ff)
            texture_format = GL_RGBA;
        else
            texture_format = GL_BGRA;
    }
    else if (bpp == 3)
    {
        if (img->format->Rmask == 0x000000ff)
            texture_format = GL_RGB;
        else
            texture_format = GL_BGR;
    }
    else if (bpp == 1)
    {
        // need to depalettize
        SDL_LockSurface(img);

        // Prefer power-of-two textures to avoid texture bleeding from
        // floating point error.
        // TODO enne - convert non-palettized to power-of-2 as well?
        new_width = 1;
        while (new_width < img->w)
            new_width *= 2;
        new_height = 1;
        while (new_height < img->h)
            new_height *= 2;

        pixels = new unsigned char[4 * new_width * new_height];

        SDL_Palette* pal = img->format->palette;
        ASSERT(pal);
        ASSERT(pal->colors);

        // Find transparent colour
        // TODO enne - this should probably be removed from rltiles
        // TODO enne - is there more than one transparent color??
        int trans_index = -1;
        for (int p = 0; p < pal->ncolors ; p++)
        {
            if (pal->colors[p].r == 71 &&
                pal->colors[p].g == 108 &&
                pal->colors[p].b == 108)
            {
                trans_index = p;
                break;
            }
        }

        int src = 0;
        int dest = 0;
        for (int y = 0; y < img->h; y++)
        {
            int x;
            for (x = 0; x < img->w; x++)
            {
                int index = ((unsigned char*)img->pixels)[src++];
                pixels[dest*4    ] = pal->colors[index].r;
                pixels[dest*4 + 1] = pal->colors[index].g;
                pixels[dest*4 + 2] = pal->colors[index].b;
                pixels[dest*4 + 3] = (index == trans_index) ? 0 : 255;
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
        return false;
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

    return success;
}

bool GenericTexture::load_texture(unsigned char *pixels, unsigned int new_width,
                                  unsigned int new_height,
                                  GenericTexture::MipMapOptions mip_opt)
{
    if (!pixels || !new_width || !new_height)
        return false;

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

    return true;
}

void GenericTexture::bind()
{
    ASSERT(m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);
}

void TilesTexture::get_texcoord_doll(int part, int idx, int ymax, float &x, float &y, float &wx, float &wy, int &wx_pix, int &wy_pix, int &ox, int &oy)
{
    int tile_idx = tilep_parts_start[part];
    int nx = tilep_parts_nx[part];
    int ny = tilep_parts_ny[part];
    ox = tilep_parts_ox[part];
    oy = tilep_parts_oy[part];
    wx_pix = TILE_X / nx;
    wy_pix = TILE_Y / ny;

    if (!idx)
    {
        wy = -1;
        return;
    }

    idx--;
    tile_idx += idx / (nx * ny);

    if (oy + wy_pix > ymax)
        wy_pix -= oy + wy_pix - ymax;

    int xs = (tile_idx % TILEP_PER_ROW) * TILE_X;
    int ys = (tile_idx / TILEP_PER_ROW) * TILE_Y;
    xs += (idx % nx) * TILE_X / nx;
    ys += ((idx / nx) % ny) * TILE_Y / ny;

    x = xs / (float)m_width;
    y = ys / (float)m_height;
    wx = wx_pix / (float)m_width;
    wy = wy_pix / (float)m_height;
}

void TilesTexture::get_texcoord(int idx, float &x, float &y, 
                                float &wx, float &wy)
{
    const unsigned int tile_size = 32;
    unsigned int tiles_per_row = m_width / tile_size;

    wx = tile_size / (float)m_width;
    wy = tile_size / (float)m_height;

    unsigned int row = idx / tiles_per_row;
    unsigned int col = idx % tiles_per_row;

    x = tile_size * col / (float)m_width;
    y = tile_size * row / (float)m_height;

    ASSERT(row >= 0);
    ASSERT(col >= 0);
    ASSERT(x + wx <= m_width);
    ASSERT(y + wy <= m_height);
}

