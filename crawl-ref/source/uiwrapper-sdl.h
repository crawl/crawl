/*
 *  wrapper-sdl.h
 *  Roguelike
 *
 *  Created by Ixtli on 2/23/10.
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */

#ifndef WRAPPER_SDL_H
#define WRAPPER_SDL_H

#ifdef USE_TILE
#ifdef USE_SDL

#include "externs.h"
#include "tilereg.h"
#include "tilesdl.h"

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
    UI_NUMEVENTS = 16
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

class SDL_Surface;
class SDL_VideoInfo;
class FTFont;

class UIWrapper {
public:
    // Class functions
    UIWrapper();
    int init(coord_def *m_windowsz);
    void shutdown();
    
    // Environment state functions
    void setWindowTitle(const char *title);
    bool setWindowIcon(const char* icon_name);
    key_mod getModState();
    void setModState(key_mod mod);
    
    // System time functions
    void setTimer(unsigned int interval, ui_timer_callback callback);
    unsigned int getTicks();
    void UIDelay(unsigned int ms);
    
    // Event functions
    int raiseCustomEvent();
    int waitEvent(ui_event *event);
    unsigned int getEventCount(ui_event_type type);
    
    // Display functions
    void resize(coord_def &m_windowsz);
    void swapBuffers();
    int screenWidth();
    int screenHeight();
    

    
protected:
    
    SDL_Surface *m_context;
    const SDL_VideoInfo* video_info;
};

// Main interface for UI functions
extern UIWrapper wrapper;

#endif //USE_SDL
#endif //USE_TILE
#endif //include guard
