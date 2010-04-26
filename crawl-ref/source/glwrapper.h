#ifndef GL_WRAPPER_H
#define GL_WRAPPER_H

#ifdef USE_TILE

#include <stdlib.h>
#include <vector>

struct coord_def;

struct VColour
{
    VColour() {}
    VColour(unsigned char _r, unsigned char _g, unsigned char _b,
            unsigned char _a = 255) : r(_r), g(_g), b(_b), a(_a) {}
    VColour(const VColour &vc) : r(vc.r), g(vc.g), b(vc.b), a(vc.a) {}

    inline void set(const VColour &in)
    {
        r = in.r;
        g = in.g;
        b = in.b;
        a = in.a;
    }

    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;

    static VColour white;
    static VColour black;
    static VColour transparent;
};

struct GLW_2VF
{
    GLW_2VF() {};
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

    union {float x; float u; float s;};
    union {float y; float v; float t;};
};

struct GLW_3VF
{
    GLW_3VF() {};
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

    union {float x; float r;};
    union {float y; float g;};
    union {float z; float b;};
};

struct GLW_4VF
{
    GLW_4VF() {};
    GLW_4VF(float l, float m, float n, float p) : x(l), y(m), z(n), t(p) {}

    inline void set(float l, float m, float n, float p)
    {
        x = l;
        y = m;
        z = n;
        t = p;
    }

    inline void set(const GLW_4VF &in)
    {
        x = in.x;
        y = in.y;
        z = in.z;
        t = in.t;
    }

    union {float x; float r;                    };
    union {float y; float g;                    };
    union {float z; float b; float u; float s;  };
    union {float t; float a; float v;           };
};

enum MipMapOptions
{
    MIPMAP_CREATE,
    MIPMAP_NONE,
    MIPMAP_MAX
};

enum drawing_modes
{
    GLW_LINES,
    GLW_RECTANGLE
};

enum glw_winding
{
    GLW_CW,
    GLW_CCW
};

// Convenience structure for passing around rectangles
// (or lines) which crawl does a lot of
struct GLWRect
{
    // Constructor assumes we're always going to have a position
    GLWRect(float sx, float sy, float ex, float ey, float z = 0.0f) :
        pos_sx(sx), pos_sy(sy), pos_ex(ex), pos_ey(ey), pos_z(z),
        tex_sx(0.0f), tex_sy(0.0f), tex_ex(0.0f), tex_ey(0.0f),
        col_bl(NULL), col_br(NULL), col_tl(NULL), col_tr(NULL) {}

    inline void set_tex(float sx, float sy, float ex, float ey)
    {
        tex_sx = sx;
        tex_sy = sy;
        tex_ex = ex;
        tex_ey = ey;
    }

    inline void set_col(VColour const *bl, VColour const *br,
        VColour const *tl, VColour const *tr)
    {
        col_bl = bl;
        col_br = br;
        col_tl = tl;
        col_tr = tr;
    }

    inline void set_col(VColour const *s, VColour const *e)
    {
        col_s = s;
        col_e = e;
    }

    float pos_sx, pos_sy, pos_ex, pos_ey, pos_z;
    float tex_sx, tex_sy, tex_ex, tex_ey;
    union {VColour const *col_bl; VColour const *col_s;};
    union {VColour const *col_br; VColour const *col_e;};
    VColour const *col_tl, *col_tr;
};

// This struct defines all of the state that any particular rendering needs.
// If other rendering states are needed, they should be added here so that
// they do not introduce unneeded side effects for other parts of the code
// that have not thought about turning that new state off.

struct GLState
{
    GLState();
    GLState(const GLState &state);

    // convenience methods
    void set(const GLState &state);

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
};

class GLStateManager
{
public:
    // To silence pre 4.3 g++ compiler warnings
    virtual ~GLStateManager() {};

    // Note: the init/shutdown static functions should be written in the derived
    // implementation-specific cc file, e.g. glwrapper-ogl.cc.
    static void init();
    static void shutdown();

    // State query
    virtual glw_winding winding() = 0;

    // State Manipulation
    virtual void set(const GLState& state) = 0;
    virtual const GLState& get_state() const = 0;
    virtual void pixelstore_unpack_alignment(unsigned int bpp) = 0;
    virtual void reset_view_for_redraw(float x, float y) = 0;
    virtual void reset_view_for_resize(coord_def &m_windowsz) = 0;
    virtual void set_transform(const GLW_3VF *trans = NULL,
                               const GLW_3VF *scale = NULL) = 0;
    virtual void reset_transform() = 0;
    virtual void set_current_color(GLW_3VF &color) = 0;
    virtual void set_current_color(GLW_4VF &color) = 0;

    // Texture-specific functinos
    virtual void delete_textures(size_t count, unsigned int *textures) = 0;
    virtual void generate_textures(size_t count, unsigned int *textures) = 0;
    virtual void bind_texture(unsigned int texture) = 0;
    virtual void load_texture(unsigned char *pixels, unsigned int width,
                              unsigned int height, MipMapOptions mip_opt) = 0;

    // Debug
#ifdef ASSERTS
    static bool _valid(int num_verts, drawing_modes mode);
#endif
};

class GenericTexture;  // Defined in tiletex.h

class GLShapeBuffer
{
public:
    virtual ~GLShapeBuffer() {};

    // Static Constructors
    // Note: the init/shutdown static functions should be written in the derived
    // implementation-specific cc file, e.g. glwrapper-ogl.cc.
    static GLShapeBuffer *create(bool texture = false, bool colour = false,
                                 drawing_modes prim = GLW_RECTANGLE);

    // Accounting
    virtual const char *print_statistics() const = 0;
    virtual unsigned int size() const = 0;

    // Add a rectangle or line
    virtual void push(const GLWRect &rect) = 0;

    // Draw the buffer
    virtual void draw(GLW_3VF *pt = NULL, GLW_3VF *ps = NULL,
                      bool flush = false) = 0;

    // Manipulate state
    virtual void clear() = 0;
};

// Main interface for GL functions
extern GLStateManager *glmanager;

#endif // use_tile
#endif // include guard
