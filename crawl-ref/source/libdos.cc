/*
 *  File:       libdos.cc
 *  Summary:    Functions for DOS support.
 *  Written by: Darshan Shaligram
 *
 *  Added for Crawl Reference by $Author: nlanza $ on $Date: 2006-09-26T03:22:57.300929Z $
 */

// Every .cc must include AppHdr or bad things happen.
#include "AppHdr.h"
#include "externs.h"
#include <termios.h>
#include <conio.h>

static bool cursor_is_enabled = true;

void init_libdos()
{
    struct termios charmode;

    tcgetattr (0, &charmode);
    // Ignore Ctrl-C
    charmode.c_lflag &= ~ISIG;
    tcsetattr (0, TCSANOW, &charmode);
}

void clear_message_window()
{
    window(1, VIEW_EY + 1, get_number_of_cols(), get_number_of_lines());
    clrscr();
    window(1, 1, get_number_of_cols(), get_number_of_lines());
}

static void scroll_message_window()
{
    const int x = wherex(), y = wherey();

    textcolor(LIGHTGREY);
    movetext(1, VIEW_EY + 2, get_number_of_cols(), get_number_of_lines(),
             1, VIEW_EY + 1);
    gotoxy(1, get_number_of_lines());
    clreol();

    // Cursor also scrolls up so prompts don't look brain-damaged.
    if (y == get_number_of_lines())
        gotoxy(x, y - 1);
}

extern int get_message_window_height();
void message_out(int which_line, int colour, const char *s, int firstcol,
                 bool newline)
{
    if (!firstcol)
        firstcol = Options.delay_message_clear? 2 : 1;

    gotoxy(firstcol, which_line + VIEW_EY + 1);
    textcolor(colour);

    cprintf("%s", s);

    if (newline && which_line == get_message_window_height() - 1)
        scroll_message_window();
}

void set_cursor_enabled(bool enabled)
{
    cursor_is_enabled = enabled;
    _setcursortype( enabled? _NORMALCURSOR : _NOCURSOR );
}

bool is_cursor_enabled()
{
    return (cursor_is_enabled);
}

// This will force the cursor down to the next line.
void clear_to_end_of_line()
{
    clreol();
}

int get_number_of_lines()
{
    return (25);
}

int get_number_of_cols()
{
    return (80);
}
