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
    virtual const GLState& get_state() const { return (current_state); }
    virtual void pixelstore_unpack_alignment(unsigned int bpp);
    virtual void reset_view_for_redraw(float x, float y);
    virtual void reset_view_for_resize(coord_def &m_windowsz);
    virtual void set_transform(const GLW_3VF *trans = NULL,
                               const GLW_3VF *scale = NULL);
    virtual void reset_transform();
    virtual void set_current_color(GLW_3VF &color);
    virtual void set_current_color(GLW_4VF &color);

    // Texture-specific functinos
    virtual void delete_textures(size_t count, unsigned int *textures);
    virtual void generate_textures(size_t count, unsigned int *textures);
    virtual void bind_texture(unsigned int texture);
    virtual void load_texture(unsigned char *pixels, unsigned int width,
                              unsigned int height, MipMapOptions mip_opt);
protected:
    GLState current_state;
};

class OGLShapeBuffer : public GLShapeBuffer
{
public:
    OGLShapeBuffer(bool texture = false, bool colour = false,
                   drawing_modes prim = GLW_RECTANGLE);

    // Accounting
    virtual const char *print_statistics() const;
    virtual unsigned int size() const;

    // Add a rectangle
    virtual void push(const GLWRect &rect);

    // Draw the buffer
    virtual void draw(GLW_3VF *pt = NULL, GLW_3VF *ps = NULL, bool flush = false);

    // Manipulate state
    virtual void clear();
protected:
    // Helper methods for pushing specific prim types
    void push_rect(const GLWRect &rect);
    void push_line(const GLWRect &rect);

    drawing_modes prim_type;
    bool texture_verts, colour_verts;

    std::vector<GLW_3VF> position_buffer;
    std::vector<GLW_2VF> texture_buffer;
    std::vector<VColour> colour_buffer;
    std::vector<unsigned short int> ind_buffer;     //indicies
};

#endif // USE_GL
#endif // USE_TILE
#endif // OGL_GL_WRAPPER_H
