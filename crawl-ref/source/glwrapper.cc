#ifdef USE_TILE

#include "glwrapper.h"

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
