#include "tile_page.h"
#include "tile_colour.h"
#include <stdio.h>
#include <string.h>
#include <cassert>
#include "tile.h"
#include <algorithm>

tile_page::tile_page() : m_width(1024), m_height(0)
{
}

tile_page::~tile_page()
{
    for (unsigned int i = 0; i < m_tiles.size(); i++)
        delete m_tiles[i];

    m_tiles.clear();
    m_counts.clear();
    m_probs.clear();
    m_domino.clear();
    m_base_tiles.clear();
}

bool tile_page::place_images()
{
    // locate all the tiles on the page, so we can determine its size
    // and the tex coords.

    m_offsets.clear();
    m_texcoords.clear();

    int ymin, ycur, ymax;
    int xmin, xcur, xmax;
    ymin = ycur = ymax = xmin = xcur = xmax = 0;

    for (unsigned int i = 0; i < m_tiles.size(); i++)
    {
        int ofs_x, ofs_y, tilew, tileh;
        if (m_tiles[i]->shrink())
            m_tiles[i]->get_bounding_box(ofs_x, ofs_y, tilew, tileh);
        else
        {
            ofs_x = 0;
            ofs_y = 0;
            tilew = m_tiles[i]->width();
            tileh = m_tiles[i]->height();
        }

        m_offsets.push_back(ofs_x);
        m_offsets.push_back(ofs_y);
        m_offsets.push_back(m_tiles[i]->width());
        m_offsets.push_back(m_tiles[i]->height());

        if (xcur + tilew > m_width)
        {
            ycur = ymin = ymax;
            xcur = xmin = xmax = 0;
        }

        if (tileh + ycur >= ymax)
        {
            if (ycur != ymin)
            {
                ycur = ymin;
                xcur = xmax;
                xmin = xmax = xcur;
            }

            if (xcur + tilew > m_width)
            {
                ycur = ymin = ymax;
                xcur = xmin = xmax = 0;
            }

            if (ycur == ymin)
                ymax = max(ymin + (int)tileh, ymax);
        }

        m_height = ymax;

        m_texcoords.push_back(xcur);
        m_texcoords.push_back(ycur);
        m_texcoords.push_back(xcur + tilew);
        m_texcoords.push_back(ycur + tileh);

        // Only add downwards, stretching out xmax as we go.
        xmax = max(xmax, xcur + (int)tilew);
        xcur = xmin;
        ycur += tileh;
    }

    return true;
}

int tile_page::find(const string &enumname) const
{
    for (size_t i = 0; i < m_tiles.size(); ++i)
    {
        for (int c = 0; c < m_tiles[i]->enumcount(); ++c)
        {
            if (m_tiles[i]->enumname(c) == enumname)
                return i;
        }
    }

    return -1;
}

bool tile_page::add_synonym(int idx, const string &syn)
{
    if (idx < 0 || idx >= (int)m_tiles.size())
        return false;

    m_tiles[idx]->add_enumname(syn);
    return true;
}

bool tile_page::add_synonym(const string &enumname, const string &syn)
{
    int idx = find(enumname);
    if (idx == -1)
        return false;

    m_tiles[idx]->add_enumname(syn);

    return true;
}

bool tile_page::write_image(const char *filename)
{
#ifdef USE_TILE
    if (m_width * m_height <= 0)
    {
        fprintf(stderr, "Error: failed to write image. No images placed?\n");
        return false;
    }

    tile_colour *pixels = new tile_colour[m_width * m_height];
    memset(pixels, 0, m_width * m_height * sizeof(tile_colour));

    for (unsigned int i = 0; i < m_tiles.size(); i++)
    {
        int sx = m_texcoords[i*4];
        int sy = m_texcoords[i*4+1];
        int ex = m_texcoords[i*4+2];
        int ey = m_texcoords[i*4+3];
        int wx = ex - sx;
        int wy = ey - sy;
        int ofs_x = m_offsets[i*4];
        int ofs_y = m_offsets[i*4+1];

        for (int y = 0; y < wy; y++)
            for (int x = 0; x < wx; x++)
            {
                tile_colour &dest = pixels[(sx+x) + (sy+y)*m_width];
                tile_colour &src = m_tiles[i]->get_pixel(ofs_x+x, ofs_y+y);
                dest = src;

                // Clear colour from transparent areas.
                if (!dest.a)
                    dest = tile_colour::transparent;
            }
    }

    bool success = write_png(filename, pixels, m_width, m_height);
    delete[] pixels;
    return success;
#else
    return true;
#endif
}

void tile_page::add_variation(int var_idx, int base_idx, int colour)
{
    assert(var_idx < (2 << 15));
    assert(base_idx < (2 << 15));

    m_tiles[base_idx]->add_variation(colour, var_idx);
}
