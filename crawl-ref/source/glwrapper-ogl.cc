#include "AppHdr.h"

#ifdef USE_TILE_LOCAL
#ifdef USE_GL

#include "glwrapper-ogl.h"

// How do we get access to the GL calls?
// If other UI types use the -ogl wrapper they should
// include more conditional includes here.
#ifdef USE_SDL
# ifdef USE_GLES
#  ifdef __ANDROID__
#   include <SDL.h>
#  else
#   include <SDL2/SDL.h>
#   include <SDL_gles.h>
#  endif
#  include <GLES/gl.h>
# else
#  include <SDL_opengl.h>
#  include <SDL_video.h>
# endif
#endif

#include "options.h"
#include "stringutil.h"
#include "tilesdl.h"

#ifdef __ANDROID__
# include <android/log.h>
#endif

// TODO: if this gets big enough, pull out into opengl-utils.cc/h or sth
namespace opengl
{
    bool check_texture_size(const char *name, int width, int height)
    {
        int max_texture_size;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        if (width > max_texture_size || height > max_texture_size)
        {
            mprf(MSGCH_ERROR,
                "Texture %s is bigger than maximum driver texture size "
                "(%d,%d vs. %d). Sprites from this texture will not display "
                "properly.",
                name, width, height, max_texture_size);
            return false;
        }
        return true;
    }

    static string _gl_error_to_string(GLenum e)
    {
        switch (e)
        {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
#ifndef __ANDROID__
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
#endif
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY (fatal)";
        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW";
        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW";
        default:
            return make_stringf("Unknown OpenGL error %d", e);
        }
    }

    /**
     * Log any opengl errors to console. Will crash if a really bad one occurs.
     *
     * @return true if there were any errors.
     */
    bool flush_opengl_errors()
    {
        GLenum e = GL_NO_ERROR;
        bool fatal = false;
        bool errors = false;
        do
        {
            e = glGetError();
            if (e != GL_NO_ERROR)
            {
                errors = true;
                if (e == GL_OUT_OF_MEMORY)
                    fatal = true;
                mprf(MSGCH_ERROR, "OpenGL error %s",
                                        _gl_error_to_string(e).c_str());
            }
        } while (e != GL_NO_ERROR);
        if (fatal)
            die("Fatal OpenGL error; giving up");
        return errors;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Static functions from GLStateManager

GLStateManager *glmanager = nullptr;

void GLStateManager::init()
{
    if (glmanager)
        return;

    glmanager = new OGLStateManager();
}

void GLStateManager::shutdown()
{
    delete glmanager;
    glmanager = nullptr;
}

/////////////////////////////////////////////////////////////////////////////
// Static functions from GLShapeBuffer

GLShapeBuffer *GLShapeBuffer::create(bool texture, bool colour,
                                     drawing_modes prim)
{
    return new OGLShapeBuffer(texture, colour, prim);
}

/////////////////////////////////////////////////////////////////////////////
// OGLStateManager

OGLStateManager::OGLStateManager()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0, 0.0, 0.0, 1.0f);
    glDepthFunc(GL_LEQUAL);

    m_window_height = 0;
#ifndef USE_GLES
    // TODO: we probably can do this for GLES TOO, but maybe requires tweaks?

    // OpenGL doesn't specify what the GetProcAddress function returns
    // if the implementation does not support a function
    // (e.g. because it doesn't support the OpenGL version in question)
    // So we need to check the version before we try to get the
    // glGenerateMipmap function.
    const GLubyte* versionString = glGetString(GL_VERSION);
    if (versionString == nullptr)
    {
        mprf("Mipmap Setup: Failed to load OpenGL version.");
        return;
    }
    // We will never see 2 digit OpenGL major versions - 4.6 came out in 2016,
    // and Vulkan is carrying the torch now
    //
    // It's doubtful we'll even see an OpenGL 5.
    // But we'll be paranoid. We'll consider OpenGL 3.X - 9.X as all fine
    bool supported_first_digit = ('3' <= versionString[0]) &&
                                 (versionString[0] <= '9');
    // Anything other than X.Y would be very weird.
    // It's incredibly unlikely OpenGL 10 will ever exist.
    bool second_character_is_dot = versionString[1] == '.';
    if (!supported_first_digit || !second_character_is_dot)
    {
        mprf("Mipmap Setup: Disabled because OpenGL version: %s does not "
             "provide glGenerateMipmap.", versionString);
        return;
    }

    // We have to load the library dynamically before we can load the function
    // from the library via GetProcAddress.
    // That's how dynamic loading works.
    // It's possible the library is already loaded anyway,
    // but we're being careful here.
    if (SDL_GL_LoadLibrary(NULL) != 0)
    {
        // success == 0 for this API.
        // If we can't load it, we probably wouldn't get this far at all.
        // But just in case, we'll handle it.
        mprf("Mipmap Setup: Disabled because SDL_GL_LoadLibrary failed.");
        return;
    }

    // Because we already checked the version is higher enough,
    // SDL_GL_GetProcAddress should always get a non-null pointer back.
    // But we'll log in case this does somehow happen.
    m_mipmapFn = SDL_GL_GetProcAddress("glGenerateMipmap");
    if (m_mipmapFn == nullptr)
    {
        mprf("Mipmap Setup: Failed to load glGenerateMipmap function.");
        return;
    }
    else
    {
        mprf("Mipmap Setup: success, loaded with OpenGL version: %s",
             versionString);
    }
#else
    mprf("Mipmap Setup: skipped, not supported in this build configuration.");
#endif
}

void OGLStateManager::set(const GLState& state)
{
    if (state.array_vertex != m_current_state.array_vertex)
    {
        if (state.array_vertex)
        {
            glEnableClientState(GL_VERTEX_ARRAY);
            glDebug("glEnableClientState(GL_VERTEX_ARRAY)");
        }
        else
        {
            glDisableClientState(GL_VERTEX_ARRAY);
            glDebug("glDisableClientState(GL_VERTEX_ARRAY)");
        }
    }

    if (state.array_texcoord != m_current_state.array_texcoord)
    {
        if (state.array_texcoord)
        {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glDebug("glEnableClientState(GL_TEXTURE_COORD_ARRAY)");
        }
        else
        {
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDebug("glDisableClientState(GL_TEXTURE_COORD_ARRAY)");
        }
    }

    if (state.array_colour != m_current_state.array_colour)
    {
        if (state.array_colour)
        {
            glEnableClientState(GL_COLOR_ARRAY);
            glDebug("glEnableClientState(GL_COLOR_ARRAY)");
        }
        else
        {
            glDisableClientState(GL_COLOR_ARRAY);
            glDebug("glDisableClientState(GL_COLOR_ARRAY)");

            // [enne] This should *not* be necessary, but the Linux OpenGL
            // driver that I'm using sets this to the last colour of the
            // colour array. So, we need to unset it here.
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glDebug("glColor4f(1.0f, 1.0f, 1.0f, 1.0f)");
        }
    }

    if (state.texture != m_current_state.texture)
    {
        if (state.texture)
        {
            glEnable(GL_TEXTURE_2D);
            glDebug("glEnable(GL_TEXTURE_2D)");
        }
        else
        {
            glDisable(GL_TEXTURE_2D);
            glDebug("glDisable(GL_TEXTURE_2D)");
        }
    }

    if (state.blend != m_current_state.blend)
    {
        if (state.blend)
        {
            glEnable(GL_BLEND);
            glDebug("glEnable(GL_BLEND)");
        }
        else
        {
            glDisable(GL_BLEND);
            glDebug("glDisable(GL_BLEND)");
        }
    }

    if (state.depthtest != m_current_state.depthtest)
    {
        if (state.depthtest)
        {
            glEnable(GL_DEPTH_TEST);
            glDebug("glEnable(GL_DEPTH_TEST)");
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
            glDebug("glEnable(GL_DEPTH_TEST)");
        }
    }

    if (state.alphatest != m_current_state.alphatest
        || state.alpharef != m_current_state.alpharef)
    {
        if (state.alphatest)
        {
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_NOTEQUAL, state.alpharef);
            glDebug("glAlphaFunc(GL_NOTEQUAL, state.alpharef)");
        }
        else
        {
            glDisable(GL_ALPHA_TEST);
            glDebug("glDisable(GL_ALPHA_TEST)");
        }
    }

    if (state.colour != m_current_state.colour)
    {
        glColor4f(state.colour.r, state.colour.g,
                  state.colour.b, state.colour.a);
        glDebug("glColor4f");
    }

    m_current_state = state;
}

struct {
    GLW_3VF trans, scale;
} current_transform;

void OGLStateManager::set_transform(const GLW_3VF &trans, const GLW_3VF &scale)
{
    glLoadIdentity();
    glTranslatef(trans.x, trans.y, trans.z);
    glScalef(scale.x, scale.y, scale.z);
    current_transform = { trans, scale };
}

void OGLStateManager::reset_transform()
{
    set_transform({0,0,0}, {1,1,1});
}

void OGLStateManager::get_transform(GLW_3VF *trans, GLW_3VF *scale)
{
    if (trans)
        *trans = current_transform.trans;
    if (scale)
        *scale = current_transform.scale;
}

int OGLStateManager::logical_to_device(int n) const
{
    return display_density.logical_to_device(n);
}

int OGLStateManager::device_to_logical(int n, bool round) const
{
    return display_density.device_to_logical(n, round);
}

void OGLStateManager::set_scissor(int x, int y, unsigned int w, unsigned int h)
{
    glEnable(GL_SCISSOR_TEST);
    glScissor(logical_to_device(x), logical_to_device(m_window_height-y-h),
                logical_to_device(w), logical_to_device(h));
}

void OGLStateManager::reset_scissor()
{
    glDisable(GL_SCISSOR_TEST);
}

void OGLStateManager::reset_view_for_resize(const coord_def &m_windowsz,
                                            const coord_def &m_drawablesz)
{
    glViewport(0, 0, m_drawablesz.x, m_drawablesz.y);
    m_window_height = m_windowsz.y;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // For ease, vertex positions are pixel positions.
#ifdef USE_GLES
# ifdef __ANDROID__
    glOrthof(0, m_windowsz.x, m_windowsz.y, 0, -1000, 1000);
# else
    glOrthox(0, m_windowsz.x, m_windowsz.y, 0, -1000, 1000);
# endif
#else
    glOrtho(0, m_windowsz.x, m_windowsz.y, 0, -1000, 1000);
#endif
    glDebug("glOrthof");
}

void OGLStateManager::pixelstore_unpack_alignment(unsigned int bpp)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);
    glDebug("glPixelStorei");
}

void OGLStateManager::delete_textures(size_t count, unsigned int *textures)
{
    glDeleteTextures(count, (GLuint*)textures);
    glDebug("glDeleteTextures");
}

void OGLStateManager::generate_textures(size_t count, unsigned int *textures)
{
    glGenTextures(count, (GLuint*)textures);
    glDebug("glGenTextures");
}

void OGLStateManager::bind_texture(unsigned int texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glDebug("glBindTexture");
}

void OGLStateManager::load_texture(unsigned char *pixels, unsigned int width,
                                   unsigned int height, MipMapOptions mip_opt,
                                   int xoffset, int yoffset)
{
    // Assumptions...
#ifdef __ANDROID__
    const GLenum bpp = GL_RGBA;
#else
    const unsigned int bpp = 4;
#endif
    const GLenum texture_format = GL_RGBA;
    const GLenum format = GL_UNSIGNED_BYTE;
    // Also assume that the texture is already bound using bind_texture

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDebug("glTexEnvf");

#ifdef GL_CLAMP
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#else
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glDebug("glTexParameterf GL_TEXTURE_WRAP_S");
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glDebug("glTexParameterf GL_TEXTURE_WRAP_T");
#endif
#ifndef USE_GLES
    if (mip_opt == MIPMAP_CREATE)
    {
        // TODO: should min react to Options.tile_filter_scaling?
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        m_mipmapFn != nullptr ? GL_LINEAR_MIPMAP_NEAREST :
                        Options.tile_filter_scaling ? GL_LINEAR :
                        GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        Options.tile_filter_scaling ? GL_LINEAR : GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, bpp, width, height, 0,
                     texture_format, format, pixels);
        // TODO: possibly restructure this into the main block below
        // so that we support mipmapping when glTexSubImage2D should be called.
        if (m_mipmapFn != nullptr)
        {
            PFNGLGENERATEMIPMAPPROC mipmapFn =
                    reinterpret_cast<PFNGLGENERATEMIPMAPPROC>(m_mipmapFn);
            mipmapFn(GL_TEXTURE_2D);
        }
    }
    else
#endif
    {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        Options.tile_filter_scaling ? GL_LINEAR : GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        Options.tile_filter_scaling ? GL_LINEAR : GL_NEAREST);
        if (xoffset >= 0 && yoffset >= 0)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, width, height,
                         texture_format, format, pixels);
            glDebug("glTexSubImage2D");
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, bpp, width, height, 0,
                         texture_format, format, pixels);
            glDebug("glTexImage2D");
        }
    }
}

void OGLStateManager::reset_view_for_redraw()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, 1.0f);
    glDebug("glTranslatef");
}

bool OGLStateManager::glDebug(const char* msg) const
{
#if defined(__ANDROID__) || defined(DEBUG_DIAGNOSTICS)
    int e = glGetError();
    if (e > 0)
    {
# ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "Crawl.gl", "ERROR %x: %s", e, msg);
# else
        fprintf(stderr, "OGLStateManager ERROR %x: %s\n", e, msg);
# endif
        return true;
    }
#else
    UNUSED(msg);
#endif
    return false;
}

/////////////////////////////////////////////////////////////////////////////
// OGLShapeBuffer

OGLShapeBuffer::OGLShapeBuffer(bool texture, bool colour, drawing_modes prim) :
    m_prim_type(prim),
    m_texture_verts(texture),
    m_colour_verts(colour)
{
    ASSERT(prim == GLW_RECTANGLE || prim == GLW_LINES);
}

const char *OGLShapeBuffer::print_statistics() const
{
    return nullptr;
}

unsigned int OGLShapeBuffer::size() const
{
    return m_position_buffer.size();
}

void OGLShapeBuffer::add(const GLWPrim &rect)
{
    switch (m_prim_type)
    {
    case GLW_RECTANGLE:
        add_rect(rect);
        break;
    case GLW_LINES:
        add_line(rect);
        break;
    default:
        die("Invalid primitive type");
        break;
    }
}

void OGLShapeBuffer::add_rect(const GLWPrim &rect)
{
    // Copy vert positions
    size_t last = m_position_buffer.size();
    m_position_buffer.resize(last + 4);
    m_position_buffer[last    ].set(rect.pos_sx, rect.pos_sy, rect.pos_z);
    m_position_buffer[last + 1].set(rect.pos_sx, rect.pos_ey, rect.pos_z);
    m_position_buffer[last + 2].set(rect.pos_ex, rect.pos_sy, rect.pos_z);
    m_position_buffer[last + 3].set(rect.pos_ex, rect.pos_ey, rect.pos_z);

    // Copy texture coords if necessary
    if (m_texture_verts)
    {
        last = m_texture_buffer.size();
        m_texture_buffer.resize(last + 4);
        m_texture_buffer[last    ].set(rect.tex_sx, rect.tex_sy);
        m_texture_buffer[last + 1].set(rect.tex_sx, rect.tex_ey);
        m_texture_buffer[last + 2].set(rect.tex_ex, rect.tex_sy);
        m_texture_buffer[last + 3].set(rect.tex_ex, rect.tex_ey);
    }

    // Copy vert colours if necessary
    if (m_colour_verts)
    {
        last = m_colour_buffer.size();
        m_colour_buffer.resize(last + 4);
        m_colour_buffer[last    ].set(rect.col_s);
        m_colour_buffer[last + 1].set(rect.col_e);
        m_colour_buffer[last + 2].set(rect.col_s);
        m_colour_buffer[last + 3].set(rect.col_e);
    }

    // build indices
    last = m_ind_buffer.size();

    if (last > 3)
    {
        // This is not the first box so make FOUR degenerate triangles
        m_ind_buffer.resize(last + 6);
        unsigned short int val = m_ind_buffer[last - 1];

        // the first three degens finish the previous box and move
        // to the first position of the new one we just added and
        // the fourth degen creates a triangle that is a line from p1 to p3
        m_ind_buffer[last    ] = val++;
        m_ind_buffer[last + 1] = val;

        // Now add as normal
        m_ind_buffer[last + 2] = val++;
        m_ind_buffer[last + 3] = val++;
        m_ind_buffer[last + 4] = val++;
        m_ind_buffer[last + 5] = val;
    }
    else
    {
        // This is the first box so don't bother making any degenerate triangles
        m_ind_buffer.resize(last + 4);
        m_ind_buffer[0] = 0;
        m_ind_buffer[1] = 1;
        m_ind_buffer[2] = 2;
        m_ind_buffer[3] = 3;
    }
}

void OGLShapeBuffer::add_line(const GLWPrim &rect)
{
    // Copy vert positions
    size_t last = m_position_buffer.size();
    m_position_buffer.resize(last + 2);
    m_position_buffer[last    ].set(rect.pos_sx, rect.pos_sy, rect.pos_z);
    m_position_buffer[last + 1].set(rect.pos_ex, rect.pos_ey, rect.pos_z);

    // Copy texture coords if necessary
    if (m_texture_verts)
    {
        last = m_texture_buffer.size();
        m_texture_buffer.resize(last + 2);
        m_texture_buffer[last    ].set(rect.tex_sx, rect.tex_sy);
        m_texture_buffer[last + 1].set(rect.tex_ex, rect.tex_ey);
    }

    // Copy vert colours if necessary
    if (m_colour_verts)
    {
        last = m_colour_buffer.size();
        m_colour_buffer.resize(last + 2);
        m_colour_buffer[last    ].set(rect.col_s);
        m_colour_buffer[last + 1].set(rect.col_e);
    }
}

// Draw the buffer
void OGLShapeBuffer::draw(const GLState &state)
{
    if (m_position_buffer.empty())
        return;

    if (!state.array_vertex)
        return;

    glmanager->set(state);

    glVertexPointer(3, GL_FLOAT, 0, &m_position_buffer[0]);
    glDebug("glVertexPointer");

    if (state.array_texcoord && m_texture_verts)
        glTexCoordPointer(2, GL_FLOAT, 0, &m_texture_buffer[0]);
    glDebug("glTexCoordPointer");

    if (state.array_colour && m_colour_verts)
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, &m_colour_buffer[0]);
    glDebug("glColorPointer");

    switch (m_prim_type)
    {
    case GLW_RECTANGLE:
        glDrawElements(GL_TRIANGLE_STRIP, m_ind_buffer.size(),
                       GL_UNSIGNED_SHORT, &m_ind_buffer[0]);
        break;
    case GLW_LINES:
        glDrawArrays(GL_LINES, 0, m_position_buffer.size());
        break;
    default:
        die("Invalid primitive type");
        break;
    }
    glDebug("glDrawElements");
}

void OGLShapeBuffer::clear()
{
    m_position_buffer.clear();
    m_ind_buffer.clear();
    m_texture_buffer.clear();
    m_colour_buffer.clear();
}

bool OGLShapeBuffer::glDebug(const char* msg) const
{
#if defined(__ANDROID__) || defined(DEBUG_DIAGNOSTICS)
    int e = glGetError();
    if (e > 0)
    {
# ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "Crawl.gl", "ERROR %x: %s", e, msg);
# else
        fprintf(stderr, "OGLShapeBuffer ERROR %x: %s\n", e, msg);
# endif
        return true;
    }
#else
    UNUSED(msg);
#endif
    return false;
}

#endif // USE_GL
#endif // USE_TILE_LOCAL
