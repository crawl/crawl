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
    
    float x;
    float y;
    float z;
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
    unsigned int vertSize;  // Coords per vertex
    long unsigned int size;
    size_t count;

    // Primitive Data
    const void *vert_pointer;
    const void *colour_pointer;
    const void *texture_pointer;

    // Primitive render manipulations
    GLW_3VF *pretranslate;
    GLW_3VF *prescale;
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
    static void pixelStoreUnpackAlignment(unsigned int bpp);
    static void resetViewForRedraw(float x, float y);
    static void resetViewForResize(coord_def &m_windowsz);
    static void setTransform(GLW_3VF *translate, GLW_3VF *scale);
    static void resetTransform();

    // Drawing GLPrimitives
    static void drawGLPrimitive(const GLPrimitive &prim);

    // Drawing tiles-specific objects
    static void drawTextBlock(unsigned int x_pos, unsigned int y_pos,
        long unsigned int stride, bool drop_shadow, size_t count,
        const void *pos_verts, const void *tex_verts, const void *color_verts);
    static void drawColorBox(long unsigned int stride, size_t count,
        const void *pos_verts, const void *color_verts);

    // Texture-specific functinos
    static void deleteTextures(size_t count, unsigned int *textures);
    static void generateTextures(size_t count, unsigned int *textures);
    static void bindTexture(unsigned int texture);
    static void loadTexture(unsigned char *pixels, unsigned int width,
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