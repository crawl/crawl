#include "tile.h"

#include <assert.h>
#include <SDL.h>
#include <SDL_image.h>

tile::tile() : m_width(0), m_height(0), m_pixels(NULL), m_shrink(true)
{
}

tile::tile(const tile &img, const char *enumname, const char *parts_ctg) : 
    m_width(0), m_height(0), m_pixels(NULL)
{
    copy(img);

    if (enumname)
        m_enumname = enumname;
    if (parts_ctg)
        m_parts_ctg = parts_ctg;
}

tile::~tile()
{
    unload();
}

void tile::unload()
{
    delete[] m_pixels;
    m_pixels = NULL;
    m_width = m_height = 0;
}

bool tile::valid() const
{
    return m_pixels && m_width && m_height;
}

const std::string &tile::filename()
{
    return m_filename;
}

const std::string &tile::enumname()
{
    return m_enumname;
}

const std::string &tile::parts_ctg()
{
    return m_parts_ctg;
}

int tile::width()
{
    return m_width;
}

int tile::height()
{
    return m_height;
}

bool tile::shrink()
{
    return m_shrink;
}

void tile::set_shrink(bool shrink)
{
    m_shrink = shrink;
}

void tile::resize(int new_width, int new_height)
{
    delete[] m_pixels;
    m_width = new_width;
    m_height = new_height;

    m_pixels = NULL;

    if (!m_width || !m_height)
        return;

    m_pixels = new tile_colour[m_width * m_height];
}

void tile::add_rim(const tile_colour &rim)
{
    bool *flags = new bool[m_width * m_height];
    for (unsigned int y = 0; y < m_height; y++)
    {
        for (unsigned int x = 0; x < m_width; x++)
        {
            flags[x + y * m_width] = ((get_pixel(x, y).a > 0) && 
                (get_pixel(x,y) != rim));
        }
    }

    for (unsigned int y = 0; y < m_height; y++)
    {
        for (unsigned int x = 0; x < m_width; x++)
        {
            if (flags[x + y * m_width])
                continue;

            if (x > 0 && flags[(x-1) + y * m_width] ||
                y > 0 && flags[x + (y-1) * m_width] ||
                x < m_width - 1 && flags[(x+1) + y * m_width] ||
                y < m_height - 1 && flags[x + (y+1) * m_width])
            {
                get_pixel(x,y) = rim;
            }
        }
    }

    delete[] flags;
}

void tile::corpsify()
{
    // TODO enne - different wound colours for different bloods
    // TODO enne - use blood variations
    tile_colour red_blood(0, 0, 32, 255);

    int separate_x = 3;
    int separate_y = 4;

    // force all corpses into 32x32, even if bigger.
    corpsify(32, 32, separate_x, separate_y, red_blood);
}

static int corpse_cut_height(int x, int width, int height)
{
    unsigned int cy = height / 2 + 2;

    // Make the cut bend upwards in the middle
    int limit1 = width / 8;
    int limit2 = width / 3;

    if (x < limit1 || x >= width - limit1)
        cy += 2;
    else if (x < limit2 || x >= width - limit2)
        cy += 1;

    return cy;
}

// Adapted from rltiles' cp_monst_32 and then ruthlessly rewritten for clarity.
// rltiles can be found at http://rltiles.sourceforge.net
void tile::corpsify(int corpse_width, int corpse_height, 
    int cut_separate, int cut_height, const tile_colour &wound)
{
    int wound_height = std::min(2, cut_height);

    // Make a temporary backup
    tile orig(*this);
   
    resize(corpse_width, corpse_height);
    fill(tile_colour::transparent);

    // Track which pixels have been written to with valid image data
    bool *flags = new bool[corpse_width * corpse_height];
    memset(flags, 0, corpse_width * corpse_height * sizeof(bool));

#define flags(x,y) (flags[((x) + (y) * corpse_width)])

    // Find extents
    int xmin, ymin, bbwidth, bbheight;
    orig.get_bounding_box(xmin, ymin, bbwidth, bbheight);

    int xmax = xmin + bbwidth - 1;
    int ymax = ymin + bbheight - 1;
    int centerx = (xmax + xmin) / 2;
    int centery = (ymax + ymin) / 2;

    // Use maximum scale in case aspect ratios differ.
    float width_scale = (float)m_width / (float)corpse_width;
    float height_scale = (float)m_height / (float)corpse_height;
    float image_scale = std::max(width_scale, height_scale);

    // Amount to scale height by to fake a projection.
    float height_proj = 2.0f;

    for (int y = 0; y < corpse_height; y++)
    {
        for (int x = 0; x < corpse_width; x++)
        {
            int cy = corpse_cut_height(x, corpse_width, corpse_height);
            if (y > cy - cut_height && y <= cy)
                continue;

            // map new center to old center, including image scale
            int x1 = (int)((x - m_width/2)*image_scale) + centerx;
            int y1 = (int)((y - m_height/2)*height_proj*image_scale) + centery;

            if (y >= cy)
            {
                x1 -= cut_separate;
                y1 -= cut_height / 2;
            }
            else
            {
                x1 += cut_separate;
                y1 += cut_height / 2 + cut_height % 2;
            }

            if (x1 < 0 || x1 >= m_width || y1 < 0 || y1 >= m_height)
                continue;

            tile_colour &mapped = orig.get_pixel(x1, y1);

            // ignore rims, shadows, and transparent pixels.
            if (mapped == tile_colour::black ||
                mapped == tile_colour::transparent)
            {
                continue;
            }

            get_pixel(x,y) = mapped;
            flags(x, y) = true;
        }
    }

    // Add some colour to the cut wound
    for (int x = 0; x < corpse_width; x++)
    {
        unsigned int cy = corpse_cut_height(x, corpse_width, corpse_height);
        if (flags(x, cy-cut_height))
        {
            unsigned int start = cy - cut_height + 1;
            for (int y = start; y < start + wound_height; y++)
            {
                get_pixel(x, y) = wound;
            }
        }
    }

    // Add diagonal shadowing...
    for (int y = 1; y < corpse_height; y++)
    {
        for (int x = 1; x < corpse_width; x++)
        {
            if (!flags(x, y) && flags(x-1, y-1) &&
                get_pixel(x,y) == tile_colour::transparent)
            {
                get_pixel(x, y) = tile_colour::black;
            }
        }
    }

    // Extend shadow...
    for (int y = 3; y < corpse_height; y++)
    {
        for (int x = 3; x < corpse_width; x++)
        {
            // Extend shadow if there are two real pixels along
            // the diagonal.  Also, don't extend if the top or
            // left pixel is not filled in.  This prevents lone
            // shadow pixels only connected via diagonals.

            if (get_pixel(x-1,y-1) == tile_colour::black &&
                flags(x-2, y-2) && flags(x-3, y-3) &&
                get_pixel(x-1, y) == tile_colour::black &&
                get_pixel(x, y-1) == tile_colour::black)
            {
                get_pixel(x, y) = tile_colour::black;
            }
        }
    }

    delete[] flags;
}

void tile::copy(const tile &img)
{
    unload();
    
    m_width = img.m_width;
    m_height = img.m_height;
    m_filename = img.m_filename;
    m_pixels = new tile_colour[m_width * m_height];
    m_shrink = img.m_shrink;
    memcpy(m_pixels, img.m_pixels, m_width * m_height * sizeof(tile_colour));

    // enum explicitly not copied
    m_enumname.clear();
}

bool tile::compose(const tile &img)
{
    if (!valid())
    {
        fprintf(stderr, "Error: can't compose onto an unloaded image.\n");
        return false;
    }

    if (!img.valid())
    {
        fprintf(stderr, "Error: can't compose from an unloaded image.\n");
        return false;
    }

    if (m_width != img.m_width || m_height != img.m_height)
    {
        fprintf(stderr, "Error: can't compose with mismatched dimensions. "
            "(%d, %d) onto (%d, %d)\n", img.m_width, img.m_height, m_width,
            m_height);
        return false;
    }

    for (unsigned int i = 0; i < m_width * m_height; i += 1)
    {
        const tile_colour *src = &img.m_pixels[i];
        tile_colour *dest = &m_pixels[i];

        dest->r = (src->r * src->a + dest->r * (255 - src->a)) / 255;
        dest->g = (src->g * src->a + dest->g * (255 - src->a)) / 255;
        dest->b = (src->b * src->a + dest->b * (255 - src->a)) / 255;
        dest->a = (src->a * 255    + dest->a * (255 - src->a)) / 255;
    }

    return true;
}

bool tile::load(const std::string &filename)
{
    m_filename = filename;

    if (m_pixels)
    {
        unload();
    }

    SDL_Surface *img = IMG_Load(filename.c_str());
    if (!img)
    {
        return false;
    }

    m_width = img->w;
    m_height = img->h;

    // blow out all formats to non-palettised RGBA.
    m_pixels = new tile_colour[m_width * m_height];
    unsigned int bpp = img->format->BytesPerPixel;
    if (bpp == 1)
    {
        SDL_Palette *pal = img->format->palette;
        assert(pal);
        assert(pal->colors);

        int src = 0;
        int dest = 0;
        for (int y = 0; y < img->h; y++)
        {
            for (int x = 0; x < img->w; x++)
            {
                int index = ((unsigned char*)img->pixels)[src++];
                m_pixels[dest].r = pal->colors[index].r;
                m_pixels[dest].g = pal->colors[index].g;
                m_pixels[dest].b = pal->colors[index].b;
                m_pixels[dest].a = (index != img->format->colorkey ? 255 : 0);
                dest++;
            }
        }
    }
    else
    {
        SDL_LockSurface(img);

        int dest = 0; 
        for (int y = 0; y < img->h; y++)
        {
            for (int x = 0; x < img->w; x++)
            {
                unsigned char *p = (unsigned char*)img->pixels 
                                   + y*img->pitch + x*bpp;

                unsigned int pixel;
                switch (img->format->BytesPerPixel)
                {
                case 1:
                    pixel = *p;
                    break;
                case 2:
                    pixel = *(unsigned short*)p;
                    break;
                case 3:
                    if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                        pixel = p[0] << 16 | p[1] << 8 | p[2];
                    else
                        pixel = p[0] | p[1] << 8 | p[2] << 16;
                    break;
                case 4:
                    pixel = *(unsigned int*)p;
                    break;
                default:
                    assert(!"Invalid bpp");
                    SDL_UnlockSurface(img);
                    SDL_FreeSurface(img);
                    return false;
                }

                SDL_GetRGBA(pixel, img->format, &m_pixels[dest].r,
                            &m_pixels[dest].g, &m_pixels[dest].b,
                            &m_pixels[dest].a);
                dest++;
            }
        }

        SDL_UnlockSurface(img);
    }

    SDL_FreeSurface(img);

    replace_colour(tile_colour::background, tile_colour::transparent);

    return true;
}

void tile::fill(const tile_colour &fill)
{
    for (int y = 0; y < m_height; y++)
    {
        for (int x = 0; x < m_width; x++)
        {
            get_pixel(x, y) = fill;
        }
    }
}

void tile::replace_colour(tile_colour &find, tile_colour &replace)
{
    for (int y = 0; y < m_height; y++)
    {
        for (int x = 0; x < m_width; x++)
        {
            tile_colour &p = get_pixel(x, y);
            if (p == find)
                p = replace;
        }
    }
}

tile_colour &tile::get_pixel(unsigned int x, unsigned int y)
{
    assert(m_pixels && x < m_width && y < m_height);
    return m_pixels[x + y * m_width];
}

void tile::get_bounding_box(int &x0, int &y0, int &width, int &height)
{
    if (!valid())
    {
        x0 = y0 = width = height = 0;
        return;
    }

    x0 = y0 = 0;
    unsigned int x1 = m_width - 1;
    unsigned int y1 = m_height - 1;
    while (x0 <= x1)
    {
        bool found = false;
        for (unsigned int y = y0; !found && y < y1; y++)
        {
            found |= (get_pixel(x0, y).a > 0);
        }
        if (found)
            break;
        x0++;
    }

    while (x0 <= x1)
    {
        bool found = false;
        for (unsigned int y = y0; !found && y < y1; y++)
        {
            found |= (get_pixel(x1, y).a > 0);
        }
        if (found)
            break;
        x1--;
    }

    while (y0 <= y1)
    {
        bool found = false;
        for (unsigned int x = x0; !found && x < x1; x++)
        {
            found |= (get_pixel(x, y0).a > 0);
        }
        if (found)
            break;
        y0++;
    }

    while (y0 <= y1)
    {
        bool found = false;
        for (unsigned int x = x0; !found && x < x1; x++)
        {
            found |= (get_pixel(x, y1).a > 0);
        }
        if (found)
            break;
        y1--;
    }

    width = x1 - x0 + 1;
    height = y1 - y0 + 1;
}
