#include "AppHdr.h"

#ifdef USE_TILE
#ifdef USE_GL

#include "glwrapper-ogl.h"

// How do we get access to the GL calls?
// If other UI types use the -ogl wrapper they should
// include more conditional includes here.
#ifdef USE_SDL
#include <SDL_opengl.h>
#endif

#include "debug.h"

/////////////////////////////////////////////////////////////////////////////
// Static functions from GLStateManager

GLStateManager *glmanager = NULL;

void GLStateManager::init()
{
    if (glmanager)
        return;

    glmanager = new OGLStateManager();
}

void GLStateManager::shutdown()
{
    delete glmanager;
    glmanager = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// Static functions from GLShapeBuffer

GLShapeBuffer *GLShapeBuffer::create(bool texture, bool colour,
                                     drawing_modes prim)
{
    return ((GLShapeBuffer*) new OGLShapeBuffer(texture, colour, prim));
}

/////////////////////////////////////////////////////////////////////////////
// OGLStateManager

OGLStateManager::OGLStateManager()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0, 0.0, 0.0, 1.0f);
    glDepthFunc(GL_LEQUAL);
}

void OGLStateManager::set(const GLState& state)
{
    bool dirty = false;

    if(state.array_vertex != current_state.array_vertex)
    {
        if (state.array_vertex)
            glEnableClientState(GL_VERTEX_ARRAY);
        else
            glDisableClientState(GL_VERTEX_ARRAY);
        dirty = true;
    }

    if(state.array_texcoord != current_state.array_texcoord)
    {
        if (state.array_texcoord)
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        else
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        dirty = true;
    }

    if(state.array_colour != current_state.array_colour )
    {
        if (state.array_colour)
        {
            glEnableClientState(GL_COLOR_ARRAY);
        }
        else
        {
            glDisableClientState(GL_COLOR_ARRAY);

            // [enne] This should *not* be necessary, but the Linux OpenGL
            // driver that I'm using sets this to the last colour of the
            // colour array.  So, we need to unset it here.
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        dirty = true;
    }

    if(state.texture != current_state.texture)
    {
        if (state.texture)
            glEnable(GL_TEXTURE_2D);
        else
            glDisable(GL_TEXTURE_2D);
        dirty = true;
    }

    if(state.blend != current_state.blend)
    {
        if (state.blend)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
        dirty = true;
    }

    if(state.depthtest != current_state.depthtest)
    {
        if (state.depthtest)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        dirty = true;
    }

    if(state.alphatest != current_state.alphatest)
    {
        if (state.alphatest)
        {
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_NOTEQUAL, state.alpharef);
        }
        else
            glDisable(GL_ALPHA_TEST);
        dirty = true;
    }

    // Copy current state because querying it is slow
    if(dirty)
        current_state.set(state);
}

void OGLStateManager::set_transform(const GLW_3VF *trans, const GLW_3VF *scale)
{
    glLoadIdentity();
    if (trans)
        glTranslatef(trans->x, trans->y, trans->z);
    if (scale)
        glScalef(scale->x, scale->y, scale->z);
}

void OGLStateManager::reset_view_for_resize(coord_def &m_windowsz)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // For ease, vertex positions are pixel positions.
    glOrtho(0, m_windowsz.x, m_windowsz.y, 0, -1000, 1000);
}

void OGLStateManager::reset_transform()
{
    glLoadIdentity();
    glTranslatef(0,0,0);
    glScalef(1,1,1);
}

void OGLStateManager::pixelstore_unpack_alignment(unsigned int bpp)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);
}

void OGLStateManager::delete_textures(size_t count, unsigned int *textures)
{
    glDeleteTextures(count, (GLuint*)textures);
}

void OGLStateManager::generate_textures(size_t count, unsigned int *textures)
{
    glGenTextures(count, (GLuint*)textures);
}

void OGLStateManager::bind_texture(unsigned int texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
}

void OGLStateManager::load_texture(unsigned char *pixels, unsigned int width,
                                   unsigned int height, MipMapOptions mip_opt)
{
    // Assumptions...
    const unsigned int bpp = 4;
    const GLenum texture_format = GL_RGBA;
    const GLenum format = GL_UNSIGNED_BYTE;
    // Also assume that the texture is already bound using bind_texture

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    if (mip_opt == MIPMAP_CREATE)
    {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gluBuild2DMipmaps(GL_TEXTURE_2D, bpp, width, height,
                          texture_format, format, pixels);
    }
    else
    {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, bpp, width, height, 0,
                     texture_format, format, pixels);
    }
}

void OGLStateManager::reset_view_for_redraw(float x, float y)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(x, y , 1.0f);
}

void OGLStateManager::set_current_color(GLW_3VF &color)
{
    glColor3f(color.r, color.g, color.b);
}

void OGLStateManager::set_current_color(GLW_4VF &color)
{
    glColor4f(color.r, color.g, color.b, color.a);
}

/////////////////////////////////////////////////////////////////////////////
// OGLShapeBuffer

OGLShapeBuffer::OGLShapeBuffer(bool texture, bool colour, drawing_modes prim) :
    prim_type(prim),
    texture_verts(texture),
    colour_verts(colour)
{
}

// Accounting
const char *OGLShapeBuffer::print_statistics() const
{
    return (NULL);
}

unsigned int OGLShapeBuffer::size() const
{
    return (position_buffer.size());
}

// Add a rectangle
void OGLShapeBuffer::push(const GLWRect &rect)
{
    switch( prim_type )
    {
    case GLW_RECTANGLE:
        push_rect(rect);
        break;
    case GLW_LINES:
        push_line(rect);
        break;
    default:
        break;
    }
}

void OGLShapeBuffer::push_rect(const GLWRect &rect)
{
    // Copy vert positions
    size_t last = position_buffer.size();
    position_buffer.resize(last + 4);
    position_buffer[last    ].set(rect.pos_sx, rect.pos_sy, rect.pos_z);
    position_buffer[last + 1].set(rect.pos_sx, rect.pos_ey, rect.pos_z);
    position_buffer[last + 2].set(rect.pos_ex, rect.pos_sy, rect.pos_z);
    position_buffer[last + 3].set(rect.pos_ex, rect.pos_ey, rect.pos_z);

    // Copy texture coords if necessary
    if( texture_verts )
    {
        last = texture_buffer.size();
        texture_buffer.resize(last + 4);
        texture_buffer[last    ].set(rect.tex_sx, rect.tex_sy);
        texture_buffer[last + 1].set(rect.tex_sx, rect.tex_ey);
        texture_buffer[last + 2].set(rect.tex_ex, rect.tex_sy);
        texture_buffer[last + 3].set(rect.tex_ex, rect.tex_ey);
    }

    // Copy vert colours if necessary
    if( colour_verts )
    {
        // ensure that there are no NULL VColours
        // TODO: Maybe we can have a default colour here? -- ixtli
        ASSERT(rect.col_bl && rect.col_br && rect.col_tl && rect.col_tr);

        last = colour_buffer.size();
        colour_buffer.resize(last + 4);
        colour_buffer[last    ].set(*rect.col_bl);
        colour_buffer[last + 1].set(*rect.col_tl);
        colour_buffer[last + 2].set(*rect.col_br);
        colour_buffer[last + 3].set(*rect.col_tr);
    }

    // build indices
    last = ind_buffer.size();

    if( last > 3 )
    {
        // This is not the first box so make FOUR degenerate triangles
        ind_buffer.resize(last + 6);
        unsigned short int val = ind_buffer[last - 1];

        // the first three degens finish the previous box and move
        // to the first position of the new one we just added and
        // the fourth degen creates a triangle that is a line from p1 to p3
        ind_buffer[last    ] = val++;
        ind_buffer[last + 1] = val;

        // Now add as normal
        ind_buffer[last + 2] = val++;
        ind_buffer[last + 3] = val++;
        ind_buffer[last + 4] = val++;
        ind_buffer[last + 5] = val;
    }
    else
    {
        // This is the first box so don't bother making any degenerate triangles
        ind_buffer.resize(last + 4);
        ind_buffer[0] = 0;
        ind_buffer[1] = 1;
        ind_buffer[2] = 2;
        ind_buffer[3] = 3;
    }
}

void OGLShapeBuffer::push_line(const GLWRect &rect)
{
    // Copy vert positions
    size_t last = position_buffer.size();
    position_buffer.resize(last + 2);
    position_buffer[last    ].set(rect.pos_sx, rect.pos_sy, rect.pos_z);
    position_buffer[last + 1].set(rect.pos_ex, rect.pos_ey, rect.pos_z);

    // Copy texture coords if necessary
    if( texture_verts )
    {
        last = texture_buffer.size();
        texture_buffer.resize(last + 2);
        texture_buffer[last    ].set(rect.tex_sx, rect.tex_sy);
        texture_buffer[last + 1].set(rect.tex_ex, rect.tex_ey);
    }

    // Copy vert colours if necessary
    if( colour_verts )
    {
        // ensure that there are no NULL VColours
        // TODO: Maybe we can have a default colour here? -- ixtli
        ASSERT(rect.col_bl && rect.col_br);

        last = colour_buffer.size();
        colour_buffer.resize(last + 2);
        colour_buffer[last    ].set(*rect.col_s);
        colour_buffer[last + 1].set(*rect.col_e);
    }
    
}

// Draw the buffer
void OGLShapeBuffer::draw(GLW_3VF *pt, GLW_3VF *ps, bool flush)
{
    // Make sure we're drawing something
    if (position_buffer.size() == 0)
        return;

    // Get the current renderer state;
    const GLState &state = glmanager->get_state();

    // Set position pointers
    // TODO: Is there ever a reason to set array_vertex to false and still
    // try and draw? -- ixtli
    if (state.array_vertex)
        glVertexPointer(3, GL_FLOAT, 0, &position_buffer[0]);

    // set texture pointer
    if (state.array_texcoord && texture_verts)
        glTexCoordPointer(2, GL_FLOAT, 0, &texture_buffer[0]);

    // set colour pointer
    if (state.array_colour && colour_verts)
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, &colour_buffer[0]);

    // Handle pre-render matrix manipulations
    if (pt || ps)
    {
        glPushMatrix();
        if (pt)
            glTranslatef(pt->x, pt->y, pt->z);
        if (ps)
            glScalef(ps->x, ps->y, ps->z);
    }

    // Draw!
    switch (prim_type)
    {
    case GLW_RECTANGLE:
        glDrawElements( GL_TRIANGLE_STRIP, ind_buffer.size(),
                        GL_UNSIGNED_SHORT, &ind_buffer[0]);
        break;
    case GLW_LINES:
        glDrawArrays( GL_LINES, 0, position_buffer.size());
        break;
    default:
        break;
    }

    // Clean up
    if (pt || ps)
        glPopMatrix();

    if(flush)
        clear();
}

void OGLShapeBuffer::clear()
{
    // reset all buffers
    position_buffer.clear();
    if( prim_type != GLW_LINES )
        ind_buffer.clear();
    if( texture_verts )
        texture_buffer.clear();
    if( colour_verts )
        colour_buffer.clear();
}

#endif // USE_GL
#endif // USE_TILE
