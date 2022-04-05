/**
 * @file
 * @brief Functions for windows console mode support.
**/

#include "AppHdr.h"

#if defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)

// WINDOWS INCLUDES GO HERE
/*
 * Exclude parts of windows.h that are not needed
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
#define NOCOLOR           /* COLOR_* colour values */
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
 * Exclude parts of windows.h that are not needed (Win32)
 */
#define WIN32_LEAN_AND_MEAN
#define NONLS             /* All NLS defines and routines */
#define NOSERVICE         /* All Service Controller routines, SERVICE_ equates, etc. */
#define NOKANJI           /* Kanji support stuff. */
#define NOMCX             /* Modem Configuration Extensions */

#include <excpt.h>
#include <stdarg.h>
#include <windows.h>
#undef max
#undef S_NORMAL

// END -- WINDOWS INCLUDES

#ifdef TARGET_COMPILER_MINGW
#include <signal.h>
#endif

#include <string.h>
#include <stdio.h>

#include "cio.h"
#include "defines.h"
#include "libutil.h"
#include "options.h"
#include "state.h"
#include "unicode.h"
#include "version.h"
#include "viewgeom.h"
#include "view.h"

wchar_t oldTitle[80];

static HANDLE inbuf = nullptr;
static HANDLE outbuf = nullptr;
static HANDLE old_outbuf = nullptr;
static int current_colour = -1;
static bool cursor_is_enabled = false;
static CONSOLE_CURSOR_INFO initial_cci;
static bool have_initial_cci = false;
// dirty line (sx,ex,y)
static int chsx = 0, chex = 0, chy = -1;
// cursor position (start at 0,0 --> 1,1)
static int cx = 0, cy = 0;

// and now, for the screen buffer
static CHAR_INFO *screen = nullptr;
static COORD screensize;
#define SCREENINDEX(x,y) ((x)+screensize.X*(y))
static unsigned InputCP, OutputCP;
static const unsigned PREFERRED_CODEPAGE = 437;

static bool w32_smart_cursor = true;

// we can do straight translation of DOS colour to win32 console colour.
#define WIN32COLOR(col) (WORD)(col)
static void writeChar(char32_t c);
static void bFlush();
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

void writeChar(char32_t c)
{
    if (c == '\t')
    {
        for (int i = 0; i < 8; ++i)
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

    // check for upper Unicode which Windows can't handle
    if (c > 0xFFFF)
        c = U'\xbf'; //¿

    int tc = WIN32COLOR(current_colour);
    pci = &screen[SCREENINDEX(cx,cy)];

    // is this a no-op?
    if (pci->Char.UnicodeChar != c)
        noop = false;
    else if (pci->Attributes != tc)
        noop = false;

    if (!noop)
    {
        // write the info and update the dirty area
        pci->Char.UnicodeChar = c;
        pci->Attributes = tc;

        if (chy < 0)
            chsx = cx;
        chy  = cy;
        chex = cx;
    }

    // update x position
    cx += 1;
    if (cx >= screensize.X)
        cx = screensize.X - 1;
}

void enable_smart_cursor(bool cursor)
{
    w32_smart_cursor = cursor;
}

bool is_smart_cursor_enabled()
{
    return w32_smart_cursor;
}

void bFlush()
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

    WriteConsoleOutputW(outbuf, screen, screensize, source, &target);

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

static void _set_string_input(bool value)
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

    if (SetConsoleMode(inbuf, inmodes) == 0)
    {
        fputs("Error initialising console input mode.", stderr);
        exit(0);
    }

    if (SetConsoleMode(outbuf, outmodes) == 0)
    {
        fputs("Error initialising console output mode.", stderr);
        exit(0);
    }

    // now flush it
    FlushConsoleInputBuffer(inbuf);
}

// Fake the user pressing Esc to break out of wait-for-input loops.
// Just one should be enough as we check the seen_hups flag, we just
// need to interrupt the syscall.
void w32_insert_escape()
{
    INPUT_RECORD esc;
    esc.EventType = KEY_EVENT;
    esc.Event.KeyEvent.bKeyDown = TRUE;
    esc.Event.KeyEvent.wRepeatCount = 1;
    esc.Event.KeyEvent.wVirtualKeyCode = VK_ESCAPE;
    // .wVirtualScanCode ?
    esc.Event.KeyEvent.uChar.UnicodeChar = ESCAPE;
    esc.Event.KeyEvent.dwControlKeyState = 0;
    WriteConsoleInputW(inbuf, &esc, 1, nullptr);
}

#ifdef TARGET_COMPILER_MINGW
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
        screen = nullptr;
    }

    screen = new CHAR_INFO[screensize.X * screensize.Y];

    COORD topleft;
    SMALL_RECT used;
    topleft.X = topleft.Y = 0;
    ::ReadConsoleOutputW(outbuf, screen, screensize, topleft, &used);
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

void console_startup()
{
    inbuf = GetStdHandle(STD_INPUT_HANDLE);
    old_outbuf = GetStdHandle(STD_OUTPUT_HANDLE);

    // Create a new "console screen buffer" so we don't tramp all over
    // the user's scrollback.
    outbuf = CreateConsoleScreenBuffer(
        GENERIC_READ |GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, // shared
        nullptr,                    // default security attributes
        CONSOLE_TEXTMODE_BUFFER, // must be TEXTMODE
        nullptr);                   // reserved; must be nullptr

    SetConsoleActiveScreenBuffer(outbuf);

    if (inbuf == INVALID_HANDLE_VALUE || outbuf == INVALID_HANDLE_VALUE)
    {
        fputs("Could not initialise libw32c console support.", stderr);
        exit(0);
    }

    string title = CRAWL " " + string(Version::Long);

    if (!GetConsoleTitleW(oldTitle, 78))
        *oldTitle = 0;
    SetConsoleTitleW(OUTW(title));

    // Use the initial Windows setting for cursor size if it exists.
    // TODO: Respect changing cursor size manually while Crawl is running.
    have_initial_cci = GetConsoleCursorInfo(outbuf, &initial_cci);

#ifdef TARGET_COMPILER_MINGW
    install_sighandlers();
#endif

    // by default, set string input to false:  use char-input only
    _set_string_input(false);

    // set up screen size
    set_w32_screen_size();

    // initialise text colour
    textcolour(DARKGREY);

    // initialise cursor to NONE.
    _setcursortype_internal(false);

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

void console_shutdown()
{
    // don't do anything if we were never initted
    if (inbuf == nullptr && outbuf == nullptr && old_outbuf == nullptr)
        return;

    // JWM, 06/12/2004: Code page stuff. If it was the preferred code page, it
    // doesn't need restoring. Shouldn't be an error and too bad if there is.
    if (InputCP && InputCP != PREFERRED_CODEPAGE)
        SetConsoleCP(InputCP);

    if (OutputCP && OutputCP != PREFERRED_CODEPAGE)
        SetConsoleOutputCP(OutputCP);

    // restore console attributes for normal function
    _set_string_input(true);

    // set cursor and normal textcolour
    _setcursortype_internal(true);
    textcolour(DARKGREY);

    inbuf = nullptr;

    delete [] screen;
    screen = nullptr;

    // finally, restore title
    if (*oldTitle)
        SetConsoleTitleW(oldTitle);

    // and switch back to the former console buffer
    if (old_outbuf)
    {
        SetConsoleActiveScreenBuffer(old_outbuf);
        CloseHandle(outbuf);
        old_outbuf = 0;
        outbuf = 0;
    }
}

void set_cursor_enabled(bool enabled)
{
    if (!w32_smart_cursor)
        _setcursortype_internal(enabled);
}

bool is_cursor_enabled()
{
    return cursor_is_enabled;
}

static void _setcursortype_internal(bool curstype)
{
    CONSOLE_CURSOR_INFO cci;

    if (curstype == cursor_is_enabled)
        return;

    cci.dwSize = have_initial_cci && initial_cci.dwSize ? initial_cci.dwSize
                                                        : 5;

    cci.bVisible = curstype ? TRUE : FALSE;
    cursor_is_enabled = curstype;
    SetConsoleCursorInfo(outbuf, &cci);

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

void clrscr()
{
    int x,y;
    COORD source;
    SMALL_RECT target;

    PCHAR_INFO pci = screen;

    for (x = 0; x < screensize.X; x++)
        for (y = 0; y < screensize.Y; y++)
        {
            pci->Char.UnicodeChar = ' ';
            pci->Attributes = 0;
            pci++;
        }

    source.X = 0;
    source.Y = 0;
    target.Left = 0;
    target.Top = 0;
    target.Right = screensize.X - 1;
    target.Bottom = screensize.Y - 1;

    WriteConsoleOutputW(outbuf, screen, screensize, source, &target);

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

void textcolour(int c)
{
    // change current colour used to stamp chars
    short fg = c & 0xF;
    short bg = (c >> 4) & 0xF;
    short macro_fg = Options.colour[fg];
    short macro_bg = Options.colour[bg];

    current_colour = macro_fg | (macro_bg << 4);
}

void textbackground(int c)
{
    // change current background colour used to stamp chars
    // parameter does NOT come bitshifted by four
    short bg = c & 0xF;
    short macro_bg = Options.colour[bg];

    current_colour = current_colour | (macro_bg << 4);
}

static void cprintf_aux(const char *s)
{
    // early out -- not initted yet
    if (outbuf == nullptr)
    {
        printf("%S", OUTW(s));
        return;
    }

    // loop through string
    char32_t c;
    while (int taken = utf8towc(&c, s))
    {
        s += taken;
        writeChar(c);
    }

    // flush string
    bFlush();
}

void cprintf(const char *format, ...)
{
    va_list argp;
    char buffer[4096]; // one could hope it's enough

    va_start(argp, format);

    vsnprintf(buffer, sizeof(buffer), format, argp);
    cprintf_aux(buffer);

    va_end(argp);
}

int wherex()
{
    return cx+1;
}

int wherey()
{
    return cy+1;
}

void putwch(char32_t c)
{
    if (c == 0)
        c = ' ';
    writeChar(c);
}

// translate virtual keys

#define VKEY_MAPPINGS 11
static int vk_tr[4][VKEY_MAPPINGS] = // virtual key, unmodified, shifted, control
{
    { VK_END, VK_DOWN, VK_NEXT, VK_LEFT, VK_CLEAR, VK_RIGHT,
      VK_HOME, VK_UP, VK_PRIOR, VK_INSERT, VK_TAB },
    { CK_END, CK_DOWN, CK_PGDN, CK_LEFT, CK_CLEAR, CK_RIGHT,
      CK_HOME, CK_UP, CK_PGUP , CK_INSERT, CONTROL('I') },
    { CK_SHIFT_END, CK_SHIFT_DOWN, CK_SHIFT_PGDN, CK_SHIFT_LEFT, CK_SHIFT_CLEAR, CK_SHIFT_RIGHT,
      CK_SHIFT_HOME, CK_SHIFT_UP, CK_SHIFT_PGUP, CK_SHIFT_INSERT, CK_SHIFT_TAB },
    { CK_CTRL_END, CK_CTRL_DOWN, CK_CTRL_PGDN, CK_CTRL_LEFT, CK_CTRL_CLEAR, CK_CTRL_RIGHT,
      CK_CTRL_HOME, CK_CTRL_UP, CK_CTRL_PGUP, CK_CTRL_INSERT, CK_CTRL_TAB },
};

static int ck_tr[] =
{
    'k', 'j', 'h', 'l', '0', 'y', 'b', '.', 'u', 'n', '0',
    // 'b', 'j', 'n', 'h', '.', 'l', 'y', 'k', 'u', (autofight)
    '8', '2', '4', '6', '0', '7', '1', '5', '9', '3', '0',
    // '1', '2', '3', '4', '5', '6', '7', '8', '9', (non-move autofight)
    11, 10, 8, 12, '0', 25, 2, 0, 21, 14, '0',
    // 2, 10, 14, 8, 0, 12, 25, 11, 21,
};

static int key_to_command(int keyin)
{
    if (keyin >= CK_UP && keyin <= CK_CTRL_PGDN)
        return ck_tr[ keyin - CK_UP ];

    if (keyin == CK_DELETE)
        return '.';

    return keyin;
}

static int vk_translate(WORD VirtCode, WCHAR c, DWORD cKeys)
{
    bool shftDown = false;
    bool ctrlDown = false;
    bool altDown  = !!(cKeys & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED));

    // step 1 - we don't care about shift or control
    if (VirtCode == VK_SHIFT || VirtCode == VK_CONTROL
        || VirtCode == VK_MENU || VirtCode == VK_CAPITAL
        || VirtCode == VK_NUMLOCK)
    {
        return 0;
    }

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

    // now translate the key. Dammit. This is !@#$(*& garbage.

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
    return getchk();
}

static int w32_proc_mouse_event(const MOUSE_EVENT_RECORD &mer)
{
    const coord_def pos(mer.dwMousePosition.X + 1, mer.dwMousePosition.Y + 1);
    crawl_view.mousep = pos;

    if (!crawl_state.mouse_enabled)
        return 0;

    c_mouse_event cme(pos);
    if (mer.dwEventFlags & MOUSE_MOVED)
        return CK_MOUSE_MOVE;

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
        return CK_MOUSE_CLICK;
    }

    return 0;
}


void set_getch_returns_resizes(bool rr)
{
    UNUSED(rr);
    // no-op on windows console: see mantis issue #11532
}

int getch_ck()
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
        if (crawl_state.seen_hups)
            return ESCAPE;

        if (ReadConsoleInputW(inbuf, &ir, 1, &nread) == 0)
            fputs("Error in ReadConsoleInputW()!", stderr);
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
                    key = vk_translate(kr->wVirtualKeyCode,
                                       kr->uChar.UnicodeChar,
                                       kr->dwControlKeyState);
                    if (key != 0)
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

int getchk()
{
    int c = getch_ck();
    return key_to_command(c);
}

bool kbhit()
{
    if (crawl_state.seen_hups)
        return 1;

    INPUT_RECORD ir[10];
    DWORD read_count = 0;
    PeekConsoleInputW(inbuf, ir, ARRAYSZ(ir), &read_count);
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

void delay(unsigned int ms)
{
    if (crawl_state.disables[DIS_DELAY])
        return;

    Sleep((DWORD)ms);
}

void puttext(int x1, int y1, const crawl_view_buffer &vbuf)
{
    const screen_cell_t *cell = vbuf;
    const coord_def size = vbuf.size();
    for (int y = 0; y < size.y; ++y)
    {
        cgotoxy(x1, y1 + y);
        for (int x = 0; x < size.x; ++x)
        {
            textcolour(cell->colour);
            putwch(cell->glyph);
            cell++;
        }
    }
    update_screen();
    textcolour(WHITE);
}

void update_screen()
{
    bFlush();
}

int get_number_of_lines()
{
    return screensize.Y;
}

int get_number_of_cols()
{
    return screensize.X;
}

int num_to_lines(int num)
{
    return num;
}

#endif /* #if defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL) */
