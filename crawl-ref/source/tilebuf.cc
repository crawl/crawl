/*
 *  File:       tilebuf.cc
 *  Summary:    Vertex buffer implementaions
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: ennewalker $ on $Date: 2008-10-11 $
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "tilebuf.h"
#include "tilefont.h"
#include "tilesdl.h"

#include <SDL.h>
#include <SDL_opengl.h>

/////////////////////////////////////////////////////////////////////////////
// VColour

VColour VColour::white(255, 255, 255, 255);
VColour VColour::black(0, 0, 0, 255);
VColour VColour::transparent(0, 0, 0, 0);

/////////////////////////////////////////////////////////////////////////////
// VertBuffer

static bool _valid(int num_verts, int prim)
{
    switch (prim)
    {
    case GL_QUADS:
        return (num_verts % 4 == 0);
    case GL_TRIANGLES:
        return (num_verts % 3 == 0);
    case GL_LINES:
        return (num_verts % 2 == 0);
    case GL_POINTS:
        return (true);
    default:
        return (false);
    }
}

template<>
void VertBuffer<PTVert>::draw() const
{
    if (m_verts.size() == 0)
        return;

    ASSERT(_valid(m_verts.size(), m_prim));
    ASSERT(m_tex);

    GLState state;
    state.array_vertex = true;
    state.array_texcoord = true;
    state.blend = true;
    state.texture = true;
    GLStateManager::set(state);

    m_tex->bind();
    glVertexPointer(2, GL_FLOAT, sizeof(PTVert), &m_verts[0].pos_x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(PTVert), &m_verts[0].tex_x);
    glDrawArrays(m_prim, 0, m_verts.size());
}

template<>
void VertBuffer<PCVert>::draw() const
{
    if (m_verts.size() == 0)
        return;

    ASSERT(_valid(m_verts.size(), m_prim));
    ASSERT(!m_tex);

    GLState state;
    state.array_vertex = true;
    state.array_colour = true;
    state.blend = true;
    state.texture = true;
    GLStateManager::set(state);

    glVertexPointer(2, GL_FLOAT, sizeof(PCVert), &m_verts[0].pos_x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(PCVert), &m_verts[0].col);
    glDrawArrays(m_prim, 0, m_verts.size());
}

template<>
void VertBuffer<PTCVert>::draw() const
{
    if (m_verts.size() == 0)
        return;

    ASSERT(_valid(m_verts.size(), m_prim));
    ASSERT(m_tex);

    GLState state;
    state.array_vertex = true;
    state.array_texcoord = true;
    state.array_colour = true;
    state.blend = true;
    state.texture = true;
    GLStateManager::set(state);

    m_tex->bind();
    glVertexPointer(2, GL_FLOAT, sizeof(PTCVert), &m_verts[0].pos_x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(PTCVert), &m_verts[0].tex_x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(PTCVert), &m_verts[0].col);
    glDrawArrays(m_prim, 0, m_verts.size());
}

/////////////////////////////////////////////////////////////////////////////
// FontBuffer

FontBuffer::FontBuffer(FTFont *font) :
    VertBuffer<PTCVert>(font->font_tex(), GL_QUADS), m_font(font)
{
    ASSERT(m_font);
    ASSERT(m_tex);
}

void FontBuffer::add(const formatted_string &fs, float x, float y)
{
    m_font->store(*this, x, y, fs);
    ASSERT(_valid(m_verts.size(), m_prim));
}

void FontBuffer::add(const std::string &s, const VColour &col, float x, float y)
{
    m_font->store(*this, x, y, s, col);
    ASSERT(_valid(m_verts.size(), m_prim));
}

/////////////////////////////////////////////////////////////////////////////
// TileBuffer

TileBuffer::TileBuffer(const TilesTexture *t) : VertBuffer<PTVert>(t, GL_QUADS)
{
}

void TileBuffer::add(int idx, float x, float y)
{
    const tile_info &inf = ((TilesTexture*)m_tex)->get_info(idx);

    float pos_sx = x + inf.offset_x;
    float pos_sy = y + inf.offset_y;
    float pos_ex = pos_sx + (inf.ex - inf.sx);
    float pos_ey = pos_sy + (inf.ey - inf.sy);

    float fwidth = m_tex->width();
    float fheight = m_tex->height();

    float tex_sx = inf.sx / fwidth;
    float tex_sy = inf.sy / fheight;
    float tex_ex = inf.ex / fwidth;
    float tex_ey = inf.ey / fheight;

    {
        PTVert &v = get_next();
        v.pos_x = pos_sx;
        v.pos_y = pos_sy;
        v.tex_x = tex_sx;
        v.tex_y = tex_sy;
    }
    {
        PTVert &v = get_next();
        v.pos_x = pos_sx;
        v.pos_y = pos_ey;
        v.tex_x = tex_sx;
        v.tex_y = tex_ey;
    }
    {
        PTVert &v = get_next();
        v.pos_x = pos_ex;
        v.pos_y = pos_ey;
        v.tex_x = tex_ex;
        v.tex_y = tex_ey;
    }
    {
        PTVert &v = get_next();
        v.pos_x = pos_ex;
        v.pos_y = pos_sy;
        v.tex_x = tex_ex;
        v.tex_y = tex_sy;
    }

    ASSERT(_valid(m_verts.size(), m_prim));
}

void TileBuffer::set_tex(const TilesTexture *tex)
{
    ASSERT(tex);
    m_tex = tex;
}

/////////////////////////////////////////////////////////////////////////////
// ShapeBuffer

ShapeBuffer::ShapeBuffer() : VertBuffer<PCVert>(NULL, GL_QUADS)
{
}

void ShapeBuffer::add(float pos_sx, float pos_sy, float pos_ex, float pos_ey,
                      const VColour &col)
{
    {
        PCVert &v = get_next();
        v.pos_x = pos_sx;
        v.pos_y = pos_sy;
        v.col = col;
    }
    {
        PCVert &v = get_next();
        v.pos_x = pos_sx;
        v.pos_y = pos_ey;
        v.col = col;
    }
    {
        PCVert &v = get_next();
        v.pos_x = pos_ex;
        v.pos_y = pos_ey;
        v.col = col;
    }
    {
        PCVert &v = get_next();
        v.pos_x = pos_ex;
        v.pos_y = pos_sy;
        v.col = col;
    }

    ASSERT(_valid(m_verts.size(), m_prim));
}

/////////////////////////////////////////////////////////////////////////////
// LineBuffer

LineBuffer::LineBuffer() : VertBuffer<PCVert>(NULL, GL_LINES)
{
}

void LineBuffer::add(float pos_sx, float pos_sy, float pos_ex, float pos_ey,
                     const VColour &col)
{
    {
        PCVert &v = get_next();
        v.pos_x = pos_sx;
        v.pos_y = pos_sy;
        v.col = col;
    }
    {
        PCVert &v = get_next();
        v.pos_x = pos_ex;
        v.pos_y = pos_ey;
        v.col = col;
    }

    ASSERT(_valid(m_verts.size(), m_prim));
}

void LineBuffer::add_square(float sx, float sy, float ex, float ey,
                            const VColour &col)
{
    add(sx, sy, ex, sy, col);
    add(ex, sy, ex, ey, col);
    add(ex, ey, sx, ey, col);
    add(sx, ey, sx, sy, col);
}
