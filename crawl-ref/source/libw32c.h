/*
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef LIBW32C_H
#define LIBW32C_H

#include <string>
#include <stdarg.h>
#include <stdio.h>

typedef unsigned short screen_buffer_t;

void init_libw32c(void);
void deinit_libw32c(void);

void message_out(int mline, int colour, const char *str, int firstcol = 0,
                 bool newline = true);
void clear_message_window();

int get_number_of_lines();
int get_number_of_cols();

void set_cursor_enabled(bool enabled);
bool is_cursor_enabled();

void clrscr(void);
void clear_to_end_of_line();
void gotoxy_sys(int x, int y);
void textcolor(int c);
void textattr(int c);
void cprintf( const char *format, ... );
// void cprintf(const char *s);
void set_string_input(bool value);
bool set_buffering(bool value);
int get_console_string(char *buf, int maxlen);
void print_timings(void);

void window(int x, int y, int lx, int ly);
int wherex(void);
int wherey(void);
void putch(char c);
void putwch(unsigned c);
int getch(void);
int getch_ck(void);
int key_to_command(int);
int getche(void);
int kbhit(void);
void delay(int ms);
void textbackground(int c);
void puttext(int x1, int y1, int x2, int y2, const screen_buffer_t *);
void update_screen();

void enable_smart_cursor(bool cursor);
bool is_smart_cursor_enabled();
void set_mouse_enabled(bool enabled);

inline void put_colour_ch(int colour, unsigned ch)
{
    textattr(colour);
    putwch(ch);
}

#endif
