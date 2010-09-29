/*
 *  File:       defines.h
 *  Summary:    Various definess used by Crawl.
 *  Written by: Linley Henzel
 *
 *      Abstract:       A variety of miscellaneous constant values are found here.
 *
 *  Copyright © 1999 Brian Robinson.  // Me?  How come?
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

#define NON_ENTITY 27000

enum extra_monster_index_type
{
    MAX_MONSTERS = 700,         // max size of monster array {dlb}
    NON_MONSTER  = NON_ENTITY,  // no monster

    MHITNOT = NON_MONSTER,
    MHITYOU,

    ZOT_TRAP_MISCAST,
    WIELD_MISCAST,
    MELEE_MISCAST,
    MISC_KNOWN_MISCAST,
    MISC_UNKNOWN_MISCAST,
};

// number of monster enchantments
#define NUM_MON_ENCHANTS 6

// size of Pan monster sets
#define MAX_MONS_ALLOC 10

#define MAX_SUBTYPES    50

// max size of item list {dlb}:
#define MAX_ITEMS 1000
// non-item -- (ITEMS + 1) {dlb}
#define NON_ITEM  NON_ENTITY

#if NON_ITEM <= MAX_ITEMS
#error NON_ITEM must be > MAX_ITEMS
#endif

// max size of cloud array {dlb}:
#define MAX_CLOUDS 600

// empty cloud -- (CLOUDS + 1) {dlb}:
#define EMPTY_CLOUD NON_ENTITY

#if EMPTY_CLOUD <= MAX_CLOUDS
#error EMPTY_CLOUD must be > MAX_CLOUDS
#endif

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

// default LOS radius
#define LOS_RADIUS 8
// default LOS radius squared, for comparison with distance()
#define LOS_RADIUS_SQ (LOS_RADIUS * LOS_RADIUS + 1)
// maximal LOS radius
#define LOS_MAX_RADIUS LOS_RADIUS
#define LOS_MAX_RADIUS_SQ (LOS_MAX_RADIUS * LOS_MAX_RADIUS + 1)
// maximal horizontal or vertical LOS range:
//   a quadrant needs to fit inside an 2D array with
//     0 <= x, y <= LOS_MAX_RANGE
#define LOS_MAX_RANGE LOS_MAX_RADIUS
#define ENV_SHOW_OFFSET LOS_MAX_RANGE
#define ENV_SHOW_DIAMETER (ENV_SHOW_OFFSET * 2 + 1)

#define VIEW_BASE_WIDTH 33      // FIXME: never used
#define VIEW_MIN_WIDTH  ENV_SHOW_DIAMETER
#define VIEW_MIN_HEIGHT ENV_SHOW_DIAMETER
#define MSG_MIN_HEIGHT  5

// max traps per level
#define MAX_TRAPS         400

// max shops per level
#define MAX_SHOPS         100

// max shops randomly generated in a level.
// changing this affects the total number of shops in a game
#define MAX_RANDOM_SHOPS  5

// Can be passed to monster_die to indicate that a friendly did the killing.
const int ANON_FRIENDLY_MONSTER = -1999;

// This value is used to make test_hit checks always succeed
#define AUTOMATIC_HIT           1500

// Yes, I know we have 32-bit ints now.
const int DEBUG_COOKIE = 32767;

const int MAX_SKILL_LEVEL = 27;
const int MAX_EXP_TOTAL = 8999999;
const int MAX_EXP_POOL = 20000;
const int FULL_EXP_POOL = MAX_EXP_POOL;

const int MIN_HIT_MISS_PERCENTAGE = 5;

// grids that monsters can see
const int MONSTER_LOS_RANGE = LOS_RADIUS;

// Maximum charge level for rods
const int MAX_ROD_CHARGE  = 17;
const int ROD_CHARGE_MULT = 100;

const int BASELINE_DELAY  = 10;
const int GOURMAND_MAX            = 200 * BASELINE_DELAY;
const int GOURMAND_NUTRITION_BASE = 10  * BASELINE_DELAY;

const int CHUNK_BASE_NUTRITION    = 1000;

const int ICEMAIL_MAX  = 10;
const int ICEMAIL_TIME = 300 * BASELINE_DELAY;

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

// The time (in aut) for a staff of power to decay 1 mp.
#define POWER_DECAY 50

const int MAX_KNOWN_SPELLS = 21;

const int INVALID_ABSDEPTH = -1000;

const int DEPTH_ABYSS = 51;
const int DEPTH_PAN   = 52;

const int BRANCH_DUNGEON_DEPTH = 27;

// Size of unique_items in player class
#define MAX_UNRANDARTS 100

// some shortcuts:
#define menv   env.mons
#define mitm   env.item
#define grd    env.grid
#define mgrd   env.mgrid
#define igrd   env.igrid

// colors, such pretty colors ...
#ifndef TARGET_OS_DOS
// The order is important (IRGB bit patterns).
enum COLORS
{
    BLACK,
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    BROWN,
    LIGHTGRAY,
    LIGHTGREY = LIGHTGRAY,
    DARKGRAY,
    DARKGREY = DARKGRAY,
    LIGHTBLUE,
    LIGHTGREEN,
    LIGHTCYAN,
    LIGHTRED,
    LIGHTMAGENTA,
    YELLOW,
    WHITE,
    MAX_TERM_COLOUR
};
#else
# include <conio.h>
# define LIGHTGREY LIGHTGRAY
# define DARKGREY DARKGRAY
# define MAX_TERM_COLOUR 16
#endif

// Colour options... these are used as bit flags along with the colour
// value in the low byte.

// This is used to signal curses (which has seven base colours) to
// try to get a brighter version using recommisioned attribute flags.
#define COLFLAG_CURSES_BRIGHTEN          0x0080

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

    CHATTR_COLMASK = 0xF00, // Mask with this to get extra colour info.
};

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
const char * const CORPSE_NEVER_DECAYS = "corpse-no-decay";

// Synthetic keys:
#define KEY_MACRO_MORE_PROTECT -10
#define KEY_MACRO_DISABLE_MORE -1
#define KEY_MACRO_ENABLE_MORE  -2

// cgotoxy regions
enum GotoRegion
{
    GOTO_CRT,  // cprintf > crt
    GOTO_MSG,  // cprintf > message
    GOTO_STAT, // cprintf > character status
    GOTO_DNGN, // cprintf > dungeon screen
    GOTO_MLIST,// cprintf > monster list
};

// Mouse modes (for tiles)
enum mouse_mode
{
    MOUSE_MODE_NORMAL,
    MOUSE_MODE_COMMAND,
    MOUSE_MODE_TARGET,
    MOUSE_MODE_TARGET_DIR,
    MOUSE_MODE_TARGET_PATH,
    MOUSE_MODE_MORE,
    MOUSE_MODE_MACRO,
    MOUSE_MODE_MAX,
};

#define PI 3.14159265359f

#endif
