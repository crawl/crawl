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
 *
 *  Change History (most recent first):
 *
 *       <5>     april2009      Cha     added Zot Defense defines
 *       <4>     7/29/00        JDJ     Renamed MNG NON_MONSTER, MNST MAX_MONSTERS, ITEMS MAX_ITEMS,
 *                                      ING NON_ITEM, CLOUDS MAX_CLOUDS, CNG EMPTY_CLOUD, NTRAPS MAX_TRAPS.
 *       <3>     9/25/99        CDL     linuxlib -> liblinux
 *       <2>     6/17/99        BCR     indented and added header
 *       <1>     --/--/--       LRH     Created
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

// // length of a single zot defense cycle
#define CYCLE_LENGTH 200
// // peak size of a random spawn
#define SPAWN_SIZE 1
// // 
#define BOSS_MONSTER_EXTRA_POWER 5
// // number of waves to pass between bosses generated with a rune
#define FREQUENCY_OF_RUNES 9

// max size of inventory array {dlb}:
#define ENDOFPACK 52

// minimum value for strength required on armour and weapons
#define STR_REQ_THRESHOLD       10

// Max ghosts on a level.
#define MAX_GHOSTS   10

// max size of monter array {dlb}:
#define MAX_MONSTERS     500
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
#define MAX_ITEMS 2000   // //
// non-item -- (ITEMS + 1) {dlb}
#define NON_ITEM 2001    // //

// max size of cloud array {dlb}:
#define MAX_CLOUDS 180

// empty cloud -- (CLOUDS + 1) {dlb}:
#define EMPTY_CLOUD (MAX_CLOUDS + 1)

// max x-bound for level generation {dlb}
#define GXM 80
// max y-bound for level generation {dlb}
#define GYM 70

#define INFINITE_DISTANCE       30000

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

#ifdef USE_TILE
#define VIEW_BASE_WIDTH (tile_dngn_x)
#define VIEW_MIN_WIDTH (tile_dngn_x)
#define VIEW_MIN_HEIGHT (tile_dngn_y)
#else
#define VIEW_BASE_WIDTH 33
#define VIEW_MIN_WIDTH  17
#define VIEW_MIN_HEIGHT 17
#endif

// max traps per level
#define MAX_TRAPS 100

// max shops per level
#define MAX_SHOPS         15

// max shops randomly generated in a level.
#define MAX_RANDOM_SHOPS  5

// Can be passed to monster_die to indicate that a friendly did the killing.
#define ANON_FRIENDLY_MONSTER   -1999

// This value is used to make test_hit checks always succeed
#define AUTOMATIC_HIT           1500

// Yes, I know we have 32-bit ints now.
#define DEBUG_COOKIE            32767

#define MAX_SKILL_LEVEL 27

#define MIN_HIT_MISS_PERCENTAGE  5

// grids that monsters can see
#define MONSTER_LOS_RANGE       8

// Maximum charge level for rods
#define MAX_ROD_CHARGE                  17
#define ROD_CHARGE_MULT                 100

// Should never exceed 255 - durations are saved as single bytes.
#define GOURMAND_MAX                    200
#define GOURMAND_NUTRITION_BASE         10

#define CHUNK_BASE_NUTRITION            1000

// The maximum number of abilities any god can have
#define MAX_GOD_ABILITIES               5

// This value is used to mark immune levels of MR
#define MAG_IMMUNE                      5000

// This is the damage amount used to signal insta-death
#define INSTANT_DEATH                   -9999

// grids that monsters can see
#define MONSTER_LOS_RANGE               8

// most items allowed in a shop
#define MAX_SHOP_ITEMS                  16

// sound level standards
// mininum is the base, we add mult * radius to it:
#define SL_EXPLODE_MIN                  10
#define SL_EXPLODE_MULT                 10

// #define SL_BOW                          3
#define SL_TRAP_CLICK                   3
#define SL_HISS                         6
#define SL_BUZZ                         6
#define SL_GROWL                        8
#define SL_MOAN                         8
#define SL_SPLASH                       8
#define SL_CREAK                        8
#define SL_CROAK                        8
#define SL_BARK                         10
#define SL_YELL                         10
#define SL_TRAP_JAM                     12
#define SL_SHRIEK                       12
#define SL_ROAR                         15
#define SL_DIG                          15
#define SL_NOISY_WEAPON                 20
#define SL_HORN                         25
#define SL_NOISE_SCROLL                 30
#define SL_THUNDER                      30
#define SL_PROJECTED_NOISE              30
#define SL_EARTHQUAKE                   30
#define SL_TRAP_ZOT                     30

// Maximum enchantment on weapons/armour/secondary armours
// Note: use armour_max_enchant(item) to get the correct limit for item
#define MAX_WPN_ENCHANT                 5
#define MAX_ARM_ENCHANT                 5
#define MAX_SEC_ENCHANT                 2

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

#define MINIMUM( xxx, yyy )     (((xxx) < (yyy)) ? (xxx) : (yyy))
#define MAXIMUM( xxx, yyy )     (((xxx) > (yyy)) ? (xxx) : (yyy))

// Convert capital letters into mystic numbers representing
// CTRL sequences.  This is a macro because a lot of the type
// it wants to be used in case labels.
#define CONTROL( xxx )          ((xxx) - 'A' + 1)

#define ARRAYSZ(x) (sizeof(x) / sizeof(x[0]))
#define RANDOM_ELEMENT(x) (x[random2(ARRAYSZ(x))])

#define MIN(x, y) MINIMUM(x, y)
#define MAX(x,y) (((x) > (y)) ? (x) : (y))

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
enum MouseMode
{
     MOUSE_MODE_NORMAL,
     MOUSE_MODE_COMMAND,
     MOUSE_MODE_TARGET,
     MOUSE_MODE_TARGET_DIR,
     MOUSE_MODE_MORE,
     MOUSE_MODE_MACRO
};


#endif
