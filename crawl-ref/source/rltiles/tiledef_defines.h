#pragma once

#include <vector>

typedef uint64_t tileidx_t;

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
        ASSERT(width == _width);
        ASSERT(height == _height);
        ASSERT(offset_x == _offset_x);
        ASSERT(offset_y == _offset_y);
        ASSERT(sx == _sx);
        ASSERT(sy == _sy);
        ASSERT(ex == _ex);
        ASSERT(ey == _ey);
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

typedef unsigned int (tile_count_func)(tileidx_t);
typedef const char *(tile_name_func)(tileidx_t);
typedef tile_info &(tile_info_func)(tileidx_t);

struct tile_variation
{
    tile_variation(int i, int c) : idx(i), col(c) { }
    int idx;
    int col;

    static int cmp(tile_variation left, tile_variation right)
    {
        if (left.idx < right.idx)
            return -1;
        if (left.idx > right.idx)
            return 1;
        if (left.col < right.col)
            return -1;
        if (left.col > right.col)
            return 1;
        return 0;
    }
};

template<class F, class R>
bool binary_search(F find, pair<F, R> *arr, int num_pairs,
                   int (*cmpfnc)(F, F), R *result)
{
    int first = 0;
    int last = num_pairs - 1;

    do
    {
        int half = (last - first) / 2 + first;
        int cmp = cmpfnc(find, arr[half].first);
        if (cmp < 0)
            last = half - 1;
        else if (cmp > 0)
            first = half + 1;
        else
        {
            *result = arr[half].second;
            return true;
        }

    } while (first <= last);

    return false;
}
