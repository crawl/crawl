/*
 *  File:       libdos.cc
 *  Summary:    Functions for DOS support.
 *  Written by: Darshan Shaligram
 *
 *  Added for Crawl Reference by $Author: nlanza $ on $Date: 2006-09-26T03:22:57.300929Z $
 */

// Every .cc must include AppHdr or bad things happen.
#include "AppHdr.h"
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
    window(1, 18, 78, 25);
    clrscr();
    window(1, 1, 80, 25);
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
    const int pos = wherex();
    const int cols = get_number_of_cols();
    if (pos <= cols)
        cprintf("%*s", cols - pos + 1, "");
}

int get_number_of_lines()
{
    return (25);
}

int get_number_of_cols()
{
    return (80);
}
