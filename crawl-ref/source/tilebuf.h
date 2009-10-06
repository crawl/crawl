/*
 *  File:       tilebuf.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#ifndef TILEBUF_H
#define TILEBUF_H

#include "tiles.h"

class FTFont;
class formatted_string;
class GenericTexture;
class TilesTexture;

#include <string>
#include <vector>

struct VColour
{
    VColour() {}
    VColour(unsigned char _r, unsigned char _g, unsigned char _b,
            unsigned char _a = 255) : r(_r), g(_g), b(_b), a(_a) {}

    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;

    static VColour white;
    static VColour black;
    static VColour transparent;
};

struct PTCVert
{
    float pos_x;
    float pos_y;
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

    VertBuffer(const GenericTexture *tex, int prim);

    // Vertices are fat, so to avoid an extra copy of all the data members,
    // pre-construct the vertex and return a reference to it.
    V& get_next();
    void draw() const;

protected:
    const GenericTexture *m_tex;
    int m_prim;
};

class FontBuffer : public VertBuffer<PTCVert>
{
public:
    FontBuffer(FTFont *font);
    void add(const formatted_string &fs, float x, float y);
    void add(const std::string &s, const VColour &col, float x, float y);
protected:
    FTFont *m_font;
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
inline VertBuffer<V>::VertBuffer(const GenericTexture *tex, int prim) :
    m_tex(tex), m_prim(prim)
{
}

template<class V>
inline V& VertBuffer<V>::get_next()
{
    size_t last = std::vector<V>::size();
    std::vector<V>::resize(last + 1);
    return ((*this)[last]);
}

#endif
