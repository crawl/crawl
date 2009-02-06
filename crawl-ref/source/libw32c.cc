#include "AppHdr.h"
REVISION("$Rev$");

#if defined(WIN32CONSOLE)

/*
 *  File:       libw32c.cc
 *  Summary:    Functions for windows32 console mode support
 *  Written by: Gordon Lipford
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

// WINDOWS INCLUDES GO HERE
/*
 * Exclude parts of WINDOWS.H that are not needed
 */
#define NOCOMM            /* Comm driver APIs and definitions */
#define NOLOGERROR        /* LogError() and related definitions */
#define NOPROFILER        /* Profiler APIs */
#define NOLFILEIO         /* _l* file I/O routines */
#define NOOPENFILE        /* OpenFile and related definitions */
#define NORESOURCE        /* Resource management */
#define NOATOM            /* Atom management */
#define NOLANGUAGE        /* Character test routines */
#define NOLSTRING         /* lstr* string management routines */
#define NODBCS            /* Double-byte character set routines */
#define NOKEYBOARDINFO    /* Keyboard driver routines */
#define NOCOLOR           /* COLOR_* color values */
#define NODRAWTEXT        /* DrawText() and related definitions */
#define NOSCALABLEFONT    /* Truetype scalable font support */
#define NOMETAFILE        /* Metafile support */
#define NOSYSTEMPARAMSINFO /* SystemParametersInfo() and SPI_* definitions */
#define NODEFERWINDOWPOS  /* DeferWindowPos and related definitions */
#define NOKEYSTATES       /* MK_* message key state flags */
#define NOWH              /* SetWindowsHook and related WH_* definitions */
#define NOCLIPBOARD       /* Clipboard APIs and definitions */
#define NOICONS           /* IDI_* icon IDs */
#define NOMDI             /* MDI support */
#define NOCTLMGR          /* Control management and controls */
#define NOHELP            /* Help support */
/*
 * Exclude parts of WINDOWS.H that are not needed (Win32)
 */
#define WIN32_LEAN_AND_MEAN
#define NONLS             /* All NLS defines and routines */
#define NOSERVICE         /* All Service Controller routines, SERVICE_ equates, etc. */
#define NOKANJI           /* Kanji support stuff. */
#define NOMCX             /* Modem Configuration Extensions */
#ifndef _X86_
#define _X86_                     /* target architecture */
#endif

#include <excpt.h>
#include <stdarg.h>
#undef ARRAYSZ
#include <windows.h>
#undef max

// END -- WINDOWS INCLUDES

#ifdef __MINGW32__
#include <signal.h>
#endif

#include <string.h>
#include <stdio.h>
#include "cio.h"
#include "defines.h"
#include "libutil.h"
#include "state.h"
#include "stuff.h"
#include "version.h"
#include "view.h"

char oldTitle[80];

static HANDLE inbuf = NULL;
static HANDLE outbuf = NULL;
static int current_color = -1;
static bool cursor_is_enabled = false;
// dirty line (sx,ex,y)
static int chsx = 0, chex = 0, chy = -1;
// cursor position (start at 0,0 --> 1,1)
static int cx = 0, cy = 0;

// and now, for the screen buffer
static CHAR_INFO *screen = NULL;
static COORD screensize;
#define SCREENINDEX(x,y) ((x)+screensize.X*(y))
static bool buffering = false;
// static const char *windowTitle = "Crawl " VERSION;
static unsigned InputCP, OutputCP;
static const unsigned PREFERRED_CODEPAGE = 437;

static bool w32_smart_cursor = true;

// we can do straight translation of DOS color to win32 console color.
#define WIN32COLOR(col) (WORD)(col)
static void writeChar(char c);
static void bFlush(void);
static void _setcursortype_internal(bool curstype);

// [ds] Unused for portability reasons
/*
static DWORD crawlColorData[16] =
// BGR data, easier to put in registry
{
   0x00000000,  // BLACK
   0x00ff00cd,  // BLUE
   0x0046b964,  // GREEN
   0x00b4b400,  // CYAN
   0x000085ff,  // RED
   0x00ee82ee,  // MAGENTA
   0x005a6fcd,  // BROWN
   0x00c0c0c0,  // LT GREY
   0x00808080,  // DK GREY
   0x00ff8600,  // LT BLUE
   0x0000ff85,  // LT GREEN
   0x00ffff00,  // LT CYAN
   0x000000ff,  // LT RED
   0x00bf7091,  // LT MAGENTA
   0x0000ffff,  // YELLOW
   0x00ffffff   // WHITE
};
 */

void writeChar(char c)
{
    if ( c == '\t' )
    {
        for ( int i = 0; i < 8; ++i )
            writeChar(' ');
        return;
    }

    bool noop = true;
    PCHAR_INFO pci;

    // check for CR: noop
    if (c == 0x0D)
        return;

    // check for newline
    if (c == 0x0A)
    {
        // must flush current buffer
        bFlush();

        // reposition
        cgotoxy(1, cy+2);

        return;
    }

    int tc = WIN32COLOR(current_color);
    pci = &screen[SCREENINDEX(cx,cy)];

    // is this a no-op?
    if (pci->Char.AsciiChar != c)
        noop = false;
    else if (pci->Attributes != tc)
        noop = false;

    if (!noop)
    {
        // write the info and update the dirty area
        pci->Char.AsciiChar = c;
        pci->Attributes = tc;

        if (chy < 0)
            chsx = cx;
        chy = cy;
        chex = cx;

        // if we're not buffering, flush
        if (!buffering)
            bFlush();
    }

    // update x position
    cx += 1;
    if (cx >= screensize.X) cx = screensize.X - 1;
}

void enable_smart_cursor(bool cursor)
{
    w32_smart_cursor = cursor;
}

bool is_smart_cursor_enabled()
{
    return (w32_smart_cursor);
}

void bFlush(void)
{
    COORD source;
    SMALL_RECT target;

    // see if we have a dirty area
    if (chy < 0)
        return;

    // set up call
    source.X = chsx;
    source.Y = chy;

    target.Left = chsx;
    target.Top = chy;
    target.Right = chex;
    target.Bottom = chy;

    WriteConsoleOutput(outbuf, screen, screensize, source, &target);

    chy = -1;

    // if cursor is not NOCURSOR, update screen
    if (cursor_is_enabled)
    {
        COORD xy;
        xy.X = cx;
        xy.Y = cy;
        SetConsoleCursorPosition(outbuf, xy);
    }
}

void set_mouse_enabled(bool enabled)
{
    DWORD inmode;
    if (::GetConsoleMode(inbuf, &inmode))
    {
        if (enabled)
            inmode |= ENABLE_MOUSE_INPUT;
        else
            inmode &= ~ENABLE_MOUSE_INPUT;

        ::SetConsoleMode(inbuf, inmode);
    }
}

void set_string_input(bool value)
{
    DWORD inmodes, outmodes;
    if (value == TRUE)
    {
        inmodes = ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT
                     | ENABLE_PROCESSED_INPUT
                     | ENABLE_MOUSE_INPUT
                     | ENABLE_WINDOW_INPUT;
        outmodes = ENABLE_PROCESSED_OUTPUT;
    }
    else
    {
        inmodes = ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT;
        outmodes = 0;
    }

    if ( SetConsoleMode( inbuf, inmodes ) == 0)
    {
        fputs("Error initialising console input mode.", stderr);
        exit(0);
    }

    if ( SetConsoleMode( outbuf, outmodes ) == 0)
    {
        fputs("Error initialising console output mode.", stderr);
        exit(0);
    }

    // now flush it
    FlushConsoleInputBuffer( inbuf );
}

// this apparently only works for Win2K+ and ME+

static void init_colors(char *windowTitle)
{
   UNUSED( windowTitle );

   // look up the Crawl shortcut

   // if found, modify the colortable entries in the NT_CONSOLE_PROPS
   // structure.

   // if not found, quit.
}

#ifdef __MINGW32__
static void install_sighandlers()
{
    signal(SIGINT, SIG_IGN);
}
#endif

static void set_w32_screen_size()
{
    CONSOLE_SCREEN_BUFFER_INFO cinf;
    if (::GetConsoleScreenBufferInfo(outbuf, &cinf))
    {
        screensize.X = cinf.srWindow.Right - cinf.srWindow.Left + 1;
        screensize.Y = cinf.srWindow.Bottom - cinf.srWindow.Top + 1;
    }
    else
    {
        screensize.X = 80;
        screensize.Y = 25;
    }

    if (screen)
    {
        delete [] screen;
        screen = NULL;
    }

    screen = new CHAR_INFO[screensize.X * screensize.Y];

    COORD topleft;
    SMALL_RECT used;
    topleft.X = topleft.Y = 0;
    ::ReadConsoleOutput(outbuf, screen, screensize, topleft, &used);
}

static void w32_handle_resize_event()
{
    if (crawl_state.waiting_for_command)
        handle_terminal_resize(true);
    else
        crawl_state.terminal_resized = true;
}

static void w32_check_screen_resize()
{
    CONSOLE_SCREEN_BUFFER_INFO cinf;
    if (::GetConsoleScreenBufferInfo(outbuf, &cinf))
    {
        if (screensize.X != cinf.srWindow.Right - cinf.srWindow.Left + 1
            || screensize.Y != cinf.srWindow.Bottom - cinf.srWindow.Top + 1)
        {
            w32_handle_resize_event();
        }
    }
}

static void w32_term_resizer()
{
    set_w32_screen_size();
    crawl_view.init_geometry();
}

void init_libw32c(void)
{
    inbuf = GetStdHandle( STD_INPUT_HANDLE );
    outbuf = GetStdHandle( STD_OUTPUT_HANDLE );

    if (inbuf == INVALID_HANDLE_VALUE || outbuf == INVALID_HANDLE_VALUE)
    {
        fputs("Could not initialise libw32c console support.", stderr);
        exit(0);
    }

    GetConsoleTitle( oldTitle, 78 );
    SetConsoleTitle( CRAWL " " VERSION );

#ifdef __MINGW32__
    install_sighandlers();
#endif

    init_colors(oldTitle);

    // by default, set string input to false:  use char-input only
    set_string_input( false );

    // set up screen size
    set_w32_screen_size();

    // initialise text color
    textcolor(DARKGREY);

    // initialise cursor to NONE.
    _setcursortype_internal(false);

    // buffering defaults to ON -- very important!
    set_buffering(true);

    crawl_state.terminal_resize_handler = w32_term_resizer;
    crawl_state.terminal_resize_check   = w32_check_screen_resize;

    // JWM, 06/12/2004: Code page setting, as XP does not use ANSI 437 by
    // default.
    InputCP = GetConsoleCP();
    OutputCP = GetConsoleOutputCP();

    // Don't kill Crawl if we can't set the codepage. Windows 95/98/ME
    // don't have support for setting the input and output codepage.
    // I'm also not convinced we need to set the input codepage at all.
    if (InputCP != PREFERRED_CODEPAGE)
        SetConsoleCP(PREFERRED_CODEPAGE);

    if (OutputCP != PREFERRED_CODEPAGE)
        SetConsoleOutputCP(PREFERRED_CODEPAGE);
}

void deinit_libw32c(void)
{
    // don't do anything if we were never initted
    if (inbuf == NULL || outbuf == NULL)
        return;

    // JWM, 06/12/2004: Code page stuff.  If it was the preferred code page, it
    // doesn't need restoring.  Shouldn't be an error and too bad if there is.
    if (InputCP && InputCP != PREFERRED_CODEPAGE)
        SetConsoleCP(InputCP);

    if (OutputCP && OutputCP != PREFERRED_CODEPAGE)
        SetConsoleOutputCP(OutputCP);

    // restore console attributes for normal function
    set_string_input(true);

    // set cursor and normal textcolor
    _setcursortype_internal(true);
    textcolor(DARKGREY);

    delete [] screen;
    screen = NULL;

    // finally, restore title
    SetConsoleTitle( oldTitle );
}

void set_cursor_enabled(bool enabled)
{
    if (!w32_smart_cursor)
        _setcursortype_internal(enabled);
}

bool is_cursor_enabled()
{
    return (cursor_is_enabled);
}

void _setcursortype_internal(bool curstype)
{
    CONSOLE_CURSOR_INFO cci;

    if (curstype == cursor_is_enabled)
        return;

    cci.dwSize = 5;
    cci.bVisible = curstype? TRUE : FALSE;
    cursor_is_enabled = curstype;
    SetConsoleCursorInfo( outbuf, &cci );

    // now, if we just changed from NOCURSOR to CURSOR,
    // actually move screen cursor
    if (cursor_is_enabled)
        cgotoxy(cx+1, cy+1);
}

// This will force the cursor down to the next line.
void clear_to_end_of_line()
{
    const int pos = wherex();
    const int cols = get_number_of_cols();
    if (pos <= cols)
        cprintf("%*s", cols - pos + 1, "");
}

void clrscr(void)
{
    int x,y;
    COORD source;
    SMALL_RECT target;

    PCHAR_INFO pci = screen;

    for (x = 0; x < screensize.X; x++)
        for (y = 0; y < screensize.Y; y++)
        {
            pci->Char.AsciiChar = ' ';
            pci->Attributes = 0;
            pci++;
        }

    source.X = 0;
    source.Y = 0;
    target.Left = 0;
    target.Top = 0;
    target.Right = screensize.X - 1;
    target.Bottom = screensize.Y - 1;

    WriteConsoleOutput(outbuf, screen, screensize, source, &target);

    // reset cursor to 1,1 for convenience
    cgotoxy(1,1);
}

void gotoxy_sys(int x, int y)
{
    // always flush on goto
    bFlush();

    // bounds check
    if (x < 1)
        x = 1;
    if (x > screensize.X)
        x = screensize.X;
    if (y < 1)
        y = 1;
    if (y > screensize.Y)
        y = screensize.Y;

    // change current cursor
    cx = x - 1;
    cy = y - 1;

    // if cursor is not NOCURSOR, update screen
    if (cursor_is_enabled)
    {
        COORD xy;
        xy.X = cx;
        xy.Y = cy;
        if (SetConsoleCursorPosition(outbuf, xy) == 0)
            fputs("SetConsoleCursorPosition() failed!", stderr);
    }
}

void textattr(int c)
{
    textcolor(c);
}

void textcolor(int c)
{
    // change current color used to stamp chars
    short fg = c & 0xF;
    short bg = (c >> 4) & 0xF;
    short macro_fg = Options.colour[fg];
    short macro_bg = Options.colour[bg];

    current_color = macro_fg | (macro_bg << 4);
}

void clear_message_window()
{
    PCHAR_INFO pci = screen + SCREENINDEX(crawl_view.msgp.x - 1,
                                          crawl_view.msgp.y - 1);
    for (int x = 0; x < screensize.X; x++)
    {
        for (int y = 0; y < crawl_view.msgsz.y; y++)
        {
            pci->Char.AsciiChar = ' ';
            pci->Attributes = 0;
            pci++;
        }
    }

    COORD source;
    SMALL_RECT target;

    source.X = crawl_view.msgp.x - 1;
    source.Y = crawl_view.msgp.y - 1;
    target.Left = crawl_view.msgp.x - 1;
    target.Top = crawl_view.msgp.y - 1;
    target.Right = screensize.X - 1;
    target.Bottom = screensize.Y - 1;

    WriteConsoleOutput(outbuf, screen, screensize, source, &target);
}

static void scroll_message_buffer()
{
    memmove( screen + SCREENINDEX(crawl_view.msgp.x - 1, crawl_view.msgp.y - 1),
             screen + SCREENINDEX(crawl_view.msgp.x - 1, crawl_view.msgp.y),
             crawl_view.msgsz.y * screensize.X * sizeof(*screen) );
}

static void scroll_message_window()
{
    SMALL_RECT scroll_rectangle, clip_rectangle;
    scroll_rectangle.Left   = crawl_view.msgp.x - 1;
    scroll_rectangle.Top    = crawl_view.msgp.y;
    scroll_rectangle.Right  = screensize.X - 1;
    scroll_rectangle.Bottom = screensize.Y - 1;

    clip_rectangle          = scroll_rectangle;
    clip_rectangle.Top      = crawl_view.msgp.y - 1;

    COORD new_origin;
    new_origin.X            = crawl_view.msgp.x - 1;
    new_origin.Y            = crawl_view.msgp.y - 1;

    CHAR_INFO fill;
    fill.Char.AsciiChar     = ' ';
    fill.Attributes         = WIN32COLOR(LIGHTGREY);

    ::ScrollConsoleScreenBuffer(
            outbuf, &scroll_rectangle, &clip_rectangle,
            new_origin, &fill);

    scroll_message_buffer();

    // Cursor also scrolls up so prompts don't look brain-damaged.
    if (wherey() == screensize.Y)
        cgotoxy(wherex(), wherey() - 1);
}

void message_out(int which_line, int colour, const char *s, int firstcol,
                 bool newline)
{
    if (!firstcol)
        firstcol = Options.delay_message_clear? 2 : 1;

    cgotoxy(firstcol - 1 + crawl_view.msgp.x,
           which_line + crawl_view.msgp.y);
    textcolor(colour);

    cprintf("%s", s);

    if (newline && which_line == crawl_view.msgsz.y - 1)
        scroll_message_window();
}

static void cprintf_aux(const char *s)
{
    // early out -- not initted yet
    if (outbuf == NULL)
    {
        printf(s);
        return;
    }

    // turn buffering ON (temporarily)
    bool oldValue = buffering;
    set_buffering(true);

    // loop through string
    char *p = (char *)s;
    while (*p)
    {
        writeChar(*p++);
    }

    // reset buffering
    set_buffering(oldValue);

    // flush string
    bFlush();
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

void window(int x, int y, int lx, int ly)
{
    // do nothing
    UNUSED( x );
    UNUSED( y );
    UNUSED( lx );
    UNUSED( ly );
}

int wherex(void)
{
    return cx+1;
}

int wherey(void)
{
    return cy+1;
}

void putch(char c)
{
    // special case: check for '0' char: map to space
    if (c == 0)
        c = ' ';

    writeChar(c);
}

void putwch(unsigned c)
{
    putch((char) c);
}

// translate virtual keys

#define VKEY_MAPPINGS 10
static int vk_tr[4][VKEY_MAPPINGS] = // virtual key, unmodified, shifted, control
{
   { VK_END, VK_DOWN, VK_NEXT, VK_LEFT, VK_CLEAR, VK_RIGHT, VK_HOME, VK_UP, VK_PRIOR, VK_INSERT },
   { CK_END, CK_DOWN, CK_PGDN, CK_LEFT, CK_CLEAR, CK_RIGHT, CK_HOME, CK_UP, CK_PGUP , CK_INSERT },
   { CK_SHIFT_END, CK_SHIFT_DOWN, CK_SHIFT_PGDN, CK_SHIFT_LEFT, CK_SHIFT_CLEAR, CK_SHIFT_RIGHT, CK_SHIFT_HOME, CK_SHIFT_UP, CK_SHIFT_PGUP, CK_SHIFT_INSERT },
   { CK_CTRL_END, CK_CTRL_DOWN, CK_CTRL_PGDN, CK_CTRL_LEFT, CK_CTRL_CLEAR, CK_CTRL_RIGHT, CK_CTRL_HOME, CK_CTRL_UP, CK_CTRL_PGUP, CK_CTRL_INSERT },
   };

static int ck_tr[] =
{
    'k', 'j', 'h', 'l', '0', 'y', 'b', '.', 'u', 'n',
    // 'b', 'j', 'n', 'h', '.', 'l', 'y', 'k', 'u' ,
    '8', '2', '4', '6', '0', '7', '1', '5', '9', '3',
    // '1', '2', '3', '4', '5', '6', '7', '8', '9' ,
    11, 10, 8, 12, '0', 25, 2, 0, 21, 14
    // 2, 10, 14, 8, 0, 12, 25, 11, 21 ,
};

int key_to_command( int keyin )
{
    if (keyin >= CK_UP && keyin <= CK_CTRL_PGDN)
        return ck_tr[ keyin - CK_UP ];

    if (keyin == CK_DELETE)
        return '.';

    return keyin;
}

int vk_translate( WORD VirtCode, CHAR c, DWORD cKeys)
{
    bool shftDown = false;
    bool ctrlDown = false;
    bool altDown  = !!(cKeys & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED));

    // step 1 - we don't care about shift or control
    if (VirtCode == VK_SHIFT || VirtCode == VK_CONTROL ||
        VirtCode == VK_MENU || VirtCode == VK_CAPITAL ||
        VirtCode == VK_NUMLOCK)
        return 0;

    // step 2 - translate the <Esc> key to 0x1b
    if (VirtCode == VK_ESCAPE)
        return 0x1b;            // same as it ever was..

    // step 3 - translate shifted or controlled numeric keypad keys
    if (cKeys & SHIFT_PRESSED)
        shftDown = true;
    if (cKeys & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
        ctrlDown = true;           // control takes precedence over shift

    // hack - translate ^P and ^Q since 16 and 17 are taken by CTRL and SHIFT
    if ((VirtCode == 80 || VirtCode == 81) && ctrlDown)
        return VirtCode & 0x003f;     // shift back down

    if (VirtCode == VK_DELETE && !ctrlDown)         // assume keypad '.'
        return CK_DELETE;

    // see if we're a vkey
    int mkey;
    for (mkey = 0; mkey<VKEY_MAPPINGS; mkey++)
        if (VirtCode == vk_tr[0][mkey])
            break;

    // step 4 - just return the damn key.
    if (mkey == VKEY_MAPPINGS)
    {
        if (c)
            return c;

        // ds -- Icky hacks to allow keymaps with funky keys.
        if (ctrlDown)
            VirtCode |= 512;
        if (shftDown)
            VirtCode |= 1024;
        if (altDown)
            VirtCode |= 2048;

        // ds -- Cheat and returns 256 + VK if the char is zero. This allows us
        // to use the VK for macros and is on par for evil with the rest of
        // this function anyway.
        return VirtCode | 256;
    }

    // now translate the key.  Dammit.  This is !@#$(*& garbage.

    // control key?
    if (ctrlDown)
        return vk_tr[3][mkey];

    // shifted?
    if (shftDown)
        return vk_tr[2][mkey];
    return vk_tr[1][mkey];
}

int m_getch()
{
    return getch();
}

static int w32_proc_mouse_event(const MOUSE_EVENT_RECORD &mer)
{
    const coord_def pos(mer.dwMousePosition.X + 1, mer.dwMousePosition.Y + 1);
    crawl_view.mousep = pos;

    if (!crawl_state.mouse_enabled)
        return (0);

    c_mouse_event cme(pos);
    if (mer.dwEventFlags & MOUSE_MOVED)
        return (CK_MOUSE_MOVE);

    if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
        cme.bstate |= c_mouse_event::BUTTON1;
    else if (mer.dwButtonState & RIGHTMOST_BUTTON_PRESSED)
        cme.bstate |= c_mouse_event::BUTTON3;

    if ((mer.dwEventFlags & MOUSE_WHEELED) && mer.dwButtonState)
    {
        if (!(mer.dwButtonState & 0x10000000UL))
            cme.bstate |= c_mouse_event::BUTTON_SCRL_UP;
        else
            cme.bstate |= c_mouse_event::BUTTON_SCRL_DN;
    }

    if (cme)
    {
        new_mouse_event(cme);
        return (CK_MOUSE_CLICK);
    }

    return (0);
}

int getch_ck(void)
{
    INPUT_RECORD ir;
    DWORD nread;
    int key = 0;
    static int repeat_count = 0;
    static int repeat_key = 0;

    KEY_EVENT_RECORD *kr;

    // handle key repeats
    if (repeat_count > 0)
    {
        repeat_count -= 1;
        return repeat_key;
    }

    const bool oldValue = cursor_is_enabled;
    if (w32_smart_cursor)
        _setcursortype_internal(true);

    bool waiting_for_event = true;
    while (waiting_for_event)
    {
        if (ReadConsoleInput( inbuf, &ir, 1, &nread) == 0)
            fputs("Error in ReadConsoleInput()!", stderr);
        if (nread > 0)
        {
            // ignore if it isn't a keyboard event.
            switch (ir.EventType)
            {
            case KEY_EVENT:
                kr = &ir.Event.KeyEvent;
                // ignore if it is a 'key up' - we only want 'key down'
                if (kr->bKeyDown)
                {
                    key = vk_translate( kr->wVirtualKeyCode,
                                        kr->uChar.AsciiChar,
                                        kr->dwControlKeyState );
                    if (key > 0)
                    {
                        repeat_count = kr->wRepeatCount - 1;
                        repeat_key = key;
                        waiting_for_event = false;
                        break;
                    }
                }
                break;

            case WINDOW_BUFFER_SIZE_EVENT:
                w32_handle_resize_event();
                break;

            case MOUSE_EVENT:
                if ((key = w32_proc_mouse_event(ir.Event.MouseEvent)))
                    waiting_for_event = false;
                break;
            }
        }
    }

    if (w32_smart_cursor)
        _setcursortype_internal(oldValue);

    return key;
}

int getch(void)
{
    int c = getch_ck();
    return key_to_command( c );
}

int getche(void)
{
    // turn buffering off temporarily
    const bool oldValue = buffering;
    set_buffering(false);

    int val = getch();

    if (val != 0)
        putch(val);

    // restore buffering value
    set_buffering(oldValue);

    return val;
}

int kbhit()
{
    INPUT_RECORD ir[10];
    DWORD read_count = 0;
    PeekConsoleInput(inbuf, ir, sizeof ir / sizeof(ir[0]), &read_count);
    if (read_count > 0)
    {
        for (unsigned i = 0; i < read_count; ++i)
            if (ir[i].EventType == KEY_EVENT)
            {
                KEY_EVENT_RECORD *kr;
                kr = &(ir[i].Event.KeyEvent);

                if (kr->bKeyDown)
                    return 1;
            }
    }
    return 0;
}

void delay(int ms)
{
    Sleep((DWORD)ms);
}

void textbackground(int c)
{
    // do nothing
    UNUSED( c );
}

int get_console_string(char *buf, int maxlen)
{
    DWORD nread;
    // set console input to line mode
    set_string_input( true );

    // force cursor
    const bool oldValue = cursor_is_enabled;

    if (w32_smart_cursor)
        _setcursortype_internal(true);

    // set actual screen color to current color
    SetConsoleTextAttribute( outbuf, WIN32COLOR(current_color) );

    if (ReadConsole( inbuf, buf, (DWORD)(maxlen-1), &nread, NULL) == 0)
        fputs("Error in ReadConsole()!", stderr);

    // terminate string, then strip CRLF, replace with \0
    buf[maxlen-1] = 0;
    for (unsigned i=(nread<3 ? 0 : nread-3); i<nread; i++)
    {
        if (buf[i] == 0x0A || buf[i] == 0x0D)
        {
            buf[i] = '\0';
            break;
        }
    }

    // reset console mode - also flushes if player has typed in
    // too long of a name so we don't get silly garbage on return.
    set_string_input( false );

    // restore old cursor
    if (w32_smart_cursor)
        _setcursortype_internal(oldValue);

    // return # of bytes read
    return (int)nread;
}

void puttext(int x1, int y1, int x2, int y2, const screen_buffer_t *buf)
{
    for (int y = y1; y <= y2; ++y)
    {
        cgotoxy(x1, y);
        for (int x = x1; x <= x2; x++)
        {
            textattr( buf[1] );
            putwch( *buf );
            buf += 2;
        }
    }
    update_screen();
    textattr(WHITE);
}

void update_screen()
{
    bFlush();
}

bool set_buffering( bool value )
{
    bool oldValue = buffering;

    if (value == false)
    {
        // must flush buffer
        bFlush();
    }
    buffering = value;

    return oldValue;
}

int get_number_of_lines()
{
    return (screensize.Y);
}

int get_number_of_cols()
{
    return (screensize.X);
}

#if _MSC_VER
struct DIR
{
 public:
    DIR()
        : hFind(INVALID_HANDLE_VALUE),
          wfd_valid(false)
    {
        memset(&wfd, 0, sizeof(wfd));
        memset(&entry, 0, sizeof(entry));
    }

    ~DIR()
    {
        if (hFind != INVALID_HANDLE_VALUE)
        {
            FindClose(hFind);
        }
    }

    bool init(const char* szFind)
    {
        // Check that it's a directory, first.
        {
            const DWORD dwAttr = GetFileAttributes(szFind);
            if (dwAttr == INVALID_FILE_ATTRIBUTES)
                return (false);
            if ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) == 0)
                return (false);
        }

        find = szFind;
        find += "\\*";

        hFind = FindFirstFileA(find.c_str(), &wfd);
        wfd_valid = (hFind != INVALID_HANDLE_VALUE);
        return (true);
    }

    dirent* readdir()
    {
        if (!wfd_valid)
            return 0;

        _convert_wfd_to_dirent();
        wfd_valid = (bool) FindNextFileA(hFind, &wfd);

        return (&entry);
    }

 private:
    void _convert_wfd_to_dirent()
    {
        entry.d_reclen = sizeof(dirent);
        entry.d_namlen = strlen(entry.d_name);
        entry.d_type = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ? DT_DIR : DT_REG;
        strncpy(entry.d_name, wfd.cFileName, sizeof(entry.d_name));
        entry.d_name[sizeof(entry.d_name)-1] = 0;
    }

 private:
    HANDLE hFind;
    bool wfd_valid;
    WIN32_FIND_DATA wfd;
    std::string find;
    dirent entry;
    // since opendir calls FindFirstFile, we need a means of telling the
    // first call to readdir that we already have a file.
    // that's the case iff this is == 0; we use a counter rather than a
    // flag because that allows keeping statistics.
    int num_entries_scanned;
};


DIR* opendir(char* path)
{
    DIR* d = new DIR();
    if (d->init(path))
    {
        return d;
    }
    else
    {
        delete d;
        return 0;
    }
}

dirent* readdir(DIR* d)
{
    return d->readdir();
}

int closedir(DIR* d)
{
    delete d;
    return 0;
}

int ftruncate(int fp, int size)
{
    ASSERT(false);              // unimplemented
    return 0;
}

#endif /* #if _MSC_VER */

#endif /* #if defined(WIN32CONSOLE) */
