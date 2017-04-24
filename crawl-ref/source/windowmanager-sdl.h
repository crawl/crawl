#pragma once

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
    virtual int init(coord_def *m_windowsz, int *densityNum, int *densityDen)
        override;

    // Environment state functions
    virtual void set_window_title(const char *title) override;
    virtual bool set_window_icon(const char* icon_name) override;
#ifdef TARGET_OS_WINDOWS
    virtual void set_window_placement(coord_def *m_windowsz);
#endif
    virtual key_mod get_mod_state() const override;
    virtual void set_mod_state(key_mod mod) override;

    // System time functions
    virtual unsigned int set_timer(unsigned int interval,
                                   wm_timer_callback callback) override;
    virtual void remove_timer(unsigned int timer_id) override;
    virtual unsigned int get_ticks() const override;
    virtual void delay(unsigned int ms) override;

    // Event functions
    virtual int raise_custom_event() override;
    virtual int wait_event(wm_event *event) override;
    virtual unsigned int get_event_count(wm_event_type type) override;
    virtual void show_keyboard() override;

    // Display functions
    virtual void resize(coord_def &m_windowsz) override;
    virtual void swap_buffers() override;
    virtual int screen_width() const override;
    virtual int screen_height() const override;
    virtual int desktop_width() const override;
    virtual int desktop_height() const override;

    // Texture loading
    virtual bool load_texture(GenericTexture *tex, const char *filename,
                              MipMapOptions mip_opt, unsigned int &orig_width,
                              unsigned int &orig_height,
                              tex_proc_func proc = nullptr,
                              bool force_power_of_two = true) override;

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
