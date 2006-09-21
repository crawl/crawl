/*
 *  File:       defines.h
 *  Summary:    Various definess used by Crawl.
 *  Written by: Linley Henzel
 *
 *      Abstract:       A variety of miscellaneous constant values are found here.
 *                      I think we should move the colors into an enum or something
 *                      because there are in numerical order.  But I'm too lazy to
 *                      do it myself.
 *
 *  Copyright © 1999 Brian Robinson.  // Me?  How come?
 *
 *  Change History (most recent first):
 *
 *       <4>     7/29/00        JDJ     Renamed MNG NON_MONSTER, MNST MAX_MONSTERS, ITEMS MAX_ITEMS,
 *                                      ING NON_ITEM, CLOUDS MAX_CLOUDS, CNG EMPTY_CLOUD, NTRAPS MAX_TRAPS.
 *       <3>     9/25/99        CDL     linuxlib -> liblinux
 *       <2>     6/17/99        BCR     indented and added header
 *       <1>     --/--/--       LRH     Created
 */

#ifndef DEFINES_H
#define DEFINES_H

#define ESCAPE '\x1b'           // most ansi-friendly way I can think of defining this.

// there's got to be a better way...
#ifndef _LIBUNIX_IMPLEMENTATION
#else
    #ifndef TRUE
     #define TRUE 1
    #endif

    #ifndef FALSE
     #define FALSE 0
    #endif
#endif


// max size of inventory array {dlb}:
#define ENDOFPACK 52

// minimum value for strength required on armour and weapons
#define STR_REQ_THRESHOLD       10

// max size of monter array {dlb}:
#define MAX_MONSTERS 200
// number of monster enchantments
#define NUM_MON_ENCHANTS 6
// non-monster for mgrd[][] -- (MNST + 1) {dlb}:
#define NON_MONSTER 201

#define MAX_SUBTYPES    50

// max size of item list {dlb}:
#define MAX_ITEMS 500
// non-item -- (ITEMS + 1) {dlb}
#define NON_ITEM 501

// max size of cloud array {dlb}:
#define MAX_CLOUDS 100

// empty cloud -- (CLOUDS + 1) {dlb}:
#define EMPTY_CLOUD 101

// max x-bound for level generation {dlb}
#define GXM 80
// max y-bound for level generation {dlb}
#define GYM 70

#define LOS_SX     8
#define LOS_EX    25
#define LOS_SY     1
#define LOS_EY    17

#define VIEW_SX    1
#define VIEW_EX   33
#define VIEW_SY    1
#define VIEW_EY   17

#define VIEW_Y_DIFF  (((VIEW_EX - VIEW_SX + 1) - (VIEW_EY - VIEW_SY + 1)) / 2) 

// View centre must be the same as LOS centre.
// VIEW_CX == 17
#define VIEW_CX   ((VIEW_SX + VIEW_EX) / 2)
// VIEW_CY == 9
#define VIEW_CY   ((VIEW_SY + VIEW_EY) / 2)

// max traps per level
#define MAX_TRAPS 30

// max shops per level
#define MAX_SHOPS 5

// lowest grid value which can be passed by walking etc.
#define MINMOVE 31

// lowest grid value which can be seen through
#define MINSEE 11

// This value is used to make test_hit checks always succeed
#define AUTOMATIC_HIT           1500

// grids that monsters can see
#define MONSTER_LOS_RANGE       8

// Maximum charge level for rods
#define MAX_ROD_CHARGE                  17
#define ROD_CHARGE_MULT                 100

// Maximum enchantment on weapons/armour/secondary armours
// Note: use armour_max_enchant(item) to get the correct limit for item
#define MAX_WPN_ENCHANT                 5
#define MAX_ARM_ENCHANT                 5
#define MAX_SEC_ENCHANT                 2

#define NUM_STAVE_ADJ                   9

// some shortcuts:
#define menv   env.mons
#define mitm   env.item
#define grd    env.grid
#define mgrd   env.mgrid
#define igrd   env.igrid

#define ENVF_FLAGS          0xFF00U
#define ENVF_DETECT_MONS    0x0100U
#define ENVF_DETECT_ITEM    0x0200U

// This square is known because of detect-creatures/detect-items
#define ENVF_DETECTED       0x0800U

#define ENVF_COLOR(x)       (((x) >> 12) & 0xF)

// (MNG) -- for a reason! see usage {dlb}:
#define MHITNOT 201
// (MNG + 1) -- for a reason! see usage {dlb}:
#define MHITYOU 202

// colors, such pretty colors ...
#ifndef DOS
    #define BLACK 0
    #define BLUE 1
    #define GREEN 2
    #define CYAN 3
    #define RED 4
    #define MAGENTA 5
    #define BROWN 6
    #define LIGHTGREY 7
    #define DARKGREY 8
    #define LIGHTBLUE 9
    #define LIGHTGREEN 10
    #define LIGHTCYAN 11
    #define LIGHTRED 12
    #define LIGHTMAGENTA 13
    #define YELLOW 14
    #define WHITE 15

    #define LIGHTGRAY LIGHTGREY
    #define DARKGRAY DARKGREY
#else
    #include <conio.h>
    #define LIGHTGREY LIGHTGRAY
    #define DARKGREY DARKGRAY
#endif

// Colour options... these are used as bit flags along with the colour
// value in the low byte.

// This is used to signal curses (which has seven base colours) to
// try to get a brighter version using recommisioned attribute flags.
#define COLFLAG_CURSES_BRIGHTEN          0x0080

//#ifdef USE_COLOUR_OPTS

    #define COLFLAG_FRIENDLY_MONSTER         0x0100
    #define COLFLAG_ITEM_HEAP                0x0200
    #define COLFLAG_WILLSTAB                 0x0400
    #define COLFLAG_MAYSTAB                  0x0800

    enum CHAR_ATTRIBUTES
    {
        CHATTR_NORMAL,          /* 0 */
        CHATTR_STANDOUT,
        CHATTR_BOLD,
        CHATTR_BLINK,
        CHATTR_UNDERLINE,
        CHATTR_REVERSE,         /* 5 */
        CHATTR_DIM,
        CHATTR_HILITE,
        
        CHATTR_ATTRMASK = 0xF,  /* 15 (well, obviously) */

        CHATTR_COLMASK = 0xF00 // Mask with this to get extra colour info.
    };

//#endif

#define MINIMUM( xxx, yyy )     (((xxx) < (yyy)) ? (xxx) : (yyy))
#define MAXIMUM( xxx, yyy )     (((xxx) > (yyy)) ? (xxx) : (yyy))

// Convert capital letters into mystic numbers representing 
// CTRL sequences.  This is a macro because a lot of the type 
// it wants to be used in case labels.
#define CONTROL( xxx )          (xxx - 'A' + 1)

#endif
