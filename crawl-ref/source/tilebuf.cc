/**
 * @file
 * @brief Vertex buffer implementaions
**/

#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilebuf.h"
#include "tilefont.h"

/////////////////////////////////////////////////////////////////////////////
// VertBuffer

VertBuffer::VertBuffer(bool texture, bool colour, const GenericTexture *tex,
                       drawing_modes prim) :
    m_tex(tex),
    m_prim(prim),
    m_colour_verts(colour),
    m_texture_verts(texture)
{
    m_vert_buf = GLShapeBuffer::create(texture, m_colour_verts, m_prim);
    ASSERT(m_vert_buf);

    // Default state for most buffers.
    m_state.array_vertex = true;
    m_state.blend = true;
    if (m_texture_verts)
    {
        m_state.array_texcoord = true;
        m_state.texture = true;
    }
    if (m_colour_verts)
        m_state.array_colour = true;
}

void VertBuffer::draw() const
{
    // Note:    This check is here because MenuRegion::render will try and
    //          render m_tile_buf[2] which, before the item description table
    //          is created, will be bound to a texture that has not been loaded.
    //          It never crashes, though, because m_tile_buf[2] will always be
    //          empty (no verts) before you start a game. -- ixtli
    if (size() == 0)
        return;

    if (m_texture_verts)
    {
        ASSERT(m_tex);
        m_tex->bind();
    }

    m_vert_buf->draw(m_state);
}

void VertBuffer::clear()
{
    m_vert_buf->clear();
}

void VertBuffer::add_primitive(const GLWPrim &rect)
{
    m_vert_buf->add(rect);
}

unsigned int VertBuffer::size() const
{
    return m_vert_buf->size();
}

void VertBuffer::set_tex(const GenericTexture *new_tex)
{
    ASSERT(m_texture_verts);
    ASSERT(new_tex);
    m_tex = new_tex;
}

VertBuffer::~VertBuffer()
{
    delete m_vert_buf;
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

void FontBuffer::add(const string &s, const VColour &col, float x, float y)
{
    m_font->store(*this, x, y, s, col);
}

/////////////////////////////////////////////////////////////////////////////
// TileBuffer

TileBuffer::TileBuffer(const TilesTexture *t) : VertBuffer(true, false, t)
{
}

void TileBuffer::add_unscaled(tileidx_t idx, float x, float y, int ymax,
                              float scale)
{
    float pos_sx = x;
    float pos_sy = y;
    float pos_ex, pos_ey, tex_sx, tex_sy, tex_ex, tex_ey;
    TilesTexture *tex = (TilesTexture *)m_tex;
    bool drawn = tex->get_coords(idx, 0, 0,
                                 pos_sx, pos_sy, pos_ex, pos_ey,
                                 tex_sx, tex_sy, tex_ex, tex_ey,
                                 true, -1, ymax, 1.0f / scale, 1.0f / scale);

    if (!drawn)
        return;

    GLWPrim rect(pos_sx, pos_sy, pos_ex, pos_ey);
    rect.set_tex(tex_sx, tex_sy, tex_ex, tex_ey);
    add_primitive(rect);
}

void TileBuffer::add(tileidx_t idx, int x, int y, int ox, int oy,
                     bool centre, int ymax, float tile_x, float tile_y)
{
    float pos_sx = x;
    float pos_sy = y;
    float pos_ex, pos_ey, tex_sx, tex_sy, tex_ex, tex_ey;
    TilesTexture *tex = (TilesTexture *)m_tex;
    bool drawn = tex->get_coords(idx, ox, oy,
                                 pos_sx, pos_sy, pos_ex, pos_ey,
                                 tex_sx, tex_sy, tex_ex, tex_ey,
                                 centre, -1, ymax, tile_x, tile_y);

    if (!drawn)
        return;

    GLWPrim rect(pos_sx, pos_sy, pos_ex, pos_ey);
    rect.set_tex(tex_sx, tex_sy, tex_ex, tex_ey);
    add_primitive(rect);
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

    int ret = static_cast<int>(alpha_top * (1.0f - lerp) + alpha_bottom * lerp);

    ret = min(max(0, ret), 255);
    return static_cast<unsigned char>(ret);
}

void ColouredTileBuffer::add(tileidx_t idx, int x, int y, int z,
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

    GLWPrim rect(pos_sx, pos_sy, pos_ex, pos_ey, pos_z);
    rect.set_tex(tex_sx, tex_sy, tex_ex, tex_ey);
    rect.set_col(col_sy, col_ey);
    add_primitive(rect);
}

/////////////////////////////////////////////////////////////////////////////
// SubmergedTileBuffer


SubmergedTileBuffer::SubmergedTileBuffer(const TilesTexture *tex,
                                         int water_level) :
    m_water_level(water_level),
    m_below_water(tex),
    m_above_water(tex)
{
}

void SubmergedTileBuffer::add(tileidx_t idx, int x, int y, int z, bool submerged,
                              bool ghost, int ox, int oy, int ymax)
{
    // Arbitrary alpha values for the top and bottom.
    int alpha_top = ghost ? 100 : 255;
    int alpha_bottom = ghost ? 0 : 40;

    if (submerged)
    {
        m_below_water.add(idx, x, y, z, ox, oy, m_water_level, ymax,
                          alpha_top, alpha_bottom);
        m_above_water.add(idx, x, y, z, ox, oy, -1, m_water_level,
                          alpha_top, alpha_top);
    }
    else
        m_above_water.add(idx, x, y, z, ox, oy, -1, ymax, alpha_top, alpha_top);
}

// Adds a tile masked by the water later
void SubmergedTileBuffer::add_masked(tileidx_t idx, int x, int y, int z,
                                    int ox, int oy, int ymax,
                                    int alpha_above, int alpha_below,
                                    int water_level)
{
    if (!water_level) water_level = m_water_level;
    m_below_water.add(idx, x, y, z, ox, oy, water_level, ymax, alpha_above, alpha_below);
    m_above_water.add(idx, x, y, z, ox, oy, -1, water_level, alpha_above, alpha_above);
}

// Adds a tile with a specified alpha value
void SubmergedTileBuffer::add_alpha(tileidx_t idx, int x, int y, int z,
                                    int ox, int oy, int ymax, int alpha)
{
    m_above_water.add(idx, x, y, z, ox, oy, -1, ymax, alpha, alpha);
}

void SubmergedTileBuffer::draw() const
{
    m_below_water.draw();
    m_above_water.draw();
}

void SubmergedTileBuffer::clear()
{
    m_below_water.clear();
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
    GLWPrim rect(pos_sx, pos_sy, pos_ex, pos_ey);
    rect.set_col(col);
    add_primitive(rect);
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
    GLWPrim rect(pos_sx, pos_sy, pos_ex, pos_ey);
    rect.set_col(col);
    add_primitive(rect);
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
