#ifdef USE_TILE

#include "glwrapper.h"

/////////////////////////////////////////////////////////////////////////////
// VColour

VColour VColour::white(255, 255, 255, 255);
VColour VColour::black(0, 0, 0, 255);
VColour VColour::transparent(0, 0, 0, 0);

/////////////////////////////////////////////////////////////////////////////
// GLPrimitive
GLPrimitive::GLPrimitive(size_t sz, size_t ct, unsigned int vs,
                         const void* v_pt, const void *c_pt, const void *t_pt) :
    mode(GLW_QUADS),
    vert_size(vs),
    size(sz),
    count(ct),
    vert_pointer(v_pt),
    colour_pointer(c_pt),
    texture_pointer(t_pt),
    pretranslate(NULL),
    prescale(NULL)
{
}

/////////////////////////////////////////////////////////////////////////////
// GLState

// Note: these defaults should match the OpenGL defaults
GLState::GLState() :
    array_vertex(false),
    array_texcoord(false),
    array_colour(false),
    blend(false),
    texture(false),
    depthtest(false),
    alphatest(false),
    alpharef(0)
{
}

GLState::GLState(const GLState &state) :
    array_vertex(state.array_vertex),
    array_texcoord(state.array_texcoord),
    array_colour(state.array_colour),
    blend(state.blend),
    texture(state.texture),
    depthtest(state.depthtest),
    alphatest(state.alphatest),
    alpharef(state.alpharef)
{
}

void GLState::set(const GLState &state)
{
    array_vertex = state.array_vertex;
    array_texcoord = state.array_texcoord;
    array_colour = state.array_colour;
    blend = state.blend;
    texture = state.texture;
    depthtest = state.depthtest;
    alphatest = state.alphatest;
    alpharef = state.alpharef;
}

/////////////////////////////////////////////////////////////////////////////
// GLStateManager

#ifdef ASSERTS
bool GLStateManager::_valid(int num_verts, drawing_modes mode)
{
    switch (mode)
    {
    case GLW_QUADS:
        return (num_verts % 4 == 0);
    case GLW_RECTANGLE:
        return (num_verts % 4 == 0);
    case GLW_LINES:
        return (num_verts % 2 == 0);
    default:
        return (false);
    }
}
#endif // ASSERTS

#endif // USE_TILE
