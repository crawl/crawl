#include "tile_colour.h"
#include <cassert>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#ifdef USE_TILE
 #include <png.h>
#endif
using namespace std;

tile_colour tile_colour::background(71, 108, 108, 255);
tile_colour tile_colour::transparent(0, 0, 0, 0);
tile_colour tile_colour::black(0, 0, 0, 255);

bool tile_colour::operator==(const tile_colour &rhs) const
{
    return (r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a);
}

bool tile_colour::operator!=(const tile_colour &rhs) const
{
    return (r != rhs.r || g != rhs.g || b != rhs.b || a != rhs.a);
}

const tile_colour &tile_colour::operator=(const tile_colour &rhs)
{
    r = rhs.r;
    g = rhs.g;
    b = rhs.b;
    a = rhs.a;

    return *this;
}

unsigned char &tile_colour::operator[](int idx)
{
    assert(idx >= 0 && idx <= 4);
    switch (idx)
    {
    default:
    case 0: return r;
    case 1: return g;
    case 2: return b;
    case 3: return a;
    }
}

unsigned char tile_colour::operator[](int idx) const
{
    assert(idx >= 0 && idx <= 4);
    switch (idx)
    {
    default:
    case 0: return r;
    case 1: return g;
    case 2: return b;
    case 3: return a;
    }
}

int tile_colour::get_hue() const
{
    int max_rgb = get_max_rgb();
    int min_rgb = get_min_rgb();

    if (max_rgb == min_rgb)
        return 0;

    int diff = max_rgb - min_rgb;

    if (max_rgb == r)
        return ((60 * (g - b)) / diff + 360) % 360;
    else if (max_rgb == g)
        return (60 * (b - r)) / diff + 120;
    else // if (max_rgb == b)
        return (60 * (r - g)) / diff + 240;
}

int tile_colour::get_max_rgb() const
{
    int max_rgb = max(max(r, g), b);
    return max_rgb;
}

int tile_colour::get_min_rgb() const
{
    int min_rgb = min(min(r, g), b);
    return min_rgb;
}

void tile_colour::set_hue(int h)
{
    set_from_hue(h, get_min_rgb(), get_max_rgb());
}

void tile_colour::set_from_hue(int h, int min_rgb, int max_rgb)
{
    // http://en.wikipedia.org/wiki/HSL_and_HSV
    // H is passed in
    // S = diff / max or 0 if max == 0
    // V = max / 255

    int v = max_rgb;
    int s = max_rgb - min_rgb;

    float f = ((float)h / 60.0f) - (int)(h / 60);

    // When calculating P, Q, T, also convert to 0..255 range.
    int p = v - s;
    int q = v - f * s;
    int t = v - (1.0f - f) * s;

    // Sanity bounds.
    q = max(min(q, 255), 0);
    t = max(min(t, 255), 0);

    int h_idx = (h / 60) % 6;

    switch (h_idx)
    {
    default:
    case 0:
        r = static_cast<unsigned char>(v);
        g = static_cast<unsigned char>(t);
        b = static_cast<unsigned char>(p);
        break;
    case 1:
        r = static_cast<unsigned char>(q);
        g = static_cast<unsigned char>(v);
        b = static_cast<unsigned char>(p);
        break;
    case 2:
        r = static_cast<unsigned char>(p);
        g = static_cast<unsigned char>(v);
        b = static_cast<unsigned char>(t);
        break;
    case 3:
        r = static_cast<unsigned char>(p);
        g = static_cast<unsigned char>(q);
        b = static_cast<unsigned char>(v);
        break;
    case 4:
        r = static_cast<unsigned char>(t);
        g = static_cast<unsigned char>(p);
        b = static_cast<unsigned char>(v);
        break;
    case 5:
        r = static_cast<unsigned char>(v);
        g = static_cast<unsigned char>(p);
        b = static_cast<unsigned char>(q);
        break;
    }
}

void tile_colour::desaturate()
{
    set_from_hue(get_hue(), get_max_rgb(), get_max_rgb());
}

float tile_colour::get_lum() const
{
    return ((get_min_rgb() + get_max_rgb()) / (255 * 2.0f));
}

float tile_colour::get_sat() const
{
    int min_rgb = get_min_rgb();
    int max_rgb = get_max_rgb();
    int sum = min_rgb + max_rgb;

    float sat;
    if (sum == 0)
        sat = 0;
    else if (sum > 255)
        sat = (max_rgb - min_rgb) / (float)(255*2 - min_rgb - max_rgb);
    else
        sat = (max_rgb - min_rgb) / (float)(min_rgb + max_rgb);

    return sat;
}

void tile_colour::set_from_hsl(int hue, float sat, float lum)
{
    float q;
    if (lum < 0.5f)
        q = lum * (1 + sat);
    else
        q = lum + sat - (lum * sat);

    float p = 2 * lum - q;

    for (int i = 0; i < 3; ++i)
    {
        int h = hue + (1 - i) * 120;
        if (h < 0)
            h += 360;
        if (h >= 360)
            h -= 360;

        float val;

        if (h < 60)
            val = p + (q - p) * h / 60.0f;
        else if (h < 180)
            val = q;
        else if (h < 240)
            val = p + (q - p) * (4 - h / 60.0f);
        else
            val = p;

        int final = val * 255;
        final = max(0, min(255, final));
        (*this)[i] = static_cast<unsigned char>(final);
    }
}

void tile_colour::change_lum(int lum_percent)
{
    int min_rgb = get_min_rgb();
    int max_rgb = get_max_rgb();
    int hue = get_hue();

    if (min_rgb == max_rgb)
    {
        int rgb_change = lum_percent * 255 / 100;

        min_rgb += rgb_change;
        max_rgb += rgb_change;

        min_rgb = max(0, min(255, min_rgb));
        max_rgb = max(0, min(255, max_rgb));

        set_from_hue(get_hue(), min_rgb, max_rgb);
        return;
    }

    float lum_change = lum_percent / 100.0f;
    float lum = get_lum() + lum_change;

    if (lum > 1.0f)
        lum = 1.0f;
    if (lum < 0.0f)
        lum = 0.0f;

    float sat = get_sat();
    set_from_hsl(hue, sat, lum);
}

#ifdef USE_TILE
bool write_png(const char *filename, tile_colour *pixels,
               unsigned int width, unsigned int height)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        fprintf(stderr, "Error: Can't open file '%s' for write.\n", filename);
        return false;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                  NULL, NULL, NULL);
    if (!png_ptr)
        return false;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
        return false;
    }

    png_init_io(png_ptr, fp);

    int bit_depth        = 8;
    int colour_type      = PNG_COLOR_TYPE_RGB_ALPHA;
    int interlace_type   = PNG_INTERLACE_NONE;
    int compression_type = PNG_COMPRESSION_TYPE_DEFAULT;
    int filter_method    = PNG_FILTER_TYPE_DEFAULT;
    png_set_IHDR(png_ptr, info_ptr, width, height,
                 bit_depth, colour_type, interlace_type,
                 compression_type, filter_method);

    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for (unsigned int y = 0; y < height; y++)
        row_pointers[y] = (png_byte*)&pixels[y * width];

    png_set_rows(png_ptr, info_ptr, row_pointers);

    int png_transforms = PNG_TRANSFORM_IDENTITY;
    png_write_png(png_ptr, info_ptr, png_transforms, NULL);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    free(row_pointers);
    fclose(fp);

    return true;
}
#endif
