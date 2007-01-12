/*
 *  File:       libunix.cc
 *  Summary:    Functions for unix and curses support
 *  Written by: ?
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <6>     10/11/99        BCR     Swapped 'v' and 'V' commands, fixed
 *                                      & for debug command.
 *      <5>     9/25/99         CDL     linuxlib -> liblinux
 *                                      changes to fix "macro problem"
 *                                      keypad -> command lookup
 *      <4>     99/07/13        BWR     added translate_keypad(), to try and
 *                                      translate keypad escape sequences into
 *                                      numeric char values.
 *      <3>     99/06/18        BCR     moved CHARACTER_SET #define to AppHdr.h
 *      <2>     99/05/12        BWR     Tchars, signals, solaris support
 *
 *      <1>      -/--/--        ?       Created
 *
 */

/* Some replacement routines missing in gcc
   Some of these are inspired by/stolen from the Linux-conio package
   by Mental EXPlotion. Hope you guys don't mind.
   The colour exchange system is perhaps a little overkill, but I wanted
   it to be general to support future changes.. The only thing not
   supported properly is black on black (used by curses for "normal" mode)
   and white on white (used by me for "bright black" (darkgray) on black

   Jan 1998 Svante Gerhard <svante@algonet.se>                          */

#include "AppHdr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#define _LIBUNIX_IMPLEMENTATION
#include "libunix.h"
#include "defines.h"

#include "enum.h"
#include "externs.h"
#include "files.h"

#include <termios.h>

static struct termios def_term;
static struct termios game_term;

#ifdef USE_UNIX_SIGNALS
#include <signal.h>
#endif

// Its best if curses comes at the end (name conflicts with Solaris). -- bwr
#ifndef CURSES_INCLUDE_FILE
    #include <curses.h>
#else
    #include CURSES_INCLUDE_FILE
#endif

// Character set variable
int Character_Set = CHARACTER_SET;

// Globals holding current text/backg. colors
short FG_COL = WHITE;
short BG_COL = BLACK;

static int curs_fg_attr(int col);
static int curs_bg_attr(int col);

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

void set_altcharset( bool alt_on )
{
    Character_Set = ((alt_on) ? A_ALTCHARSET : 0);
}

bool get_altcharset( void )
{
    return (Character_Set != 0);
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


static void termio_init()
{
    tcgetattr(0, &def_term);
    memcpy(&game_term, &def_term, sizeof(struct termios));

    def_term.c_cc[VINTR] = (char) 3;        // ctrl-C
    game_term.c_cc[VINTR] = (char) 3;       // ctrl-C

    // Lets recover some control sequences
    game_term.c_cc[VSTART] = (char) -1;     // ctrl-Q
    game_term.c_cc[VSTOP] = (char) -1;      // ctrl-S
    game_term.c_cc[VSUSP] = (char) -1;      // ctrl-Y
#ifdef VDSUSP
    game_term.c_cc[VDSUSP] = (char) -1;     // ctrl-Y
#endif

    tcsetattr(0, TCSAFLUSH, &game_term);
}


int getch_ck() {
    int c = getch();
    switch (c) {
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

#if defined(USE_UNIX_SIGNALS) && defined(SIGHUP_SAVE)

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

#endif // USE_UNIX_SIGNALS && SIGHUP_SAVE

static WINDOW *Message_Window;
static void setup_message_window()
{
    extern int get_message_window_height();
    
    Message_Window = newwin( get_message_window_height(), get_number_of_cols(),
                             VIEW_EY, 0 );
    if (!Message_Window)
    {
        fprintf(stderr, "Unable to create message window!");
        exit(1);
    }
    
    scrollok(Message_Window, true);
    idlok(Message_Window, true);
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
    extern int get_message_window_height();

    wattrset( Message_Window, curs_fg_attr(color) );

    if (!firstcol)
        firstcol = Options.delay_message_clear? 1 : 0;
    else
        firstcol--;
    
    mvwaddstr(Message_Window, which_line, firstcol, s);

    if (newline && which_line == get_message_window_height() - 1)
        scroll(Message_Window);

    wrefresh(Message_Window);
}

void unixcurses_startup( void )
{
    termio_init();

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
    start_color();
    setup_colour_pairs();

    scrollok(stdscr, TRUE);

    setup_message_window();
}

void unixcurses_shutdown()
{
    tcsetattr(0, TCSAFLUSH, &def_term);

#ifdef USE_UNIX_SIGNALS
#ifdef SIGQUIT
    signal(SIGQUIT, SIG_DFL);
#endif

#ifdef SIGINT
    signal(SIGINT, SIG_DFL);
#endif
#endif

    // resetty();
    endwin();
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


// Convert string to lowercase.
char *strlwr(char *str)
{
    unsigned int i;

    for (i = 0; i < strlen(str); i++)
        str[i] = tolower(str[i]);

    return (str);
}


int cprintf(const char *format,...)
{
    int i;
    char buffer[2048];          // One full screen if no control seq...

    va_list argp;

    va_start(argp, format);
    vsprintf(buffer, format, argp);
    va_end(argp);
    i = addstr(buffer);
    refresh();
    return (i);
}


int putch(unsigned char chr)
{
    if (chr == 0)
        chr = ' ';

    return (addch(chr));
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
    printf(DGL_CLEAR_SCREEN);
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
    return (col & COLFLAG_FRIENDLY_MONSTER)?    Options.friend_brand :
           (col & COLFLAG_ITEM_HEAP)?           Options.heap_brand :
           (col & COLFLAG_WILLSTAB)?            Options.stab_brand :
           (col & COLFLAG_MAYSTAB)?             Options.may_stab_brand :
           (col & COLFLAG_REVERSE)?             CHATTR_REVERSE :
                                                CHATTR_NORMAL;    
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

    return ( COLOR_PAIR(pair) | flags | Character_Set );
}

void textcolor(int col)
{
    attrset( curs_fg_attr(col) );
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

    return ( COLOR_PAIR(pair) | flags | Character_Set );
}

void textbackground(int col)
{
    attrset( curs_bg_attr(col) );
}


int gotoxy(int x, int y)
{
    return (move(y - 1, x - 1));
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
