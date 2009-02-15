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
 *  Copyright � 1999 Brian Robinson.  // Me?  How come?
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef DEFINES_H
#define DEFINES_H

#define NUM_MONSTER_SPELL_SLOTS  6

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
const int STR_REQ_THRESHOLD = 10;

// Max ghosts on a level.
const int MAX_GHOSTS = 10;

// max size of monter array {dlb}:
#define MAX_MONSTERS     350
// number of monster enchantments
#define NUM_MON_ENCHANTS 6
// non-monster for mgrd[][] -- (MNST + 1) {dlb}:
#define NON_MONSTER (MAX_MONSTERS + 1)

// (MNG) -- for a reason! see usage {dlb}:
#define MHITNOT (MAX_MONSTERS + 1)
// (MNG + 1) -- for a reason! see usage {dlb}:
#define MHITYOU (MAX_MONSTERS + 2)

#define MAX_SUBTYPES    50

// max size of item list {dlb}:
#define MAX_ITEMS 500
// non-item -- (ITEMS + 1) {dlb}
#define NON_ITEM 501

// max size of cloud array {dlb}:
#define MAX_CLOUDS 180

// empty cloud -- (CLOUDS + 1) {dlb}:
#define EMPTY_CLOUD (MAX_CLOUDS + 1)

// max x-bound for level generation {dlb}
#define GXM 80
// max y-bound for level generation {dlb}
#define GYM 70

const int INFINITE_DISTANCE = 30000;

// this is the size of the border around the playing area (see in_bounds())
#define BOUNDARY_BORDER         1

// This is the border that must be left around the map. I'm not sure why it's
// necessary, beyond hysterical raisins.
const int MAPGEN_BORDER    = 2;

const int LABYRINTH_BORDER = 4;

// Now some defines about the actual play area:
// Note: these boundaries are exclusive for the zone the player can move/dig,
// and are inclusive for the area that we display on the map.
// Note: that the right (bottom) boundary is one smaller here.
#define X_BOUND_1               (-1 + BOUNDARY_BORDER)
#define X_BOUND_2               (GXM - BOUNDARY_BORDER)
#define X_WIDTH                 (X_BOUND_2 - X_BOUND_1 + 1)

#define Y_BOUND_1               (-1 + BOUNDARY_BORDER)
#define Y_BOUND_2               (GYM - BOUNDARY_BORDER)
#define Y_WIDTH                 (Y_BOUND_2 - Y_BOUND_1 + 1)

// these mark the center zone where the player moves without shifting
#define ABYSS_SHIFT_RADIUS      10

#define X_ABYSS_1               (X_BOUND_1 + ABYSS_SHIFT_RADIUS)
#define X_ABYSS_2               (GXM - X_ABYSS_1)
#define X_ABYSS_WIDTH           (X_ABYSS_2 - X_ABYSS_1 + 1)
#define X_ABYSS_CENTER          (X_ABYSS_1 + X_ABYSS_WIDTH / 2)

#define Y_ABYSS_1               (Y_BOUND_1 + ABYSS_SHIFT_RADIUS)
#define Y_ABYSS_2               (GYM - Y_ABYSS_1)
#define Y_ABYSS_WIDTH           (Y_ABYSS_2 - Y_ABYSS_1 + 1)
#define Y_ABYSS_CENTER          (Y_ABYSS_1 + Y_ABYSS_WIDTH / 2)

#define LOS_RADIUS 8
#define ENV_SHOW_OFFSET (LOS_RADIUS + 1)
#define ENV_SHOW_DIAMETER (ENV_SHOW_OFFSET * 2 + 1)

#define VIEW_BASE_WIDTH 33      // FIXME: never used
#define VIEW_MIN_WIDTH  17
#define VIEW_MIN_HEIGHT 17
#define MSG_MIN_HEIGHT  7

// max traps per level
#define MAX_TRAPS 100

// max shops per level
#define MAX_SHOPS         15

// max shops randomly generated in a level.
#define MAX_RANDOM_SHOPS  5

// Can be passed to monster_die to indicate that a friendly did the killing.
const int ANON_FRIENDLY_MONSTER = -1999;

// This value is used to make test_hit checks always succeed
#define AUTOMATIC_HIT           1500

// Yes, I know we have 32-bit ints now.
const int DEBUG_COOKIE = 32767;

const int MAX_SKILL_LEVEL = 27;

const int MIN_HIT_MISS_PERCENTAGE = 5;

// grids that monsters can see
const int MONSTER_LOS_RANGE = 8;

// Maximum charge level for rods
const int MAX_ROD_CHARGE  = 17;
const int ROD_CHARGE_MULT = 100;

const int GOURMAND_MAX            = 200;
const int GOURMAND_NUTRITION_BASE = 10;

const int CHUNK_BASE_NUTRITION    = 1000;

// The maximum number of abilities any god can have
#define MAX_GOD_ABILITIES               5

// This value is used to mark immune levels of MR
const int MAG_IMMUNE = 5000;

// This is the damage amount used to signal insta-death
const int INSTANT_DEATH = -9999;

// Maximum enchantment on weapons/armour/secondary armours
// This is the same as for ammunition.
const int MAX_WPN_ENCHANT = 9;

// Note: use armour_max_enchant(item) to get the correct limit for item
const int MAX_ARM_ENCHANT = 8;
const int MAX_SEC_ENCHANT = 2;

// some shortcuts:
#define menv   env.mons
#define mitm   env.item
#define grd    env.grid
#define mgrd   env.mgrid
#define igrd   env.igrid

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

#define MAX_TERM_COLOUR 16

// Colour options... these are used as bit flags along with the colour
// value in the low byte.

// This is used to signal curses (which has seven base colours) to
// try to get a brighter version using recommisioned attribute flags.
#define COLFLAG_CURSES_BRIGHTEN          0x0080

//#ifdef USE_COLOUR_OPTS

    #define COLFLAG_FRIENDLY_MONSTER         0x0100
    #define COLFLAG_NEUTRAL_MONSTER          0x0200
    #define COLFLAG_WILLSTAB                 0x0400
    #define COLFLAG_MAYSTAB                  0x0800
    #define COLFLAG_ITEM_HEAP                0x1000
    #define COLFLAG_FEATURE_ITEM             0x2000
    #define COLFLAG_TRAP_ITEM                0x4000
    #define COLFLAG_REVERSE                  0x8000
    #define COLFLAG_MASK                     0xFF00

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

#define PDESCS(colour) (colour)
#define PDESCQ(qualifier, colour) (((qualifier) * PDC_NCOLOURS) + (colour))

#define PCOLOUR(desc) ((desc) % PDC_NCOLOURS)
#define PQUAL(desc)   ((desc) / PDC_NCOLOURS)

// Convert capital letters into mystic numbers representing
// CTRL sequences.  This is a macro because a lot of the type
// it wants to be used in case labels.
#define CONTROL( xxx )          ((xxx) - 'A' + 1)

#define ARRAYSZ(x) (sizeof(x) / sizeof(x[0]))
#define RANDOM_ELEMENT(x) (x[random2(ARRAYSZ(x))])

const char * const MONSTER_NUMBER = "monster-number";

// Synthetic keys:
#define KEY_MACRO_MORE_PROTECT -10
#define KEY_MACRO_DISABLE_MORE -1
#define KEY_MACRO_ENABLE_MORE  -2
#define KEY_REPEAT_KEYS        -3

// cgotoxy regions
enum GotoRegion
{
    GOTO_CRT,  // cprintf > crt
    GOTO_MSG,  // cprintf > message
    GOTO_STAT, // cprintf > character status
    GOTO_DNGN, // cprintf > dungeon screen
    GOTO_MLIST,// cprintf > monster list
    GOTO_LAST  // cprintf > last active region or CRT, if none
};

// Mouse modes (for tiles)
enum mouse_mode
{
     MOUSE_MODE_NORMAL,
     MOUSE_MODE_COMMAND,
     MOUSE_MODE_TARGET,
     MOUSE_MODE_TARGET_DIR,
     MOUSE_MODE_MORE,
     MOUSE_MODE_MACRO
};


#endif
