/*
 *  File:       libgui.cc
 *  Summary:    Functions for x11
 *  Written by: M.Itakura (?)
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

// TODO enne - slowly morph this into tilesdl.h

#ifndef LIBGUI_H
#define LIBGUI_H
#ifdef USE_TILE

#include <stdio.h>

#include "defines.h"

typedef unsigned int screen_buffer_t;

void set_mouse_enabled(bool enabled);

struct coord_def;
struct crawl_view_geometry;

void gui_init_view_params(crawl_view_geometry &geom);

// If mouse on dungeon map, returns true and sets gc.
// Otherwise, it just returns false.
bool gui_get_mouse_grid_pos(coord_def &gc);

/* text display */
void clrscr(void);
void textcolor(int color);
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

#define textattr(x) textcolor(x)
void set_cursor_enabled(bool enabled);
bool is_cursor_enabled();
inline void enable_smart_cursor(bool) { }
inline bool is_smart_cursor_enabled() { return false; }


void window(int x1, int y1, int x2, int y2);

int getch();
int getch_ck();
void clrscr();
void message_out(int which_line, int colour, const char *s, int firstcol = 0, bool newline = true);
void cgotoxy(int x, int y, int region = GOTO_CRT);
void clear_message_window();
void delay(int ms);
void update_screen();
int kbhit();

#ifdef UNIX
char *strlwr(char *str);
int itoa(int value, char *strptr, int radix);
int stricmp(const char *str1, const char *str2);
#endif

#endif // USE_TILE
#endif // LIBGUI_H
