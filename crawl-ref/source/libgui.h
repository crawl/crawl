#ifndef LIBGUI_H
#define LIBGUI_H
#ifdef USE_TILE

#ifdef USE_X11
  #include <X11/Xlib.h>
  #include <X11/X.h>
#elif defined(WINDOWS)
  #include <windows.h>
  // windows.h defines 'near' as an empty string for compatibility.
  // This breaks the use of that as a variable name.  Urgh.
  #ifdef near
    #undef near
  #endif
  #include <commdlg.h>
#endif

#include "defines.h"

typedef unsigned int screen_buffer_t;

void libgui_init();
void libgui_shutdown();

void edit_prefs();
/* ***********************************************************************
 Minimap related
 * called from: misc view spells2
 * *********************************************************************** */
void GmapInit(bool upd_tile);
void GmapDisplay(int linex, int liney);
void GmapUpdate(int x, int y, int what, bool upd_tile = true);

//dungeon display size
extern int tile_dngn_x;
extern int tile_dngn_y;

void set_mouse_enabled(bool enabled);

// mouse ops
// set mouse op mode
void mouse_set_mode(int mode);
// get mouse mode
int mouse_get_mode();

class mouse_control
{
public:
    mouse_control(int mode)
    {
	oldmode = mouse_get_mode();
	mouse_set_mode(mode);
    }

    ~mouse_control()
    {
	mouse_set_mode(oldmode);
    }
private:
    int oldmode;
};

class coord_def;

void gui_init_view_params(coord_def &termsz, coord_def &viewsz, 
    coord_def &msgp, coord_def &msgsz, coord_def &hudp, coord_def &hudsz);

// If mouse on dungeon map, returns true and sets gc.
// Otherwise, it just returns false.
bool gui_get_mouse_grid_pos(coord_def &gc);

enum InvAction
{
    INV_DROP,
    INV_USE,
    INV_PICKUP,
    INV_VIEW,
    INV_SELECT,
    INV_NUMACTIONS
};

void gui_set_mouse_inv(int idx, InvAction act);
void gui_get_mouse_inv(int &idx, InvAction &act);

void tile_place_cursor(int x, int y, bool display);

void lock_region(int r);
void unlock_region(int r);

enum ScreenRegion
{
    REGION_NONE,
    REGION_CRT,
    REGION_STAT,  // stat area
    REGION_MSG,   // message area
    REGION_MAP,   // overview map area
    REGION_DNGN,
    REGION_TDNGN,
    REGION_INV1,  // items in inventory
    REGION_INV2,  // items in inventory?
    REGION_XMAP,
    REGION_TIP,
    NUM_REGIONS
};

/* text display */
void clrscr(void);
void textcolor(int color);
void gotoxy(int x, int y, int region = GOTO_CRT);
void message_out(int mline, int colour, const char *str, int firstcol = 0,
                 bool newline = true);
void clear_message_window();
int wherex();
int wherey();
void cprintf(const char *format,...);    
void clear_to_end_of_line(void);
void clear_to_end_of_screen(void);
int get_number_of_lines(void);
int get_number_of_cols(void);
void get_input_line_gui(char *const buff, int len);
void _setcursortype(int curstype);
void textbackground(int bg);
void textcolor(int col);
void putch(unsigned char chr);
void putwch(unsigned chr);
void put_colour_ch(int colour, unsigned ch);
void writeWChar(unsigned char *ch);

void puttext(int x, int y, int lx, int ly, unsigned char *buf, 
             bool mono = false, int where = 0);
void ViewTextFile(const char *name);

#define textattr(x) textcolor(x)
void set_cursor_enabled(bool enabled);
bool is_cursor_enabled();
void enable_smart_cursor(bool);
bool is_smart_cursor_enabled();

#ifdef USE_X11
char *strlwr(char *str); // non-unix
int itoa(int value, char *strptr, int radix); // non-unix
int stricmp(const char *str1, const char *str2); // non-unix
#endif

void delay(unsigned long time);
int kbhit(void);
void window(int x1, int y1, int x2, int y2);
void update_screen(void);
int getch();
int getch_ck();

// types of events
#define EV_KEYIN    1
#define EV_MOVE     2
#define EV_BUTTON   3
#define EV_UNBUTTON 4

#define _NORMALCURSOR 1
#define _NOCURSOR     0

#define textcolor_cake(col) textcolor((col)<<4 | (col))

#endif // USE_TILE
#endif // LIBGUI_H
