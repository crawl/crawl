/*
*  Modified for Crawl Reference by $Author$ on $Date$
*/

#ifndef LIBW32C_H
#define LIBW32C_H

#define WIN_NUMBER_OF_LINES     25

#include <string>
// I think the following definition is all we need from STD namespace..
#ifdef __IBMCPP__			// Borland 5.01 doesn't seem to need this
typedef std::basic_string<char> string;
#endif

#include <stdarg.h>

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
void gotoxy(int x, int y);
void textcolor(int c);
void textattr(int c);
void cprintf( const char *format, ... );
// void cprintf(const char *s);
void setStringInput(bool value);
bool setBuffering(bool value);
int getConsoleString(char *buf, int maxlen);
void print_timings(void);

void window(int x, int y, int lx, int ly);
int wherex(void);
int wherey(void);
void putch(char c);
int getch(void);
int getch_ck(void);
int key_to_command(int);
int getche(void);
int kbhit(void);
void delay(int ms);
void textbackground(int c);

void enable_smart_cursor(bool cursor);
bool is_smart_cursor_enabled();

inline void srandom(unsigned int seed) { srand(seed); }

#endif


