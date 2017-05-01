#include "AppHdr.h"

#ifdef USE_TILE_LOCAL
#ifdef USE_SDL

#include "windowmanager-sdl.h"

#include "sound.h"      // Needs to be here because WINMM_PLAY_SOUNDS is used below

#ifdef __ANDROID__
# include <SDL.h>
# include <SDL_image.h>
# include <android/log.h>
# include <GLES/gl.h>
# include <signal.h>
# include <SDL_mixer.h>
#else
# ifdef TARGET_COMPILER_VC
#  include <SDL.h>
# else
#  include <SDL2/SDL.h>
# endif
# include <SDL2/SDL_image.h>
# if defined(USE_SOUND) && !defined(WINMM_PLAY_SOUNDS)
#  include <SDL2/SDL_mixer.h>
# endif
#endif

#include "cio.h"
#include "files.h"
#include "glwrapper.h"
#include "libutil.h"
#include "options.h"
#include "syscalls.h"
#include "unicode.h"
#include "version.h"
#include "windowmanager.h"

WindowManager *wm = nullptr;

#define MIN_SDL_WINDOW_SIZE_X 800
#define MIN_SDL_WINDOW_SIZE_Y 480

void WindowManager::create()
{
    if (wm)
        return;

    wm = new SDLWrapper();
#if defined(USE_SOUND) && !defined(WINMM_PLAY_SOUNDS)
    Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3);
    Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 4096);
#endif
}

void WindowManager::shutdown()
{
    delete wm;
    wm = nullptr;
#if defined(USE_SOUND) && !defined(WINMM_PLAY_SOUNDS)
    Mix_CloseAudio();
    while (Mix_Init(0))
        Mix_Quit();
#endif
}

static unsigned char _kmod_to_mod(int modifier)
{
    unsigned char mod = 0;
    if (modifier & KMOD_SHIFT)
        mod |= MOD_SHIFT;
    if (modifier & KMOD_CTRL)
        mod |= MOD_CTRL;
    if (modifier & KMOD_LALT)
        mod |= MOD_ALT;
    return mod;
}

static unsigned char _get_modifiers(SDL_Keysym &keysym)
{
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
        return _kmod_to_mod(keysym.mod);
    }
}

static void _translate_window_event(const SDL_WindowEvent &sdl_event,
                                    wm_event &tile_event)
{
    switch (sdl_event.event)
    {
        case SDL_WINDOWEVENT_SHOWN:
            tile_event.type = WME_ACTIVEEVENT;
            tile_event.active.gain = 1;
            break;
        case SDL_WINDOWEVENT_HIDDEN:
            tile_event.type = WME_ACTIVEEVENT;
            tile_event.active.gain = 0;
            break;
        case SDL_WINDOWEVENT_EXPOSED:
            tile_event.type = WME_EXPOSE;
            break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            tile_event.type = WME_RESIZE;
            tile_event.resize.w = sdl_event.data1;
            tile_event.resize.h = sdl_event.data2;
            break;
        default:
            tile_event.type = WME_NOEVENT;
            break;
    }
}

// Suppress the SDL_TEXTINPUT event from this keypress. XXX: hacks
static void _suppress_textinput()
{
    if (SDL_IsTextInputActive())
    {
        SDL_StopTextInput();
        SDL_StartTextInput();
    }
}

static int _translate_keysym(SDL_Keysym &keysym)
{
    // This function returns the key that was hit. Returning zero implies that
    // the keypress (e.g. hitting shift on its own) should be eaten and not
    // handled.

    const int shift_offset = CK_SHIFT_UP - CK_UP;
    const int ctrl_offset  = CK_CTRL_UP - CK_UP;

    const int mod = _get_modifiers(keysym);

#ifdef TARGET_OS_WINDOWS
    // AltGr looks like right alt + left ctrl on Windows. Let the input
    // method geneate a TextInput event rather than trying to handle it
    // as a KeyDown.
    if (testbits(keysym.mod, KMOD_RALT | KMOD_LCTRL))
        return 0;
#endif

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
    case SDLK_AC_BACK:
    case SDLK_ESCAPE:
        return CK_ESCAPE + offset;
    case SDLK_DELETE:
    case SDLK_KP_PERIOD:
        return CK_DELETE + offset;

    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
    case SDLK_LCTRL:
    case SDLK_RCTRL:
    case SDLK_LALT:
    case SDLK_RALT:
    case SDLK_LGUI:
    case SDLK_RGUI:
    case SDLK_NUMLOCKCLEAR:
    case SDLK_CAPSLOCK:
    case SDLK_SCROLLLOCK:
    case SDLK_MODE:
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
    case SDLK_PRINTSCREEN:
    case SDLK_SYSREQ:
    case SDLK_PAUSE:
    case SDLK_MENU:
    case SDLK_POWER:
    case SDLK_UNDO:
        ASSERT_RANGE(keysym.sym, SDLK_F1, SDLK_UNDO + 1);
        return -(keysym.sym + (SDLK_UNDO - SDLK_F1 + 1) * mod);

        // Hack. libw32c overloads clear with '5' too.
    case SDLK_KP_5:
        return CK_CLEAR + numpad_offset;

    case SDLK_KP_8:
    case SDLK_UP:
        return CK_UP + numpad_offset;
    case SDLK_KP_2:
    case SDLK_DOWN:
        return CK_DOWN + numpad_offset;
    case SDLK_KP_4:
    case SDLK_LEFT:
        return CK_LEFT + numpad_offset;
    case SDLK_KP_6:
    case SDLK_RIGHT:
        return CK_RIGHT + numpad_offset;
    case SDLK_KP_0:
    case SDLK_INSERT:
        return CK_INSERT + numpad_offset;
    case SDLK_KP_7:
    case SDLK_HOME:
        return CK_HOME + numpad_offset;
    case SDLK_KP_1:
    case SDLK_END:
        return CK_END + numpad_offset;
    case SDLK_CLEAR:
        return CK_CLEAR + numpad_offset;
    case SDLK_KP_9:
    case SDLK_PAGEUP:
        return CK_PGUP + numpad_offset;
    case SDLK_KP_3:
    case SDLK_PAGEDOWN:
        return CK_PGDN + numpad_offset;
    case SDLK_TAB:
        if (numpad_offset) // keep tab a tab
            return CK_TAB_TILE + numpad_offset;
        return SDLK_TAB;
#ifdef TOUCH_UI
    // used for zoom in/out
    case SDLK_KP_PLUS:
        return CK_NUMPAD_PLUS;
    case SDLK_KP_MINUS:
        return CK_NUMPAD_MINUS;
#endif
    default:
        break;
    }

    if (!(mod & (MOD_CTRL | MOD_ALT)))
        return 0;

    int ret = keysym.sym;

    if (mod & MOD_ALT && keysym.sym >= 128)
        ret -= 3000; // ???
    if (mod & MOD_CTRL)
        ret -= SDLK_a - 1; // XXX: this might break for strange keysyms

    return ret;
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
    default:
        // Unhandled button.
        tile_event.button = MouseEvent::NONE;
        break;
    }
    tile_event.px = sdl_event.x;
    tile_event.py = sdl_event.y;
}

static void _translate_wheel_event(const SDL_MouseWheelEvent &sdl_event,
                                   MouseEvent &tile_event)
{
    tile_event.held  = MouseEvent::NONE;
    tile_event.event = MouseEvent::PRESS; // XXX
    tile_event.button = (sdl_event.y > 0) ? MouseEvent::SCROLL_DOWN
                                          : MouseEvent::SCROLL_UP;
    tile_event.px = sdl_event.x;
    tile_event.py = sdl_event.y;
}

SDLWrapper::SDLWrapper():
    m_window(nullptr), m_context(nullptr)
{
}

SDLWrapper::~SDLWrapper()
{
    if (m_context)
        SDL_GL_DeleteContext(m_context);
    if (m_window)
        SDL_DestroyWindow(m_window);
    SDL_Quit();
}

int SDLWrapper::init(coord_def *m_windowsz, int *densityNum, int *densityDen)
{
#ifdef __ANDROID__
    // Do SDL initialization
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER
                 | SDL_INIT_NOPARACHUTE) != 0)
#else
    // Do SDL initialization
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
#endif
    {
        printf("Failed to initialise SDL: %s\n", SDL_GetError());
        return false;
    }

    SDL_DisplayMode display_mode;
    SDL_GetDesktopDisplayMode(0, &display_mode);

    _desktop_width = display_mode.w;
    _desktop_height = display_mode.h;

#ifdef __ANDROID__
    // Request OpenGL ES 1.0 context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
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

    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");

#ifdef USE_GLES
#ifdef __ANDROID__
    unsigned int flags = SDL_WINDOW_OPENGL
                         | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#else
    unsigned int flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#endif
#else
    unsigned int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
                         | SDL_WINDOW_ALLOW_HIGHDPI;
#endif

    bool too_small = (_desktop_width < 1024 || _desktop_height < 800);
    if (Options.tile_full_screen == SCREENMODE_FULL
        || too_small && Options.tile_full_screen == SCREENMODE_AUTO)
    {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    if (flags & SDL_WINDOW_FULLSCREEN)
    {
        const int x = Options.tile_window_width;
        const int y = Options.tile_window_height;
        // By default, fill the whole screen.
        m_windowsz->x = (x > 0) ? x : _desktop_width + x;
        m_windowsz->y = (y > 0) ? y : _desktop_height + y;
    }
    else
    {
        int x = Options.tile_window_width;
        int y = Options.tile_window_height;
        x = (x > 0) ? x : _desktop_width + x;
        y = (y > 0) ? y : _desktop_height + y;
#ifdef TOUCH_UI
        // allow *much* smaller windows than default, primarily for testing
        // touch_ui features in an x86 build
        m_windowsz->x = x;
        m_windowsz->y = y;
#else
        m_windowsz->x = max(MIN_SDL_WINDOW_SIZE_X, x);
        m_windowsz->y = max(MIN_SDL_WINDOW_SIZE_Y, y);
#endif

#ifdef TARGET_OS_WINDOWS
        set_window_placement(m_windowsz);
#endif
    }

    string title = string(CRAWL " ") + Version::Long;
    m_window = SDL_CreateWindow(title.c_str(),
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                m_windowsz->x, m_windowsz->y, flags);
    glDebug("SDL_CreateWindow");
    if (!m_window)
    {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "Crawl",
                            "Failed to create window: %s", SDL_GetError());
#endif
        printf("Failed to create window: %s\n", SDL_GetError());
        return false;
    }

    m_context = SDL_GL_CreateContext(m_window);
    glDebug("SDL_GL_CreateContext");
    if (!m_context)
    {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "Crawl",
                            "Failed to create GL context: %s", SDL_GetError());
#endif
        printf("Failed to create GL context: %s\n", SDL_GetError());
        return false;
    }

    int x, y;
    SDL_GetWindowSize(m_window, &x, &y);
    m_windowsz->x = x;
    m_windowsz->y = y;
#ifdef __ANDROID__
# ifndef TOUCH_UI
    SDL_StartTextInput();
# endif
    __android_log_print(ANDROID_LOG_INFO, "Crawl", "Window manager initialised");
#endif

    SDL_GL_GetDrawableSize(m_window, &x, &y);
    *densityNum = x;
    *densityDen = m_windowsz->x;
    SDL_SetWindowMinimumSize(m_window, MIN_SDL_WINDOW_SIZE_X,
                             MIN_SDL_WINDOW_SIZE_Y);
    return true;
}

int SDLWrapper::screen_width() const
{
    int w, dummy;
    SDL_GetWindowSize(m_window, &w, &dummy);
    return w;
}

int SDLWrapper::screen_height() const
{
    int dummy, h;
    SDL_GetWindowSize(m_window, &dummy, &h);
    return h;
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
    SDL_SetWindowTitle(m_window, title);
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
    SDL_SetWindowIcon(m_window, surf);
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
    coord_def m_drawablesz;
    SDL_GL_GetDrawableSize(m_window, &(m_drawablesz.x), &(m_drawablesz.y));
    glmanager->reset_view_for_resize(m_windowsz, m_drawablesz);
}

unsigned int SDLWrapper::get_ticks() const
{
    return SDL_GetTicks();
}

key_mod SDLWrapper::get_mod_state() const
{
    SDL_Keymod mod = SDL_GetModState();

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
    SDL_Keymod set_to;
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

    // translate the SDL_Event into the almost-analogous wm_event
    switch (sdlevent.type)
    {
    case SDL_WINDOWEVENT:
        SDL_SetModState(KMOD_NONE);
        _translate_window_event(sdlevent.window, *event);
        break;
    case SDL_KEYDOWN:
        if (Options.tile_key_repeat_delay <= 0 && sdlevent.key.repeat != 0)
            return 0;
        event->type = WME_KEYDOWN;
        event->key.state = sdlevent.key.state;
        event->key.keysym.scancode = sdlevent.key.keysym.scancode;
        event->key.keysym.key_mod = _get_modifiers(sdlevent.key.keysym);
        event->key.keysym.unicode = sdlevent.key.keysym.sym; // ???
        event->key.keysym.sym = _translate_keysym(sdlevent.key.keysym);

        if (!event->key.keysym.unicode && event->key.keysym.sym > 0)
            return 0;

        // If we're going to accept this keydown, don't generate subsequent
        // textinput events for the same key.
        if (event->key.keysym.sym)
            _suppress_textinput();

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
        event->key.keysym.unicode = sdlevent.key.keysym.sym; // ???
        event->key.keysym.sym = _translate_keysym(sdlevent.key.keysym);

        break;
    case SDL_TEXTINPUT:
    {
        event->type = WME_KEYPRESS;
        // XXX: handle multiple keys?
        char32_t wc;
        utf8towc(&wc, sdlevent.text.text);
        event->key.keysym.sym = wc;
        break;
    }
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
    case SDL_MOUSEWHEEL:
        event->type = WME_MOUSEBUTTONDOWN; // XXX
        _translate_wheel_event(sdlevent.wheel, event->mouse_event);
        break;
    case SDL_QUIT:
        event->type = WME_QUIT;
        break;

    // I leave these as the same, because the original tilesdl does, too
    case SDL_USEREVENT:
        event->type = WME_CUSTOMEVENT;
        event->custom.code = sdlevent.user.code;
        event->custom.data1 = sdlevent.user.data1;
        event->custom.data2 = sdlevent.user.data2;
        break;

    default:
        return 0;
    }

    return 1;
}

unsigned int SDLWrapper::set_timer(unsigned int interval,
                                   wm_timer_callback callback)
{
    return SDL_AddTimer(interval, callback, nullptr);
}

void SDLWrapper::remove_timer(unsigned int timer_id)
{
    SDL_RemoveTimer(timer_id);
}

int SDLWrapper::raise_custom_event()
{
    SDL_Event send_event;
    send_event.type = SDL_USEREVENT;
    return SDL_PushEvent(&send_event);
}

void SDLWrapper::swap_buffers()
{
    SDL_GL_SwapWindow(m_window);
}

void SDLWrapper::delay(unsigned int ms)
{
    SDL_Delay(ms);
}

unsigned int SDLWrapper::get_event_count(wm_event_type type)
{
    // Look for the presence of any keyboard events in the queue.
    Uint32 event;
    switch (type)
    {
    case WME_ACTIVEEVENT:
    case WME_RESIZE: // XXX
    case WME_EXPOSE: // XXX
        event = SDL_WINDOWEVENT;
        break;

    case WME_KEYDOWN:
        event = SDL_KEYDOWN;
        break;

    case WME_KEYUP:
        event = SDL_KEYUP;
        break;

    case WME_KEYPRESS:
        event = SDL_TEXTINPUT;
        break;

    case WME_MOUSEMOTION:
        event = SDL_MOUSEMOTION;
        break;

    case WME_MOUSEBUTTONUP:
        event = SDL_MOUSEBUTTONUP;
        break;

    case WME_MOUSEBUTTONDOWN:
        event = SDL_MOUSEBUTTONDOWN;
        break;

    case WME_QUIT:
        event = SDL_QUIT;
        break;

    case WME_CUSTOMEVENT:
        event = SDL_USEREVENT;
        break;

    default:
        // Error
        return 0;
    }

    SDL_Event store;
    SDL_PumpEvents();

    // Note: this returns -1 for error.
    int count = SDL_PeepEvents(&store, 1, SDL_PEEKEVENT, event, event);
    ASSERT(count >= 0);

    return max(count, 0);
}

void SDLWrapper::show_keyboard()
{
    SDL_StartTextInput(); // XXX: Intended for Android; harmless elsewhere?
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
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "Crawl",
                            "Couldn't find texture '%s'.", filename);
#endif
        fprintf(stderr, "Couldn't find texture '%s'.\n", filename);
        return false;
    }

    SDL_Surface *img = load_image(tex_path.c_str());

    if (!img)
    {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "Crawl",
                            "Couldn't load texture '%s'.", tex_path.c_str());
#endif
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
                pixels[dest*4 + 3] = pal->colors[index].a;
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
    SDL_Surface *surf = nullptr;
    FILE *imgfile = fopen_u(file, "rb");
    if (imgfile)
    {
        SDL_RWops *rw = SDL_RWFromFP(imgfile, SDL_FALSE);
        if (rw)
        {
            surf = IMG_Load_RW(rw, 0);
            SDL_RWclose(rw);
        }
        fclose(imgfile);
    }

    if (!surf)
        return nullptr;

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
