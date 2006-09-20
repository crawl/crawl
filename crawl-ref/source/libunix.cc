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
int character_set = CHARACTER_SET;

// Globals holding current text/backg. colors
short FG_COL = WHITE;
short BG_COL = BLACK;

// a lookup table to convert keypresses to command enums
static int key_to_command_table[KEY_MAX];

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
        break;
    case BLUE:
        return COLOR_BLUE;
        break;
    case GREEN:
        return COLOR_GREEN;
        break;
    case CYAN:
        return COLOR_CYAN;
        break;
    case RED:
        return COLOR_RED;
        break;
    case MAGENTA:
        return COLOR_MAGENTA;
        break;
    case BROWN:
        return COLOR_YELLOW;
        break;
    case LIGHTGREY:
        return COLOR_WHITE;
        break;
    case DARKGREY:
        return COLOR_BLACK + COLFLAG_CURSES_BRIGHTEN;
        break;
    case LIGHTBLUE:
        return COLOR_BLUE + COLFLAG_CURSES_BRIGHTEN;
        break;
    case LIGHTGREEN:
        return COLOR_GREEN + COLFLAG_CURSES_BRIGHTEN;
        break;
    case LIGHTCYAN:
        return COLOR_CYAN + COLFLAG_CURSES_BRIGHTEN;
        break;
    case LIGHTRED:
        return COLOR_RED + COLFLAG_CURSES_BRIGHTEN;
        break;
    case LIGHTMAGENTA:
        return COLOR_MAGENTA + COLFLAG_CURSES_BRIGHTEN;
        break;
    case YELLOW:
        return COLOR_YELLOW + COLFLAG_CURSES_BRIGHTEN;
        break;
    case WHITE:
        return COLOR_WHITE + COLFLAG_CURSES_BRIGHTEN;
        break;
    default:
        return COLOR_GREEN;
        break;                  //mainly for debugging
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

void init_key_to_command()
{
    int i;

    // initialize to "do nothing"
    for (i = 0; i < KEY_MAX; i++)
    {
        key_to_command_table[i] = CMD_NO_CMD;
    }

    // lower case
    key_to_command_table[(int) 'a'] = CMD_USE_ABILITY;
    key_to_command_table[(int) 'b'] = CMD_MOVE_DOWN_LEFT;
    key_to_command_table[(int) 'c'] = CMD_CLOSE_DOOR;
    key_to_command_table[(int) 'd'] = CMD_DROP;
    key_to_command_table[(int) 'e'] = CMD_EAT;
    key_to_command_table[(int) 'f'] = CMD_FIRE;
    key_to_command_table[(int) 'g'] = CMD_PICKUP;
    key_to_command_table[(int) 'h'] = CMD_MOVE_LEFT;
    key_to_command_table[(int) 'i'] = CMD_DISPLAY_INVENTORY;
    key_to_command_table[(int) 'j'] = CMD_MOVE_DOWN;
    key_to_command_table[(int) 'k'] = CMD_MOVE_UP;
    key_to_command_table[(int) 'l'] = CMD_MOVE_RIGHT;
    key_to_command_table[(int) 'm'] = CMD_DISPLAY_SKILLS;
    key_to_command_table[(int) 'n'] = CMD_MOVE_DOWN_RIGHT;
    key_to_command_table[(int) 'o'] = CMD_OPEN_DOOR;
    key_to_command_table[(int) 'p'] = CMD_PRAY;
    key_to_command_table[(int) 'q'] = CMD_QUAFF;
    key_to_command_table[(int) 'r'] = CMD_READ;
    key_to_command_table[(int) 's'] = CMD_SEARCH;
    key_to_command_table[(int) 't'] = CMD_THROW;
    key_to_command_table[(int) 'u'] = CMD_MOVE_UP_RIGHT;
    key_to_command_table[(int) 'v'] = CMD_EXAMINE_OBJECT;
    key_to_command_table[(int) 'w'] = CMD_WIELD_WEAPON;
    key_to_command_table[(int) 'x'] = CMD_LOOK_AROUND;
    key_to_command_table[(int) 'y'] = CMD_MOVE_UP_LEFT;
    key_to_command_table[(int) 'z'] = CMD_ZAP_WAND;

    // upper case
    key_to_command_table[(int) 'A'] = CMD_DISPLAY_MUTATIONS;
    key_to_command_table[(int) 'B'] = CMD_RUN_DOWN_LEFT;
    key_to_command_table[(int) 'C'] = CMD_EXPERIENCE_CHECK;
    key_to_command_table[(int) 'D'] = CMD_BUTCHER;
    key_to_command_table[(int) 'E'] = CMD_EVOKE;
    key_to_command_table[(int) 'F'] = CMD_NO_CMD;
    key_to_command_table[(int) 'G'] = CMD_NO_CMD;
    key_to_command_table[(int) 'H'] = CMD_RUN_LEFT;
    key_to_command_table[(int) 'I'] = CMD_OBSOLETE_INVOKE;
    key_to_command_table[(int) 'J'] = CMD_RUN_DOWN;
    key_to_command_table[(int) 'K'] = CMD_RUN_UP;
    key_to_command_table[(int) 'L'] = CMD_RUN_RIGHT;
    key_to_command_table[(int) 'M'] = CMD_MEMORISE_SPELL;
    key_to_command_table[(int) 'N'] = CMD_RUN_DOWN_RIGHT;
    key_to_command_table[(int) 'O'] = CMD_DISPLAY_OVERMAP;
    key_to_command_table[(int) 'P'] = CMD_WEAR_JEWELLERY;
    key_to_command_table[(int) 'Q'] = CMD_QUIT;
    key_to_command_table[(int) 'R'] = CMD_REMOVE_JEWELLERY;
    key_to_command_table[(int) 'S'] = CMD_SAVE_GAME;
    key_to_command_table[(int) 'T'] = CMD_REMOVE_ARMOUR;
    key_to_command_table[(int) 'U'] = CMD_RUN_UP_RIGHT;
    key_to_command_table[(int) 'V'] = CMD_GET_VERSION;
    key_to_command_table[(int) 'W'] = CMD_WEAR_ARMOUR;
    key_to_command_table[(int) 'X'] = CMD_DISPLAY_MAP;
    key_to_command_table[(int) 'Y'] = CMD_RUN_UP_LEFT;
    key_to_command_table[(int) 'Z'] = CMD_CAST_SPELL;

    // control
    key_to_command_table[ (int) CONTROL('A') ] = CMD_TOGGLE_AUTOPICKUP;
    key_to_command_table[ (int) CONTROL('B') ] = CMD_OPEN_DOOR_DOWN_LEFT;
    key_to_command_table[ (int) CONTROL('C') ] = CMD_CLEAR_MAP;

#ifdef ALLOW_DESTROY_ITEM_COMMAND
    key_to_command_table[ (int) CONTROL('D') ] = CMD_DESTROY_ITEM;
#else
    key_to_command_table[ (int) CONTROL('D') ] = CMD_NO_CMD;
#endif

    key_to_command_table[ (int) CONTROL('E') ] = CMD_FORGET_STASH;
    key_to_command_table[ (int) CONTROL('F') ] = CMD_SEARCH_STASHES;
    key_to_command_table[ (int) CONTROL('G') ] = CMD_INTERLEVEL_TRAVEL;
    key_to_command_table[ (int) CONTROL('H') ] = CMD_OPEN_DOOR_LEFT;
    key_to_command_table[ (int) CONTROL('I') ] = CMD_NO_CMD;
    key_to_command_table[ (int) CONTROL('J') ] = CMD_OPEN_DOOR_DOWN;
    key_to_command_table[ (int) CONTROL('K') ] = CMD_OPEN_DOOR_UP;
    key_to_command_table[ (int) CONTROL('L') ] = CMD_OPEN_DOOR_RIGHT;
    key_to_command_table[ (int) CONTROL('M') ] = CMD_NO_CMD;
    key_to_command_table[ (int) CONTROL('N') ] = CMD_OPEN_DOOR_DOWN_RIGHT;
    key_to_command_table[ (int) CONTROL('O') ] = CMD_EXPLORE;
    key_to_command_table[ (int) CONTROL('P') ] = CMD_REPLAY_MESSAGES;
    key_to_command_table[ (int) CONTROL('Q') ] = CMD_NO_CMD;
    key_to_command_table[ (int) CONTROL('R') ] = CMD_REDRAW_SCREEN;
    key_to_command_table[ (int) CONTROL('S') ] = CMD_MARK_STASH;
    key_to_command_table[ (int) CONTROL('T') ] = CMD_NO_CMD;
    key_to_command_table[ (int) CONTROL('U') ] = CMD_OPEN_DOOR_UP_LEFT;
    key_to_command_table[ (int) CONTROL('V') ] = CMD_TOGGLE_AUTOPRAYER;
    key_to_command_table[ (int) CONTROL('W') ] = CMD_FIX_WAYPOINT;
    key_to_command_table[ (int) CONTROL('X') ] = CMD_SAVE_GAME_NOW;
    key_to_command_table[ (int) CONTROL('Y') ] = CMD_OPEN_DOOR_UP_RIGHT;
    key_to_command_table[ (int) CONTROL('Z') ] = CMD_SUSPEND_GAME;

    // other printables
    key_to_command_table[(int) '.'] = CMD_MOVE_NOWHERE;
    key_to_command_table[(int) '<'] = CMD_GO_UPSTAIRS;
    key_to_command_table[(int) '>'] = CMD_GO_DOWNSTAIRS;
    key_to_command_table[(int) '@'] = CMD_DISPLAY_CHARACTER_STATUS;
    key_to_command_table[(int) ','] = CMD_PICKUP;
    key_to_command_table[(int) ':'] = CMD_MAKE_NOTE;
    key_to_command_table[(int) ';'] = CMD_INSPECT_FLOOR;
    key_to_command_table[(int) '!'] = CMD_SHOUT;
    key_to_command_table[(int) '^'] = CMD_DISPLAY_RELIGION;
    key_to_command_table[(int) '#'] = CMD_CHARACTER_DUMP;
    key_to_command_table[(int) '='] = CMD_ADJUST_INVENTORY;
    key_to_command_table[(int) '?'] = CMD_DISPLAY_COMMANDS;
    key_to_command_table[(int) '`'] = CMD_MACRO_ADD;
    key_to_command_table[(int) '~'] = CMD_MACRO_SAVE;
    key_to_command_table[(int) '&'] = CMD_WIZARD;
    key_to_command_table[(int) '"'] = CMD_LIST_JEWELLERY;


    // I'm making this both, because I'm tried of changing it
    // back to '[' (the character that represents armour on the map) -- bwr
    key_to_command_table[(int) '['] = CMD_LIST_ARMOUR;
    key_to_command_table[(int) ']'] = CMD_LIST_ARMOUR;

    // This one also ended up backwards this time, so it's also going on
    // both now -- should be ')'... the same character that's used to
    // represent weapons.
    key_to_command_table[(int) ')'] = CMD_LIST_WEAPONS;
    key_to_command_table[(int) '('] = CMD_LIST_WEAPONS;

    key_to_command_table[(int) '\\'] = CMD_DISPLAY_KNOWN_OBJECTS;
    key_to_command_table[(int) '\''] = CMD_WEAPON_SWAP;

    // digits
    key_to_command_table[(int) '0'] = CMD_NO_CMD;
    key_to_command_table[(int) '1'] = CMD_MOVE_DOWN_LEFT;
    key_to_command_table[(int) '2'] = CMD_MOVE_DOWN;
    key_to_command_table[(int) '3'] = CMD_MOVE_DOWN_RIGHT;
    key_to_command_table[(int) '4'] = CMD_MOVE_LEFT;
    key_to_command_table[(int) '5'] = CMD_REST;
    key_to_command_table[(int) '6'] = CMD_MOVE_RIGHT;
    key_to_command_table[(int) '7'] = CMD_MOVE_UP_LEFT;
    key_to_command_table[(int) '8'] = CMD_MOVE_UP;
    key_to_command_table[(int) '9'] = CMD_MOVE_UP_RIGHT;

    // keypad
    key_to_command_table[KEY_A1] = CMD_MOVE_UP_LEFT;
    key_to_command_table[KEY_A3] = CMD_MOVE_UP_RIGHT;
    key_to_command_table[KEY_C1] = CMD_MOVE_DOWN_LEFT;
    key_to_command_table[KEY_C3] = CMD_MOVE_DOWN_RIGHT;

    key_to_command_table[KEY_HOME] = CMD_MOVE_UP_LEFT;
    key_to_command_table[KEY_PPAGE] = CMD_MOVE_UP_RIGHT;
    key_to_command_table[KEY_END] = CMD_MOVE_DOWN_LEFT;
    key_to_command_table[KEY_NPAGE] = CMD_MOVE_DOWN_RIGHT;

    key_to_command_table[KEY_B2] = CMD_REST;

    key_to_command_table[KEY_UP] = CMD_MOVE_UP;
    key_to_command_table[KEY_DOWN] = CMD_MOVE_DOWN;
    key_to_command_table[KEY_LEFT] = CMD_MOVE_LEFT;
    key_to_command_table[KEY_RIGHT] = CMD_MOVE_RIGHT;


    // other odd things
    // key_to_command_table[ 263 ] = CMD_OPEN_DOOR_LEFT;   // backspace

    // these are invalid keys, but to help kludge running
    // pass them through unmolested
    key_to_command_table[128] = 128;
    key_to_command_table[(int) '*'] = '*';
}

int key_to_command(int keyin)
{
    bool is_userfunction(int key);

    if (is_userfunction(keyin))
        return keyin;
    return (key_to_command_table[keyin]);
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
#endif

    //savetty();

    initscr();
    // cbreak();
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
    //cbreak();

    meta(stdscr, TRUE);
    start_color();
    setup_colour_pairs();

    init_key_to_command();

#ifndef SOLARIS
    // These can cause some display problems under Solaris
    scrollok(stdscr, TRUE);
#endif
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

int get_number_of_lines_from_curses(void)
{
    return (LINES);
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
    refresh();
    return (retval);
}


void _setcursortype(int curstype)
{
    curs_set(curstype);
}

inline unsigned get_brand(int col)
{
    return (col & COLFLAG_FRIENDLY_MONSTER)?    Options.friend_brand :
           (col & COLFLAG_ITEM_HEAP)?           Options.heap_brand :
           (col & COLFLAG_WILLSTAB)?            Options.stab_brand :
           (col & COLFLAG_MAYSTAB)?             Options.may_stab_brand :
                                                CHATTR_NORMAL;    
}

void textcolor(int col)
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
    // but various termcaps may disagree (in whole or in part)
    if (bg & COLFLAG_CURSES_BRIGHTEN)
        flags |= A_BLINK;

    // Strip out all the bits above the raw 3-bit colour definition
    fg &= 0x0007;
    bg &= 0x0007;

    // figure out which colour pair we want
    const int pair = (fg == 0 && bg == 0) ? 63 : (bg * 8 + fg);

    attrset( COLOR_PAIR(pair) | flags | character_set );
}


void textbackground(int col)
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

    attrset( COLOR_PAIR(pair) | flags | character_set );
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
