#ifndef GLWRAPPER_H
#define GLWRAPPER_H

#include <stdlib.h> // For size_t
#include <vector> // for std::vector

#ifdef USE_TILE
#ifdef USE_GL

struct coord_def;

struct GLW_3VF
{
    GLW_3VF() {};
    GLW_3VF(float l, float m, float n) : x(l), y(m), z(n) {}
    
    inline void set(float l, float m, float n)
    {
        x = l;
        y = m;
        z = n;
    }
    
    float x;
    float y;
    float z;
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
    
    float x;
    float y;
    float z;
    float t;
};

enum MipMapOptions
{
    MIPMAP_CREATE,
    MIPMAP_NONE,
    MIPMAP_MAX
};

// TODO: Ixtli - Remove QUADS entirely.
typedef enum
{
    GLW_POINTS,
    GLW_LINES,
    GLW_TRIANGLES,
    GLW_TRIANGLE_STRIP,
    GLW_QUADS
} drawing_modes;

struct GLPrimitive
{
    GLPrimitive(long unsigned int sz, size_t ct, unsigned int vs,
        const void* v_pt, const void *c_pt, const void *t_pt);

    // Primitive Metadata
    drawing_modes mode;
    unsigned int vert_size;  // Coords per vertex
    long unsigned int size;
    size_t count;

    // Primitive Data
    const void *vert_pointer;
    const void *colour_pointer;
    const void *texture_pointer;

    // Primitive render manipulations
    GLW_3VF *pretranslate;
    GLW_3VF *prescale;
    
    // State manipulations
    GLW_3VF *color;
};

struct GLState
{
    GLState();

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
    static void init();

    // State Manipulation
    static void set(const GLState& state);
    static void pixelstore_unpack_alignment(unsigned int bpp);
    static void reset_view_for_redraw(float x, float y);
    static void reset_view_for_resize(coord_def &m_windowsz);
    static void set_transform(GLW_3VF *translate, GLW_3VF *scale);
    static void reset_transform();
    static void set_current_color(GLW_3VF &color);
    static void set_current_color(GLW_4VF &color);

    // Drawing GLPrimitives
    static void draw_primitive(const GLPrimitive &prim);

    // Texture-specific functinos
    static void delete_textures(size_t count, unsigned int *textures);
    static void generate_textures(size_t count, unsigned int *textures);
    static void bind_texture(unsigned int texture);
    static void load_texture(unsigned char *pixels, unsigned int width,
        unsigned int height, MipMapOptions mip_opt);

    // Debug
#ifdef DEBUG
    static bool _valid(int num_verts, drawing_modes mode);
#endif

protected:
};

#endif // use_gl
#endif // use_tile
#endif // include guard