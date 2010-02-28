#ifndef GLWRAPPER_H
#define GLWRAPPER_H

#ifdef USE_TILE
#ifdef USE_GL

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
    static void set(const GLState& state);
    static void drawPTVert(long int size, int mode, int count,
        void *vert_pointer, void *tex_pointer);
    static void drawPCVert(long int size, int mode, int count,
        void *vert_pointer, void *color_pointer);
    static void drawPTCVert(long int size, int mode, int count,
        void *vert_pointer, void *tex_pointer, void *color_pointer);
    static void drawP3TCVert(long int size, int mode, int count,
        void *vert_pointer, void *tex_pointer, void *color_pointer);
    static void pushMatrix();
    static void popMatrix();
    static void translatef(float x, float y, float z);
};

#endif // use_gl
#endif // use_tile
#endif // include guard