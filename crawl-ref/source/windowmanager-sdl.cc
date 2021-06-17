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
# include <SDL_image.h>
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
        mod |= TILES_MOD_SHIFT;
    if (modifier & KMOD_CTRL)
        mod |= TILES_MOD_CTRL;
    if (modifier & KMOD_LALT)
        mod |= TILES_MOD_ALT;
    return mod;
}

static unsigned char _get_modifiers(SDL_Keysym &keysym)
{
    switch (keysym.sym)
    {
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
        return TILES_MOD_SHIFT;
        break;
    case SDLK_LCTRL:
    case SDLK_RCTRL:
        return TILES_MOD_CTRL;
        break;
    case SDLK_LALT:
    case SDLK_RALT:
        return TILES_MOD_ALT;
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
        case SDL_WINDOWEVENT_MOVED:
            tile_event.type = WME_MOVE;
            break;
        default:
            tile_event.type = WME_NOEVENT;
            break;
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

    // XX: this comment isn't very accurate, for example many SDL keycodes are
    //     |d with 1<<30, and the alt stuff here is just a mess.
    // This is arbitrary, but here's the current mappings.
    // 0-256: ASCII, Crawl arrow keys
    // 0-1k : Other SDL keys (F1, Windows keys, etc...) and modifiers
    // 1k-3k: Non-ASCII with modifiers other than just shift or just ctrl.
    // 3k+  : ASCII with the left alt modifier.

    int offset = mod ? 1000 + 256 * mod : 0;
    int numpad_offset = 0;
    if (mod == TILES_MOD_CTRL)
        numpad_offset = ctrl_offset;
    else if (mod == TILES_MOD_SHIFT)
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

    if (!(mod & (TILES_MOD_CTRL | TILES_MOD_ALT)))
        return 0;

    int ret = keysym.sym;

    if (mod & TILES_MOD_ALT && keysym.sym >= 128)
        ret -= 3000; // ???
    if (mod & TILES_MOD_CTRL)
        ret -= SDLK_a - 1; // XXX: this might break for strange keysyms

    return ret;
}

static void _translate_event(const SDL_MouseMotionEvent &sdl_event,
                             wm_mouse_event &tile_event)
{
    tile_event.held   = wm_mouse_event::NONE;
    tile_event.event  = wm_mouse_event::MOVE;
    tile_event.button = wm_mouse_event::NONE;
    tile_event.px     = display_density.apply_game_scale(sdl_event.x);
    tile_event.py     = display_density.apply_game_scale(sdl_event.y);
    tile_event.held   = wm->get_mouse_state(nullptr, nullptr);
    tile_event.mod    = wm->get_mod_state();

    // TODO: enne - do we want the relative motion?
}

static void _translate_event(const SDL_MouseButtonEvent &sdl_event,
                             wm_mouse_event &tile_event)
{
    tile_event.held  = wm_mouse_event::NONE;
    tile_event.event = (sdl_event.type == SDL_MOUSEBUTTONDOWN) ?
    wm_mouse_event::PRESS : wm_mouse_event::RELEASE;
    switch (sdl_event.button)
    {
    case SDL_BUTTON_LEFT:
        tile_event.button = wm_mouse_event::LEFT;
        break;
    case SDL_BUTTON_RIGHT:
        tile_event.button = wm_mouse_event::RIGHT;
        break;
    case SDL_BUTTON_MIDDLE:
        tile_event.button = wm_mouse_event::MIDDLE;
        break;
    default:
        // Unhandled button.
        tile_event.button = wm_mouse_event::NONE;
        break;
    }
    tile_event.px = display_density.apply_game_scale(sdl_event.x);
    tile_event.py = display_density.apply_game_scale(sdl_event.y);
    tile_event.held = wm->get_mouse_state(nullptr, nullptr);
    tile_event.mod = wm->get_mod_state();
}

static void _translate_wheel_event(const SDL_MouseWheelEvent &sdl_event,
                                   wm_mouse_event &tile_event)
{
    tile_event.held  = wm_mouse_event::NONE;
    tile_event.event = wm_mouse_event::WHEEL;
    tile_event.button = (sdl_event.y < 0) ? wm_mouse_event::SCROLL_DOWN
                                          : wm_mouse_event::SCROLL_UP;
    tile_event.px = display_density.apply_game_scale(sdl_event.x);
    tile_event.py = display_density.apply_game_scale(sdl_event.y);
}

SDLWrapper::SDLWrapper():
    m_window(nullptr), m_context(nullptr), prev_keycode(0)
{
    m_cursors.fill(nullptr);
}

SDLWrapper::~SDLWrapper()
{
    for (const auto& cursor : m_cursors)
        if (cursor)
            SDL_FreeCursor(cursor);
    if (m_context)
        SDL_GL_DeleteContext(m_context);
    if (m_window)
        SDL_DestroyWindow(m_window);
    SDL_Quit();
}

int SDLWrapper::init(coord_def *m_windowsz)
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

    // find the current display, based on the mouse position
    // TODO: probably want to allow configuring this?
    int mouse_x, mouse_y;
    SDL_GetGlobalMouseState(&mouse_x, &mouse_y);

    int displays = SDL_GetNumVideoDisplays();
    int cur_display;
    for (cur_display = 0; cur_display < displays; cur_display++)
    {
        SDL_Rect bounds;
        SDL_GetDisplayBounds(cur_display, &bounds);
        if (mouse_x >= bounds.x && mouse_y >= bounds.y &&
            mouse_x < bounds.x + bounds.w && mouse_y < bounds.y + bounds.h)
        {
            break;
        }
    }
    if (cur_display >= displays)
        cur_display = 0; // can this happen?

    SDL_DisplayMode display_mode;
    SDL_GetDesktopDisplayMode(cur_display, &display_mode);

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
        m_windowsz->x = (x > 0) ? x : _desktop_width;
        m_windowsz->y = (y > 0) ? y : _desktop_height;
    }
    else
    {
        int y = Options.tile_window_height;
        int x = Options.tile_window_width;
        x = (x > 0) ? x : _desktop_width + x;
        y = (y > 0) ? y : _desktop_height + y;
        if (Options.tile_window_ratio > 0)
            x = min(x, y * Options.tile_window_ratio / 1000);
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
                                SDL_WINDOWPOS_CENTERED_DISPLAY(cur_display),
                                SDL_WINDOWPOS_CENTERED_DISPLAY(cur_display),
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

    // The following two lines are a part of the magical dance needed to get
    // Mojave to work with the version of SDL2 we are using.
    SDL_PumpEvents();
    SDL_SetWindowSize(m_window, m_windowsz->x, m_windowsz->y);

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
    m_windowsz->x = display_density.apply_game_scale(x);
    m_windowsz->y = display_density.apply_game_scale(y);
    init_hidpi();
#ifdef __ANDROID__
# ifndef TOUCH_UI
    SDL_StartTextInput();
# endif
    __android_log_print(ANDROID_LOG_INFO, "Crawl", "Window manager initialised");
#endif

    SDL_GL_GetDrawableSize(m_window, &x, &y);
    SDL_SetWindowMinimumSize(m_window, MIN_SDL_WINDOW_SIZE_X,
                             MIN_SDL_WINDOW_SIZE_Y);

    SDL_EnableScreenSaver();

    return true;
}

int SDLWrapper::screen_width() const
{
    int w, dummy;
    SDL_GetWindowSize(m_window, &w, &dummy);
    return display_density.apply_game_scale(w);
}

int SDLWrapper::screen_height() const
{
    int dummy, h;
    SDL_GetWindowSize(m_window, &dummy, &h);
    return display_density.apply_game_scale(h);
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
    string icon_path = datafile_path(icon_name, false, true);
    if (!icon_path.size())
    {
        mprf(MSGCH_ERROR, "Unable to find window icon '%s'", icon_name);
        return false;
    }

    SDL_Surface *surf = load_image(icon_path.c_str());
    if (!surf)
    {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "Crawl",
                            "Failed to load icon: %s", SDL_GetError());
#endif
        mprf(MSGCH_ERROR, "Failed to load icon '%s': %s\n", icon_path.c_str(),
                                                                SDL_GetError());
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

bool SDLWrapper::init_hidpi()
{
    coord_def windowsz;
    coord_def drawablesz;
    // window size is in logical pixels
    SDL_GetWindowSize(m_window, &(windowsz.x), &(windowsz.y));
    // drawable size is in device pixels
    SDL_GL_GetDrawableSize(m_window, &(drawablesz.x), &(drawablesz.y));
    return display_density.update(drawablesz.x, windowsz.x, Options.game_scale);
}

void SDLWrapper::resize(coord_def &m_windowsz)
{
    coord_def m_drawablesz;
    init_hidpi();
    SDL_GL_GetDrawableSize(m_window, &(m_drawablesz.x), &(m_drawablesz.y));
    glmanager->reset_view_for_resize(m_windowsz, m_drawablesz);
}

unsigned int SDLWrapper::get_ticks() const
{
    return SDL_GetTicks();
}

tiles_key_mod SDLWrapper::get_mod_state() const
{
    SDL_Keymod mod = SDL_GetModState();

    switch (mod)
    {
    case KMOD_LSHIFT:
    case KMOD_RSHIFT:
        return TILES_MOD_SHIFT;
        break;
    case KMOD_LCTRL:
    case KMOD_RCTRL:
        return TILES_MOD_CTRL;
        break;
    case KMOD_LALT:
    case KMOD_RALT:
        return TILES_MOD_ALT;
        break;
    case KMOD_NONE:
    default:
        return TILES_MOD_NONE;
    }
}

void SDLWrapper::set_mod_state(tiles_key_mod mod)
{
    SDL_Keymod set_to;
    switch (mod)
    {
    case TILES_MOD_NONE:
        set_to = KMOD_NONE;
        break;
    case TILES_MOD_SHIFT:
        set_to = KMOD_LSHIFT;
        break;
    case TILES_MOD_CTRL:
        set_to = KMOD_LCTRL;
        break;
    case TILES_MOD_ALT:
        set_to = KMOD_LALT;
        break;
    default:
        set_to = KMOD_NONE;
        break;
    }

    SDL_SetModState(set_to);
}

void SDLWrapper::set_mouse_cursor(mouse_cursor_type type)
{
    SDL_Cursor *cursor = m_cursors[type];

    if (!cursor)
    {
        SDL_SystemCursor sdl_cursor_id;
        switch (type)
        {
            case MOUSE_CURSOR_ARROW:
                sdl_cursor_id = SDL_SYSTEM_CURSOR_ARROW;
                break;
            case MOUSE_CURSOR_POINTER:
                sdl_cursor_id = SDL_SYSTEM_CURSOR_HAND;
                break;
            default:
                die("bad mouse cursor type");
        }
        cursor = m_cursors[type] = SDL_CreateSystemCursor(sdl_cursor_id);
        if (!cursor)
        {
            printf("Failed to create cursor: %s\n", SDL_GetError());
            return;
        }
    }

    SDL_SetCursor(cursor);
}

unsigned short SDLWrapper::get_mouse_state(int *x, int *y) const
{
    Uint32 state = SDL_GetMouseState(x, y);
    if (x)
        *x = display_density.apply_game_scale(*x);
    if (y)
        *y = display_density.apply_game_scale(*y);
    unsigned short ret = 0;
    if (state & SDL_BUTTON(SDL_BUTTON_LEFT))
        ret |= wm_mouse_event::LEFT;
    if (state & SDL_BUTTON(SDL_BUTTON_RIGHT))
        ret |= wm_mouse_event::RIGHT;
    if (state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
        ret |= wm_mouse_event::MIDDLE;
    return ret;
}

string SDLWrapper::get_clipboard()
{
    string result;
    char *clip = SDL_GetClipboardText();
    if (!clip)
        return result;
    result = string(clip);
    SDL_free(clip);
    return result;
}

bool SDLWrapper::has_clipboard()
{
    return SDL_HasClipboardText() == SDL_TRUE;
}

static char32_t _key_suppresses_textinput(int keycode)
{
    char result_char = 0;
    char32_t result = 0;
    switch (keycode)
    {
    case SDLK_KP_5:
    case SDLK_CLEAR:
        result_char = '5';
        break;
    case SDLK_KP_8:
    case SDLK_UP:
        result_char = '8';
        break;
    case SDLK_KP_2:
    case SDLK_DOWN:
        result_char = '2';
        break;
    case SDLK_KP_4:
    case SDLK_LEFT:
        result_char = '4';
        break;
    case SDLK_KP_6:
    case SDLK_RIGHT:
        result_char = '6';
        break;
    case SDLK_KP_0:
    case SDLK_INSERT:
        result_char = '0';
        break;
    case SDLK_KP_7:
    case SDLK_HOME:
        result_char = '7';
        break;
    case SDLK_KP_1:
    case SDLK_END:
        result_char = '1';
        break;
    case SDLK_KP_9:
    case SDLK_PAGEUP:
        result_char = '9';
        break;
    case SDLK_KP_3:
    case SDLK_PAGEDOWN:
        result_char = '3';
        break;
    }
    if (result_char)
        utf8towc(&result, &result_char);
    return result;
}

int SDLWrapper::send_textinput(wm_event *event)
{
    event->type = WME_KEYDOWN;
    do
    {
        // pop a key off the input queue
        char32_t wc;
        int wc_bytelen = utf8towc(&wc, m_textinput_queue.c_str());
        m_textinput_queue.erase(0, wc_bytelen);

        // SDL2 on linux sends an apparently spurious '=' text event for ctrl-=,
        // but not for key combinations like ctrl-f (no 'f' text event is sent).
        // this is relevant only for ctrl-- and ctrl-= bindings at the moment,
        // and I'm somewhat nervous about blocking genuine text entry via the alt
        // key, so for the moment this only blacklists text events with ctrl held
        bool nontext_modifier_held = wm->get_mod_state() == TILES_MOD_CTRL;

        bool should_suppress = prev_keycode && _key_suppresses_textinput(prev_keycode) == wc;
        if (nontext_modifier_held || should_suppress)
        {
            // this needs to return something, or the event loop in
            // TilesFramework::getch_ck will block. Currently, CK_NO_KEY
            // is handled in macro.cc:_getch_mul.
            prev_keycode = 0;
            if (!m_textinput_queue.empty())
                continue;
            event->key.keysym.sym = CK_NO_KEY;
            return 1;
        }
        event->key.keysym.sym = wc;
    }
    while (false);
    return 1;
}

int SDLWrapper::wait_event(wm_event *event, int timeout)
{
    SDL_Event sdlevent;

    if (!m_textinput_queue.empty())
        return send_textinput(event);

    if (!SDL_WaitEventTimeout(&sdlevent, timeout))
        return 0;

    if (sdlevent.type != SDL_TEXTINPUT)
        prev_keycode = 0;

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
        // textinput events for the same key. This mechanism assumes that a
        // fake textinput will arrive as the immediately following SDL event.
        prev_keycode = sdlevent.key.keysym.sym;

/*
 * LShift = scancode 0x30; tiles_key_mod 0x1; unicode 0x130; sym 0x130 SDLK_LSHIFT
 * LCtrl  = scancode 0x32; tiles_key_mod 0x2; unicode 0x132; sym 0x132 SDLK_LCTRL
 * LAlt   = scancode 0x34; tiles_key_mod 0x4; unicode 0x134; sym 0x134 SDLK_LALT
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
        ASSERT(m_textinput_queue.empty());
        m_textinput_queue = string(sdlevent.text.text);
        return send_textinput(event);
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
        event->type = WME_MOUSEWHEEL;
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

static unsigned int _timer_callback(unsigned int ticks, void *param)
{
    UNUSED(ticks);

    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_USEREVENT;
    event.user.data1 = param;
    SDL_PushEvent(&event);
    return 0;
}

unsigned int SDLWrapper::set_timer(unsigned int interval,
                                   wm_timer_callback callback)
{
    return SDL_AddTimer(interval, _timer_callback, reinterpret_cast<void*>(callback));
}

void SDLWrapper::remove_timer(unsigned int& timer_id)
{
    if (timer_id)
    {
        SDL_RemoveTimer(timer_id);
        timer_id = 0;
    }
}

void SDLWrapper::swap_buffers()
{
    SDL_GL_SwapWindow(m_window);
}

void SDLWrapper::delay(unsigned int ms)
{
    SDL_Delay(ms);
}

bool SDLWrapper::next_event_is(wm_event_type type)
{
    // check for enqueued characters from a multi-char textinput event
    // count is floored to 1 for consistency with other event types
    if (type == WME_KEYDOWN && m_textinput_queue.size() > 0)
        return true;

    // Look for the presence of any keyboard events in the queue.
    Uint32 event;
    switch (type)
    {
    case WME_ACTIVEEVENT:
    case WME_RESIZE: // XXX
    case WME_MOVE:
    case WME_EXPOSE: // XXX
        event = SDL_WINDOWEVENT;
        break;

    case WME_KEYDOWN:
        event = SDL_KEYDOWN;
        break;

    case WME_KEYUP:
        event = SDL_KEYUP;
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

    case WME_MOUSEWHEEL:
        event = SDL_MOUSEWHEEL;
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
    if (type == WME_KEYDOWN)
        count += SDL_PeepEvents(&store, 1, SDL_PEEKEVENT, SDL_TEXTINPUT, SDL_TEXTINPUT);
    ASSERT(count >= 0);

    return count != 0;
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
    glmanager->pixelstore_unpack_alignment(1);

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
                unsigned int index = ((unsigned char*)img->pixels)[src+x];
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
            src += img->pitch;
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
    {
        // TODO: could fail if texture is too large / if there are opengl errs
        opengl::check_texture_size(filename, new_width, new_height);
        success |= tex->load_texture(pixels, new_width, new_height, mip_opt);
        opengl::flush_opengl_errors();
    }

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
#else
    UNUSED(msg);
#endif
}
#endif // USE_SDL
#endif // USE_TILE_LOCAL
