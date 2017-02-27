#pragma once

#include <string>
#include <vector>

#include "glwrapper.h"

class FontWrapper;
class formatted_string;
class GenericTexture;
class TilesTexture;

class VertBuffer
{
public:
    VertBuffer(bool texture, bool colour, const GenericTexture *tex = nullptr,
               drawing_modes prim = GLW_RECTANGLE);
    ~VertBuffer();

    // Rendering
    // void draw(const GLW_3VF *pt = nullptr, const GLW_3VF *ps = nullptr) const;
    void draw() const;

    // State query
    const GLState& state() const { return m_state; }
    const GenericTexture* current_texture() const { return m_tex; }
    unsigned int size() const;

    // State Manipulation
    void add_primitive(const GLWPrim &rect);
    void clear();

    // Note: this could invalidate previous additions if they were
    // from a different texture.
    // But we leave it here as a convenience and because it is required to set
    // textures that load late. Specifically ones that rely on the item
    // description tables -- Ixtli
    void set_tex(const GenericTexture *tex);

protected:
    const GenericTexture *m_tex;
    GLShapeBuffer *m_vert_buf;
    GLState m_state;
    drawing_modes m_prim;
    bool m_colour_verts;
    bool m_texture_verts;
};

class FontBuffer : public VertBuffer
{
public:
    FontBuffer(FontWrapper *font);
    void add(const formatted_string &fs, float x, float y);
    void add(const string &s, const VColour &col, float x, float y);
protected:
    FontWrapper *m_font;
};

class TileBuffer : public VertBuffer
{
public:
    TileBuffer(const TilesTexture *tex = nullptr);

    void add_unscaled(tileidx_t idx, float x, float y, int ymax = TILE_Y,
                      float scale = 1.0f);
    void add(tileidx_t idx, int x, int y,
             int ox = 0, int oy = 0, bool centre = true, int ymax = -1,
             float tile_x = TILE_X, float tile_y = TILE_Y);
};

class ColouredTileBuffer : public VertBuffer
{
public:
    ColouredTileBuffer(const TilesTexture *tex = nullptr);

    void add(tileidx_t idx, int x, int y, int z,
             int ox, int oy, int ymin, int ymax,
             int alpha_top, int alpha_bottom);
};

// Helper class for tiles submerged in water.
class SubmergedTileBuffer
{
public:
    SubmergedTileBuffer(const TilesTexture *tex, int water_level);

    void add(tileidx_t idx, int x, int y, int z = 0, bool submerged = false,
             bool ghost = false, int ox = 0, int oy = 0, int ymax = -1);
    void add_alpha(tileidx_t idx, int x, int y, int z,
                   int ox, int oy, int ymax, int alpha);
    void add_masked(tileidx_t idx, int x, int y, int z,
                    int ox, int oy, int ymax,
                    int alpha_above, int alpha_below, int water_level = 0);

    void draw() const;
    void clear();

protected:
    int m_water_level;

    ColouredTileBuffer m_below_water;
    ColouredTileBuffer m_above_water;
};

class ShapeBuffer : public VertBuffer
{
public:
    ShapeBuffer();
    void add(float sx, float sy, float ex, float ey, const VColour &c);
};

class LineBuffer : public VertBuffer
{
public:
    LineBuffer();
    void add(float sx, float sy, float ex, float ey, const VColour &c);
    void add_square(float sx, float sy, float ex, float ey, const VColour &c);
};

