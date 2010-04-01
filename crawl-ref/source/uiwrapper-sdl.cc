#include "AppHdr.h"

#include "cio.h"
#include "options.h"
#include "files.h"

#ifdef USE_TILE
#include "uiwrapper.h"
#include "glwrapper.h"

#ifdef USE_SDL
#include "uiwrapper-sdl.h"
#include <SDL/SDL.h>
#include <SDL_image.h>

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
    case SDLK_KP_ENTER:
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

SDLWrapper::SDLWrapper():
    m_context(NULL)
{
}

int SDLWrapper::init(coord_def *m_windowsz)
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
        const int repdelay    = Options.tile_key_repeat_delay;
        const int interval = SDL_DEFAULT_REPEAT_INTERVAL;
        if (SDL_EnableKeyRepeat(repdelay, interval) != 0)
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

int SDLWrapper::screen_width()
{
    return (video_info->current_w);
}

int SDLWrapper::screen_height()
{
    return (video_info->current_h);
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
        printf("Failed to load icon: %s\n", SDL_GetError());
        return (false);
    }
    SDL_WM_SetIcon(surf, NULL);
    SDL_FreeSurface(surf);
    return (true);
}

void SDLWrapper::resize(coord_def &m_windowsz)
{
    glmanager->reset_view_for_resize(m_windowsz);
}

unsigned int SDLWrapper::get_ticks()
{
    return (SDL_GetTicks());
}

key_mod SDLWrapper::get_mod_state()
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

void SDLWrapper::set_mod_state(key_mod mod)
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

int SDLWrapper::wait_event(ui_event *event)
{
    SDL_Event sdlevent;
    if (!SDL_WaitEvent(&sdlevent))
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

void SDLWrapper::set_timer(unsigned int interval, ui_timer_callback callback)
{
    SDL_SetTimer(interval, callback);
}

int SDLWrapper::raise_custom_event()
{   
    SDL_Event send_event;
    send_event.type = SDL_USEREVENT;
    return (SDL_PushEvent(&send_event));
}

void SDLWrapper::swap_buffers()
{
    SDL_GL_SwapBuffers();
}

void SDLWrapper::delay(unsigned int ms)
{
    SDL_Delay(ms);
}

unsigned int SDLWrapper::get_event_count(ui_event_type type)
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

bool SDLWrapper::load_texture(  GenericTexture *tex, const char *filename,
                                MipMapOptions mip_opt, unsigned int &orig_width,
                                unsigned int &orig_height, tex_proc_func proc,
                                bool force_power_of_two)
{
    char acBuffer[512];

    std::string tex_path = datafile_path(filename);

    if (tex_path.c_str()[0] == 0)
    {
        fprintf(stderr, "Couldn't find texture '%s'.\n", filename);
        return (false);
    }

    SDL_Surface *img = load_image(tex_path.c_str());

    if (!img)
    {
        fprintf(stderr, "Couldn't load texture '%s'.\n", tex_path.c_str());
        return (false);
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

    //GLenum texture_format;
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
        //texture_format = GL_RGBA;
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
                        pixel = p[0] | p[1] << 8 | p[2];
                    SDL_GetRGBA(pixel, img->format, &pixels[dest],
                                &pixels[dest+1], &pixels[dest+2],
                                &pixels[dest+3]);
                    dest += 4;
                }
                dest += 4 * (new_width - img->w);
            }

            SDL_UnlockSurface(img);
        }
        //texture_format = GL_RGBA;
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

        bpp = 4;
        //texture_format = GL_RGBA;
    }
    else
    {
        printf("Warning: unsupported format, bpp = %d for '%s'\n",
               bpp, acBuffer);
        return (false);
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

    return (success);
}

void SDLWrapper::shutdown()
{
    SDL_Quit();
    m_context = NULL;
    video_info = NULL;
}

int SDLWrapper::byte_order()
{
    if ( SDL_BYTEORDER == SDL_BIG_ENDIAN )
        return (UI_BIG_ENDIAN);
    return (UI_LIL_ENDIAN);
}

SDL_Surface *SDLWrapper::load_image( const char *file ) const
{
    SDL_Surface *surf = NULL;
    FILE *imgfile = fopen(file, "rb");
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
        return (NULL);

    return (surf);
}

#endif // USE_SDL
#endif // USE_TILE
