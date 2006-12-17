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

void set_cursor_enabled(bool enabled)
{
    cursor_is_enabled = enabled;
    _setcursortype( enabled? _NORMALCURSOR : _NOCURSOR );
}

bool is_cursor_enabled()
{
    return (cursor_is_enabled);
}
