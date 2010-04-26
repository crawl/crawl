/*
 *  File:       tilebuf.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#ifndef TILEBUF_H
#define TILEBUF_H

#include "tiles.h"
#include "glwrapper.h"

#include <string>
#include <vector>

class FontWrapper;
class formatted_string;
class GenericTexture;
class TilesTexture;

struct PTCVert
{
    float pos_x;
    float pos_y;
    float tex_x;
    float tex_y;
    VColour col;
};

struct P3TCVert
{
    float pos_x;
    float pos_y;
    float pos_z;
    float tex_x;
    float tex_y;
    VColour col;
};

struct PTVert
{
    float pos_x;
    float pos_y;
    float tex_x;
    float tex_y;
};

struct PCVert
{
    float pos_x;
    float pos_y;
    VColour col;
};

// V: vertex data
template<class V>
class VertBuffer : public std::vector<V>
{
public:
    typedef V Vert;

    VertBuffer(const GenericTexture *tex, drawing_modes prim);

    // Vertices are fat, so to avoid an extra copy of all the data members,
    // pre-construct the vertex and return a reference to it.
    V& get_next();
    void draw(GLW_3VF *pt, GLW_3VF *ps) const;

    GLState &state() { return m_state; }

protected:
    void init_state();

    const GenericTexture *m_tex;
    drawing_modes m_prim;
    GLState m_state;
};

class FontBuffer : public VertBuffer<PTCVert>
{
public:
    FontBuffer(FontWrapper *font);
    void add(const formatted_string &fs, float x, float y);
    void add(const std::string &s, const VColour &col, float x, float y);
protected:
    FontWrapper *m_font;
};

class TileBuffer : public VertBuffer<PTVert>
{
public:
    TileBuffer(const TilesTexture *tex = NULL);

    void add_unscaled(int idx, float x, float y, int ymax = TILE_Y);
    void add(int idx, int x, int y, int ox = 0, int oy = 0, bool centre = true, int ymax = -1);


    // Note: this could invalidate previous additions if they were
    // from a different texture.
    void set_tex(const TilesTexture *tex);
};

class ColouredTileBuffer : public VertBuffer<P3TCVert>
{
public:
    ColouredTileBuffer(const TilesTexture *tex = NULL);

    void add(int idx, int x, int y, int z,
             int ox, int oy, int ymin, int ymax,
             int alpha_top, int alpha_bottom);
};

// Helper class for tiles submerged in water.
class SubmergedTileBuffer
{
public:
    // mask_idx is the tile index in tex of the mask texture
    // It should be opaque white for water and fully transparent above.
    //
    // above_max is the maximum height (from the top of the tile) where
    // there are still pixels above water.
    //
    // below_min is the minimum height (from the top of the tile) where
    // there are still pixels below water.
    //
    // All heights are from 0 (top of the tile) to TILE_Y-1 (bottom of the tile)
    SubmergedTileBuffer(const TilesTexture *tex,
                        int mask_idx, int above_max, int below_min,
                        bool better_transparency);

    void add(int idx, int x, int y, int z = 0, bool submerged = false,
             bool ghost = false, int ox = 0, int oy = 0, int ymax = -1);

    void draw() const;
    void clear();

protected:
    int m_mask_idx;
    int m_above_max;
    int m_below_min;

    int m_max_z;
    bool m_better_trans;

    ColouredTileBuffer m_below_water;
    ColouredTileBuffer m_mask;
    ColouredTileBuffer m_above_water;
};

class ShapeBuffer : public VertBuffer<PCVert>
{
public:
    ShapeBuffer();
    void add(float sx, float sy, float ex, float ey, const VColour &c);
};

class LineBuffer : public VertBuffer<PCVert>
{
public:
    LineBuffer();
    void add(float sx, float sy, float ex, float ey, const VColour &c);
    void add_square(float sx, float sy, float ex, float ey, const VColour &c);
};

/////////////////////////////////////////////////////////////////////////////
// template implementation

template<class V>
inline VertBuffer<V>::VertBuffer(const GenericTexture *tex, drawing_modes prim) :
    m_tex(tex), m_prim(prim)
{
    init_state();
}

template<class V>
inline V& VertBuffer<V>::get_next()
{
    size_t last = std::vector<V>::size();
    std::vector<V>::resize(last + 1);
    return ((*this)[last]);
}

#endif
