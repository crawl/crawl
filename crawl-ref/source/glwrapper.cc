#include "debug.h"

#include "glwrapper.h"
#include "AppHdr.h"

#ifdef USE_TILE

#ifdef USE_SDL
#include <SDL_opengl.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// GLPrimitive
GLPrimitive::GLPrimitive(long unsigned int sz, size_t ct, unsigned int vs,
        const void* v_pt, const void *c_pt, const void *t_pt) :
    mode(GLW_QUADS),
    vertSize(vs),
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

void GLStateManager::init()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0, 0.0, 0.0, 1.0f);
    glDepthFunc(GL_LEQUAL);
}

void GLStateManager::set(const GLState& state)
{
    if (state.array_vertex)
        glEnableClientState(GL_VERTEX_ARRAY);
    else
        glDisableClientState(GL_VERTEX_ARRAY);

    if (state.array_texcoord)
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    else
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

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

    if (state.texture)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (state.blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (state.depthtest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (state.alphatest)
    {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_NOTEQUAL, state.alpharef);
    }
    else
        glDisable(GL_ALPHA_TEST);
}

void GLStateManager::setTransform(GLW_3VF *translate, GLW_3VF *scale)
{
    glLoadIdentity();
    if(translate) glTranslatef(translate->x, translate->y, translate->z);
    if(scale) glScalef(scale->x, scale->y, scale->z);
}

void GLStateManager::resetViewForResize(coord_def &m_windowsz)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // For ease, vertex positions are pixel positions.
    glOrtho(0, m_windowsz.x, m_windowsz.y, 0, -1000, 1000);
}

void GLStateManager::resetTransform()
{
    glLoadIdentity();
    glTranslatef(0,0,0);
    glScalef(1,1,1);
}

void GLStateManager::pixelStoreUnpackAlignment(unsigned int bpp)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);
}

void GLStateManager::drawGLPrimitive(const GLPrimitive &prim)
{
    // Handle errors
    if( !prim.vert_pointer || prim.count < 1 || prim.size < 1 ) return;
    ASSERT(_valid(prim.count, prim.mode));
    
    // Set pointers
    glVertexPointer(prim.vertSize, GL_FLOAT, prim.size, prim.vert_pointer);
    if( prim.texture_pointer && prim.mode != GLW_LINES )
        glTexCoordPointer(2, GL_FLOAT, prim.size, prim.texture_pointer);
    if( prim.colour_pointer )
        glColorPointer(4, GL_UNSIGNED_BYTE, prim.size, prim.colour_pointer);
    
    // Handle pre-render matrix manipulations
    if( prim.pretranslate || prim.prescale )
    {
        glPushMatrix();

        if( prim.pretranslate )
        {
            glTranslatef(   prim.pretranslate->x,
                            prim.pretranslate->y,
                            prim.pretranslate->z);
        }

        if( prim.prescale )
            glScalef(prim.prescale->x, prim.prescale->y, prim.prescale->z);

    }
    
    // Draw!
    switch( prim.mode )
    {
        case GLW_QUADS:
            glDrawArrays(GL_QUADS, 0, prim.count);
            break;
        
        case GLW_LINES:
            glDrawArrays(GL_LINES, 0, prim.count);
            break;
            
        default:
            break;
    }
    
    // Clean up
    if( prim.pretranslate || prim.prescale )
    {
        glPopMatrix();
    }
}

void GLStateManager::deleteTextures(size_t count, unsigned int *textures)
{
    glDeleteTextures(count, (GLuint*)textures);
}

void GLStateManager::generateTextures( size_t count, unsigned int *textures)
{
    glGenTextures(count, (GLuint*)textures);
}

void GLStateManager::bindTexture(unsigned int texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
}

void GLStateManager::loadTexture(unsigned char *pixels, unsigned int width,
    unsigned int height, MipMapOptions mip_opt)
{
    // Assumptions...
    const unsigned int bpp = 4;
    const GLenum texture_format = GL_RGBA;
    const GLenum format = GL_UNSIGNED_BYTE;

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
    } else {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, bpp, width, height, 0,
                     texture_format, format, pixels);
    }
}

void GLStateManager::resetViewForRedraw(float x, float y)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(x, y , 1.0f);
}

void GLStateManager::drawTextBlock(unsigned int x_pos, unsigned int y_pos,
    long unsigned int stride, bool drop_shadow, size_t count,
    const void *pos_verts, const void *tex_verts, const void *color_verts)
{
    // Assume the texture is pre-bound (m_tex.bind();)
    glVertexPointer(2, GL_FLOAT, stride, pos_verts);
    glTexCoordPointer(2, GL_FLOAT, stride, tex_verts);

    if (drop_shadow)
    {
        glColor3f(0.0f, 0.0f, 0.0f);

        glLoadIdentity();
        glTranslatef(x_pos + 1, y_pos + 1, 0.0f);
        glDrawArrays(GL_QUADS, 0, count);

        glColor3f(1.0f, 1.0f, 1.0f);
    }

    glLoadIdentity();
    glTranslatef(x_pos, y_pos, 0.0f);

    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_UNSIGNED_BYTE, stride, color_verts);
    glDrawArrays(GL_QUADS, 0, count);
    glDisableClientState(GL_COLOR_ARRAY);
    
}

void GLStateManager::drawColorBox(long unsigned int stride, size_t count,
    const void *pos_verts, const void *color_verts)
{
    glVertexPointer(2, GL_FLOAT, stride, pos_verts);
    glColorPointer(4, GL_UNSIGNED_BYTE, stride, color_verts);
    glDrawArrays(GL_QUADS, 0, count);
}

#ifdef DEBUG
bool GLStateManager::_valid(int num_verts, drawing_modes mode)
{
    switch( mode )
    {
        case GLW_QUADS:
        case GLW_TRIANGLE_STRIP:
        return (num_verts % 4 == 0);
        case GLW_TRIANGLES:
        return (num_verts % 3 == 0);
        case GLW_LINES:
        return (num_verts % 2 == 0);
        case GLW_POINTS:
        return (true);
        default:
        return (false);
    }
    
}
#endif

/////////////////////////////////////////////////////////////////////////////
// Static Methods

#endif
