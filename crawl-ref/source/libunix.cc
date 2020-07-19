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

#define _LIBUNIX_IMPLEMENTATION
#include "libunix.h"

#include <cassert>
#include <cctype>
#include <clocale>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <langinfo.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>

#include "colour.h"
#include "cio.h"
#include "crash.h"
#include "state.h"
#include "tiles-build-specific.h"
#include "unicode.h"
#include "view.h"
#include "ui.h"

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

// Globals holding current text/backg. colours
// Note that these are internal colours, *not* curses colors.
/** @brief The current foreground @em colour. */
static COLOURS FG_COL = LIGHTGREY;

/** @brief The default foreground @em colour. */
static COLOURS FG_COL_DEFAULT = LIGHTGREY;

/** @brief The current background @em colour. */
static COLOURS BG_COL = BLACK;

/** @brief The default background @em colour. */
static COLOURS BG_COL_DEFAULT = BLACK;

struct curses_style
{
    attr_t attr;
    short color_pair;
};

/**
 * @brief Get curses attributes for the current internal color combination.
 *
 * @param fg
 *  The internal colour for the foreground.
 * @param bg
 *  The internal colour for the background.
 * @param adjust_background
 *  Determines which color in the pair is adjusted if adjustment is necessary.
 *  True chooses the background, while false chooses the foreground.
 *
 *  @return
 *   Returns the character attributes and colour pair index.
 */
static curses_style curs_attr(COLOURS fg, COLOURS bg, bool adjust_background = false);

/**
 * @brief Change the foreground colour and returns the resulting curses info.
 *
 * @param col
 *  An internal color with attached brand.
 *
 *  @return
 *   Returns the character attributes and colour pair index.
 */
static curses_style curs_attr_bg(int col);

/**
 * @brief Change the background colour and returns the resulting curses info.
 *
 * @param col
 *  An internal color with attached brand.
 *
 *  @return
 *   Returns the character attributes and colour pair index.
 */
static curses_style curs_attr_fg(int col);

/**
 * @brief Get curses attributes for the passed internal color combination.
 *
 * Performs colour mapping for both default colours as well as the color map.
 * Additionally, this function takes into consideration a passed brand.
 *
 * @param fg
 *  The internal colour for the foreground.
 * @param bg
 *  The internal colour for the background.
 * @param brand
 *  Internal color branding information.
 *
 *  @return
 *   Returns the character attributes and colour pair index.
 */
static curses_style curs_attr_mapped(COLOURS fg, COLOURS bg, int brand);

/**
 * @brief Returns a curses color pair index for the passed fg/bg combo.
 *
 * Strips brightening flags from the passed colors according to the game
 * options. The current FG_COL_DEFAULT and BG_COL_DEFAULT values are used for
 * determining the default pair.
 *
 * @param fg
 *  The flagged curses color index for the foreground.
 * @param bg
 *  The flagged curses color index for the background.
 *
 * @return
 *  Returns the pair index associated with the combination, or the default
 *  pair, 0, if there is no associated pair.
 */
static short curs_calc_pair_safe(short fg, short bg);

/**
 * @brief Returns a curses color pair index for the passed fg/bg combo.
 *
 * Unlike curs_calc_pair_safe(short, short), the passed colors are treated
 * as-is and default colors are passed explicitly.
 *
 * @param fg
 *  The curses color index for the foreground.
 * @param bg
 *  The curses color index for the background.
 * @param fg_default
 *  The explicit curses color to consider the default foreground.
 * @param bg_default
 *  The explicit curses color to consider the default background.
 *
 * @return
 *  Returns the pair index associated with the combination, or the default
 *  pair, 0, if there is no associated pair.
 */
static short curs_calc_pair_safe(short fg, short bg, short fg_default,
    short bg_default);

/**
 * @brief Can extended colors be used directly instead of character attributes?
 *
 * @return
 *  True if extended colors can be used in lieu of attributes, false otherwise.
 */
static bool curs_can_use_extended_colors();

/**
 * @brief Is there a curses pair for the passed color combination?
 *
 * @param fg
 *  The raw curses foreground color.
 * @param bg
 *  The raw curses background color.
 *
 * @return
 *  True if there is a valid curses color pair for the combo, false otherwise.
 */
static bool curs_color_combo_has_pair(short fg, short bg);

/**
 * @brief Adjust a curses color pair to not be visually identical.
 *
 * Colors which are not equal in value may still be considered identical
 * in appearance depending on the current rendering assumption options.
 *
 * If the pair is not considered visually identical, no change is performed.
 *
 * @param fg
 *  The pair's brightness-flagged foreground color.
 * @param bg
 *  The pair's brightness-flagged background color.
 * @param adjust_background
 *  Determines which color in the pair is adjusted if adjustment is necessary.
 *  True chooses the background, while false chooses the foreground.
 */
static void curs_adjust_color_pair_to_non_identical(short &fg, short &bg,
    bool adjust_background = false);

/**
 * @brief Get the @em maximum palette size currently supported by the program.
 *
 * This value is limited by both the internal color palette and reported
 * terminal color capabilities.
 *
 * This value is @em not affected by the rendering assumption options.
 *
 * @return The maximum palette size currently supported by the program.
 */
static short curs_palette_size();

/**
 * @brief Sets the default colors for the terminal.
 *
 * Attempt to set the default colors for the terminal based on the current
 * game options. If a default color cannot be set, the curses default will be
 * used (COLOR_WHITE for foreground, COLOR_BLACK for background).
 */
static void curs_set_default_colors();

/**
 * @brief Translates a flagged curses color to an internal COLOUR.
 *
 * @param col
 *  The flagged curses color to convert.
 *
 * @return
 *  The equivalent internal colour.
 */
static COLOURS curses_color_to_internal_colour(short col);

/**
 * @brief Flip the foreground and background color of the @p ch.
 *
 * @see flip_colour(short &, attr_t &, short, attr_t)
 *
 * @param ch
 *  The curses character whose colors will be flipped.
 */
static void flip_colour(cchar_t &ch);

/**
 * @brief Flip the foreground and background color of the @p ch.
 *
 * This flip respects the current rendering assumptions. Therefore, the
 * resulting color combination may not be a strict color swap, but one that
 * is guaranteed to be visible.
 *
 * @param attr
 *  The character attributes to flip.
 * @param color
 *  The color pair to flip.
 *
 *  @return
 *   Returns the flipped character attributes and colour pair index.
 */
static curses_style flip_colour(curses_style style);

/**
 * @brief Translate internal colours to flagged curses colors.
 *
 * @param col
 *  The internal colour to translate.
 *
 * @return
 *  The equivalent flagged curses colour.
 */
static short translate_colour(COLOURS col);

/**
 * @brief Write a complex curses character to specified screen location.
 *
 * There are two additional effects of this function:
 *  - The screen's character attributes are set to those contained in @p ch.
 *  - The cursor is moved to the passed coordinate.
 *
 * @param y
 *  The y-component of the location to draw @p ch.
 * @param x
 *  The x-component of the location to draw @p ch.
 * @param ch
 *  The complex character to render.
 */
static void write_char_at(int y, int x, const cchar_t &ch);

static bool cursor_is_enabled = true;

static unsigned int convert_to_curses_style(int chattr)
{
    switch (chattr & CHATTR_ATTRMASK)
    {
    case CHATTR_STANDOUT:       return WA_STANDOUT;
    case CHATTR_BOLD:           return WA_BOLD;
    case CHATTR_BLINK:          return WA_BLINK;
    case CHATTR_UNDERLINE:      return WA_UNDERLINE;
    case CHATTR_DIM:            return WA_DIM;
    default:                    return WA_NORMAL;
    }
}

// see declaration
static short translate_colour(COLOURS col)
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
        return COLOR_BLACK | COLFLAG_CURSES_BRIGHTEN;
    case LIGHTBLUE:
        return COLOR_BLUE | COLFLAG_CURSES_BRIGHTEN;
    case LIGHTGREEN:
        return COLOR_GREEN | COLFLAG_CURSES_BRIGHTEN;
    case LIGHTCYAN:
        return COLOR_CYAN | COLFLAG_CURSES_BRIGHTEN;
    case LIGHTRED:
        return COLOR_RED | COLFLAG_CURSES_BRIGHTEN;
    case LIGHTMAGENTA:
        return COLOR_MAGENTA | COLFLAG_CURSES_BRIGHTEN;
    case YELLOW:
        return COLOR_YELLOW | COLFLAG_CURSES_BRIGHTEN;
    case WHITE:
        return COLOR_WHITE | COLFLAG_CURSES_BRIGHTEN;
    default:
        return COLOR_GREEN;
    }
}

/**
 * @internal
 * Attempt to exhaustively allocate color pairs, relying on
 * curs_calc_pair_safe() to order pairs.
 *
 * 256-color considerations:
 *  - For ncurses stable, need ncurses 6.0 or later for >256 pairs.
 *  - Pairs use a short type.
 *    We will likely run out of pairs in a 256-color terminal.
 *
 *    For short = int16, highest pair = 2^15 - 1:
 *    Assuming an implicit default pair:
 *      (bg_max * num_colors) - 1 = 2^15 - 1; num_colors = 2^8
 *       bg_max = 2^15/2^8 = 2^7 = 128
 *    So, 128 maximum background colors for 256 foreground colors.
 *
 *    Alternatively, limit to a 181-color palette; sqrt(2^15) = ~181.02
 *
 * @todo
 *  If having access to all possible 256-color pairs (non-simultaneously) is
 *  important for the project, implement paging for color-pairs.
 */
static void setup_colour_pairs()
{
    // Only generate pairs which we may need.
    short num_colors = curs_palette_size();

    for (short i = 0; i < num_colors; i++)
    {
        for (short j = 0; j < num_colors; j++)
        {
            short pair = curs_calc_pair_safe(j, i, COLOR_WHITE, COLOR_BLACK);
            if (pair > 0)
                init_pair(pair, j, i);
        }
    }
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
    UNUSED(c);

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

static int _get_key_from_curses()
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

#if defined(KEY_RESIZE) || defined(USE_UNIX_SIGNALS)
static void unix_handle_resize_event()
{
    crawl_state.last_winch = time(0);
    if (crawl_state.waiting_for_command)
        handle_terminal_resize();
    else
        crawl_state.terminal_resized = true;
}
#endif

static bool getch_returns_resizes;
void set_getch_returns_resizes(bool rr)
{
    getch_returns_resizes = rr;
}

int getch_ck()
{
    while (true)
    {
        int c = _get_key_from_curses();

#ifdef NCURSES_MOUSE_VERSION
        if (c == -KEY_MOUSE)
        {
            MEVENT me;
            getmouse(&me);
            c = proc_mouse_event(c, &me);

            if (!crawl_state.mouse_enabled)
                continue;
        }
#endif

#ifdef KEY_RESIZE
        if (c == -KEY_RESIZE)
        {
            unix_handle_resize_event();

            // XXX: Before ncurses get_wch() returns KEY_RESIZE, it
            // updates LINES and COLS to the new window size. The resize
            // handler will only redraw the whole screen if the main view
            // is being shown, for slightly insane reasons, which results
            // in crawl_view.termsz being out of sync.
            //
            // This causes crashiness: e.g. in a menu, make the window taller,
            // then scroll down one line. To fix this, we always sync termsz:
            crawl_view.init_geometry();

            if (!getch_returns_resizes)
                continue;
        }
#endif

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
#ifdef KEY_RESIZE
        case -KEY_RESIZE: return CK_RESIZE;
#endif
        case -KEY_BTAB: return CK_SHIFT_TAB;
        default:         return c;
        }
    }
}

static void unix_handle_terminal_resize()
{
    console_shutdown();
    console_startup();
}

static void unixcurses_defkeys()
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

    // variants. Ugly curses won't allow us to return the same code...
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
        // generates KEY_BACKSPACE for the ctrl-h key. Work around that by
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

void console_startup()
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
# ifndef KEY_RESIZE
    signal(SIGWINCH, unix_handle_resize_event);
# endif
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
    // Since it may swap pairs, set default colors *after* setting up all pairs.
    curs_set_default_colors();

    scrollok(stdscr, FALSE);

    // Must call refresh() for ncurses to update COLS and LINES.
    refresh();
    crawl_view.init_geometry();

    set_mouse_enabled(false);

    // TODO: how does this relate to what tiles.resize does?
    ui::resize(crawl_view.termsz.x, crawl_view.termsz.y);

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
# ifndef KEY_RESIZE
    signal(SIGWINCH, SIG_DFL);
# endif
#endif
}

void cprintf(const char *format, ...)
{
    char buffer[2048];          // One full screen if no control seq...

    va_list argp;

    va_start(argp, format);
    vsnprintf(buffer, sizeof(buffer), format, argp);
    va_end(argp);

    char32_t c;
    char *bp = buffer;
    while (int s = utf8towc(&c, bp))
    {
        bp += s;
        putwch(c);
    }
}

void putwch(char32_t chr)
{
    wchar_t c = chr;
    if (!c)
        c = ' ';
    // TODO: recognize unsupported characters and try to transliterate
    addnwstr(&c, 1);

#ifdef USE_TILE_WEB
    char32_t buf[2];
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
}

// These next four are front functions so that we can reduce
// the amount of curses special code that occurs outside this
// this file. This is good, since there are some issues with
// name space collisions between curses macros and the standard
// C++ string class.  -- bwr
void update_screen()
{
    // In objstat and similar modes, there might not be a screen to update.
    if (stdscr)
    {
        // Refreshing the default colors helps keep colors synced in ttyrecs.
        curs_set_default_colors();
        refresh();
    }

#ifdef USE_TILE_WEB
    tiles.set_need_redraw();
#endif
}

void clear_to_end_of_line()
{
    textcolour(LIGHTGREY);
    textbackground(BLACK);
    clrtoeol();

#ifdef USE_TILE_WEB
    tiles.clear_to_end_of_line();
#endif
}

int get_number_of_lines()
{
    return LINES;
}

int get_number_of_cols()
{
    return COLS;
}

int num_to_lines(int num)
{
    return num;
}

void clrscr_sys()
{
    textcolour(LIGHTGREY);
    textbackground(BLACK);
    clear();
#ifdef DGAMELAUNCH
    printf("%s", DGL_CLEAR_SCREEN);
    fflush(stdout);
#endif

}

void set_cursor_enabled(bool enabled)
{
    curs_set(cursor_is_enabled = enabled);
#ifdef USE_TILE_WEB
    tiles.set_text_cursor(enabled);
#endif
}

bool is_cursor_enabled()
{
    return cursor_is_enabled;
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
           (col & COLFLAG_REVERSE)          ? unsigned{CHATTR_REVERSE}
                                            : unsigned{CHATTR_NORMAL};
}

// see declaration
static curses_style curs_attr(COLOURS fg, COLOURS bg, bool adjust_background)
{
    curses_style style;
    style.attr = 0;
    style.color_pair = 0;
    bool monochrome_output_requested = curs_palette_size() == 0;

    // Convert over to curses colours.
    short fg_curses = translate_colour(fg);
    short bg_curses = translate_colour(bg);

    // Resolve fg/bg colour conflicts.
    curs_adjust_color_pair_to_non_identical(fg_curses, bg_curses,
        adjust_background);

    if (!monochrome_output_requested)
    {
        // Grab the colour pair.
        style.color_pair = curs_calc_pair_safe(fg_curses, bg_curses);

        // Request decolourise if the pair doesn't actually exist.
        if (style.color_pair == 0
            && !curs_color_combo_has_pair(fg_curses, bg_curses))
        {
            monochrome_output_requested = true;
        }
    }

    if (!curs_can_use_extended_colors())
    {
        // curses typically uses WA_BOLD to give bright foreground colour,
        // but various termcaps may disagree
        if ((fg_curses & COLFLAG_CURSES_BRIGHTEN)
            && (Options.bold_brightens_foreground
                || Options.best_effort_brighten_foreground))
        {
            style.attr |= WA_BOLD;
        }

        // curses typically uses WA_BLINK to give bright background colour,
        // but various termcaps may disagree (in whole or in part)
        if ((bg_curses & COLFLAG_CURSES_BRIGHTEN)
            && (Options.blink_brightens_background
                || Options.best_effort_brighten_background))
        {
            style.attr |= WA_BLINK;
        }
    }

    if (monochrome_output_requested)
    {
        // Decolourise the output if necessary.
        if (curs_palette_size() != 0)
            style.color_pair = curs_calc_pair_safe(COLOR_WHITE, COLOR_BLACK);

        // Do the best we can for backgrounds with monochrome output.
        if (bg_curses != COLOR_BLACK)
            style.attr |= WA_REVERSE;
    }

    return style;
}

// see declaration
static curses_style curs_attr_bg(int col)
{
    BG_COL = static_cast<COLOURS>(col & 0x00ff);
    return curs_attr_mapped(FG_COL, BG_COL, get_brand(col));
}

// see declaration
static curses_style curs_attr_fg(int col)
{
    FG_COL = static_cast<COLOURS>(col & 0x00ff);
    return curs_attr_mapped(FG_COL, BG_COL, get_brand(col));
}

// see declaration
static curses_style curs_attr_mapped(COLOURS fg, COLOURS bg, int brand)
{
    COLOURS fg_mod = fg;
    COLOURS bg_mod = bg;
    attr_t flags = 0;

    // calculate which curses flags we need...
    if (brand != CHATTR_NORMAL)
    {
        flags |= convert_to_curses_style(brand);

        // Allow highlights to override the current background color.
        if ((brand & CHATTR_ATTRMASK) == CHATTR_HILITE)
            bg_mod = static_cast<COLOURS>((brand & CHATTR_COLMASK) >> 8);
    }

    // Respect color remapping.
    fg_mod = static_cast<COLOURS>(macro_colour(static_cast<short>(fg_mod)));
    bg_mod = static_cast<COLOURS>(macro_colour(static_cast<short>(bg_mod)));

    // Use the default foreground for lightgrey, with reverse mapping.
    if (fg_mod == LIGHTGREY)
        fg_mod = FG_COL_DEFAULT;
    else if (fg_mod == FG_COL_DEFAULT)
        fg_mod = LIGHTGREY;

    // Use the default background for black, with reverse mapping.
    if (bg_mod == BLACK)
        bg_mod = BG_COL_DEFAULT;
    else if (bg_mod == BG_COL_DEFAULT)
        bg_mod = BLACK;

    // Done with color mapping; get the resulting attributes.
    curses_style ret = curs_attr(fg_mod, bg_mod);
    ret.attr |= flags;

    // Reverse color manually to ensure correct brightening attrs.
    if ((brand & CHATTR_ATTRMASK) == CHATTR_REVERSE)
        return flip_colour(ret);

    return ret;
}

// see declaration
static short curs_calc_pair_safe(short fg, short bg)
{
    short fg_stripped = fg;
    short bg_stripped = bg;
    short fg_col_default_curses = curses_color_to_internal_colour(
        FG_COL_DEFAULT);
    short bg_col_default_curses = curses_color_to_internal_colour(
        BG_COL_DEFAULT);

    if (!curs_can_use_extended_colors())
    {
        // Strip out all the bits above the raw 3-bit colour definition
        fg_stripped &= 0x07;
        bg_stripped &= 0x07;
    }

    // Pass along to the more generic function for the heavy lifting.
    short pair = curs_calc_pair_safe(fg_stripped, bg_stripped,
        fg_col_default_curses, bg_col_default_curses);

    return pair;
}

/**
 * @internal
 *  Always use the default pair, 0, for the current default (fg, bg) combo.
 *  For the pair (fg=white, bg=black):
 *    Use a pair index which depends on the current default color combination.
 *    - If the default combo is this special combo, simply use pair 0.
 *    - Otherwise, use the color pair that would correspond to the default
 *      combo if the default combo was not the default.
 */
static short curs_calc_pair_safe(short fg, short bg, short fg_default,
    short bg_default)
{
    // Use something wider than short for the calculation in case of overflow.
    long pair = 0;

    if (fg == fg_default && bg == bg_default)
        pair = 0;
    else
    {
        // Remap the *default* default curses pair as necessary.
        short fg_mapped = fg;
        short bg_mapped = bg;

        if (fg == COLOR_WHITE && bg == COLOR_BLACK)
        {
            fg_mapped = fg_default;
            bg_mapped = bg_default;
        }

        // Work around the *default* default combination hole.
        const short num_colors = curs_palette_size();
        const short default_color_hole = 1 + COLOR_BLACK * num_colors
            + COLOR_WHITE;

        // Calculate the pair index, starting from 1.
        pair = 1 + bg_mapped * num_colors + fg_mapped;
        if (pair > default_color_hole)
            pair--;
    }

    // Last, validate the pair. Guard against overflow and bogus input.
    if (pair > COLOR_PAIRS || pair < 0)
        pair = 0;

    return static_cast<short>(pair);
}

// see declaration
static bool curs_can_use_extended_colors()
{
    return Options.allow_extended_colours && COLORS >= NUM_TERM_COLOURS;
}

// see declaration
static bool curs_color_combo_has_pair(short fg, short bg)
{
    short fg_col_curses = translate_colour(FG_COL_DEFAULT);
    short bg_col_curses = translate_colour(BG_COL_DEFAULT);

    bool has_pair = false;

    if (curs_palette_size() > 0)
    {
        if (fg == fg_col_curses && bg == bg_col_curses)
            has_pair = true;
        else
            has_pair = curs_calc_pair_safe(fg, bg, COLOR_WHITE, COLOR_BLACK);
    }

    return has_pair;
}

// see declaration
static void curs_adjust_color_pair_to_non_identical(short &fg, short &bg,
    bool adjust_background)
{
    // The default colors.
    short fg_default = translate_colour(FG_COL_DEFAULT);
    short bg_default = translate_colour(BG_COL_DEFAULT);

    // The expected output colors.
    short fg_to_compare = fg;
    short bg_to_compare = bg;
    short fg_default_to_compare = fg_default;
    short bg_default_to_compare = bg_default;

    // Adjust the expected output color depending on the game options.
    if (!curs_can_use_extended_colors())
    {
        if (!Options.bold_brightens_foreground)
        {
            fg_to_compare = fg & ~COLFLAG_CURSES_BRIGHTEN;
            fg_default_to_compare = fg_default & ~COLFLAG_CURSES_BRIGHTEN;
        }

        if (!Options.blink_brightens_background)
        {
            bg_to_compare = bg & ~COLFLAG_CURSES_BRIGHTEN;
            bg_default_to_compare = bg_default & ~COLFLAG_CURSES_BRIGHTEN;
        }
    }

    if (fg_to_compare != bg_to_compare)
        return;  // colours look different; no need to adjust

    // Choose terminal's current default colors as the default failsafe.
    short failsafe_col = adjust_background ? fg_default : bg_default;

    if (!adjust_background && fg_to_compare == bg_default_to_compare)
    {
        /*
            * Replacing the *foreground* color with a secondary failsafe.
            *
            * In general, use black as a failsafe for non-black backgrounds
            * and white as a failsafe for black backgrounds. Black tends to
            * look good on any visible background.
            *
            * However, for black and white *default* background colours,
            * mitigate information contrast issues with bright black
            * foregrounds by using blue as a special-case failsafe color.
            */
        switch (bg_default_to_compare)
        {
        case COLOR_BLACK:
            if (fg_default_to_compare == COLOR_WHITE
                || fg_default_to_compare == COLOR_BLACK)
            {
                failsafe_col = COLOR_BLUE;
            }
            else
                failsafe_col = COLOR_WHITE;
            break;
        case COLOR_WHITE:
            if (fg_default_to_compare == COLOR_WHITE
                || fg_default_to_compare == COLOR_BLACK)
            {
                failsafe_col = COLOR_BLUE;
            }
            else
                failsafe_col = COLOR_BLACK;
            break;
        default:
            failsafe_col = COLOR_BLACK;
            break;
        }
    }
    else if (adjust_background && bg_to_compare == fg_default_to_compare)
    {
        /*
            * Replacing the *background* color with a secondary failsafe.
            *
            * Don't bother special-casing bright black:
            *  - The information contrast issue is not as prevalent for
            *    backgrounds as foregrounds.
            *  - A visible bright-black-on-black glyph actively changing into
            *    black-on-blue when reversed looks much worse than a change to
            *    black-on-white.
            */
        if (fg_default_to_compare == COLOR_BLACK)
            failsafe_col = COLOR_WHITE;
        else
            failsafe_col = COLOR_BLACK;
    }

    // Update the appropriate color in the pair.
    if (adjust_background)
        bg = failsafe_col;
    else
        fg = failsafe_col;
}

// see declaration
static short curs_palette_size()
{
    int palette_size = std::min(COLORS, static_cast<int>(NUM_TERM_COLOURS));

    if (palette_size > SHRT_MAX)
        palette_size = SHRT_MAX;
    else if (palette_size < 8)
        palette_size = 0;

    return palette_size;
}

/**
 * @internal
 * There are ncurses-specific extensions to change the default color pair
 * from COLOR_WHITE on COLOR_BLACK.
 *
 * The use_default_colors() function is dangerous to use for the program,
 * unless it is running on a monochrome terminal.
 *
 * The color_content() function cannot be used on a default color, and
 * use_default_colors() may or may not be matched with an existing color in the
 * game's palette. There's a very likely chance of a color collision which
 * cannot be avoided programmatically.
 *
 * So, use the safer assume_default_colors() function with manually-defined
 * default colors for color terminals.
 */
static void curs_set_default_colors()
{
    // The *default* default curses colors.
    const COLOURS failsafe_fg = curses_color_to_internal_colour(COLOR_WHITE);
    const COLOURS failsafe_bg = curses_color_to_internal_colour(COLOR_BLACK);

    int default_colors_loaded = ERR;
    COLOURS default_fg_prev = FG_COL_DEFAULT;
    COLOURS default_bg_prev = BG_COL_DEFAULT;
    COLOURS default_fg = static_cast<COLOURS>(Options.foreground_colour);
    COLOURS default_bg = static_cast<COLOURS>(Options.background_colour);

#ifdef NCURSES_VERSION
    if (!curs_can_use_extended_colors())
    {
        // Deny colours outside the standard 8-color palette.
        if (is_high_colour(default_fg))
            default_fg = failsafe_fg;
        if (is_high_colour(default_bg))
            default_bg = failsafe_bg;
    }

    // Deny default background colours which can't pair with all foregrounds.
    if (!curs_color_combo_has_pair(curs_palette_size() - 1,
        translate_colour(default_bg)))
    {
        default_bg = failsafe_bg;
    }

    // Assume new default colors.
    if (curs_palette_size() == 0)
        default_colors_loaded = use_default_colors();
    else
    {
        default_colors_loaded = assume_default_colors(
            translate_colour(default_fg), translate_colour(default_bg));
    }
#endif

    // Check if a failsafe is needed.
    if (default_colors_loaded == ERR)
    {
        default_fg = failsafe_fg;
        default_bg = failsafe_bg;
    }

    // Store the validated default colors.
    // The new default color pair is now in pair 0.
    FG_COL_DEFAULT = default_fg;
    BG_COL_DEFAULT = default_bg;

    // Restore the previous default color pair.
    short default_fg_prev_curses = translate_colour(default_fg_prev);
    short default_bg_prev_curses = translate_colour(default_bg_prev);
    short prev_default_pair = curs_calc_pair_safe(default_fg_prev_curses,
        default_bg_prev_curses, COLOR_WHITE, COLOR_BLACK);
    if (prev_default_pair != 0)
    {
        init_pair(prev_default_pair, default_fg_prev_curses,
            default_bg_prev_curses);
    }

    // Make sure the *default* default color pair has a home.
    short new_default_default_pair = curs_calc_pair_safe(COLOR_WHITE,
        COLOR_BLACK, translate_colour(default_fg),
        translate_colour(default_bg));
    if (new_default_default_pair != 0)
        init_pair(new_default_default_pair, COLOR_WHITE, COLOR_BLACK);
}

// see declaration
static COLOURS curses_color_to_internal_colour(short col)
{
    switch (col)
    {
    case COLOR_BLACK:
        return BLACK;
    case COLOR_BLUE:
        return BLUE;
    case COLOR_GREEN:
        return GREEN;
    case COLOR_CYAN:
        return CYAN;
    case COLOR_RED:
        return RED;
    case COLOR_MAGENTA:
        return MAGENTA;
    case COLOR_YELLOW:
        return BROWN;
    case COLOR_WHITE:
        return LIGHTGREY;
    case (COLOR_BLACK | COLFLAG_CURSES_BRIGHTEN):
        return DARKGREY;
    case (COLOR_BLUE | COLFLAG_CURSES_BRIGHTEN):
        return LIGHTBLUE;
    case (COLOR_GREEN | COLFLAG_CURSES_BRIGHTEN):
        return LIGHTGREEN;
    case (COLOR_CYAN | COLFLAG_CURSES_BRIGHTEN):
        return LIGHTCYAN;
    case (COLOR_RED | COLFLAG_CURSES_BRIGHTEN):
        return LIGHTRED;
    case (COLOR_MAGENTA | COLFLAG_CURSES_BRIGHTEN):
        return LIGHTMAGENTA;
    case (COLOR_YELLOW | COLFLAG_CURSES_BRIGHTEN):
        return YELLOW;
    case (COLOR_WHITE | COLFLAG_CURSES_BRIGHTEN):
        return WHITE;
    default:
        return GREEN;
    }
}

void textcolour(int col)
{
    const auto style = curs_attr_fg(col);
    attr_set(style.attr, style.color_pair, nullptr);

#ifdef USE_TILE_WEB
    tiles.textcolour(col);
#endif
}

void textbackground(int col)
{
    const auto style = curs_attr_bg(col);
    attr_set(style.attr, style.color_pair, nullptr);

#ifdef USE_TILE_WEB
    tiles.textbackground(col);
#endif
}

void gotoxy_sys(int x, int y)
{
    move(y - 1, x - 1);
}

static inline cchar_t character_at(int y, int x)
{
    cchar_t c;
    // (void) is to hush an incorrect clang warning.
    (void)mvin_wch(y, x, &c);
    return c;
}

/**
 * @internal
 * Writing out a cchar_t to the screen using one of the add_wch functions does
 * not necessarily set the character attributes contained within.
 *
 * So, explicitly set the screen attributes to those contained within the passed
 * cchar_t before writing it to the screen. This *should* guarantee that that
 * all attributes within the cchar_t (and only those within the cchar_t) are
 * actually set.
 */
static void write_char_at(int y, int x, const cchar_t &ch)
{
    attr_t attr = 0;
    short color_pair = 0;
    wchar_t *wch = nullptr;

    // Make sure to allocate enough space for the characters.
    int chars_to_allocate = getcchar(&ch, nullptr, &attr, &color_pair, nullptr);
    if (chars_to_allocate > 0)
        wch = new wchar_t[chars_to_allocate];

    // Good to go. Grab the color / attr info.
    getcchar(&ch, wch, &attr, &color_pair, nullptr);

    // Clean up.
    if (chars_to_allocate > 0)
        delete [] wch;

    attr_set(attr, color_pair, nullptr);
    mvadd_wchnstr(y, x, &ch, 1);
}

// see declaration
static void flip_colour(cchar_t &ch)
{
    attr_t attr = 0;
    short color_pair = 0;
    wchar_t *wch = nullptr;

    // Make sure to allocate enough space for the characters.
    int chars_to_allocate = getcchar(&ch, nullptr, &attr,
        &color_pair, nullptr);
    if (chars_to_allocate > 0)
        wch = new wchar_t[chars_to_allocate];

    // Good to go. Grab the color / attr info.
    curses_style style;
    getcchar(&ch, wch, &style.attr, &style.color_pair, nullptr);
    style = flip_colour(style);

    // Assign the new, reversed info and clean up.
    setcchar(&ch, wch, style.attr, style.color_pair, nullptr);
    if (chars_to_allocate > 0)
        delete [] wch;
}

// see declaration
static curses_style flip_colour(curses_style style)
{
    short fg = COLOR_WHITE;
    short bg = COLOR_BLACK;

    if (style.color_pair != 0)
        pair_content(style.color_pair, &fg, &bg);
    else
    {
        // Default pair; use the current default colors.
        fg = translate_colour(FG_COL_DEFAULT);
        bg = translate_colour(BG_COL_DEFAULT);
    }

    bool need_attribute_only_flip = !curs_color_combo_has_pair(bg, fg);

    // Adjust brightness flags for low-color modes.
    if (!curs_can_use_extended_colors())
    {
        // Check if these were brightened colours.
        if (style.attr & WA_BOLD)
            fg |= COLFLAG_CURSES_BRIGHTEN;
        if (style.attr & WA_BLINK)
            bg |= COLFLAG_CURSES_BRIGHTEN;
        style.attr &= ~(WA_BOLD | WA_BLINK);
    }

    if (!need_attribute_only_flip)
    {
        // Perform a flip using the inverse color pair, preferring to adjust
        // the background color in case of a conflict.
        const auto out = curs_attr(curses_color_to_internal_colour(bg),
                                   curses_color_to_internal_colour(fg), true);
        style.color_pair = out.color_pair;
        style.attr |= out.attr;
    }
    else
    {
        // The best we can do is a purely attribute-based flip.

        // Swap potential brightness flags.
        if (!curs_can_use_extended_colors())
        {
            if ((fg & COLFLAG_CURSES_BRIGHTEN)
                && (Options.blink_brightens_background
                    || Options.best_effort_brighten_background))
            {
                style.attr |= WA_BLINK;
            }
            if ((bg & COLFLAG_CURSES_BRIGHTEN)
                && (Options.bold_brightens_foreground
                    || Options.best_effort_brighten_foreground))
            {
                style.attr |= WA_BOLD;
            }
        }

        style.attr ^= WA_REVERSE;
    }

    return style;
}

void fakecursorxy(int x, int y)
{
    int x_curses = x - 1;
    int y_curses = y - 1;

    cchar_t c = character_at(y_curses, x_curses);
    flip_colour(c);
    write_char_at(y_curses, x_curses, c);
    // the above still results in changes to the return values for wherex and
    // wherey, so set the cursor region to ensure that the cursor position is
    // valid after this call. (This also matches the behavior of real cursorxy.)
    set_cursor_region(GOTO_CRT);
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
    if (time)
    {
        tiles.send_message("{\"msg\":\"delay\",\"t\":%d}", time);
        tiles.flush_messages();
    }
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

    if (result && c != 0)
        pending = c;

    return result;
#endif
}
