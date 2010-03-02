#ifndef GLWRAPPER_H
#define GLWRAPPER_H

#include <stdlib.h> // For size_t
#include <vector> // for std::vector

//#include "tiletex.h" // for generic mipmap enums

#ifdef USE_TILE
#ifdef USE_GL

enum MipMapOptions
{
    MIPMAP_CREATE,
    MIPMAP_NONE,
    MIPMAP_MAX
};

// TODO: Ixtli - Remove QUADS entirely.  Then,
// TODO: Ixtli - Remove drawing_modes entirely
typedef enum
{
    GLW_POINTS,
    GLW_LINES,
    GLW_TRIANGLES,
    GLW_TRIANGLE_STRIP,
    GLW_QUADS
} drawing_modes;

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
    static void pushMatrix();
    static void popMatrix();
    static void setProjectionMatrixMode();
    static void setModelviewMatrixMode();
    static void translatef(float x, float y, float z);
    static void scalef(float x, float y, float z);
    static void loadIdentity();
    static void clearBuffers();
    
    // Drawing Quads
    static void drawQuadPTVert( long unsigned int size, size_t count,
        const void *vert_pointer, const void *tex_pointer);
    static void drawQuadPCVert( long unsigned int size, size_t count,
        const void *vert_pointer, const void *color_pointer);
    static void drawQuadPTCVert( long unsigned int size, size_t count,
        const void *vert_pointer, const void *tex_pointer,
        const void *color_pointer);
    static void drawQuadP3TCVert( long unsigned int size, size_t count,
        const void *vert_pointer, const void *tex_pointer,
        const void *color_pointer);
    
    // Drawing Lines
    static void drawLinePCVert( long unsigned int size, size_t count,
        const void *vert_pointer, const void *color_pointer);
    
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