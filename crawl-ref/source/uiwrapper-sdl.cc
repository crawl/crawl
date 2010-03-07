/*
 *  wrapper-sdl.cc
 *  Roguelike
 *
 *  Created by Ixtli on 2/23/10.
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */

#include "AppHdr.h"

#include "cio.h"
#include "options.h"
#include "files.h"

#include "uiwrapper-sdl.h"

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#ifdef USE_GL
#include "glwrapper.h"
#endif

#ifdef USE_TILE

static unsigned char _get_modifiers(SDL_keysym &keysym)
{
    // keysym.mod can't be used to keep track of the modifier state.
    // If shift is hit by itself, this will not include KMOD_SHIFT.
    // Instead, look for the key itself as a separate event.
    switch (keysym.sym)
    {
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
        return (MOD_SHIFT);
        break;
    case SDLK_LCTRL:
    case SDLK_RCTRL:
        return (MOD_CTRL);
        break;
    case SDLK_LALT:
    case SDLK_RALT:
        return (MOD_ALT);
        break;
    default:
        return (0);
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
        return (CK_ENTER + offset);
    case SDLK_BACKSPACE:
        return (CK_BKSP + offset);
    case SDLK_ESCAPE:
        return (CK_ESCAPE + offset);
    case SDLK_DELETE:
        return (CK_DELETE + offset);
        
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
        return (0);
        
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
        ASSERT(keysym.sym >= SDLK_F1 && keysym.sym <= SDLK_UNDO);
        return (keysym.sym + (SDLK_UNDO - SDLK_F1 + 1) * mod);
        
        // Hack.  libw32c overloads clear with '5' too.
    case SDLK_KP5:
        return (CK_CLEAR + numpad_offset);
        
    case SDLK_KP8:
    case SDLK_UP:
        return (CK_UP + numpad_offset);
    case SDLK_KP2:
    case SDLK_DOWN:
        return (CK_DOWN + numpad_offset);
    case SDLK_KP4:
    case SDLK_LEFT:
        return (CK_LEFT + numpad_offset);
    case SDLK_KP6:
    case SDLK_RIGHT:
        return (CK_RIGHT + numpad_offset);
    case SDLK_KP0:
    case SDLK_INSERT:
        return CK_INSERT + numpad_offset;
    case SDLK_KP7:
    case SDLK_HOME:
        return (CK_HOME + numpad_offset);
    case SDLK_KP1:
    case SDLK_END:
        return (CK_END + numpad_offset);
    case SDLK_CLEAR:
        return (CK_CLEAR + numpad_offset);
    case SDLK_KP9:
    case SDLK_PAGEUP:
        return (CK_PGUP + numpad_offset);
    case SDLK_KP3:
    case SDLK_PAGEDOWN:
        return (CK_PGDN + numpad_offset);
    default:
        break;
    }

    // Alt does not get baked into keycodes like shift and ctrl, so handle it.
    const int key_offset = (mod & MOD_ALT) ? 3000 : 0;

    const bool is_ascii = ((keysym.unicode & 0xFF80) == 0);
    return (is_ascii ? (keysym.unicode & 0x7F) + key_offset : 0);
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

UIWrapper wrapper;

UIWrapper::UIWrapper():
    m_context(NULL)
{
}

int UIWrapper::init(coord_def *m_windowsz)
{
    // Do SDL initialization
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Failed to initialise SDL: %s\n", SDL_GetError());
        return (false);
    }

    video_info = SDL_GetVideoInfo();

    SDL_EnableUNICODE(true);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    if (Options.tile_key_repeat_delay > 0)
    {
        const int delay    = Options.tile_key_repeat_delay;
        const int interval = SDL_DEFAULT_REPEAT_INTERVAL;
        if (SDL_EnableKeyRepeat(delay, interval) != 0)
            printf("Failed to set key repeat mode: %s\n", SDL_GetError());
    }

    unsigned int flags = SDL_OPENGL;

    bool too_small = (video_info->current_w < 1024 || video_info->current_h < 800);
    if (Options.tile_full_screen == SCREENMODE_FULL
        || too_small && Options.tile_full_screen == SCREENMODE_AUTO)
    {
        flags |= SDL_FULLSCREEN;
    }

    if (Options.tile_window_width && Options.tile_window_height)
    {
        m_windowsz->x = Options.tile_window_width;
        m_windowsz->y = Options.tile_window_height;
    }
    else if (flags & SDL_FULLSCREEN)
    {
        // By default, fill the whole screen.
        m_windowsz->x = video_info->current_w;
        m_windowsz->y = video_info->current_h;
    }
    else
    {
        m_windowsz->x = std::max(800, video_info->current_w  - 90);
        m_windowsz->y = std::max(480, video_info->current_h - 90);
    }

    m_context = SDL_SetVideoMode(m_windowsz->x, m_windowsz->y, 0, flags);
    if (!m_context)
    {
        printf("Failed to set video mode: %s\n", SDL_GetError());
        return (false);
    }

    return (true);
}

int UIWrapper::screenWidth()
{
    return (video_info->current_w);
}

int UIWrapper::screenHeight()
{
    return (video_info->current_h);
}

void UIWrapper::setWindowTitle(const char *title)
{
    SDL_WM_SetCaption(title, CRAWL);
}

bool UIWrapper::setWindowIcon(const char* icon_name)
{
    // TODO: Figure out how to move this IMG_Load command to cgcontext
    // so that we're not dependant on SDL_image here
    SDL_Surface *icon = IMG_Load(datafile_path(icon_name, true, true).c_str());
    if (!icon)
    {
        printf("Failed to load icon: %s\n", SDL_GetError());
        return (false);
    }
    SDL_WM_SetIcon(icon, NULL);
    return (true);
}

void UIWrapper::resize(coord_def &m_windowsz)
{
    GLStateManager::resetViewForResize(m_windowsz);
}

unsigned int UIWrapper::getTicks()
{
    return (SDL_GetTicks());
}

key_mod UIWrapper::getModState()
{
    SDLMod mod = SDL_GetModState();
    
    switch (mod) {
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

void UIWrapper::setModState(key_mod mod)
{
    SDLMod set_to;
    switch (mod) {
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

int UIWrapper::waitEvent(ui_event *event)
{
    SDL_Event sdlevent;
    if(!SDL_WaitEvent(&sdlevent))
        return (0);

    // translate the SDL_Event into the almost-analogous ui_event
    switch (sdlevent.type) {
    case SDL_ACTIVEEVENT:
        SDL_SetModState(KMOD_NONE);
        event->type = UI_ACTIVEEVENT;
        event->active.gain = sdlevent.active.gain;
        event->active.state = sdlevent.active.state;
        break;
    case SDL_KEYDOWN:
        event->type = UI_KEYDOWN;
        event->key.state = sdlevent.key.state;
        event->key.keysym.scancode = sdlevent.key.keysym.scancode;
        event->key.keysym.key_mod = _get_modifiers(sdlevent.key.keysym);
        event->key.keysym.unicode = sdlevent.key.keysym.unicode;
        event->key.keysym.sym = _translate_keysym(sdlevent.key.keysym);
        break;
    case SDL_KEYUP:
        event->type = UI_KEYUP;
        event->key.state = sdlevent.key.state;
        event->key.keysym.scancode = sdlevent.key.keysym.scancode;
        event->key.keysym.key_mod = _get_modifiers(sdlevent.key.keysym);
        event->key.keysym.unicode = sdlevent.key.keysym.unicode;
        event->key.keysym.sym = _translate_keysym(sdlevent.key.keysym);
        break;
    case SDL_MOUSEMOTION:
        event->type = UI_MOUSEMOTION;
        _translate_event(sdlevent.motion, event->mouse_event);
        break;
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEBUTTONDOWN:
        event->type = UI_MOUSEBUTTONDOWN;
        _translate_event(sdlevent.button, event->mouse_event);
        break;
    case SDL_VIDEORESIZE:
        event->type = UI_RESIZE;
        event->resize.w = sdlevent.resize.w;
        event->resize.h = sdlevent.resize.h;
        break;
    case SDL_VIDEOEXPOSE:
        event->type = UI_EXPOSE;
        break;
    case SDL_QUIT:
        event->type = UI_QUIT;
        break;

        // I leave these as the same, because the original tilesdl does, too
    case SDL_USEREVENT:
    default:
        event->type = UI_CUSTOMEVENT;
        event->custom.code = sdlevent.user.code;
        event->custom.data1 = sdlevent.user.data1;
        event->custom.data2 = sdlevent.user.data2;
        break;
    }

    return (1);
}

void UIWrapper::setTimer(unsigned int interval, ui_timer_callback callback)
{
    SDL_SetTimer(interval, callback);
}

int UIWrapper::raiseCustomEvent()
{   
    SDL_Event send_event;
    send_event.type = SDL_USEREVENT;
    return (SDL_PushEvent(&send_event));
}

void UIWrapper::swapBuffers()
{
    SDL_GL_SwapBuffers();
}

void UIWrapper::UIDelay(unsigned int ms)
{
    SDL_Delay(ms);
}

unsigned int UIWrapper::getEventCount(ui_event_type type)
{
    // Look for the presence of any keyboard events in the queue.
    Uint32 eventmask;
    switch( type )
    {
    case UI_ACTIVEEVENT:
        eventmask = SDL_EVENTMASK( SDL_ACTIVEEVENT );
        break;

    case UI_KEYDOWN:
        eventmask = SDL_EVENTMASK( SDL_KEYDOWN );
        break;

    case UI_KEYUP:
        eventmask = SDL_EVENTMASK( SDL_KEYUP );
        break;

    case UI_MOUSEMOTION:
        eventmask = SDL_EVENTMASK( SDL_MOUSEMOTION );
        break;

    case UI_MOUSEBUTTONUP:
        eventmask = SDL_EVENTMASK( SDL_MOUSEBUTTONUP );
        break;

    case UI_MOUSEBUTTONDOWN:
        eventmask = SDL_EVENTMASK( SDL_MOUSEBUTTONDOWN );
        break;

    case UI_QUIT:
        eventmask = SDL_EVENTMASK( SDL_QUIT );
        break;

    case UI_CUSTOMEVENT:
        eventmask = SDL_EVENTMASK( SDL_USEREVENT );
        break;

    case UI_RESIZE:
        eventmask = SDL_EVENTMASK( SDL_VIDEORESIZE );
        break;

    case UI_EXPOSE:
        eventmask = SDL_EVENTMASK( SDL_VIDEOEXPOSE );
        break;

    default:
        return (-1); // Error
    }

    SDL_Event store;
    SDL_PumpEvents();

    // Note: this returns -1 for error.
    int count = SDL_PeepEvents(&store, 1, SDL_PEEKEVENT, eventmask);

    return (count);
}

void UIWrapper::shutdown()
{
    SDL_Quit();
    m_context = NULL;
    video_info = NULL;
}

int UIWrapper::byteOrder()
{
    if( SDL_BYTEORDER == SDL_BIG_ENDIAN )
        return (UI_BIG_ENDIAN);
    return (UI_LIL_ENDIAN);
}

#endif
