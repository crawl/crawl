#ifndef SDL_WINDOWMANAGER_H
#define SDL_WINDOWMANAGER_H

#ifdef USE_TILE_LOCAL
#ifdef USE_SDL

#include "windowmanager.h"

struct SDL_Surface;
struct SDL_Window;
typedef void* SDL_GLContext;

class SDLWrapper : public WindowManager
{
public:
    SDLWrapper();
    ~SDLWrapper();

    // Class functions
    virtual int init(coord_def *m_windowsz, int *densityNum, int *densityDen);

    // Environment state functions
    virtual void set_window_title(const char *title);
    virtual bool set_window_icon(const char* icon_name);
#ifdef TARGET_OS_WINDOWS
    virtual void set_window_placement(coord_def *m_windowsz);
#endif
    virtual key_mod get_mod_state() const;
    virtual void set_mod_state(key_mod mod);

    // System time functions
    virtual unsigned int set_timer(unsigned int interval,
                                   wm_timer_callback callback);
    virtual void remove_timer(unsigned int timer_id);
    virtual unsigned int get_ticks() const;
    virtual void delay(unsigned int ms);

    // Event functions
    virtual int raise_custom_event();
    virtual int wait_event(wm_event *event);
    virtual unsigned int get_event_count(wm_event_type type);
    virtual void show_keyboard();

    // Display functions
    virtual void resize(coord_def &m_windowsz);
    virtual void swap_buffers();
    virtual int screen_width() const;
    virtual int screen_height() const;
    virtual int desktop_width() const;
    virtual int desktop_height() const;

    // Texture loading
    virtual bool load_texture(GenericTexture *tex, const char *filename,
                              MipMapOptions mip_opt, unsigned int &orig_width,
                              unsigned int &orig_height,
                              tex_proc_func proc = nullptr,
                              bool force_power_of_two = true);

protected:
    // Helper functions
    SDL_Surface *load_image(const char *file) const;

    SDL_Window *m_window;
    SDL_GLContext m_context;
    int _desktop_width;
    int _desktop_height;

private:
    void glDebug(const char *msg);
};

#endif // USE_SDL
#endif // USE_TILE_LOCAL

#endif // SDL_WINDOWMANAGER_H
