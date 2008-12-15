#ifndef TILEDEF_DEFINES_H
#define TILEDEF_DEFINES_H

#include <cassert>

class tile_info
{
public:
    tile_info(int _width, int _height, int _offset_x, int _offset_y,
        int _sx, int _sy, int _ex, int _ey) :
        width(_width),
        height(_height),
        offset_x(_offset_x),
        offset_y(_offset_y),
        sx(_sx),
        sy(_sy),
        ex(_ex),
        ey(_ey)
    {
        // verify all params are larger than zero and fit in storage
        assert(width == _width);
        assert(height == _height);
        assert(offset_x == _offset_x);
        assert(offset_y == _offset_y);
        assert(sx == _sx);
        assert(sy == _sy);
        assert(ex == _ex);
        assert(ey == _ey);
    }

    // size of the original tile
    unsigned char width;
    unsigned char height;

    // offset to draw this image at (texcoords may be smaller than orig image)
    unsigned char offset_x;
    unsigned char offset_y;

    // texcoords in the tile page
    unsigned short sx;
    unsigned short sy;
    unsigned short ex;
    unsigned short ey;
};

typedef unsigned int (tile_count_func)(unsigned int);
typedef const char *(tile_name_func)(unsigned int);
typedef tile_info &(tile_info_func)(unsigned int);

#endif
