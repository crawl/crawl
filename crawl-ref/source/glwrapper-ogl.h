#pragma once

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
    virtual void set(const GLState& state) override;
    virtual void pixelstore_unpack_alignment(unsigned int bpp) override;
    virtual void reset_view_for_redraw(float x, float y) override;
    virtual void reset_view_for_resize(const coord_def &m_windowsz,
                                       const coord_def &m_drawablesz) override;
    virtual void set_transform(const GLW_3VF &trans, const GLW_3VF &scale) override;
    virtual void reset_transform() override;
    virtual void get_transform(GLW_3VF *trans, GLW_3VF *scale) override;
    virtual void set_scissor(int x, int y, unsigned int w, unsigned int h) override;
    virtual void reset_scissor() override;
#ifdef __ANDROID__
    virtual void fixup_gl_state() override;
#endif

    // Texture-specific functinos
    virtual void delete_textures(size_t count, unsigned int *textures) override;
    virtual void generate_textures(size_t count, unsigned int *textures) override;
    virtual void bind_texture(unsigned int texture) override;
    virtual void load_texture(unsigned char *pixels, unsigned int width,
                              unsigned int height, MipMapOptions mip_opt,
                              int xoffset=-1, int yoffset=-1) override;
    int logical_to_device(int n) const override;
    int device_to_logical(int n, bool round=true) const override;
protected:
    GLState m_current_state;
#ifdef __ANDROID__
    GLint m_last_tex;
#endif
    int m_window_height;

private:
    void glDebug(const char* msg);
};

class OGLShapeBuffer : public GLShapeBuffer
{
public:
    OGLShapeBuffer(bool texture = false, bool colour = false,
                   drawing_modes prim = GLW_RECTANGLE);

    virtual const char *print_statistics() const override;
    virtual unsigned int size() const override;

    virtual void add(const GLWPrim &rect) override;
    virtual void draw(const GLState &state) override;
    virtual void clear() override;

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

struct HiDPIState;
extern HiDPIState display_density;

#endif // USE_GL
#endif // USE_TILE_LOCAL
