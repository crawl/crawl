/*
 *  File:       libunix.cc
 *  Summary:    Functions for unix and curses support
 *  Written by: ?
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

/* Some replacement routines missing in gcc
   Some of these are inspired by/stolen from the Linux-conio package
   by Mental EXPlotion. Hope you guys don't mind.
   The colour exchange system is perhaps a little overkill, but I wanted
   it to be general to support future changes.. The only thing not
   supported properly is black on black (used by curses for "normal" mode)
   and white on white (used by me for "bright black" (darkgrey) on black

   Jan 1998 Svante Gerhard <svante@algonet.se>                          */

#include "AppHdr.h"
REVISION("$Rev$");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#define _LIBUNIX_IMPLEMENTATION
#include "libunix.h"
#include "defines.h"

#include "cio.h"
#include "enum.h"
#include "externs.h"
#include "files.h"
#include "initfile.h"
#include "state.h"
#include "stuff.h"
#include "view.h"

#ifdef DGL_ENABLE_CORE_DUMP
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifdef UNICODE_GLYPHS
#include <wchar.h>
#include <locale.h>
#include <langinfo.h>
#endif

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
    #include <curses.h>
#else
    #include CURSES_INCLUDE_FILE
#endif

// Globals holding current text/backg. colors
static short FG_COL = WHITE;
static short BG_COL = BLACK;
static int   Current_Colour = COLOR_PAIR(BG_COL * 8 + FG_COL);

static int curs_fg_attr(int col);
static int curs_bg_attr(int col);
static int waddstr_with_altcharset(WINDOW* w, const char* s);

static bool cursor_is_enabled = true;

static unsigned int convert_to_curses_attr( int chattr )
{
    switch (chattr & CHATTR_ATTRMASK)
    {
    case CHATTR_STANDOUT:       return (A_STANDOUT);
    case CHATTR_BOLD:           return (A_BOLD);
    case CHATTR_BLINK:          return (A_BLINK);
    case CHATTR_UNDERLINE:      return (A_UNDERLINE);
    case CHATTR_REVERSE:        return (A_REVERSE);
    case CHATTR_DIM:            return (A_DIM);
    default:                    return (A_NORMAL);
    }
}

static inline short macro_colour( short col )
{
    return (Options.colour[ col ]);
}

// Translate DOS colors to curses.
static short translate_colour( short col )
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

static void setup_colour_pairs( void )
{
    short i, j;

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            if (( i > 0 ) || ( j > 0 ))
                init_pair(i * 8 + j, j, i);
        }
    }

    init_pair(63, COLOR_BLACK, Options.background);
}          // end setup_colour_pairs()

#ifdef UNICODE_GLYPHS
static std::string unix_glyph2string(unsigned gly)
{
    char buf[50];  // Overkill, I know.
    wchar_t wcbuf[2];
    wcbuf[0] = gly;
    wcbuf[1] = 0;
    if (wcstombs(buf, wcbuf, sizeof buf) != (size_t) -1)
        return (buf);

    return std::string(1, gly);
}

static int unix_multibyte_strlen(const std::string &s)
{
    const char *cs = s.c_str();
    size_t len = mbsrtowcs(NULL, &cs, 0, NULL);
    return (len == (size_t) -1? s.length() : len);
}
#endif

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

    crawl_state.unicode_ok = false;
#ifdef UNICODE_GLYPHS
    if (setlocale(LC_ALL, UNICODE_LOCALE)
        && !strcmp(nl_langinfo(CODESET), "UTF-8"))
    {
        crawl_state.unicode_ok       = true;
        crawl_state.glyph2strfn      = unix_glyph2string;
        crawl_state.multibyte_strlen = unix_multibyte_strlen;
    }
#endif

    crawl_state.terminal_resize_handler = unix_handle_terminal_resize;
}

void set_mouse_enabled(bool enabled)
{
#ifdef NCURSES_MOUSE_VERSION
    const int mask = enabled? ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION : 0;
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
        return (CK_MOUSE_MOVE);

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
        return (CK_MOUSE_CLICK);
    }

    return (CK_MOUSE_MOVE);
}
#endif

static int raw_m_getch()
{
    const int c = getch();
    switch (c)
    {
#ifdef NCURSES_MOUSE_VERSION
    case KEY_MOUSE:
    {
        MEVENT me;
        getmouse(&me);
        return (proc_mouse_event(c, &me));
    }
#endif
    default:
        // getch() returns -1 on EOF, convert that into an Escape. Evil hack,
        // but the alternative is to explicitly check for -1 everywhere where
        // we might otherwise spin in a tight keyboard input loop.
        return (c == -1? ESCAPE : c);
    }
}

int m_getch()
{
    int c;
    do
        c = raw_m_getch();
    while ((c == CK_MOUSE_MOVE || c == CK_MOUSE_CLICK)
           && !crawl_state.mouse_enabled);

    return (c);
}

int getch_ck()
{
    int c = m_getch();
    switch (c)
    {
    // [dshaligram] MacOS ncurses returns 127 for backspace.
    case 127:
    case KEY_BACKSPACE: return CK_BKSP;
    case KEY_DC:    return CK_DELETE;
    case KEY_HOME:  return CK_HOME;
    case KEY_PPAGE: return CK_PGUP;
    case KEY_END:   return CK_END;
    case KEY_NPAGE: return CK_PGDN;
    case KEY_UP:    return CK_UP;
    case KEY_DOWN:  return CK_DOWN;
    case KEY_LEFT:  return CK_LEFT;
    case KEY_RIGHT: return CK_RIGHT;
    default:        return c;
    }
}

#if defined(USE_UNIX_SIGNALS)

static void handle_sigwinch(int)
{
    if (crawl_state.waiting_for_command)
        handle_terminal_resize();
}

#ifdef SIGHUP_SAVE
/* [ds] This SIGHUP handling is primitive and far from safe, but it
 * should be better than nothing. Feel free to get rigorous on this.
 */
static void handle_hangup(int)
{
    if (crawl_state.seen_hups++)
        return;

    if (crawl_state.saving_game || crawl_state.updating_scores)
        return;

    crawl_state.saving_game = true;
    if (crawl_state.need_save)
    {
        // Clean up all the hooks.
        for (unsigned i = 0; i < crawl_state.exit_hooks.size(); ++i)
            crawl_state.exit_hooks[i]->restore_state();

        crawl_state.exit_hooks.clear();

        // save_game(true) also exits, saving us the trouble of doing so.
        save_game(true);
    }
    else
    {
        // CROAK! No attempt to clean up curses. This is probably not an
        // issue since the term has gone away.
        exit(1);
    }
}
#endif // SIGHUP_SAVE

#endif // USE_UNIX_SIGNALS

static WINDOW *Message_Window;
static void setup_message_window()
{
    Message_Window = newwin( crawl_view.msgsz.y, crawl_view.msgsz.x,
                             crawl_view.msgp.y - 1, crawl_view.msgp.x - 1 );
    if (!Message_Window)
    {
        if (crawl_state.need_save)
            save_game(true);

        // Never reaches here unless the game didn't need saving.
        end(1, false, "Unable to create message window!");
    }

    scrollok(Message_Window, true);
    idlok(Message_Window, true);
}

static void unix_handle_terminal_resize()
{
    unixcurses_shutdown();
    unixcurses_startup();
}

void clear_message_window()
{
    wattrset( Message_Window, curs_fg_attr(LIGHTGREY) );
    werase( Message_Window );
    wrefresh( Message_Window );
}

void message_out(int which_line, int color, const char *s, int firstcol,
                 bool newline)
{
    wattrset( Message_Window, curs_fg_attr(color) );

    if (!firstcol)
        firstcol = Options.delay_message_clear? 1 : 0;
    else
        firstcol--;

    wmove(Message_Window, which_line, firstcol);
    waddstr_with_altcharset(Message_Window, s);

    if (newline && which_line == crawl_view.msgsz.y - 1)
    {
        int x, y;
        getyx(Message_Window, y, x);
        scroll(Message_Window);
        wmove(Message_Window, y - 1, x);
    }

    // Fix stdscr cursor to same place as Message_Window cursor. This
    // is necessary because when reading input we use stdscr.
    {
        int x, y;
        getyx(Message_Window, y, x);
        move(y + crawl_view.msgp.y - 1, crawl_view.msgp.x - 1 + x);
    }

    wrefresh(Message_Window);
}

static void unixcurses_defkeys( void )
{
#ifdef NCURSES_VERSION
    // keypad 0-9 (only if the "application mode" was successfully initialized)
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
    switch(keyin)
    {
    // 1001..1009: passed without change
    case 1031: return 1007;
    case 1034: return 1001;
    case 1040: return 1005;

    case KEY_HOME:	return 1007;
    case KEY_END:	return 1001;
    case KEY_DOWN:	return 1002;
    case KEY_UP:	return 1008;
    case KEY_LEFT:	return 1004;
    case KEY_RIGHT:	return 1006;
    case KEY_NPAGE:	return 1003;
    case KEY_PPAGE:	return 1009;
    case KEY_A1:	return 1007;
    case KEY_A3:	return 1009;
    case KEY_B2:	return 1005;
    case KEY_C1:	return 1001;
    case KEY_C3:	return 1003;
    case KEY_SHOME:	return 'Y';
    case KEY_SEND:	return 'B';
    case KEY_SLEFT:	return 'H';
    case KEY_SRIGHT:	return 'L';
    }
    return keyin;
}

// Certain terminals support vt100 keypad application mode only after some
// extra goading.
#define KPADAPP "\033[?1051l\033[?1052l\033[?1060l\033[?1061h"
#define KPADCUR "\033[?1051l\033[?1052l\033[?1060l\033[?1061l"

void unixcurses_startup( void )
{
    termio_init();

#ifdef CURSES_USE_KEYPAD
    write(1, KPADAPP, strlen(KPADAPP));
#endif

#ifdef DGAMELAUNCH
    // Force timezone to UTC.
    setenv("TZ", "", 1);
    tzset();
#endif

#ifdef USE_UNIX_SIGNALS
#ifdef SIGQUIT
    signal(SIGQUIT, SIG_IGN);
#endif

#ifdef SIGINT
    signal(SIGINT, SIG_IGN);
#endif

#ifdef SIGHUP_SAVE
    signal(SIGHUP, handle_hangup);
#endif

    signal(SIGWINCH, handle_sigwinch);

#endif

#ifdef DGL_ENABLE_CORE_DUMP
    rlimit lim;
    if (!getrlimit(RLIMIT_CORE, &lim)) {
        lim.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &lim);
    }
#endif

    initscr();
    raw();
    noecho();

    nonl();
    intrflush(stdscr, FALSE);
#ifdef CURSES_USE_KEYPAD
    keypad(stdscr, TRUE);

#if CURSES_SET_ESCDELAY
    ESCDELAY = CURSES_SET_ESCDELAY;
#endif
#endif

    meta(stdscr, TRUE);
    unixcurses_defkeys();
    start_color();
    setup_colour_pairs();

    scrollok(stdscr, TRUE);

    crawl_view.init_geometry();
    setup_message_window();

    set_mouse_enabled(false);
}

void unixcurses_shutdown()
{
    if (Message_Window)
    {
        delwin(Message_Window);
        Message_Window = NULL;
    }

    // resetty();
    endwin();

    tcsetattr(0, TCSAFLUSH, &def_term);
#ifdef CURSES_USE_KEYPAD
    write(1, KPADCUR, strlen(KPADCUR));
#endif

#ifdef USE_UNIX_SIGNALS
#ifdef SIGQUIT
    signal(SIGQUIT, SIG_DFL);
#endif

#ifdef SIGINT
    signal(SIGINT, SIG_DFL);
#endif

#ifdef SIGHUP_SAVE
    signal(SIGHUP, SIG_DFL);
#endif

    signal(SIGWINCH, SIG_DFL);
#endif
}


/* Convert value to string */
int itoa(int value, char *strptr, int radix)
{
    unsigned int bitmask = 32768;
    int ctr = 0;
    int startflag = 0;

    if (radix == 10)
    {
        sprintf(strptr, "%i", value);
    }
    if (radix == 2)             /* int to "binary string" */
    {
        while (bitmask)
        {
            if (value & bitmask)
            {
                startflag = 1;
                sprintf(strptr + ctr, "1");
            }
            else
            {
                if (startflag)
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
    return (OK);                /* Me? Fail? Nah. */
}

int cprintf(const char *format,...)
{
    int i;
    char buffer[2048];          // One full screen if no control seq...

    va_list argp;

    va_start(argp, format);
    vsprintf(buffer, format, argp);
    va_end(argp);
    i = waddstr_with_altcharset(stdscr, buffer);
    refresh();
    return (i);
}

// Wrapper around curses waddstr(); handles switching to the alt charset
// when necessary, and performing the funny DEC translation.
// Returns a curses success value.
static int waddstr_with_altcharset(WINDOW* w, const char* str)
{
    int ret = OK;
    if (Options.char_set == CSET_ASCII || Options.char_set == CSET_UNICODE)
    {
        // In ascii, we don't expect any high-bit chars.
        // In Unicode, str is UTF-8 and we shouldn't touch anything.
        ret = waddstr(w, str);
    }
    else
    {
        // Otherwise, high bit indicates alternate charset.
        // DEC line-drawing chars don't have the high bit, so
        // we map them into 0xE0 and above.
        const bool bDEC = (Options.char_set == CSET_DEC);
        const char* begin = str;

        while (true)
        {
            const char* end = begin;
            // Output a range of normal characters (the common case)
            while (*end && (unsigned char)*end <= 127) ++end;
            if (end-begin) ret = waddnstr(w, begin, end-begin);

            // Then a single high-bit character
            chtype c = (unsigned char)*end;
            if (c == 0)
                break;
            else if (bDEC && c >= 0xE0)
                ret = waddch(w, (c & 0x7F) | A_ALTCHARSET);
            else // if (*end > 127)
                ret = waddch(w, c | A_ALTCHARSET);
            begin = end+1;
        }
    }
    return ret;
}

int putch(unsigned char chr)
{
    if (chr == 0)
        chr = ' ';

    return (addch(chr));
}

int putwch(unsigned chr)
{
#ifdef UNICODE_GLYPHS
    if (chr <= 127 || Options.char_set != CSET_UNICODE)
        return (putch(chr));

    wchar_t c = chr;
    return (addnwstr(&c, 1));
#else
    return (putch(static_cast<unsigned char>(chr)));
#endif
}

char getche()
{
    char chr;

    chr = getch();
    addch(chr);
    refresh();
    return (chr);
}


int window(int x1, int y1, int x2, int y2)
{
    x1 = y1 = x2 = y2 = 0;      /* Do something to them.. makes gcc happy :) */
    return (refresh());
}

void put_colour_ch(int colour, unsigned ch)
{
    if (Options.char_set == CSET_ASCII || Options.char_set == CSET_UNICODE)
    {
        // Unicode can't or-in curses attributes; but it also doesn't have
        // to fool around with altcharset
        textattr(colour);
        putwch(ch);
    }
    else
    {
        const unsigned oldch = ch;
        if (oldch & 0x80) ch |= A_ALTCHARSET;
        // Shift the DEC line drawing set
        if (oldch >= 0xE0 && Options.char_set == CSET_DEC)
            ch ^= 0x80;
        textattr(colour);

        // putch truncates ch to a byte, so don't use it.
        if ((ch & 0x7f) == 0) ch |= ' ';
        addch(ch);
    }
}

void puttext(int x1, int y1, int x2, int y2, const screen_buffer_t *buf)
{
    const bool will_scroll = (x2 == get_number_of_cols());

    if (will_scroll)
        scrollok(stdscr, FALSE);

    for (int y = y1; y <= y2; ++y)
    {
        cgotoxy(x1, y);
        for (int x = x1; x <= x2; ++x)
        {
            put_colour_ch( buf[1], buf[0] );
            buf += 2;
        }
    }
    update_screen();

    if (will_scroll)
        scrollok(stdscr, TRUE);
}

// These next four are front functions so that we can reduce
// the amount of curses special code that occurs outside this
// this file.  This is good, since there are some issues with
// name space collisions between curses macros and the standard
// C++ string class.  -- bwr
void update_screen(void)
{
    refresh();
}

void clear_to_end_of_line(void)
{
    textcolor( LIGHTGREY );
    textbackground( BLACK );
    clrtoeol();
}

void clear_to_end_of_screen(void)
{
    textcolor( LIGHTGREY );
    textbackground( BLACK );
    clrtobot();
}

int get_number_of_lines(void)
{
    return (LINES);
}

int get_number_of_cols(void)
{
    return (COLS);
}

void get_input_line_from_curses( char *const buff, int len )
{
    echo();
    wgetnstr( stdscr, buff, len );
    noecho();
}

int clrscr()
{
    int retval;

    textcolor( LIGHTGREY );
    textbackground( BLACK );
    retval = clear();
#ifndef DGAMELAUNCH
    refresh();
#else
    printf("%s", DGL_CLEAR_SCREEN);
    fflush(stdout);
#endif
    return (retval);
}

void set_cursor_enabled(bool enabled)
{
    curs_set(cursor_is_enabled = enabled);
}

bool is_cursor_enabled()
{
    return (cursor_is_enabled);
}

inline unsigned get_brand(int col)
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

void textattr(int col)
{
    textcolor(col);
}

static int curs_fg_attr(int col)
{
    short fg, bg;

    FG_COL = col & 0x00ff;
    fg = translate_colour( macro_colour( FG_COL ) );
    bg = translate_colour( (BG_COL == BLACK) ? Options.background : BG_COL );

    // calculate which curses flags we need...
    unsigned int flags = 0;

#ifdef USE_COLOUR_OPTS
    unsigned brand = get_brand(col);
    if (brand != CHATTR_NORMAL)
    {
        flags |= convert_to_curses_attr( brand );

        if ((brand & CHATTR_ATTRMASK) == CHATTR_HILITE)
        {
            bg = translate_colour(
                    macro_colour( (brand & CHATTR_COLMASK) >> 8 ));

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
#endif

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

    return ( COLOR_PAIR(pair) | flags );
}

void textcolor(int col)
{
    attrset( Current_Colour = curs_fg_attr(col) );
}

static int curs_bg_attr(int col)
{
    short fg, bg;

    BG_COL = col & 0x00ff;
    fg = translate_colour( macro_colour( FG_COL ) );
    bg = translate_colour( (BG_COL == BLACK) ? Options.background : BG_COL );

    unsigned int flags = 0;

#ifdef USE_COLOUR_OPTS
    unsigned brand = get_brand(col);
    if (brand != CHATTR_NORMAL)
    {
        flags |= convert_to_curses_attr( brand );

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
#endif

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

    return ( COLOR_PAIR(pair) | flags );
}

void textbackground(int col)
{
    attrset( Current_Colour = curs_bg_attr(col) );
}


int gotoxy_sys(int x, int y)
{
    return (move(y - 1, x - 1));
}

#ifdef UNICODE_GLYPHS
typedef cchar_t char_info;
inline bool operator == (const cchar_t &a, const cchar_t &b)
{
    return (a.attr == b.attr && *a.chars == *b.chars);
}
inline char_info character_at(int y, int x)
{
    cchar_t c;
    mvin_wch(y, x, &c);
    return (c);
}
inline bool valid_char(const cchar_t &c)
{
    return *c.chars;
}
inline void write_char_at(int y, int x, const cchar_t &ch)
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
    ch.attr = COLOR_PAIR(newpair) | (ch.attr & A_ALTCHARSET);
}
#else // ! UNICODE_GLYPHS
typedef unsigned long char_info;
#define character_at(y,x)    mvinch(y,x)
#define valid_char(x) (x)
#define write_char_at(y,x,c) mvaddch(y, x, c)

#define char_info_character(c) ((c) & A_CHARTEXT)
#define char_info_colour(c)    ((c) & A_COLOR)
#define char_info_attributes(c) ((c) & A_ATTRIBUTES)

static void flip_colour(char_info &ch)
{
    const unsigned colour = char_info_colour(ch);
    const int pair        = PAIR_NUMBER(colour);

    int fg     = pair & 7;
    int bg     = (pair >> 3) & 7;

    if (pair == 63)
    {
        fg    = COLOR_WHITE;
        bg    = COLOR_BLACK;
    }

    const int newpair = (fg * 8 + bg);
    ch = (char_info_character(ch) | COLOR_PAIR(newpair) |
          (char_info_attributes(ch) & A_ALTCHARSET));
}
#endif

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
    write_char_at( y - 1, x - 1, oldmangledch = c);
    move(y - 1, x - 1);
}

int wherex()
{
    int x, y;

    getyx(stdscr, y, x);
    return (x + 1);
}


int wherey()
{
    int x, y;

    getyx(stdscr, y, x);
    return (y + 1);
}


int stricmp( const char *str1, const char *str2 )
{
    return (strcmp(str1, str2));
}


void delay( unsigned long time )
{
    usleep( time * 1000 );
}


/*
   Note: kbhit now in macro.cc
 */

/* This is Juho Snellman's modified kbhit, to work with macros */
int kbhit()
{
    int i;

    nodelay(stdscr, TRUE);
    timeout(0);  // apparently some need this to guarantee non-blocking -- bwr
    i = wgetch(stdscr);
    nodelay(stdscr, FALSE);

    if (i == -1)
        i = 0;
    else
        ungetch(i);

    return (i);
}

extern "C" {
    // Convert string to lowercase.
    char *strlwr(char *str)
    {
        unsigned int i;

        for (i = 0; i < strlen(str); i++)
            str[i] = tolower(str[i]);

        return (str);
    }
}


