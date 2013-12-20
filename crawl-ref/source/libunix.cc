/**
 * @file
 * @brief Functions for unix and curses support
**/

/* Emulation of ancient Borland conio.
   Some of these are inspired by/stolen from the Linux-conio package
   by Mental EXPlotion. Hope you guys don't mind.
   The colour exchange system is perhaps a little overkill, but I wanted
   it to be general to support future changes.. The only thing not
   supported properly is black on black (used by curses for "normal" mode)
   and white on white (used by me for "bright black" (darkgrey) on black

   Jan 1998 Svante Gerhard <svante@algonet.se>                          */

#include "AppHdr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <term.h>
#define _LIBUNIX_IMPLEMENTATION
#include "libunix.h"
#include "defines.h"

#include "cio.h"
#include "crash.h"
#include "enum.h"
#include "externs.h"
#include "libutil.h"
#include "options.h"
#include "files.h"
#include "state.h"
#include "unicode.h"
#include "view.h"
#include "viewgeom.h"

#include <wchar.h>
#include <locale.h>
#include <langinfo.h>
#include <termios.h>

static struct termios def_term;
static struct termios game_term;

#ifdef USE_UNIX_SIGNALS
#include <signal.h>
#endif

#include <time.h>

// Its best if curses comes at the end (name conflicts with Solaris). -- bwr
#ifndef CURSES_INCLUDE_FILE
    #ifndef _XOPEN_SOURCE_EXTENDED
    #define _XOPEN_SOURCE_EXTENDED
    #endif

    // ncurses has redundant declarations between term.h and curses.h
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wredundant-decls"
    #include <curses.h>
    #pragma GCC diagnostic pop
#else
    #include CURSES_INCLUDE_FILE
#endif

// Globals holding current text/backg. colors
static short FG_COL = WHITE;
static short BG_COL = BLACK;
static int   Current_Colour = COLOR_PAIR(BG_COL * 8 + FG_COL);

static int curs_fg_attr(int col);
static int curs_bg_attr(int col);

static bool cursor_is_enabled = true;

static unsigned int convert_to_curses_attr(int chattr)
{
    switch (chattr & CHATTR_ATTRMASK)
    {
    case CHATTR_STANDOUT:       return A_STANDOUT;
    case CHATTR_BOLD:           return A_BOLD;
    case CHATTR_BLINK:          return A_BLINK;
    case CHATTR_UNDERLINE:      return A_UNDERLINE;
    case CHATTR_REVERSE:        return A_REVERSE;
    case CHATTR_DIM:            return A_DIM;
    default:                    return A_NORMAL;
    }
}

static inline short macro_colour(short col)
{
    return Options.colour[ col ];
}

// Translate DOS colors to curses.
static short translate_colour(short col)
{
    switch (col)
    {
    case BLACK:
        return COLOR_BLACK;
    case BLUE:
        return COLOR_BLUE;
    case GREEN:
        return COLOR_GREEN;
    case CYAN:
        return COLOR_CYAN;
    case RED:
        return COLOR_RED;
    case MAGENTA:
        return COLOR_MAGENTA;
    case BROWN:
        return COLOR_YELLOW;
    case LIGHTGREY:
        return COLOR_WHITE;
    case DARKGREY:
        return COLOR_BLACK + COLFLAG_CURSES_BRIGHTEN;
    case LIGHTBLUE:
        return COLOR_BLUE + COLFLAG_CURSES_BRIGHTEN;
    case LIGHTGREEN:
        return COLOR_GREEN + COLFLAG_CURSES_BRIGHTEN;
    case LIGHTCYAN:
        return COLOR_CYAN + COLFLAG_CURSES_BRIGHTEN;
    case LIGHTRED:
        return COLOR_RED + COLFLAG_CURSES_BRIGHTEN;
    case LIGHTMAGENTA:
        return COLOR_MAGENTA + COLFLAG_CURSES_BRIGHTEN;
    case YELLOW:
        return COLOR_YELLOW + COLFLAG_CURSES_BRIGHTEN;
    case WHITE:
        return COLOR_WHITE + COLFLAG_CURSES_BRIGHTEN;
    default:
        return COLOR_GREEN;
    }
}

static void setup_colour_pairs(void)
{
    short i, j;

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
        {
            if ((i > 0) || (j > 0))
                init_pair(i * 8 + j, j, i);
        }

    init_pair(63, COLOR_BLACK, Options.background_colour);
}

static void unix_handle_terminal_resize();

static void termio_init()
{
    tcgetattr(0, &def_term);
    memcpy(&game_term, &def_term, sizeof(struct termios));

    def_term.c_cc[VINTR] = (char) 3;        // ctrl-C
    game_term.c_cc[VINTR] = (char) 3;       // ctrl-C

    // Let's recover some control sequences
    game_term.c_cc[VSTART] = (char) -1;     // ctrl-Q
    game_term.c_cc[VSTOP] = (char) -1;      // ctrl-S
    game_term.c_cc[VSUSP] = (char) -1;      // ctrl-Y
#ifdef VDSUSP
    game_term.c_cc[VDSUSP] = (char) -1;     // ctrl-Y
#endif

    tcsetattr(0, TCSAFLUSH, &game_term);

    crawl_state.terminal_resize_handler = unix_handle_terminal_resize;
}

void set_mouse_enabled(bool enabled)
{
#ifdef NCURSES_MOUSE_VERSION
    const int mask = enabled ? ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION : 0;
    mmask_t oldmask = 0;
    mousemask(mask, &oldmask);
#endif
}

#ifdef NCURSES_MOUSE_VERSION
static int proc_mouse_event(int c, const MEVENT *me)
{
    crawl_view.mousep.x = me->x + 1;
    crawl_view.mousep.y = me->y + 1;

    if (!crawl_state.mouse_enabled)
        return CK_MOUSE_MOVE;

    c_mouse_event cme(crawl_view.mousep);
    if (me->bstate & BUTTON1_CLICKED)
        cme.bstate |= c_mouse_event::BUTTON1;
    else if (me->bstate & BUTTON1_DOUBLE_CLICKED)
        cme.bstate |= c_mouse_event::BUTTON1_DBL;
    else if (me->bstate & BUTTON2_CLICKED)
        cme.bstate |= c_mouse_event::BUTTON2;
    else if (me->bstate & BUTTON2_DOUBLE_CLICKED)
        cme.bstate |= c_mouse_event::BUTTON2_DBL;
    else if (me->bstate & BUTTON3_CLICKED)
        cme.bstate |= c_mouse_event::BUTTON3;
    else if (me->bstate & BUTTON3_DOUBLE_CLICKED)
        cme.bstate |= c_mouse_event::BUTTON3_DBL;
    else if (me->bstate & BUTTON4_CLICKED)
        cme.bstate |= c_mouse_event::BUTTON4;
    else if (me->bstate & BUTTON4_DOUBLE_CLICKED)
        cme.bstate |= c_mouse_event::BUTTON4_DBL;

    if (cme)
    {
        new_mouse_event(cme);
        return CK_MOUSE_CLICK;
    }

    return CK_MOUSE_MOVE;
}
#endif

static int pending = 0;

int getchk()
{
#ifdef WATCHDOG
    // If we have (or wait for) actual keyboard input, it's not an infinite
    // loop.
    watchdog();
#endif

    if (pending)
    {
        int c = pending;
        pending = 0;
        return c;
    }

    wint_t c;

#ifdef USE_TILE_WEB
    refresh();

    tiles.redraw();
    tiles.await_input(c, true);

    if (c != 0)
        return c;
#endif

    switch (get_wch(&c))
    {
    case ERR:
        // getch() returns -1 on EOF, convert that into an Escape. Evil hack,
        // but the alternative is to explicitly check for -1 everywhere where
        // we might otherwise spin in a tight keyboard input loop.
        return ESCAPE;
    case OK:
        // a normal (printable) key
        return c;
    }

    return -c;
}

int m_getch()
{
    int c;
    do
    {
        c = getchk();

#ifdef NCURSES_MOUSE_VERSION
        if (c == -KEY_MOUSE)
        {
            MEVENT me;
            getmouse(&me);
            c = proc_mouse_event(c, &me);
        }
#endif
    } while (
#ifdef KEY_RESIZE
             c == -KEY_RESIZE ||
#endif
             ((c == CK_MOUSE_MOVE || c == CK_MOUSE_CLICK)
                 && !crawl_state.mouse_enabled));

    return c;
}

int getch_ck()
{
    int c = m_getch();
    switch (c)
    {
    // [dshaligram] MacOS ncurses returns 127 for backspace.
    case 127:
    case -KEY_BACKSPACE: return CK_BKSP;
    case -KEY_DC:    return CK_DELETE;
    case -KEY_HOME:  return CK_HOME;
    case -KEY_PPAGE: return CK_PGUP;
    case -KEY_END:   return CK_END;
    case -KEY_NPAGE: return CK_PGDN;
    case -KEY_UP:    return CK_UP;
    case -KEY_DOWN:  return CK_DOWN;
    case -KEY_LEFT:  return CK_LEFT;
    case -KEY_RIGHT: return CK_RIGHT;
    default:         return c;
    }
}

#if defined(USE_UNIX_SIGNALS)

static void handle_sigwinch(int)
{
    crawl_state.last_winch = time(0);
    if (crawl_state.waiting_for_command)
        handle_terminal_resize();
    else
        crawl_state.terminal_resized = true;
}

#endif // USE_UNIX_SIGNALS

static void unix_handle_terminal_resize()
{
    console_shutdown();
    console_startup();
}

static void unixcurses_defkeys(void)
{
#ifdef NCURSES_VERSION
    // keypad 0-9 (only if the "application mode" was successfully initialised)
    define_key("\033Op", 1000);
    define_key("\033Oq", 1001);
    define_key("\033Or", 1002);
    define_key("\033Os", 1003);
    define_key("\033Ot", 1004);
    define_key("\033Ou", 1005);
    define_key("\033Ov", 1006);
    define_key("\033Ow", 1007);
    define_key("\033Ox", 1008);
    define_key("\033Oy", 1009);

    // non-arrow keypad keys (for macros)
    define_key("\033OM", 1010); // Enter
    define_key("\033OP", 1011); // NumLock
    define_key("\033OQ", 1012); // /
    define_key("\033OR", 1013); // *
    define_key("\033OS", 1014); // -
    define_key("\033Oj", 1015); // *
    define_key("\033Ok", 1016); // +
    define_key("\033Ol", 1017); // +
    define_key("\033Om", 1018); // .
    define_key("\033On", 1019); // .
    define_key("\033Oo", 1020); // -

    // variants.  Ugly curses won't allow us to return the same code...
    define_key("\033[1~", 1031); // Home
    define_key("\033[4~", 1034); // End
    define_key("\033[E",  1040); // center arrow
#endif
}

int unixcurses_get_vi_key(int keyin)
{
    switch (-keyin)
    {
    // -1001..-1009: passed without change
    case 1031: return -1007;
    case 1034: return -1001;
    case 1040: return -1005;

    case KEY_HOME:   return -1007;
    case KEY_END:    return -1001;
    case KEY_DOWN:   return -1002;
    case KEY_UP:     return -1008;
    case KEY_LEFT:   return -1004;
    case KEY_RIGHT:  return -1006;
    case KEY_NPAGE:  return -1003;
    case KEY_PPAGE:  return -1009;
    case KEY_A1:     return -1007;
    case KEY_A3:     return -1009;
    case KEY_B2:     return -1005;
    case KEY_C1:     return -1001;
    case KEY_C3:     return -1003;
    case KEY_SHOME:  return 'Y';
    case KEY_SEND:   return 'B';
    case KEY_SLEFT:  return 'H';
    case KEY_SRIGHT: return 'L';
    case KEY_BTAB:   return CK_SHIFT_TAB;
    case KEY_BACKSPACE:
        // If terminfo's entry for backspace (kbs) is ctrl-h, curses
        // generates KEY_BACKSPACE for the ctrl-h key.  Work around that by
        // converting back to CK_BKSP.
        // Note that this mangling occurs entirely on the machine Crawl runs
        // on (and even within crawl's process) rather than where the user's
        // terminal is, so this check is reliable.
        static char kbskey[] = "kbs"; // tigetstr wants a non-const pointer :(
        static const char * const kbs = tigetstr(kbskey);
        static const int bskey = (kbs && kbs != (const char *) -1
                                      && kbs == string("\010")) ? CK_BKSP
                                                                : KEY_BACKSPACE;
        return bskey;
    }
    return keyin;
}

// Certain terminals support vt100 keypad application mode only after some
// extra goading.
#define KPADAPP "\033[?1051l\033[?1052l\033[?1060l\033[?1061h"
#define KPADCUR "\033[?1051l\033[?1052l\033[?1060l\033[?1061l"

void console_startup(void)
{
    termio_init();

#ifdef CURSES_USE_KEYPAD
    // If hardening is enabled (default on recent distributions), glibc
    // declares write() with __attribute__((warn_unused_result)) which not
    // only spams when not relevant, but cannot even be selectively hushed
    // by (void) casts like all other such warnings.
    // "if ();" is an unsightly hack...
    if (write(1, KPADAPP, strlen(KPADAPP))) {};
#endif

#ifdef USE_UNIX_SIGNALS
    signal(SIGWINCH, handle_sigwinch);
#endif

    initscr();
    raw();
    noecho();

    nonl();
    intrflush(stdscr, FALSE);
#ifdef CURSES_USE_KEYPAD
    keypad(stdscr, TRUE);

#ifdef CURSES_SET_ESCDELAY
#ifdef NCURSES_REENTRANT
    set_escdelay(CURSES_SET_ESCDELAY);
#else
    ESCDELAY = CURSES_SET_ESCDELAY;
#endif
#endif
#endif

    meta(stdscr, TRUE);
    unixcurses_defkeys();
    start_color();
    setup_colour_pairs();

    scrollok(stdscr, FALSE);

    // Must call refresh() for ncurses to update COLS and LINES.
    refresh();
    crawl_view.init_geometry();

    set_mouse_enabled(false);

#ifdef USE_TILE_WEB
    tiles.resize();
#endif
}

void console_shutdown()
{
    // resetty();
    endwin();

    tcsetattr(0, TCSAFLUSH, &def_term);
#ifdef CURSES_USE_KEYPAD
    // "if ();" to avoid undisableable spurious warning.
    if (write(1, KPADCUR, strlen(KPADCUR))) {};
#endif

#ifdef USE_UNIX_SIGNALS
    signal(SIGWINCH, SIG_DFL);
#endif
}

void cprintf(const char *format, ...)
{
    char buffer[2048];          // One full screen if no control seq...

    va_list argp;

    va_start(argp, format);
    vsnprintf(buffer, sizeof(buffer), format, argp);
    va_end(argp);

    ucs_t c;
    char *bp = buffer;
    while (int s = utf8towc(&c, bp))
    {
        bp += s;
        putwch(c);
    }
}

void putwch(ucs_t chr)
{
    wchar_t c = chr;
    if (!c)
        c = ' ';
    // TODO: recognize unsupported characters and try to transliterate
    addnwstr(&c, 1);

#ifdef USE_TILE_WEB
    ucs_t buf[2];
    buf[0] = chr;
    buf[1] = 0;
    tiles.put_ucs_string(buf);
#endif
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
            put_colour_ch(cell->colour, cell->glyph);
            cell++;
        }
    }
    update_screen();
}

// These next four are front functions so that we can reduce
// the amount of curses special code that occurs outside this
// this file.  This is good, since there are some issues with
// name space collisions between curses macros and the standard
// C++ string class.  -- bwr
void update_screen(void)
{
    refresh();

#ifdef USE_TILE_WEB
    tiles.set_need_redraw();
#endif
}

void clear_to_end_of_line(void)
{
    textcolor(LIGHTGREY);
    textbackground(BLACK);
    clrtoeol();

#ifdef USE_TILE_WEB
    tiles.clear_to_end_of_line();
#endif
}

int get_number_of_lines(void)
{
    return LINES;
}

int get_number_of_cols(void)
{
    return COLS;
}

int num_to_lines(int num)
{
    return num;
}

void clrscr()
{
    textcolor(LIGHTGREY);
    textbackground(BLACK);
    clear();
#ifdef DGAMELAUNCH
    printf("%s", DGL_CLEAR_SCREEN);
    fflush(stdout);
#endif

#ifdef USE_TILE_WEB
    tiles.clrscr();
#endif
}

void set_cursor_enabled(bool enabled)
{
#ifdef USE_TILE_WEB
    if (cursor_is_enabled != enabled)
    {
        tiles.send_message("{\"msg\":\"text_cursor\",\"enabled\":%s}",
                           enabled ? "true" : "false");
    }
#endif
    curs_set(cursor_is_enabled = enabled);
}

bool is_cursor_enabled()
{
    return cursor_is_enabled;
}

bool is_smart_cursor_enabled()
{
    return false;
}

void enable_smart_cursor(bool dummy)
{
}

static inline unsigned get_brand(int col)
{
    return (col & COLFLAG_FRIENDLY_MONSTER) ? Options.friend_brand :
           (col & COLFLAG_NEUTRAL_MONSTER)  ? Options.neutral_brand :
           (col & COLFLAG_ITEM_HEAP)        ? Options.heap_brand :
           (col & COLFLAG_WILLSTAB)         ? Options.stab_brand :
           (col & COLFLAG_MAYSTAB)          ? Options.may_stab_brand :
           (col & COLFLAG_FEATURE_ITEM)     ? Options.feature_item_brand :
           (col & COLFLAG_TRAP_ITEM)        ? Options.trap_item_brand :
           (col & COLFLAG_REVERSE)          ? CHATTR_REVERSE
                                            : CHATTR_NORMAL;
}

static int curs_fg_attr(int col)
{
    short fg, bg;

    FG_COL = col & 0x00ff;
    fg = translate_colour(macro_colour(FG_COL));
    bg = translate_colour(BG_COL == BLACK ? Options.background_colour
                                           : BG_COL);

    // calculate which curses flags we need...
    unsigned int flags = 0;

    unsigned brand = get_brand(col);
    if (brand != CHATTR_NORMAL)
    {
        flags |= convert_to_curses_attr(brand);

        if ((brand & CHATTR_ATTRMASK) == CHATTR_HILITE)
        {
            bg = translate_colour(
                    macro_colour((brand & CHATTR_COLMASK) >> 8));

            if (fg == bg)
                fg = COLOR_BLACK;
        }

        // If we can't do a dark grey friend brand, then we'll
        // switch the colour to light grey.
        if (Options.no_dark_brand
                && fg == (COLOR_BLACK | COLFLAG_CURSES_BRIGHTEN)
                && bg == 0)
        {
            fg = COLOR_WHITE;
        }
    }

    // curses typically uses A_BOLD to give bright foreground colour,
    // but various termcaps may disagree
    if (fg & COLFLAG_CURSES_BRIGHTEN)
        flags |= A_BOLD;

    // curses typically uses A_BLINK to give bright background colour,
    // but various termcaps may disagree (in whole or in part)
    if (bg & COLFLAG_CURSES_BRIGHTEN)
        flags |= A_BLINK;

    // Strip out all the bits above the raw 3-bit colour definition
    fg &= 0x0007;
    bg &= 0x0007;

    // figure out which colour pair we want
    const int pair = (fg == 0 && bg == 0) ? 63 : (bg * 8 + fg);

    return COLOR_PAIR(pair) | flags;
}

void textcolor(int col)
{
    (void)attrset(Current_Colour = curs_fg_attr(col));

#ifdef USE_TILE_WEB
    tiles.textcolor(col);
#endif
}

static int curs_bg_attr(int col)
{
    short fg, bg;

    BG_COL = col & 0x00ff;
    fg = translate_colour(macro_colour(FG_COL));
    bg = translate_colour(BG_COL == BLACK ? Options.background_colour
                                           : BG_COL);

    unsigned int flags = 0;

    unsigned brand = get_brand(col);
    if (brand != CHATTR_NORMAL)
    {
        flags |= convert_to_curses_attr(brand);

        if ((brand & CHATTR_ATTRMASK) == CHATTR_HILITE)
        {
            bg = (brand & CHATTR_COLMASK) >> 8;
            if (fg == bg)
                fg = COLOR_BLACK;
        }

        // If we can't do a dark grey friend brand, then we'll
        // switch the colour to light grey.
        if (Options.no_dark_brand
                && fg == (COLOR_BLACK | COLFLAG_CURSES_BRIGHTEN)
                && bg == 0)
        {
            fg = COLOR_WHITE;
        }
    }

    // curses typically uses A_BOLD to give bright foreground colour,
    // but various termcaps may disagree
    if (fg & COLFLAG_CURSES_BRIGHTEN)
        flags |= A_BOLD;

    // curses typically uses A_BLINK to give bright background colour,
    // but various termcaps may disagree
    if (bg & COLFLAG_CURSES_BRIGHTEN)
        flags |= A_BLINK;

    // Strip out all the bits above the raw 3-bit colour definition
    fg &= 0x0007;
    bg &= 0x0007;

    // figure out which colour pair we want
    const int pair = (fg == 0 && bg == 0) ? 63 : (bg * 8 + fg);

    return COLOR_PAIR(pair) | flags;
}

void textbackground(int col)
{
    (void)attrset(Current_Colour = curs_bg_attr(col));

#ifdef USE_TILE_WEB
    tiles.textbackground(col);
#endif
}

void gotoxy_sys(int x, int y)
{
    move(y - 1, x - 1);
}

typedef cchar_t char_info;
static inline bool operator == (const cchar_t &a, const cchar_t &b)
{
    return a.attr == b.attr && *a.chars == *b.chars;
}

static inline char_info character_at(int y, int x)
{
    cchar_t c;
    // (void) is to hush an incorrect clang warning.
    (void)mvin_wch(y, x, &c);
    return c;
}

static inline bool valid_char(const cchar_t &c)
{
    return *c.chars;
}

static inline void write_char_at(int y, int x, const cchar_t &ch)
{
    move(y, x);
    add_wchnstr(&ch, 1);
}

static void flip_colour(cchar_t &ch)
{
    const unsigned colour = (ch.attr & A_COLOR);
    const int pair        = PAIR_NUMBER(colour);

    int fg     = pair & 7;
    int bg     = (pair >> 3) & 7;

    if (pair == 63)
    {
        fg    = COLOR_WHITE;
        bg    = COLOR_BLACK;
    }

    const int newpair = (fg * 8 + bg);
    ch.attr = COLOR_PAIR(newpair);
}

static char_info oldch, oldmangledch;
static int faked_x = -1, faked_y;

void fakecursorxy(int x, int y)
{
    if (valid_char(oldch) && faked_x != -1
        && character_at(faked_y, faked_x) == oldmangledch)
    {
        if (faked_x != x - 1 || faked_y != y - 1)
            write_char_at(faked_y, faked_x, oldch);
        else
            return;
    }

    char_info c = character_at(y - 1, x - 1);
    oldch   = c;
    faked_x = x - 1;
    faked_y = y - 1;
    flip_colour(c);
    write_char_at(y - 1, x - 1, oldmangledch = c);
    move(y - 1, x - 1);
}

int wherex()
{
    return getcurx(stdscr) + 1;
}

int wherey()
{
    return getcury(stdscr) + 1;
}

void delay(unsigned int time)
{
    if (crawl_state.disables[DIS_DELAY])
        return;

#ifdef USE_TILE_WEB
    tiles.redraw();
    tiles.send_message("{\"msg\":\"delay\",\"t\":%d}", time);
    tiles.flush_messages();
#endif

    refresh();
    if (time)
        usleep(time * 1000);
}

/* This is Juho Snellman's modified kbhit, to work with macros */
bool kbhit()
{
    if (pending)
        return true;

    wint_t c;
#ifndef USE_TILE_WEB
    int i;

    nodelay(stdscr, TRUE);
    timeout(0);  // apparently some need this to guarantee non-blocking -- bwr
    i = get_wch(&c);
    nodelay(stdscr, FALSE);

    switch (i)
    {
    case OK:
        pending = c;
        return true;
    case KEY_CODE_YES:
        pending = -c;
        return true;
    default:
        return false;
    }
#else
    bool result = tiles.await_input(c, false);

    if (result && (c != 0))
        pending = c;

    return result;
#endif
}
