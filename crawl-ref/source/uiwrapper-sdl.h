#ifndef SDL_UI_WRAPPER_H
#define SDL_UI_WRAPPER_H

#ifdef USE_TILE
#include "uiwrapper.h"

#ifdef USE_SDL
struct SDL_Surface;
struct SDL_VideoInfo;

class SDLWrapper : public UIWrapper {
public:
    SDLWrapper();

    // Class functions
    virtual int init(coord_def *m_windowsz);
    virtual void shutdown();

    // Environment state functions
    virtual void set_window_title(const char *title);
    virtual bool set_window_icon(const char* icon_name);
    virtual key_mod get_mod_state();
    virtual void set_mod_state(key_mod mod);
    virtual int byte_order();

    // System time functions
    virtual void set_timer( unsigned int interval,
                            ui_timer_callback callback);
    virtual unsigned int get_ticks();
    virtual void delay(unsigned int ms);

    // Event functions
    virtual int raise_custom_event();
    virtual int wait_event(ui_event *event);
    virtual unsigned int get_event_count(ui_event_type type);

    // Display functions
    virtual void resize(coord_def &m_windowsz);
    virtual void swap_buffers();
    virtual int screen_width();
    virtual int screen_height();

protected:
    SDL_Surface *m_context;
    const SDL_VideoInfo* video_info;
};

#endif // USE_SDL
#endif // USE_TILE

#endif // SDL_UI_WRAPPER_H
