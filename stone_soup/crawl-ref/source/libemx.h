#ifndef LIBEMX_H
#define LIBEMX_H
#ifndef __LIBEMX_C__


#include <stdlib.h>
#include <conio.h>

#define itoa _itoa


#define _NORMALCURSOR 1
#define _NOCURSOR 0


void init_emx();
void deinit_emx();
void _setcursortype(int curstype);
void clrscr();
void gotoxy(int x, int y);
void textcolor(int c);
void cprintf (const char *format, ...);

void puttext(int x, int y, int lx, int ly, const char *buf);
void puttext(int x, int y, int lx, int ly, unsigned const char *buf);
void gettext(int x, int y, int lx, int ly, char *buf);
void gettext(int x, int y, int lx, int ly, unsigned char *buf);
void window(int x, int y, int lx, int ly);
int wherex();
int wherey();
void putch(char c);
int kbhit();
void delay(int ms);
void textbackground(int c);


#endif // __LIBEMX_C__
#endif
