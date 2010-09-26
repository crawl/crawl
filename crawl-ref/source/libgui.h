/*
 *  File:       libgui.cc
 *  Summary:    Functions for x11
 *  Written by: M.Itakura (?)
 */

// TODO enne - slowly morph this into tilesdl.h

#ifndef LIBGUI_H
#define LIBGUI_H
#ifdef USE_TILE

#include <stdio.h>

#include "defines.h"

void set_mouse_enabled(bool enabled);

struct coord_def;
struct crawl_view_geometry;

void gui_init_view_params(crawl_view_geometry &geom);

// If mouse on dungeon map, returns true and sets gc.
// Otherwise, it just returns false.
bool gui_get_mouse_grid_pos(coord_def &gc);

/* text display */
void textcolor(int color);
int wherex();
int wherey();
int cprintf(const char *format,...);
void clear_to_end_of_line(void);
void clear_to_end_of_screen(void);
int get_number_of_lines(void);
int get_number_of_cols(void);
void _setcursortype(int curstype);
void textbackground(int bg);
void textcolor(int col);
int putch(unsigned char chr);
int putwch(unsigned chr);
void put_colour_ch(int colour, unsigned ch);
void writeWChar(unsigned char *ch);

#define textattr(x) textcolor(x)
void set_cursor_enabled(bool enabled);
bool is_cursor_enabled();
inline void enable_smart_cursor(bool) { }
inline bool is_smart_cursor_enabled() { return false; }


extern "C" int getch();
int getch_ck();
int clrscr();
void cgotoxy(int x, int y, GotoRegion region = GOTO_CRT);
coord_def cgetpos(GotoRegion region = GOTO_CRT);
GotoRegion get_cursor_region();
void delay(int ms);
void update_screen();
int kbhit();

#ifdef UNIX
extern "C" char *strlwr(char *str);
int itoa(int value, char *strptr, int radix);
#endif

#endif // USE_TILE
#endif // LIBGUI_H
