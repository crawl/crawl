#pragma once

#ifdef USE_TILE_LOCAL

#include "tiletex.h"

#include <stdlib.h>
#include <vector>

namespace opengl
{
    bool flush_opengl_errors();
    bool check_texture_size(const char *name, int width, int height);
}

struct coord_def;

struct GLW_2VF
{
    GLW_2VF() {}
    GLW_2VF(float l, float m) : x(l), y(m) {}

    inline void set(float l, float m)
    {
        x = l;
        y = m;
    }

    inline void set(const GLW_2VF &in)
    {
        x = in.x;
        y = in.y;
    }

    float x;
    float y;
};

struct GLW_3VF
{
    GLW_3VF() {}
    GLW_3VF(float l, float m, float n) : x(l), y(m), z(n) {}
    GLW_3VF(float l, float m) : x(l), y(m), z(0.0) {}

    inline void set(float l, float m, float n = 0.0)
    {
        x = l;
        y = m;
        z = n;
    }

    inline void set(const GLW_3VF &in)
    {
        x = in.x;
        y = in.y;
        z = in.z;
    }

    float x;
    float y;
    float z;
};

enum drawing_modes
{
    GLW_LINES,
    GLW_RECTANGLE,
};

// Convenience structure for passing around primitives (lines or quads)
class GLWPrim
{
public:
    // Constructor assumes we're always going to have a position
    GLWPrim(float sx, float sy, float ex, float ey, float z = 0.0f) :
            pos_sx(sx), pos_sy(sy), pos_ex(ex), pos_ey(ey), pos_z(z),
            tex_sx(0.0f), tex_sy(0.0f), tex_ex(0.0f), tex_ey(0.0f),
            col_s(VColour::white), col_e(VColour::white) {}

    inline void set_tex(float sx, float sy, float ex, float ey)
    {
        tex_sx = sx;
        tex_sy = sy;
        tex_ex = ex;
        tex_ey = ey;
    }

    inline void set_col(const VColour &vc)
    {
        col_s = vc;
        col_e = vc;
    }

    inline void set_col(const VColour &s, const VColour &e)
    {
        col_s = s;
        col_e = e;
    }

    float pos_sx, pos_sy, pos_ex, pos_ey, pos_z;
    float tex_sx, tex_sy, tex_ex, tex_ey;
    VColour col_s, col_e;
};

// This struct defines all of the state that any particular rendering needs.
// If other rendering states are needed, they should be added here so that
// they do not introduce unneeded side effects for other parts of the code
// that have not thought about turning that new state off.

struct GLState
{
    GLState();
    GLState(const GLState &state);

    const GLState &operator=(const GLState &state);
    bool operator==(const GLState &state) const;

    // vertex arrays
    bool array_vertex;
    bool array_texcoord;
    bool array_colour;

    // render state
    bool blend;
    bool texture;
    bool depthtest;
    bool alphatest;
    unsigned char alpharef;
    VColour colour;
};

class GLStateManager
{
public:
    // To silence pre 4.3 g++ compiler warnings
    virtual ~GLStateManager() {}

    // Note: the init/shutdown static functions should be written in the derived
    // implementation-specific cc file, e.g. glwrapper-ogl.cc.
    static void init();
    static void shutdown();

    // State Manipulation
    virtual void set(const GLState& state) = 0;
    virtual void pixelstore_unpack_alignment(unsigned int bpp) = 0;
    virtual void reset_view_for_redraw() = 0;
    virtual void reset_view_for_resize(const coord_def &m_windowsz,
                                       const coord_def &m_drawablesz) = 0;
    virtual void set_transform(const GLW_3VF &trans, const GLW_3VF &scale) = 0;
    virtual void reset_transform() = 0;
    virtual void get_transform(GLW_3VF *trans, GLW_3VF *scale) = 0;
    virtual void set_scissor(int x, int y, unsigned int w, unsigned int h) = 0;
    virtual void reset_scissor() = 0;

    // Texture-specific functions
    virtual void delete_textures(size_t count, unsigned int *textures) = 0;
    virtual void generate_textures(size_t count, unsigned int *textures) = 0;
    virtual void bind_texture(unsigned int texture) = 0;
    virtual void load_texture(LoadTextureArgs texture) = 0;

    virtual int logical_to_device(int n) const = 0;
    virtual int device_to_logical(int n, bool round=true) const = 0;

    // Debug
#ifdef ASSERTS
    static bool _valid(int num_verts, drawing_modes mode);
#endif
};

class GenericTexture;

class GLShapeBuffer
{
public:
    virtual ~GLShapeBuffer() {}

    // Static Constructors
    // Note: the init/shutdown static functions should be written in the derived
    // implementation-specific cc file, e.g. glwrapper-ogl.cc.
    static GLShapeBuffer *create(bool texture = false, bool colour = false,
                                 drawing_modes prim = GLW_RECTANGLE);

    virtual const char *print_statistics() const = 0;
    virtual unsigned int size() const = 0;

    // Add a primitive to be drawn.
    virtual void add(const GLWPrim &prim) = 0;

    // Draw all the primitives in the buffer.
    virtual void draw(const GLState &state) = 0;

    // Clear all the primitives from the buffer.
    virtual void clear() = 0;
};

// Main interface for GL functions
extern GLStateManager *glmanager;

#endif // use_tile
