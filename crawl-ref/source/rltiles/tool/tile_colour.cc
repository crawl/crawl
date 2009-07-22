#include "tile_colour.h"
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <png.h>

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

    return (*this);
}

bool write_png(const char *filename, tile_colour *pixels,
               unsigned int width, unsigned int height)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        fprintf(stderr, "Error: Can't open file '%s' for write.\n", filename);
        return (false);
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                  NULL, NULL, NULL);
    if (!png_ptr)
        return (false);

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
        return (false);
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

    return (true);
}


