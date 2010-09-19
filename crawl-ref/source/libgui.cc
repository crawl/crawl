/*
 *  File:       libgui.cc
 *  Summary:    Functions that any display port needs to implement.
 *  Written by: M.Itakura
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cio.h"
#include "defines.h"
#include "env.h"
#include "externs.h"
#include "tilereg-text.h"
#include "message.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tiledef-main.h"
#include "travel.h"
#include "viewgeom.h"
#include "windowmanager.h"

int m_getch()
{
    return getch();
}

void set_mouse_enabled(bool enabled)
{
    crawl_state.mouse_enabled = enabled;
}

void gui_init_view_params(crawl_view_geometry &geom)
{
    // The tile version handles its own layout on a pixel-by-pixel basis.
    // Pretend that all of the regions start at character location (1,1).

    geom.termp.x = 1;
    geom.termp.y = 1;

    geom.termsz.x = 80;
    geom.termsz.y = 24;

    geom.viewp.x = 1;
    geom.viewp.y = 1;

    geom.hudp.x = 1;
    geom.hudp.y = 1;

    geom.msgp.x = 1;
    geom.msgp.y = 1;

    geom.mlistp.x  = 1;
    geom.mlistp.y  = 1;
    geom.mlistsz.x = 0;
    geom.mlistsz.y = 0;

    geom.viewsz.x = 17;
    geom.viewsz.y = 17;
}

int putch(unsigned char chr)
{
    // object's method
    TextRegion::text_mode->putch(chr);
    return 0;
}

int putwch(unsigned chr)
{
    // No unicode support.
    putch(static_cast<unsigned char>(chr));
    return 0;
}

void writeWChar(unsigned char *ch)
{
    // object's method
    TextRegion::text_mode->writeWChar(ch);
}

void clear_to_end_of_line()
{
    // object's method
    TextRegion::text_mode->clear_to_end_of_line();
}

int cprintf(const char *format,...)
{
    char buffer[2048];          // One full screen if no control seq...
    va_list argp;
    va_start(argp, format);
    vsnprintf(buffer, sizeof(buffer), format, argp);
    va_end(argp);
    // object's method
    TextRegion::text_mode->addstr(buffer);
    return 0;
}

void textcolor(int color)
{
    TextRegion::textcolor(color);
}

void textbackground(int bg)
{
    TextRegion::textbackground(bg);
}

void set_cursor_enabled(bool enabled)
{
    if (enabled)
        TextRegion::_setcursortype(1);
    else
        TextRegion::_setcursortype(0);
}

bool is_cursor_enabled()
{
    if (TextRegion::cursor_flag)
        return (true);

    return (false);
}

int wherex()
{
    return TextRegion::wherex();
}

int wherey()
{
    return TextRegion::wherey();
}

int get_number_of_lines()
{
    return tiles.get_number_of_lines();
}

int get_number_of_cols()
{
    return tiles.get_number_of_cols();
}

void put_colour_ch(int colour, unsigned ch)
{
    textcolor(colour);
    putwch(ch);
}

int getch_ck()
{
    return (tiles.getch_ck());
}

int getch()
{
    return getch_ck();
}

int clrscr()
{
    tiles.clrscr();
    return 0;
}

void cgotoxy(int x, int y, GotoRegion region)
{
    tiles.cgotoxy(x, y, region);
}

coord_def cgetpos(GotoRegion region)
{
    ASSERT(region == get_cursor_region());
    return (coord_def(wherex(), wherey()));
}

GotoRegion get_cursor_region()
{
    return (tiles.get_cursor_region());
}

void delay(int ms)
{
    tiles.redraw();
    wm->delay(ms);
}

void update_screen()
{
    tiles.set_need_redraw();
}

int kbhit()
{
    // Look for the presence of any keyboard events in the queue.
    int count = wm->get_event_count(WM_KEYDOWN);
    return (count > 0);
}

#ifdef UNIX
int itoa(int value, char *strptr, int radix)
{
    unsigned int bitmask = 32768;
    int ctr = 0;
    int startflag = 0;

    if (radix == 10)
    {
        sprintf(strptr, "%i", value);
    }
    else if (radix == 2)             /* int to "binary string" */
    {
        while (bitmask)
        {
            if (value & bitmask)
            {
                startflag = 1;
                sprintf(strptr + ctr, "1");
            }
            else if (startflag)
            {
                sprintf(strptr + ctr, "0");
            }

            bitmask = bitmask >> 1;
            if (startflag)
                ctr++;
        }

        if (!startflag)         /* Special case if value == 0 */
            sprintf((strptr + ctr++), "0");

        strptr[ctr] = (char) NULL;
    }
    return (0);                /* Me? Fail? Nah. */
}


// Convert string to lowercase.
char *strlwr(char *str)
{
    unsigned int i;

    for (i = 0; i < strlen(str); i++)
        str[i] = tolower(str[i]);

    return (str);
}

#endif // #ifdef UNIX
#endif // #ifdef USE_TILE
