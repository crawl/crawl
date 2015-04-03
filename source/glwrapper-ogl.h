#ifndef OGL_GL_WRAPPER_H
#define OGL_GL_WRAPPER_H

#ifdef USE_TILE_LOCAL
#ifdef USE_GL

#include "glwrapper.h"
#ifdef __ANDROID__
#include <GLES/gl.h>
#endif
class OGLStateManager : public GLStateManager
{
public:
    OGLStateManager();

    // State Manipulation
    virtual void set(const GLState& state);
    virtual void pixelstore_unpack_alignment(unsigned int bpp);
    virtual void reset_view_for_redraw(float x, float y);
    virtual void reset_view_for_resize(const coord_def &m_windowsz,
                                       const coord_def &m_drawablesz);
    virtual void set_transform(const GLW_3VF &trans, const GLW_3VF &scale);
    virtual void reset_transform();
#ifdef __ANDROID__
    virtual void fixup_gl_state();
#endif

    // Texture-specific functinos
    virtual void delete_textures(size_t count, unsigned int *textures);
    virtual void generate_textures(size_t count, unsigned int *textures);
    virtual void bind_texture(unsigned int texture);
    virtual void load_texture(unsigned char *pixels, unsigned int width,
                              unsigned int height, MipMapOptions mip_opt,
                              int xoffset=-1, int yoffset=-1);
protected:
    GLState m_current_state;
#ifdef __ANDROID__
    GLint m_last_tex;
#endif

private:
    void glDebug(const char* msg);
};

class OGLShapeBuffer : public GLShapeBuffer
{
public:
    OGLShapeBuffer(bool texture = false, bool colour = false,
                   drawing_modes prim = GLW_RECTANGLE);

    virtual const char *print_statistics() const;
    virtual unsigned int size() const;

    virtual void add(const GLWPrim &rect);
    virtual void draw(const GLState &state);
    virtual void clear();

protected:
    // Helper methods for adding specific primitives.
    void add_rect(const GLWPrim &rect);
    void add_line(const GLWPrim &rect);

    drawing_modes m_prim_type;
    bool m_texture_verts;
    bool m_colour_verts;

    vector<GLW_3VF> m_position_buffer;
    vector<GLW_2VF> m_texture_buffer;
    vector<VColour> m_colour_buffer;
    vector<unsigned short int> m_ind_buffer;

private:
    void glDebug(const char* msg);
};

#endif // USE_GL
#endif // USE_TILE_LOCAL
#endif // OGL_GL_WRAPPER_H
