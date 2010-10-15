/*
 *  File:       libdos.cc
 *  Summary:    Functions for DOS support.
 *              Needed by makefile.dos.
 *  Written by: Darshan Shaligram
 *
 *  Added for Crawl Reference by dshaligram on Wed Nov 22 08:41:20 2006 UTC
 */

// Every .cc must include AppHdr or bad things happen.
#include "AppHdr.h"

#include "cio.h"
#include <termios.h>
#include <conio.h>
#include "options.h"
#include "viewgeom.h"

#if defined(TARGET_OS_DOS)

static bool cursor_is_enabled = true;

void init_libdos()
{
    struct termios charmode;

    tcgetattr (0, &charmode);
    // Ignore Ctrl-C
    charmode.c_lflag &= ~ISIG;
    tcsetattr (0, TCSANOW, &charmode);

    // Turn off blink.
    intensevideo();
}

void set_cursor_enabled(bool enabled)
{
    cursor_is_enabled = enabled;
    _setcursortype(enabled? _NORMALCURSOR : _NOCURSOR);
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

int getch_ck()
{
    int c = getch();
    if (!c)
    {
        switch (c = getch())
        {
        case 'O':  return CK_END;
        case 'P':  return CK_DOWN;
        case 'I':  return CK_PGUP;
        case 'H':  return CK_UP;
        case 'G':  return CK_HOME;
        case 'K':  return CK_LEFT;
        case 'Q':  return CK_PGDN;
        case 'M':  return CK_RIGHT;
        case 119:  return CK_CTRL_HOME;
        case 141:  return CK_CTRL_UP;
        case 132:  return CK_CTRL_PGUP;
        case 116:  return CK_CTRL_RIGHT;
        case 118:  return CK_CTRL_PGDN;
        case 145:  return CK_CTRL_DOWN;
        case 117:  return CK_CTRL_END;
        case 115:  return CK_CTRL_LEFT;
        case 'S':  return CK_DELETE;
        }
    }
    return c;
}

int m_getch()
{
    return getch();
}

void putwch(unsigned c)
{
    putch(static_cast<char>(c));
}

#endif /* #if defined(TARGET_OS_DOS) */
