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
#include "unicode.h"
#include "view.h"

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

/**
 * @brief Get curses attributes for the current internal color combination.
 *
 * @param attr
 *  The destination for the resulting character attributes.
 * @param color_pair
 *  The destination for the resulting color pair index.
 * @param fg
 *  The internal colour for the foreground.
 * @param bg
 *  The internal colour for the background.
 */
static void curs_attr(attr_t &attr, short &color_pair, COLOURS fg, COLOURS bg);

/**
 * @brief Change the foreground colour and returns the resulting curses info.
 *
 * @param attr
 *  The destination for the resulting character attributes.
 * @param color_pair
 *  The destination for the resulting color pair index.
 * @param col
 *  An internal color with attached brand.
 */
static void curs_attr_bg(attr_t &attr, short &color_pair, int col);

/**
 * @brief Change the background colour and returns the resulting curses info.
 *
 * @param attr
 *  The destination for the resulting character attributes.
 * @param color_pair
 *  The destination for the resulting color pair index.
 * @param col
 *  An internal color with attached brand.
 */
static void curs_attr_fg(attr_t &attr, short &color_pair, int col);

/**
 * @brief Get curses attributes for the passed internal color combination.
 *
 * Performs colour mapping for both default colours as well as the color map.
 * Additionally, this function takes into consideration a passed brand.
 *
 * @copydetails curs_attr(attr_t &, short *, COLOURS, COLOURS, int)
 * @param brand
 *  Internal color branding information.
 */
static void curs_attr_mapped(attr_t &attr, short &color_pair, COLOURS fg,
    COLOURS bg, int brand);

/**
 * @brief Returns a curses color pair index for the passed fg/bg combo.
 *
 * Explicitly returns the default pair (0) if the pair would exceed COLOR_PAIRS.
 *
 * @param fg
 *  The flagged curses color index for the foreground.
 * @param bg
 *  The flagged curses color index for the background.
 * @param raw
 *  If true, never strip brightening flags.
 *  Otherwise, fg and bg may have their brightening flags stripped depending on
 *  cur_can_use_extended_colors().
 */
static short curs_calc_pair_safe(short fg, short bg, bool raw = false);

/**
 * @brief Can extended colors be used directly instead of character attributes?
 *
 * @return
 *  True if extended colors can be used in lieu of attributes, false otherwise.
 */
static bool curs_can_use_extended_colors();

/**
 * @brief Get a foreground color which is not the same as the background.
 *
 * Colors which are not equal in value may still be considered identical
 * in appearance depending on the current options.
 *
 * @param fg
 *  The foreground flagged curses color.
 * @param bg
 *  The background flagged curses color.
 *
 * @return
 *  A flagged curses colour not visually-identical to @p bg.
 */
static short curs_get_fg_color_non_identical(short fg, short bg);

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
 * This flip respects the current rendering assumptions. Therefore, the
 * resulting color combination may not be a strict color swap, but one that
 * is guaranteed to be visible.
 *
 * @param ch
 *  The curses character whose colors will be flipped.
 */
static void flip_colour(cchar_t &ch);

/**
 * @brief Translate internal colours to flagged curses colors.
 *
 * @param col
 *  The internalcolour to translate.
 *
 * @return
 *  The equivalent flagged curses colour.
 */
static short translate_colour(COLOURS col);

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
 * curs_calc_pair_safe() to order pairs and reject unwanted combinations.
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
            short pair = curs_calc_pair_safe(j, i, true);
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
    // Set up defaults before pairs on the off-chance it increases COLOR_PAIRS.
    curs_set_default_colors();
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
    update_screen();
}

// These next four are front functions so that we can reduce
// the amount of curses special code that occurs outside this
// this file. This is good, since there are some issues with
// name space collisions between curses macros and the standard
// C++ string class.  -- bwr
void update_screen()
{
    // Refreshing the default colors helps keep colors synced in ttyrecs.
    curs_set_default_colors();
    refresh();

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

void clrscr()
{
    textcolour(LIGHTGREY);
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
    curs_set(cursor_is_enabled = enabled);
#ifdef USE_TILE_WEB
    tiles.set_text_cursor(enabled);
#endif
}

bool is_cursor_enabled()
{
    return cursor_is_enabled;
}

bool is_smart_cursor_enabled()
{
    return false;
}

void enable_smart_cursor(bool /*cursor*/)
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

// see declaration
static void curs_attr(attr_t &attr, short &color_pair, COLOURS fg, COLOURS bg)
{
    attr_t flags = 0;

    // Convert over to curses colors.
    short fg_curses = translate_colour(fg);
    short bg_curses = translate_colour(bg);

    // Resolve fg/bg color conflicts.
    fg_curses = curs_get_fg_color_non_identical(fg_curses, bg_curses);

    if (!curs_can_use_extended_colors())
    {
        // curses typically uses A_BOLD to give bright foreground colour,
        // but various termcaps may disagree
        if (fg_curses & COLFLAG_CURSES_BRIGHTEN)
            flags |= A_BOLD;

        // curses typically uses A_BLINK to give bright background colour,
        // but various termcaps may disagree (in whole or in part)
        if (bg_curses & COLFLAG_CURSES_BRIGHTEN)
            flags |= A_BLINK;
    }

    // Got everything we need -- write out the results.
    color_pair = curs_calc_pair_safe(fg_curses, bg_curses);
    attr = flags;
}

// see declaration
static void curs_attr_bg(attr_t &attr, short &color_pair, int col)
{
    BG_COL = static_cast<COLOURS>(col & 0x00ff);
    curs_attr_mapped(attr, color_pair, FG_COL, BG_COL, get_brand(col));
}

// see declaration
static void curs_attr_fg(attr_t &attr, short &color_pair, int col)
{
    FG_COL = static_cast<COLOURS>(col & 0x00ff);
    curs_attr_mapped(attr, color_pair, FG_COL, BG_COL, get_brand(col));
}

// see declaration
static void curs_attr_mapped(attr_t &attr, short &color_pair, COLOURS fg,
    COLOURS bg, int brand)
{
    COLOURS fg_mod = fg;
    COLOURS bg_mod = bg;
    attr_t flags = 0;

    // calculate which curses flags we need...
    if (brand != CHATTR_NORMAL)
    {
        flags |= convert_to_curses_attr(brand);
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

    curs_attr(attr, color_pair, fg_mod, bg_mod);
    attr |= flags;
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
static short curs_calc_pair_safe(short fg, short bg, bool raw)
{
    // Use something wider than short for the calculation in case of overflow.
    long pair = 0;

    short fg_stripped = fg;
    short bg_stripped = bg;
    short fg_col_default_curses = curses_color_to_internal_colour(
        FG_COL_DEFAULT);
    short bg_col_default_curses = curses_color_to_internal_colour(
        BG_COL_DEFAULT);

    if (!raw && !curs_can_use_extended_colors())
    {
        // Strip out all the bits above the raw 3-bit colour definition
        fg_stripped &= 0x07;
        bg_stripped &= 0x07;
    }

    if (fg_stripped == fg_col_default_curses
        && bg_stripped == bg_col_default_curses)
    {
        // This pair is the current default.
        pair = 0;
    }
    else
    {
        // Remap the default curses default pair, if necessary.
        if (fg_stripped == COLOR_WHITE && bg_stripped == COLOR_BLACK)
        {
            fg_stripped = fg_col_default_curses;
            bg_stripped = bg_col_default_curses;
        }

        // Work around the *default* default combination hole.
        const short num_colors = curs_palette_size();
        const short default_color_hole = 1 + COLOR_BLACK * num_colors
            + COLOR_WHITE;

        // Combine to get the actual pair index, starting from 1.
        pair = 1 + bg_stripped * num_colors + fg_stripped;
        if (pair > default_color_hole)
            pair--;
    }

    // Last, guard against overflow and bogus input.
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
static short curs_get_fg_color_non_identical(short fg, short bg)
{
    // The color to return.
    short fg_non_conflicting = fg;

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

    // Special case: bright black, due to the history of broken terminals.
    if (Options.no_dark_brand)
    {
        const short curses_black = translate_colour(BLACK);
        const short curses_darkgrey = translate_colour(DARKGREY);
        short * colours_to_change[] =
        {
            &fg_to_compare,
            &bg_to_compare,
            &fg_default_to_compare,
            &bg_default_to_compare
        };

        // Convert any darkgreys to black.
        for (size_t i = 0; i < ARRAYSZ(colours_to_change); i++)
        {
            if (*(colours_to_change[i]) == curses_darkgrey)
                *(colours_to_change[i]) = curses_black;
        }
    }

    // Got the adjusted colors -- resolve any conflict.
    if (fg_to_compare == bg_to_compare)
    {
        // Choose the background color as the default failsafe.
        short failsafe_col = bg_default;

        if (fg_to_compare == bg_default_to_compare)
        {
            /*
             * For default background colors other than black and lightgrey,
             * use black as the failsafe. It looks good on any background.
             *
             * For black and lightgrey backgrounds, however, use blue to
             * mitigate information contrast issues with darkgrey.
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
                failsafe_col = COLOR_BLUE;
                break;
            case COLOR_WHITE:
                if (fg_default_to_compare == COLOR_WHITE
                    || fg_default_to_compare == COLOR_BLACK)
                {
                    failsafe_col = COLOR_BLUE;
                }
                else
                    failsafe_col = COLOR_BLACK;
                failsafe_col = COLOR_BLUE;
                break;
            default:
                failsafe_col = COLOR_BLACK;
                break;
            }
        }

        fg_non_conflicting = failsafe_col;
    }

    return fg_non_conflicting;
}

// see declaration
static short curs_palette_size()
{
    int palette_size = std::min(COLORS, static_cast<int>(NUM_TERM_COLOURS));

    if (palette_size > SHRT_MAX)
        palette_size = SHRT_MAX;

    return palette_size;
}

/**
 * @internal
 * There are ncurses-specific extensions to change the default color pair
 * from COLOR_WHITE on COLOR_BLACK.
 *
 * However, use_default_colors() alone is dangerous to use for the program.
 *
 * The color_content() function cannot be used on a default color, and
 * use_default_colors() may or may not be matched with an existing color in the
 * game's palette. So, there's a very likely chance of a color collision which
 * cannot be avoided programmatically.
 *
 * This leaves the assume_default_colors() function using a manually-specified
 * background_colour option.
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
    else
    {
        // Deny default background colours which can't be indexed.
        short default_fg_curses = translate_colour(default_fg);
        short default_bg_curses = translate_colour(default_bg);

        if (!(default_fg_curses == COLOR_BLACK
            && default_bg_curses == COLOR_WHITE))
        {
            FG_COL_DEFAULT = failsafe_fg;
            BG_COL_DEFAULT = failsafe_bg;

            if (!curs_calc_pair_safe(curs_palette_size() - 1,
                default_bg_curses))
            {
                // Can't index all combinations with that background. Denied.
                default_fg = failsafe_fg;
                default_bg = failsafe_bg;
            }

            FG_COL_DEFAULT = default_fg_prev;
            BG_COL_DEFAULT = default_bg_prev;
        }
    }

    // Assume new default colors.
    use_default_colors();
    default_colors_loaded = assume_default_colors(translate_colour(default_fg),
        translate_colour(default_bg));
#endif

    // Check if a failsafe is needed.
    if (default_colors_loaded == ERR)
    {
        default_fg = failsafe_fg;
        default_bg = failsafe_bg;
    }

    // Store the validated default colors.
    FG_COL_DEFAULT = default_fg;
    BG_COL_DEFAULT = default_bg;

    // The new default color pair is now in pair 0.
    // Make sure the *default* default color pair has a home.
    short fg_col_default_curses = translate_colour(FG_COL_DEFAULT);
    short bg_col_default_curses = translate_colour(BG_COL_DEFAULT);
    if (!(fg_col_default_curses == COLOR_WHITE
        && bg_col_default_curses == COLOR_BLACK))
    {
        init_pair(curs_calc_pair_safe(COLOR_WHITE, COLOR_BLACK), COLOR_WHITE,
            COLOR_BLACK);
    }

    // Restore the previous default color pair to normal.
    if (!(default_fg == default_fg_prev && default_bg == default_bg_prev))
    {
        short default_fg_prev_curses = translate_colour(default_fg_prev);
        short default_bg_prev_curses = translate_colour(default_bg_prev);
        // ... but not if it was the *default* default pair.
        if (!(default_fg_prev_curses == COLOR_WHITE
            && default_bg_prev_curses == COLOR_BLACK))
        {
            init_pair(
                curs_calc_pair_safe(default_fg_prev_curses,
                    default_bg_prev_curses, true), default_fg_prev_curses,
                default_bg_prev_curses);
        }
    }

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
    attr_t attr = 0;
    short color_pair = 0;
    curs_attr_fg(attr, color_pair, col);
    attr_set(attr, color_pair, nullptr);

#ifdef USE_TILE_WEB
    tiles.textcolour(col);
#endif
}

void textbackground(int col)
{
    attr_t attr = 0;
    short color_pair = 0;
    curs_attr_bg(attr, color_pair, col);
    attr_set(attr, color_pair, nullptr);

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
    attr_t attr = 0;
    short color_pair = 0;
    wchar_t *wch = nullptr;

    // Make sure to allocate enough space for the characters.
    int chars_to_allocate = getcchar(&ch, nullptr, &attr, &color_pair, nullptr);
    if (chars_to_allocate > 0)
        wch = new wchar_t[chars_to_allocate];

    // Good to go. Grab the color / attr info.
    getcchar(&ch, wch, &attr, &color_pair, nullptr);

    if (color_pair == 0)
    {
        // Cannot get meaningful color information from the default pair.
        // So, invert the reverse flag and hope for the best.
        if (attr & A_REVERSE)
            attr &= ~A_REVERSE;
        else
            attr |= A_REVERSE;
    }
    else
    {
        // Have a color pair which can be manually reversed.
        short fg = COLOR_WHITE;
        short bg = COLOR_BLACK;
        pair_content(color_pair, &fg, &bg);

        // Check if these were brightened colours.
        if (!curs_can_use_extended_colors())
        {
            if (attr & A_BOLD)
                fg |= COLFLAG_CURSES_BRIGHTEN;
            if (attr & A_BLINK)
                bg |= COLFLAG_CURSES_BRIGHTEN;
            attr &= ~(A_BOLD | A_BLINK);
        }

        // Perform the flip, preserving most attrs.
        attr_t newattr;
        curs_attr(newattr, color_pair, curses_color_to_internal_colour(bg),
            curses_color_to_internal_colour(fg));
        attr |= newattr;
    }

    // Assign the new, reversed info and clean up.
    setcchar(&ch, wch, attr, color_pair, nullptr);
    if (chars_to_allocate > 0)
        delete [] wch;
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
