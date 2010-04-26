#ifndef OGL_GL_WRAPPER_H
#define OGL_GL_WRAPPER_H

#ifdef USE_TILE
#ifdef USE_GL

#include "glwrapper.h"

class OGLStateManager : public GLStateManager
{
public:
    OGLStateManager();

    // State query
    virtual glw_winding winding() { return (GLW_CW); }

    // State Manipulation
    virtual void set(const GLState& state);
    virtual void pixelstore_unpack_alignment(unsigned int bpp);
    virtual void reset_view_for_redraw(float x, float y);
    virtual void reset_view_for_resize(coord_def &m_windowsz);
    virtual void set_transform(const GLW_3VF *trans = NULL,
                               const GLW_3VF *scale = NULL);
    virtual void reset_transform();
    virtual void set_current_color(GLW_3VF &color);
    virtual void set_current_color(GLW_4VF &color);

    // Drawing GLPrimitives
    virtual void draw_primitive(const GLPrimitive &prim);

    // Texture-specific functinos
    virtual void delete_textures(size_t count, unsigned int *textures);
    virtual void generate_textures(size_t count, unsigned int *textures);
    virtual void bind_texture(unsigned int texture);
    virtual void load_texture(unsigned char *pixels, unsigned int width,
                              unsigned int height, MipMapOptions mip_opt);
};

#endif // USE_GL
#endif // USE_TILE
#endif // OGL_GL_WRAPPER_H
