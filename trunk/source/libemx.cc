#include <stdio.h>
#include <stdarg.h>
#include <emx/syscalls.h>
#include <sys/video.h>

#include "libemx.h"


static int cursor_start = 0, cursor_end = 0;
static int gx = 0, gy = 0, gxx = 79, gyy = 24;
static char buf[4096];




void init_emx()
{
    v_init();
    v_getctype(&cursor_start, &cursor_end);
}




void deinit_emx()
{
    // nothing to do
}




void _setcursortype(int curstype)
{
    if ( curstype == _NOCURSOR )
      v_hidecursor();
    else
      v_ctype(cursor_start, cursor_end);
}




void clrscr()
{
    if ( (gx == 0) && (gy == 0) && (gxx == 79) && (gyy == 24) )
      {
        v_clear();
        v_gotoxy(0, 0);
      }
    else
      {
        for (int i = gy; i <= gyy; ++i)
          {
            v_gotoxy(gx, i);
            v_putn(' ', gxx - gx + 1);
          }
        v_gotoxy(gx, gy);
      }
}




void gotoxy(int x, int y)
{
    v_gotoxy(x - 1 + gx, y - 1 + gy);
}




void textcolor(int c)
{
    v_attrib(c);
}




static void cprintf_aux(const char *s)
{
    char *ptr = buf;

    while (*s)
    {
        if ( *s == '\n' )
          {
            *ptr = 0;
            v_puts(buf);
            int x, y;

            v_getxy(&x, &y);
            if ( y != 24 )
              v_gotoxy(gx, y + 1);
            else
              v_putc('\n');

            ptr = buf;
          }
        else if ( *s != '\r' )
          *ptr++ = *s;

        ++s;
    }

    *ptr = 0;
    v_puts(buf);

}

void cprintf(const char *format, ...)
{
   va_list argp;
   char buffer[4096]; // one could hope it's enough

   va_start( argp, format );

   vsprintf(buffer, format, argp);
   cprintf_aux(buffer);

   va_end(argp);
}

void puttext(int x, int y, int lx, int ly, unsigned const char *buf)
{
    puttext(x, y, lx, ly, (const char *) buf);
}




void puttext(int x, int y, int lx, int ly, const char *buf)
{
    int count = (lx - x + 1);

    for (int i = y - 1; i < ly; ++i)
      {
        v_putline(buf + 2 * count * i, x - 1, i, count);
      }
}




void gettext(int x, int y, int lx, int ly, unsigned char *buf)
{
    gettext(x, y, lx, ly, (char *) buf);
}




void gettext(int x, int y, int lx, int ly, char *buf)
{
    int count = (lx - x + 1);

    for (int i = y - 1; i < ly; ++i)
      {
        v_getline(buf + 2 * count * i, x - 1, i, count);
      }
}




void window(int x, int y, int lx, int ly)
{
    gx = x - 1;
    gy = y - 1;
    gxx = lx - 1;
    gyy = ly - 1;
}




int wherex()
{
    int x, y;

    v_getxy(&x, &y);

    return x + 1 - gx;
}




int wherey()
{
    int x, y;

    v_getxy(&x, &y);

    return y + 1 - gy;
}




void putch(char c)
{
    v_putc(c);
}




int kbhit()
{
    return 0;
}




void delay(int ms)
{
    __sleep2(ms);
}




void textbackground(int c)
{
    if ( c != 0 )
      {
        fprintf(stderr, "bad background=%d", c);
        exit(1);
      }
}
