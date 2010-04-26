/*
 *  File:       tilebuf.cc
 *  Summary:    Vertex buffer implementaions
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tilebuf.h"
#include "tilefont.h"

/////////////////////////////////////////////////////////////////////////////
// VColour

VColour VColour::white(255, 255, 255, 255);
VColour VColour::black(0, 0, 0, 255);
VColour VColour::transparent(0, 0, 0, 0);

/////////////////////////////////////////////////////////////////////////////
// VertBuffer

VertBuffer::VertBuffer(bool texture, bool colour, const GenericTexture *tex,
        drawing_modes prim, bool flush) :
    m_tex(tex),
    m_prim(prim),
    flush_verts(flush),
    colour_verts(colour),
    texture_verts(texture)
{
    vert_buff = GLShapeBuffer::create(texture, colour_verts, m_prim);
    ASSERT(vert_buff);

    // Set default gl state
    m_state.array_vertex = true;
    m_state.blend = true; // This seems to be a safe default here
    if(texture_verts)
    {
        m_state.array_texcoord = true;
        m_state.texture = true;
    }
    if(colour_verts)
    {
        m_state.array_colour = true;
    }
}

void VertBuffer::draw(GLW_3VF *pt, GLW_3VF *ps) const
{
    // Note:    This check is here because MenuRegion::render will try and
    //          render m_tile_buf[2] which, before the item description table
    //          is created, will be bound to a texture that has not been loaded.
    //          It never crashes, though, because m_tile_buf[2] will always be
    //          empty (no pushed verts) before you start a game. -- ixtli
    if (size() == 0)
        return;

    // Set state
    glmanager->set(m_state);

    // Handle texture
    if(texture_verts)
    {
        ASSERT(m_tex);
        m_tex->bind();
    }

    // Draw
    vert_buff->draw(pt, ps, flush_verts);
}

void VertBuffer::set_state(const GLState &s)
{
    // At this point, don't allow toggling of prim_type, texturing, or colouring
    // as GLVertexBuffer doesn't support it
    m_state.blend = s.blend;
    // This needs more testing
    //m_state.texture = state.texture; 
    m_state.depthtest = s.depthtest;
    m_state.alphatest = s.alphatest;
    m_state.alpharef = s.alpharef;
}

void VertBuffer::clear()
{
    vert_buff->clear();
}

void VertBuffer::push(const GLWRect &rect)
{
    vert_buff->push(rect);
}

unsigned int VertBuffer::size() const
{
    return vert_buff->size();
}

void VertBuffer::set_tex(const GenericTexture *new_tex)
{
    ASSERT(texture_verts);
    ASSERT(new_tex);
    m_tex = new_tex;
}

VertBuffer::~VertBuffer()
{
    if(vert_buff)
        delete vert_buff;
}

/////////////////////////////////////////////////////////////////////////////
// FontBuffer

FontBuffer::FontBuffer(FontWrapper *font) :
    VertBuffer(true, true, font->font_tex()), m_font(font)
{
    m_state.array_colour = true;

    ASSERT(m_font);
    ASSERT(m_tex);
}

void FontBuffer::add(const formatted_string &fs, float x, float y)
{
    m_font->store(*this, x, y, fs);
}

void FontBuffer::add(const std::string &s, const VColour &col, float x, float y)
{
    m_font->store(*this, x, y, s, col);
}

/////////////////////////////////////////////////////////////////////////////
// TileBuffer

TileBuffer::TileBuffer(const TilesTexture *t) : VertBuffer(true, false, t)
{
}

void TileBuffer::add_unscaled(int idx, float x, float y, int ymax)
{
    float pos_sx = x;
    float pos_sy = y;
    float pos_ex, pos_ey, tex_sx, tex_sy, tex_ex, tex_ey;
    TilesTexture *tex = (TilesTexture *)m_tex;
    bool drawn = tex->get_coords(idx, 0, 0,
                                 pos_sx, pos_sy, pos_ex, pos_ey,
                                 tex_sx, tex_sy, tex_ex, tex_ey,
                                 true, -1, ymax, 1.0f, 1.0f);

    if (!drawn)
        return;

    // Construct rectangle
    GLWRect rect(pos_sx, pos_sy, pos_ex, pos_ey);
    rect.set_tex(tex_sx, tex_sy, tex_ex, tex_ey);

    // Push it
    push(rect);
}

void TileBuffer::add(int idx, int x, int y, int ox, int oy, bool centre, int ymax)
{
    float pos_sx = x;
    float pos_sy = y;
    float pos_ex, pos_ey, tex_sx, tex_sy, tex_ex, tex_ey;
    TilesTexture *tex = (TilesTexture *)m_tex;
    bool drawn = tex->get_coords(idx, ox, oy,
                                 pos_sx, pos_sy, pos_ex, pos_ey,
                                 tex_sx, tex_sy, tex_ex, tex_ey,
                                 centre, -1, ymax, TILE_X, TILE_Y);

    if (!drawn)
        return;

    // Construct rectangle
    GLWRect rect(pos_sx, pos_sy, pos_ex, pos_ey);
    rect.set_tex(tex_sx, tex_sy, tex_ex, tex_ey);

    // Push it
    push(rect);
}

/////////////////////////////////////////////////////////////////////////////
// ColouredTileBuffer

ColouredTileBuffer::ColouredTileBuffer(const TilesTexture *t) :
    VertBuffer(true, true, t)
{
    m_state.array_colour   = true;
    m_state.depthtest      = true;
    m_state.alphatest      = true;
    m_state.alpharef       = 0;
}

static unsigned char _get_alpha(float lerp, int alpha_top, int alpha_bottom)
{
    if (lerp < 0.0f)
        lerp = 0.0f;
    if (lerp > 1.0f)
        lerp = 1.0f;

    int ret = alpha_top * (1.0f - lerp) + alpha_bottom * lerp;

    ret = std::min(std::max(0, ret), 255);
    return (static_cast<unsigned char>(ret));
}

void ColouredTileBuffer::add(int idx, int x, int y, int z,
                             int ox,  int oy, int ymin, int ymax,
                             int alpha_top, int alpha_bottom)
{
    float pos_sx = x;
    float pos_sy = y;
    float pos_ex, pos_ey, tex_sx, tex_sy, tex_ex, tex_ey;
    TilesTexture *tex = (TilesTexture *)m_tex;
    bool drawn = tex->get_coords(idx, ox, oy,
                                 pos_sx, pos_sy, pos_ex, pos_ey,
                                 tex_sx, tex_sy, tex_ex, tex_ey,
                                 true, -1, ymax);

    if (!drawn)
        return;

    float lerp_s = (pos_sy - y);
    float lerp_e = (pos_ey - y);

    // Need to calculate the alpha at the top and bottom.  As the quad
    // being drawn may not actually start on the grid boundary, we need
    // to figure out what alpha value its vertices should use.
    unsigned char alpha_s = _get_alpha(lerp_s, alpha_top, alpha_bottom);
    unsigned char alpha_e = _get_alpha(lerp_e, alpha_top, alpha_bottom);

    VColour col_sy(255, 255, 255, alpha_s);
    VColour col_ey(255, 255, 255, alpha_e);

    float pos_z = (float)z;

    // Construct rectangle
    GLWRect rect(pos_sx, pos_sy, pos_ex, pos_ey, pos_z);
    rect.set_tex(tex_sx, tex_sy, tex_ex, tex_ey);
    rect.set_col(&col_sy, &col_sy, &col_ey, &col_ey);

    // Push it
    push(rect);
}

/////////////////////////////////////////////////////////////////////////////
// SubmergedTileBuffer


SubmergedTileBuffer::SubmergedTileBuffer(const TilesTexture *tex,
                                         int mask_idx, int above_max,
                                         int below_min,
                                         bool better_trans) :
    m_mask_idx(mask_idx),
    m_above_max(above_max),
    m_below_min(below_min),
    m_max_z(-1000),
    m_better_trans(better_trans),
    m_below_water(tex),
    m_mask(tex),
    m_above_water(tex)
{
    // Adjust the state of the mask buffer so that we can use it to mask out
    // the bottom half of the tile when drawing the character a second time.
    // See the draw() function for more details.
    GLState new_state;
    new_state.set( m_mask.state() );
    new_state.blend = true;
    new_state.depthtest = true;
    new_state.alphatest = true;
    new_state.alpharef = 255;
    m_mask.set_state(new_state);
}

void SubmergedTileBuffer::add(int idx, int x, int y, int z, bool submerged,
                              bool ghost, int ox, int oy, int ymax)
{
    // Keep track of the maximum z.  We'll use this for drawing the mask
    // later, to ensure it is closer than all other tiles drawn.
    m_max_z = std::max(m_max_z, z);

    // Arbitrary alpha values for the top and bottom.
    int alpha_top = ghost ? 100 : 255;
    int alpha_bottom = ghost ? 0 : 40;

    if (submerged)
    {
        if (m_better_trans)
        {
            m_below_water.add(idx, x, y, z, ox, oy, m_below_min, ymax,
                              alpha_top, alpha_bottom);

            m_mask.add(m_mask_idx, x, y, 0, 0, 0, -1, -1, 255, 255);
            int above_max = (ymax == -1) ? m_above_max
                                         : std::min(ymax, m_above_max);
            m_above_water.add(idx, x, y, z, ox, oy, -1, above_max,
                              alpha_top, alpha_top);
        }
        else
        {
            int water_level = (m_above_max + m_below_min) / 2;
            m_below_water.add(idx, x, y, z, ox, oy, water_level, ymax,
                              alpha_top, alpha_bottom);
            m_above_water.add(idx, x, y, z, ox, oy, -1, water_level,
                              alpha_top, alpha_top);
        }
    }
    else
    {
        // Unsubmerged tiles don't need a mask.
        m_above_water.add(idx, x, y, z, ox, oy, -1, ymax, alpha_top, alpha_top);
    }
}

void SubmergedTileBuffer::draw() const
{
    if (m_better_trans)
    {
        // First, draw anything below water.  Alpha blending is turned on,
        // so these tiles will blend with anything previously drawn.
        m_below_water.draw();

        // Second, draw all of the mask tiles.  The mask is white
        // (255,255,255,255) above water and transparent (0,0,0,0) below water.
        //
        // Alpha testing is turned on with a ref value of 255, so none of the
        // white pixels will be drawn because they will culled by alpha
        // testing.  The transparent pixels below water will be "drawn", but
        // because they are fully transparent, they will not affect any colours
        // on screen.  Instead, they will will set the z-buffer to a z-value
        // (depth) that is greater than the closest depth in either the below
        // or above buffers.  Because all of the above water tiles are at lower
        // z-values than this maximum, they will not draw over these
        // transparent pixels, effectively masking them out. (My kingdom for a
        // shader.)
        GLW_3VF trans(0, 0, m_max_z + 1);
        m_mask.draw(&trans, NULL);

        // Now, draw all the above water tiles.  Some of these may draw over
        // top of part of the below water tiles, but that's fine as the mask
        // will take care of only drawing the correct parts.
        m_above_water.draw();
    }
    else
    {
        m_below_water.draw();
        m_above_water.draw();

        ASSERT(m_mask.size() == 0);
    }
}

void SubmergedTileBuffer::clear()
{
    m_below_water.clear();
    m_mask.clear();
    m_above_water.clear();
}

/////////////////////////////////////////////////////////////////////////////
// ShapeBuffer

// [enne] - GL_POINTS should probably be used here, but there's (apparently)
// a bug in the OpenGL driver that I'm using and it doesn't respect
// glPointSize unless GL_SMOOTH_POINTS is on.  GL_SMOOTH_POINTS is
// *terrible* for performance if it has to fall back on software rendering,
// so instead we'll just make quads.
ShapeBuffer::ShapeBuffer() : VertBuffer(false, true)
{
    m_state.array_colour = true;
}

void ShapeBuffer::add(float pos_sx, float pos_sy, float pos_ex, float pos_ey,
                      const VColour &col)
{
    // Construct rectangle
    GLWRect rect(pos_sx, pos_sy, pos_ex, pos_ey);
    rect.set_col(&col, &col, &col, &col);

    // Push it
    push(rect);
}

/////////////////////////////////////////////////////////////////////////////
// LineBuffer

LineBuffer::LineBuffer() : VertBuffer(false, true, NULL, GLW_LINES)
{
    m_state.array_colour = true;
}

void LineBuffer::add(float pos_sx, float pos_sy, float pos_ex, float pos_ey,
                     const VColour &col)
{
    // Construct rectangle
    GLWRect rect(pos_sx, pos_sy, pos_ex, pos_ey);
    rect.set_col(&col, &col);

    // Push it
    push(rect);
}

void LineBuffer::add_square(float sx, float sy, float ex, float ey,
                            const VColour &col)
{
    add(sx, sy, ex, sy, col);
    add(ex, sy, ex, ey, col);
    add(ex, ey, sx, ey, col);
    add(sx, ey, sx, sy, col);
}

#endif
