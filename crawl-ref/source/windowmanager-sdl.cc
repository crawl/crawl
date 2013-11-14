#include "AppHdr.h"

#ifdef USE_TILE_LOCAL
#ifdef USE_SDL

#include "windowmanager-sdl.h"

#ifdef TARGET_COMPILER_VC
# include <SDL.h>
#else
# include <SDL/SDL.h>
#endif
#include <SDL_image.h>

#include "cio.h"
#include "files.h"
#include "glwrapper.h"
#include "libutil.h"
#include "options.h"
#include "syscalls.h"
#include "version.h"
#include "windowmanager.h"

#ifdef __ANDROID__
#include <android/log.h>
#include <GLES/gl.h>
#include <signal.h>
#include <SDL_mixer.h>
#include <SDL_android.h>
#endif

WindowManager *wm = NULL;

void WindowManager::create()
{
    if (wm)
        return;

    wm = new SDLWrapper();
#ifdef __ANDROID__
    Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3);
    Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 4096);
#endif
}

void WindowManager::shutdown()
{
    delete wm;
    wm = NULL;
#ifdef __ANDROID__
    Mix_CloseAudio();
    while (Mix_Init(0))
        Mix_Quit();
#endif
}

static unsigned char _get_modifiers(SDL_keysym &keysym)
{
    // keysym.mod can't be used to keep track of the modifier state.
    // If shift is hit by itself, this will not include KMOD_SHIFT.
    // Instead, look for the key itself as a separate event.
    switch (keysym.sym)
    {
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
        return MOD_SHIFT;
        break;
    case SDLK_LCTRL:
    case SDLK_RCTRL:
        return MOD_CTRL;
        break;
    case SDLK_LALT:
    case SDLK_RALT:
        return MOD_ALT;
        break;
    default:
        return 0;
    }
}

static int _translate_keysym(SDL_keysym &keysym)
{
    // This function returns the key that was hit.  Returning zero implies that
    // the keypress (e.g. hitting shift on its own) should be eaten and not
    // handled.

    const int shift_offset = CK_SHIFT_UP - CK_UP;
    const int ctrl_offset  = CK_CTRL_UP - CK_UP;

    int mod = 0;
    if (keysym.mod & KMOD_SHIFT)
        mod |= MOD_SHIFT;
    if (keysym.mod & KMOD_CTRL)
        mod |= MOD_CTRL;
    if (keysym.mod & KMOD_LALT)
        mod |= MOD_ALT;

    // This is arbitrary, but here's the current mappings.
    // 0-256: ASCII, Crawl arrow keys
    // 0-1k : Other SDL keys (F1, Windows keys, etc...) and modifiers
    // 1k-3k: Non-ASCII with modifiers other than just shift or just ctrl.
    // 3k+  : ASCII with the left alt modifier.

    int offset = mod ? 1000 + 256 * mod : 0;
    int numpad_offset = 0;
    if (mod == MOD_CTRL)
        numpad_offset = ctrl_offset;
    else if (mod == MOD_SHIFT)
        numpad_offset = shift_offset;
    else
        numpad_offset = offset;

    switch (keysym.sym)
    {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return CK_ENTER + offset;
    case SDLK_BACKSPACE:
        return CK_BKSP + offset;
    case SDLK_ESCAPE:
        return CK_ESCAPE + offset;
    case SDLK_DELETE:
        return CK_DELETE + offset;

#ifdef __ANDROID__
    // i think android's SDL port treats these differently? they certainly
    // shouldn't be interpreted as unicode characters!
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
    case SDLK_LALT:
    case SDLK_RALT:
    case SDLK_LCTRL:
    case SDLK_RCTRL:
#endif
    case SDLK_NUMLOCK:
    case SDLK_CAPSLOCK:
    case SDLK_SCROLLOCK:
    case SDLK_RMETA:
    case SDLK_LMETA:
    case SDLK_LSUPER:
    case SDLK_RSUPER:
    case SDLK_MODE:
    case SDLK_COMPOSE:
        // Don't handle these.
        return 0;

    case SDLK_F1:
    case SDLK_F2:
    case SDLK_F3:
    case SDLK_F4:
    case SDLK_F5:
    case SDLK_F6:
    case SDLK_F7:
    case SDLK_F8:
    case SDLK_F9:
    case SDLK_F10:
    case SDLK_F11:
    case SDLK_F12:
    case SDLK_F13:
    case SDLK_F14:
    case SDLK_F15:
    case SDLK_HELP:
    case SDLK_PRINT:
    case SDLK_SYSREQ:
    case SDLK_BREAK:
    case SDLK_MENU:
    case SDLK_POWER:
    case SDLK_EURO:
    case SDLK_UNDO:
        ASSERT_RANGE(keysym.sym, SDLK_F1, SDLK_UNDO + 1);
        return -(keysym.sym + (SDLK_UNDO - SDLK_F1 + 1) * mod);

        // Hack.  libw32c overloads clear with '5' too.
    case SDLK_KP5:
        return CK_CLEAR + numpad_offset;

    case SDLK_KP8:
    case SDLK_UP:
        return CK_UP + numpad_offset;
    case SDLK_KP2:
    case SDLK_DOWN:
        return CK_DOWN + numpad_offset;
    case SDLK_KP4:
    case SDLK_LEFT:
        return CK_LEFT + numpad_offset;
    case SDLK_KP6:
    case SDLK_RIGHT:
        return CK_RIGHT + numpad_offset;
    case SDLK_KP0:
    case SDLK_INSERT:
        return CK_INSERT + numpad_offset;
    case SDLK_KP7:
    case SDLK_HOME:
        return CK_HOME + numpad_offset;
    case SDLK_KP1:
    case SDLK_END:
        return CK_END + numpad_offset;
    case SDLK_CLEAR:
        return CK_CLEAR + numpad_offset;
    case SDLK_KP9:
    case SDLK_PAGEUP:
        return CK_PGUP + numpad_offset;
    case SDLK_KP3:
    case SDLK_PAGEDOWN:
        return CK_PGDN + numpad_offset;
    case SDLK_TAB:
        if (numpad_offset) // keep tab a tab
            return CK_TAB_TILE + numpad_offset;
#ifdef TOUCH_UI
        break;
    // used for zoom in/out
    case SDLK_KP_PLUS:
        return CK_NUMPAD_PLUS;
    case SDLK_KP_MINUS:
        return CK_NUMPAD_MINUS;
#endif
    default:
        break;
    }

    // Alt does not get baked into keycodes like shift and ctrl, so handle it.
    const int key_offset = (mod & MOD_ALT) ? -3000 : 0;

    const bool is_ascii = keysym.unicode < 127;
    return is_ascii ? (keysym.unicode & 0x7F) + key_offset : keysym.unicode;
}

static void _translate_event(const SDL_MouseMotionEvent &sdl_event,
                             MouseEvent &tile_event)
{
    tile_event.held   = MouseEvent::NONE;
    tile_event.event  = MouseEvent::MOVE;
    tile_event.button = MouseEvent::NONE;
    tile_event.px     = sdl_event.x;
    tile_event.py     = sdl_event.y;

    // TODO: enne - do we want the relative motion?
}

static void _translate_event(const SDL_MouseButtonEvent &sdl_event,
                             MouseEvent &tile_event)
{
    tile_event.held  = MouseEvent::NONE;
    tile_event.event = (sdl_event.type == SDL_MOUSEBUTTONDOWN) ?
    MouseEvent::PRESS : MouseEvent::RELEASE;
    switch (sdl_event.button)
    {
    case SDL_BUTTON_LEFT:
        tile_event.button = MouseEvent::LEFT;
        break;
    case SDL_BUTTON_RIGHT:
        tile_event.button = MouseEvent::RIGHT;
        break;
    case SDL_BUTTON_MIDDLE:
        tile_event.button = MouseEvent::MIDDLE;
        break;
    case SDL_BUTTON_WHEELUP:
        tile_event.button = MouseEvent::SCROLL_UP;
        break;
    case SDL_BUTTON_WHEELDOWN:
        tile_event.button = MouseEvent::SCROLL_DOWN;
        break;
    default:
        // Unhandled button.
        tile_event.button = MouseEvent::NONE;
        break;
    }
    tile_event.px = sdl_event.x;
    tile_event.py = sdl_event.y;
}

SDLWrapper::SDLWrapper():
    m_context(NULL)
{
}

SDLWrapper::~SDLWrapper()
{
    SDL_Quit();
}

#ifdef __ANDROID__
void SDLWrapper::appPutToBackground()
{
    SDL_ANDROID_PauseAudioPlayback();
    raise(SIGHUP);
}

void SDLWrapper::appPutToForeground()
{
    SDL_ANDROID_ResumeAudioPlayback();
    // re-init
    wm->swap_buffers();
}
#endif

int SDLWrapper::init(coord_def *m_windowsz)
{
#ifdef __ANDROID__
    // set up callbacks for background/foreground events
    SDL_ANDROID_SetApplicationPutToBackgroundCallback(
            &SDLWrapper::appPutToBackground, &SDLWrapper::appPutToForeground);
    // Do SDL initialization
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
#else
    // Do SDL initialization
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
#endif
    {
        printf("Failed to initialise SDL: %s\n", SDL_GetError());
        return false;
    }

    video_info = SDL_GetVideoInfo();

    _desktop_width = video_info->current_w;
    _desktop_height = video_info->current_h;

#if !SDL_VERSION_ATLEAST(2,0,0)
    SDL_EnableUNICODE(true);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    glDebug("SDL_GL_DOUBLEBUFFER");
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 8);
    glDebug("SDL_GL_DEPTH_SIZE 8");
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    glDebug("SDL_GL_RED_SIZE 8");
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    glDebug("SDL_GL_GREEN_SIZE 8");
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    glDebug("SDL_GL_BLUE_SIZE 8");
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    glDebug("SDL_GL_ALPHA_SIZE 8");

#if !SDL_VERSION_ATLEAST(2,0,0)
    if (Options.tile_key_repeat_delay > 0)
    {
        const int repdelay    = Options.tile_key_repeat_delay;
        const int interval = SDL_DEFAULT_REPEAT_INTERVAL;
        if (SDL_EnableKeyRepeat(repdelay, interval) != 0)
#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_INFO, "Crawl",
                                "Failed to set key repeat mode: %s",
                                SDL_GetError());
#else
            printf("Failed to set key repeat mode: %s\n", SDL_GetError());
#endif
    }
#endif

#ifdef USE_GLES
#ifdef __ANDROID__
    unsigned int flags = SDL_OPENGL | SDL_FULLSCREEN | SDL_INIT_NOPARACHUTE;
#else
    unsigned int flags = SDL_SWSURFACE;
#endif
#else
    unsigned int flags = SDL_OPENGL;
#endif

    bool too_small = (video_info->current_w < 1024 || video_info->current_h < 800);
    if (Options.tile_full_screen == SCREENMODE_FULL
        || too_small && Options.tile_full_screen == SCREENMODE_AUTO)
    {
        flags |= SDL_FULLSCREEN;
    }

    if (flags & SDL_FULLSCREEN)
    {
        // By default, fill the whole screen.
        m_windowsz->x = video_info->current_w;
        m_windowsz->y = video_info->current_h;
    }
    else
    {
        int x = Options.tile_window_width;
        int y = Options.tile_window_height;
        x = (x > 0) ? x : video_info->current_w + x;
        y = (y > 0) ? y : video_info->current_h + y;
#ifdef TOUCH_UI
        // allow *much* smaller windows than default, primarily for testing
        // touch_ui features in an x86 build
        m_windowsz->x = x;
        m_windowsz->y = y;
#else
        m_windowsz->x = max(800, x);
        m_windowsz->y = max(480, y);
#endif

#ifdef TARGET_OS_WINDOWS
        set_window_placement(m_windowsz);
#endif
    }

    m_context = SDL_SetVideoMode(m_windowsz->x, m_windowsz->y, 0, flags);
glDebug("SDL_SetVideoMode");
    if (!m_context)
    {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "Crawl",
                            "Failed to set video mode: %s", SDL_GetError());
#endif
        printf("Failed to set video mode: %s\n", SDL_GetError());
        return false;
    }

#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "Crawl", "Window manager initialised");
#endif
    return true;
}

int SDLWrapper::screen_width() const
{
    return video_info->current_w;
}

int SDLWrapper::screen_height() const
{
    return video_info->current_h;
}

int SDLWrapper::desktop_width() const
{
    return _desktop_width;
}

int SDLWrapper::desktop_height() const
{
    return _desktop_height;
}

void SDLWrapper::set_window_title(const char *title)
{
    SDL_WM_SetCaption(title, CRAWL);
}

bool SDLWrapper::set_window_icon(const char* icon_name)
{
    SDL_Surface *surf =load_image(datafile_path(icon_name, true, true).c_str());
    if (!surf)
    {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "Crawl",
                            "Failed to load icon: %s", SDL_GetError());
#endif
        printf("Failed to load icon: %s\n", SDL_GetError());
        return false;
    }
    SDL_WM_SetIcon(surf, NULL);
    SDL_FreeSurface(surf);
    return true;
}

#ifdef TARGET_OS_WINDOWS
void SDLWrapper::set_window_placement(coord_def *m_windowsz)
{
    // We move the window if it overlaps the taskbar.
    const int title_bar = 29; // Title bar
    const int border = 3; // Window border
    int delta_x = (wm->desktop_width() - m_windowsz->x) / 2;
    int delta_y = (wm->desktop_height() - m_windowsz->y) / 2;
    taskbar_pos tpos = get_taskbar_pos();
    int tsize = get_taskbar_size();

    if (tpos == TASKBAR_TOP)
        tsize += title_bar;
    else
        tsize += border;

    int overlap = tsize - (tpos & TASKBAR_H ? delta_y : delta_x);

    if (overlap > 0)
    {
        char env_str[50];
        int x = delta_x;
        int y = delta_y;

        if (tpos & TASKBAR_H)
            y += tpos == TASKBAR_TOP ? overlap : -overlap;
        else
            x += tpos == TASKBAR_LEFT ? overlap : -overlap;

        // Keep the window in the screen.
        x = max(x, border);
        y = max(y, title_bar);

        //We resize the window so that it fits in the screen.
        m_windowsz->x = min(m_windowsz->x, wm->desktop_width()
                            - (tpos & TASKBAR_V ? tsize : 0)
                            - border * 2);
        m_windowsz->y = min(m_windowsz->y, wm->desktop_height()
                            - (tpos & TASKBAR_H ? tsize : 0)
                            - (tpos & TASKBAR_TOP ? 0 : title_bar)
                            - border);
        sprintf(env_str, "SDL_VIDEO_WINDOW_POS=%d,%d", x, y);
        putenv(env_str);
    }
    else
    {
        putenv("SDL_VIDEO_WINDOW_POS=center");
        putenv("SDL_VIDEO_CENTERED=1");
    }
}
#endif

void SDLWrapper::resize(coord_def &m_windowsz)
{
    glmanager->reset_view_for_resize(m_windowsz);
}

unsigned int SDLWrapper::get_ticks() const
{
    return SDL_GetTicks();
}

key_mod SDLWrapper::get_mod_state() const
{
    SDLMod mod = SDL_GetModState();

    switch (mod)
    {
    case KMOD_LSHIFT:
    case KMOD_RSHIFT:
        return MOD_SHIFT;
        break;
    case KMOD_LCTRL:
    case KMOD_RCTRL:
        return MOD_CTRL;
        break;
    case KMOD_LALT:
    case KMOD_RALT:
        return MOD_ALT;
        break;
    case KMOD_NONE:
    default:
        return MOD_NONE;
    }
}

void SDLWrapper::set_mod_state(key_mod mod)
{
    SDLMod set_to;
    switch (mod)
    {
    case MOD_NONE:
        set_to = KMOD_NONE;
        break;
    case MOD_SHIFT:
        set_to = KMOD_LSHIFT;
        break;
    case MOD_CTRL:
        set_to = KMOD_LCTRL;
        break;
    case MOD_ALT:
        set_to = KMOD_LALT;
        break;
    default:
        set_to = KMOD_NONE;
        break;
    }

    SDL_SetModState(set_to);
}

int SDLWrapper::wait_event(wm_event *event)
{
    SDL_Event sdlevent;
    if (!SDL_WaitEvent(&sdlevent))
        return 0;

#ifdef __ANDROID__
    while (sdlevent.type > SDL_VIDEOEXPOSE) // last credible event type in SDL_events.h
        if (!SDL_WaitEvent(&sdlevent)) return 0;
#endif

    // translate the SDL_Event into the almost-analogous wm_event
    switch (sdlevent.type)
    {
    case SDL_ACTIVEEVENT:
        SDL_SetModState(KMOD_NONE);
        event->type = WME_ACTIVEEVENT;
        event->active.gain = sdlevent.active.gain;
        event->active.state = sdlevent.active.state;
        break;
    case SDL_KEYDOWN:
        event->type = WME_KEYDOWN;
        event->key.state = sdlevent.key.state;
        event->key.keysym.scancode = sdlevent.key.keysym.scancode;
        event->key.keysym.key_mod = _get_modifiers(sdlevent.key.keysym);
        event->key.keysym.unicode = sdlevent.key.keysym.unicode;
        event->key.keysym.sym = _translate_keysym(sdlevent.key.keysym);

        if (!event->key.keysym.unicode && event->key.keysym.sym > 0)
            return 0;

/*
 * LShift = scancode 0x30; key_mod 0x1; unicode 0x130; sym 0x130 SDLK_LSHIFT
 * LCtrl  = scancode 0x32; key_mod 0x2; unicode 0x132; sym 0x132 SDLK_LCTRL
 * LAlt   = scancode 0x34; key_mod 0x4; unicode 0x134; sym 0x134 SDLK_LALT
 */
        break;
    case SDL_KEYUP:
        event->type = WME_KEYUP;
        event->key.state = sdlevent.key.state;
        event->key.keysym.scancode = sdlevent.key.keysym.scancode;
        event->key.keysym.key_mod = _get_modifiers(sdlevent.key.keysym);
        event->key.keysym.unicode = sdlevent.key.keysym.unicode;
        event->key.keysym.sym = _translate_keysym(sdlevent.key.keysym);

        break;
    case SDL_MOUSEMOTION:
        event->type = WME_MOUSEMOTION;
        _translate_event(sdlevent.motion, event->mouse_event);
        break;
    case SDL_MOUSEBUTTONUP:
        event->type = WME_MOUSEBUTTONUP;
        _translate_event(sdlevent.button, event->mouse_event);
        break;
    case SDL_MOUSEBUTTONDOWN:
        event->type = WME_MOUSEBUTTONDOWN;
        _translate_event(sdlevent.button, event->mouse_event);
        break;
    case SDL_VIDEORESIZE:
        event->type = WME_RESIZE;
        event->resize.w = sdlevent.resize.w;
        event->resize.h = sdlevent.resize.h;
        break;
    case SDL_VIDEOEXPOSE:
        event->type = WME_EXPOSE;
        break;
    case SDL_QUIT:
        event->type = WME_QUIT;
        break;

    // I leave these as the same, because the original tilesdl does, too
    case SDL_USEREVENT:
    default:
        event->type = WME_CUSTOMEVENT;
        event->custom.code = sdlevent.user.code;
        event->custom.data1 = sdlevent.user.data1;
        event->custom.data2 = sdlevent.user.data2;
        break;
    }

    return 1;
}

void SDLWrapper::set_timer(unsigned int interval, wm_timer_callback callback)
{
    SDL_SetTimer(interval, callback);
}

int SDLWrapper::raise_custom_event()
{
    SDL_Event send_event;
    send_event.type = SDL_USEREVENT;
    return SDL_PushEvent(&send_event);
}

void SDLWrapper::swap_buffers()
{
    SDL_GL_SwapBuffers();
}

void SDLWrapper::delay(unsigned int ms)
{
    SDL_Delay(ms);
}

unsigned int SDLWrapper::get_event_count(wm_event_type type)
{
    // Look for the presence of any keyboard events in the queue.
    Uint32 eventmask;
    switch (type)
    {
    case WME_ACTIVEEVENT:
        eventmask = SDL_EVENTMASK(SDL_ACTIVEEVENT);
        break;

    case WME_KEYDOWN:
        eventmask = SDL_EVENTMASK(SDL_KEYDOWN);
        break;

    case WME_KEYUP:
        eventmask = SDL_EVENTMASK(SDL_KEYUP);
        break;

    case WME_MOUSEMOTION:
        eventmask = SDL_EVENTMASK(SDL_MOUSEMOTION);
        break;

    case WME_MOUSEBUTTONUP:
        eventmask = SDL_EVENTMASK(SDL_MOUSEBUTTONUP);
        break;

    case WME_MOUSEBUTTONDOWN:
        eventmask = SDL_EVENTMASK(SDL_MOUSEBUTTONDOWN);
        break;

    case WME_QUIT:
        eventmask = SDL_EVENTMASK(SDL_QUIT);
        break;

    case WME_CUSTOMEVENT:
        eventmask = SDL_EVENTMASK(SDL_USEREVENT);
        break;

    case WME_RESIZE:
        eventmask = SDL_EVENTMASK(SDL_VIDEORESIZE);
        break;

    case WME_EXPOSE:
        eventmask = SDL_EVENTMASK(SDL_VIDEOEXPOSE);
        break;

    default:
        // Error
        return 0;
    }

    SDL_Event store;
    SDL_PumpEvents();

    // Note: this returns -1 for error.
    int count = SDL_PeepEvents(&store, 1, SDL_PEEKEVENT, eventmask);
    ASSERT(count >= 0);

    return max(count, 0);
}

bool SDLWrapper::load_texture(GenericTexture *tex, const char *filename,
                              MipMapOptions mip_opt, unsigned int &orig_width,
                              unsigned int &orig_height, tex_proc_func proc,
                              bool force_power_of_two)
{
    char acBuffer[512];

    string tex_path = datafile_path(filename);

    if (tex_path.c_str()[0] == 0)
    {
        fprintf(stderr, "Couldn't find texture '%s'.\n", filename);
        return false;
    }

    SDL_Surface *img = load_image(tex_path.c_str());

    if (!img)
    {
        fprintf(stderr, "Couldn't load texture '%s'.\n", tex_path.c_str());
        return false;
    }

    unsigned int bpp = img->format->BytesPerPixel;
    glmanager->pixelstore_unpack_alignment(bpp);

    // Determine texture format
    unsigned char *pixels = (unsigned char*)img->pixels;

    int new_width;
    int new_height;
    if (force_power_of_two)
    {
        new_width = 1;
        while (new_width < img->w)
            new_width *= 2;

        new_height = 1;
        while (new_height < img->h)
            new_height *= 2;
    }
    else
    {
        new_width = img->w;
        new_height = img->h;
    }

    // GLenum texture_format
    if (bpp == 4)
    {
        // Even if the size is the same, still go through
        // SDL_GetRGBA to put the image in the right format.
        SDL_LockSurface(img);
        pixels = new unsigned char[4 * new_width * new_height];
        memset(pixels, 0, 4 * new_width * new_height);

        int dest = 0;
        for (int y = 0; y < img->h; y++)
        {
            for (int x = 0; x < img->w; x++)
            {
                unsigned char *p = ((unsigned char*)img->pixels
                                  + y * img->pitch + x * bpp);
                unsigned int pixel = *(unsigned int*)p;
                SDL_GetRGBA(pixel, img->format, &pixels[dest],
                            &pixels[dest+1], &pixels[dest+2],
                            &pixels[dest+3]);
                dest += 4;
            }
            dest += 4 * (new_width - img->w);
        }

        SDL_UnlockSurface(img);
    }
    else if (bpp == 3)
    {
        if (new_width != img->w || new_height != img->h)
        {
            SDL_LockSurface(img);
            pixels = new unsigned char[4 * new_width * new_height];
            memset(pixels, 0, 4 * new_width * new_height);

            int dest = 0;
            for (int y = 0; y < img->h; y++)
            {
                for (int x = 0; x < img->w; x++)
                {
                    unsigned char *p = ((unsigned char*)img->pixels
                                       + y * img->pitch + x * bpp);
                    unsigned int pixel;
                    if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                        pixel = p[0] << 16 | p[1] << 8 | p[2];
                    else
                        pixel = p[0] | p[1] << 8 | p[2] << 16;
                    SDL_GetRGBA(pixel, img->format, &pixels[dest],
                                &pixels[dest+1], &pixels[dest+2],
                                &pixels[dest+3]);
                    dest += 4;
                }
                dest += 4 * (new_width - img->w);
            }

            SDL_UnlockSurface(img);
        }
    }
    else if (bpp == 1)
    {
        // need to depalettize
        SDL_LockSurface(img);

        pixels = new unsigned char[4 * new_width * new_height];

        SDL_Palette* pal = img->format->palette;
        ASSERT(pal);
        ASSERT(pal->colors);

        int src = 0;
        int dest = 0;
        for (int y = 0; y < img->h; y++)
        {
            int x;
            for (x = 0; x < img->w; x++)
            {
                unsigned int index = ((unsigned char*)img->pixels)[src++];
                pixels[dest*4    ] = pal->colors[index].r;
                pixels[dest*4 + 1] = pal->colors[index].g;
                pixels[dest*4 + 2] = pal->colors[index].b;
                pixels[dest*4 + 3] = (index != img->format->colorkey ? 255 : 0);
                dest++;
            }
            while (x++ < new_width)
            {
                // Extend to the right with transparent pixels
                pixels[dest*4    ] = 0;
                pixels[dest*4 + 1] = 0;
                pixels[dest*4 + 2] = 0;
                pixels[dest*4 + 3] = 0;
                dest++;
            }
        }
        while (dest < new_width * new_height)
        {
            // Extend down with transparent pixels
            pixels[dest*4    ] = 0;
            pixels[dest*4 + 1] = 0;
            pixels[dest*4 + 2] = 0;
            pixels[dest*4 + 3] = 0;
            dest++;
        }

        SDL_UnlockSurface(img);
    }
    else
    {
        printf("Warning: unsupported format, bpp = %d for '%s'\n",
               bpp, acBuffer);
        return false;
    }

    bool success = false;
    if (!proc || proc(pixels, new_width, new_height))
        success |= tex->load_texture(pixels, new_width, new_height, mip_opt);

    // If conversion has occurred, delete converted data.
    if (pixels != img->pixels)
        delete[] pixels;

    orig_width  = img->w;
    orig_height = img->h;

    SDL_FreeSurface(img);

    return success;
}

SDL_Surface *SDLWrapper::load_image(const char *file) const
{
    SDL_Surface *surf = NULL;
    FILE *imgfile = fopen_u(file, "rb");
    if (imgfile)
    {
        SDL_RWops *rw = SDL_RWFromFP(imgfile, 0);
        if (rw)
        {
            surf = IMG_Load_RW(rw, 0);
            SDL_RWclose(rw);
        }
        fclose(imgfile);
    }

    if (!surf)
        return NULL;

    return surf;
}

void SDLWrapper::glDebug(const char* msg)
{
#ifdef __ANDROID__
    int e = glGetError();
    if (e > 0)
       __android_log_print(ANDROID_LOG_INFO, "Crawl", "ERROR %x: %s", e, msg);
#endif
}
#endif // USE_SDL
#endif // USE_TILE_LOCAL
