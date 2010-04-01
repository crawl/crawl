#ifndef UI_WRAPPER_H
#define UI_WRAPPER_H

#include "externs.h"

#ifdef USE_TILE
#include "tilereg.h"
#include "tilesdl.h"
#include "tiletex.h" // For tex_proc_func
#include "glwrapper.h"  // for MipMapOptions enum

typedef enum {
    UI_BIG_ENDIAN,
    UI_LIL_ENDIAN
} ui_endianness;

typedef enum {
    UI_NOEVENT = 0,
    UI_ACTIVEEVENT,
    UI_KEYDOWN,
    UI_KEYUP,
    UI_MOUSEMOTION,
    UI_MOUSEBUTTONUP,
    UI_MOUSEBUTTONDOWN,
    UI_QUIT,
    UI_CUSTOMEVENT,
    UI_RESIZE,
    UI_EXPOSE,
    UI_NUMEVENTS = 15
} ui_event_type;

typedef struct{
    unsigned char scancode;
    int sym;
    unsigned char key_mod;
    unsigned int unicode;
} ui_keysym;

typedef struct{
    unsigned char type;
    unsigned char gain;
    unsigned char state;
} ui_active_event;

typedef struct{
    unsigned char type;
    unsigned char state;
    ui_keysym keysym;
} ui_keyboard_event;

typedef struct{
    unsigned char type;
    int w, h;
} ui_resize_event;

typedef struct{
    unsigned char type;
} ui_expose_event;

typedef struct{
    unsigned char type;
} ui_quit_event;

typedef struct{
    unsigned char type;
    int code;
    void *data1;
    void *data2;
} ui_custom_event;

// Basically a generic SDL_Event
typedef struct{
    unsigned char type;
    ui_active_event active;
    ui_keyboard_event key;
    MouseEvent mouse_event;
    ui_resize_event resize;
    ui_expose_event expose;
    ui_quit_event quit;
    ui_custom_event custom;
} ui_event;

// custom timer callback function
typedef unsigned int (*ui_timer_callback)(unsigned int interval);

class FTFont;

class WindowManager {
public:
    // To silence pre 4.3 g++ compiler warnings
    virtual ~WindowManager() {};

    // Static Alloc/deallocators
    static void create();

    // Class functions
    virtual int init(coord_def *m_windowsz) = 0;
    virtual void shutdown() = 0;

    // Environment state functions
    virtual void set_window_title(const char *title) = 0;
    virtual bool set_window_icon(const char* icon_name) = 0;
    virtual key_mod get_mod_state() = 0;
    virtual void set_mod_state(key_mod mod) = 0;
    virtual int byte_order() = 0;

    // System time functions
    virtual void set_timer( unsigned int interval,
                            ui_timer_callback callback) = 0;
    virtual unsigned int get_ticks() = 0;
    virtual void delay(unsigned int ms) = 0;

    // Event functions
    virtual int raise_custom_event() = 0;
    virtual int wait_event(ui_event *event) = 0;
    virtual unsigned int get_event_count(ui_event_type type) = 0;

    // Display functions
    virtual void resize(coord_def &m_windowsz) = 0;
    virtual void swap_buffers() = 0;
    virtual int screen_width() = 0;
    virtual int screen_height() = 0;

    // Texture loading
    virtual bool load_texture(  GenericTexture *tex, const char *filename,
                                MipMapOptions mip_opt, unsigned int &orig_width,
                                unsigned int &orig_height,
                                tex_proc_func proc = NULL,
                                bool force_power_of_two = true) = 0;

};

// Main interface for UI functions
extern WindowManager *wm;

#endif //USE_TILE
#endif //include guard
