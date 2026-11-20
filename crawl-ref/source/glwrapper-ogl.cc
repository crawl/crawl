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
#   include <GLES2/gl2.h>
#  else
#   include <SDL2/SDL.h>
#   include <SDL_gles.h>
#  endif
#  include <GLES/gl.h>
# else
#  include <SDL_opengl.h>
# endif
#  include <SDL_video.h>
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

const char* GLESVersionStringPrefix = "OpenGL ES ";
/////////////////////////////////////////////////////////////////////////////
// OGLStateManager
OGLStateManager::OGLStateManager()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0, 0.0, 0.0, 1.0f);
    glDepthFunc(GL_LEQUAL);

    m_window_height = 0;
    // OpenGL doesn't specify what the GetProcAddress function returns
    // if the implementation does not support a function
    // (e.g. because it doesn't support the OpenGL version in question)
    // So we need to check the version before we try to get the
    // glGenerateMipmap function.
    //
    // Testing notes: if you have a Linux or Nnix system with Mesa,
    // Mesa has environment variables that let you enable or disable extensions
    // or protocol versions (if your computer can support them).
    // See https://docs.mesa3d.org/envvars.html
    // Recommended combinations include:
    // No support for mipmapping:
    // * MESA_GL_VERSION_OVERRIDE=2.1;MESA_EXTENSION_OVERRIDE=GL_ARB_framebuffer_object
    // Simulating pre-OpenGL 3 HW, but with extension:
    // * MESA_GL_VERSION_OVERRIDE=2.1;MESA_EXTENSION_OVERRIDE=-GL_ARB_framebuffer_object
    // Testing on a OpenGL 3 or higher system
    // * check OpenGL core profile version string via glxinfo | grep "version string"
    // GLES _might_ be possible to test too, but I havne't gotten it to work.
    // * MESA_GLES_VERSION_OVERRIDE=2.0
    const char* glVersion = reinterpret_cast<const char*>(
            glGetString(GL_VERSION));
    if (glVersion == nullptr)
    {
        mprf("Mipmap Setup: Failed to load OpenGL version.");
        return;
    }

    // We will never see 2 digit OpenGL or OpenGL ES major versions.
    // * OpenGL 4.6 came out in 2016
    // * OpenGL ES 3.2 came out in 2015
    // Vulkan is carrying the torch now for new development.
    // It'd be a major surprise if we even see OpenGL 5 or OpenGL ES 4.
    // So we won't worry about double digit version numbers
    bool glGenerateMipmapAvailable = false;
    const char* standard;
    if (strncmp(glVersion,
                GLESVersionStringPrefix,
                strlen(GLESVersionStringPrefix)) == 0)
    {
        standard = "OpenGL ES";
        // Unlike OpenGL, which leads with the version number, OpenGL ES leads
        // with OpenGL ES, then the version number.
        // per https://registry.khronos.org/OpenGL-Refpages/es2.0/

        // offset by the prefix
        const char* esVersion = glVersion + strlen(GLESVersionStringPrefix);
        // glGenerateMipmap is supported on OpenGL ES 2+:
        // * https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glGenerateMipmap.xhtml
        bool ok_first_digit = ('2' <= esVersion[0]) && (esVersion[0] <= '9');
        bool ok_second_digit = esVersion[1] == '.' || esVersion[1] == ' ';
        glGenerateMipmapAvailable |= ok_first_digit && ok_second_digit;
    }
    else
    {
        standard = "OpenGL";
        // glGenerateMipmap is supported on OpenGL 3+:
        // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGenerateMipmap.xhtml
        bool ok_first_digit = ('3' <= glVersion[0]) && (glVersion[0] <= '9');
        // Anything other than X.Y or X would be very weird.
        bool ok_second_digit = glVersion[1] == '.' || glVersion[1] == ' ';
        glGenerateMipmapAvailable |= ok_first_digit && ok_second_digit;
    }

#ifdef USE_SDL
    // ARB_framebuffer_object came first, and many old OpenGL implementions
    // support it as an extension even though it's not in OpenGL core.
    // Extension details:
    // https://registry.khronos.org/OpenGL/extensions/ARB/ARB_framebuffer_object.txt
    // Supported Hardware:
    // https://feedback.wildfiregames.com/report/opengl/feature/GL_ARB_framebuffer_object
    // Note that even e.g. Westmere & Sandy Bridge support the extension.
    // Fortunately, we don't have to do more string parsing.
    // We can check via https://wiki.libsdl.org/SDL2/SDL_GL_ExtensionSupported
    bool glGenerateMipmapAvailableViaExtension =
            SDL_GL_ExtensionSupported("GL_ARB_framebuffer_object");
#else
    bool glGenerateMipmapAvailableViaExtension = false;
#endif
    glGenerateMipmapAvailable |= glGenerateMipmapAvailableViaExtension;

    if (!glGenerateMipmapAvailable)
    {
        mprf("Mipmap Setup: Disabled because %s version: %s does not provide "
             "glGenerateMipmap and "
             "extension ARB_framebuffer_object is not present",
             standard, glVersion);
        return;
    }
#ifdef USE_SDL
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
    // But we'll log in case there's a buggy driver.
    m_mipmapFn = SDL_GL_GetProcAddress("glGenerateMipmap");
    //m_mipmapFn = nullptr;
    if (m_mipmapFn == nullptr)
    {
        mprf("Mipmap Setup: Failed to load glGenerateMipmap function.");
        return;
    }
    else
    {
        mprf("Mipmap Setup: success, loaded with OpenGL version: %s "
             "(ARB_framebuffer_object: %s)",
             glVersion,
             glGenerateMipmapAvailableViaExtension ? "TRUE" : "FALSE");
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

void OGLStateManager::load_texture(LoadTextureArgs texture)
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

    const bool should_mipmap = texture.mip_opt == MipMapOptions::CREATE &&
                             m_mipmapFn != nullptr;
    // TODO: should min react to Options.tile_filter_scaling in the
    //  mipmap case? GL_TEXTURE_MIN_FILTER has 4 options that use mipmaps.
    // Note that GL_TEXTURE_MAG_FILTER only has two valid options. Mipmapping
    // is not relevant to that.
    const GLfloat simple_filter_option = Options.tile_filter_scaling ?
            GL_LINEAR : GL_NEAREST;
    const GLfloat min_filter_option = should_mipmap ? GL_LINEAR_MIPMAP_NEAREST:
                                                      simple_filter_option;

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter_option);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, simple_filter_option);

    if (texture.has_offset())
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        texture.xoffset, texture.yoffset,
                        texture.width, texture.height,
                        texture_format, format, texture.pixels);
        glDebug("glTexSubImage2D");
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, bpp,
                     texture.width, texture.height,
                     0,
                     texture_format, format, texture.pixels);
        glDebug("glTexImage2D");
    }

    if (should_mipmap)
    {
        // Note: can't be nullptr because should_mipmap checks that
        PFNGLGENERATEMIPMAPPROC mipmapFn =
                reinterpret_cast<PFNGLGENERATEMIPMAPPROC>(m_mipmapFn);
        mipmapFn(GL_TEXTURE_2D);
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
