/**
 * @file
 * @brief Various definess used by Crawl.
 *
 * A variety of miscellaneous constant values are found here.
**/

#pragma once

#include <cstdint>

#include "macros.h"

// In this case, an x86 CPU will use x87 math for floating point calculations,
// which uses 80 bit intermediate results, andleads to difference from the
// (much more common, in 2019) SSE-based calculations.
// probably far from the only case where seeding isn't reliable...
#if defined(TARGET_CPU_X86) && !defined(__SSE__)
#define SEEDING_UNRELIABLE
#endif

// Minimum terminal size allowed.
#define MIN_COLS  79
#define MIN_LINES 24

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

// Max ghosts in a bones file.
const int MAX_GHOSTS = 127;

enum extra_monster_index_type
{
    MAX_MONSTERS = 700,                  // max size of monster array {dlb}
    ANON_FRIENDLY_MONSTER = MAX_MONSTERS,// unknown/dead ally, for actor blaming
    YOU_FAULTLESS,                       // full xp but no penalty (reflection)
    NON_MONSTER  = 27000,                // no monster

    MHITNOT = NON_MONSTER,
    MHITYOU,
};

// number of monster attack specs
#define MAX_NUM_ATTACKS 4

// size of Pan monster sets
#define PAN_MONS_ALLOC 10
#define MAX_MONS_ALLOC 20

#define MAX_SUBTYPES   60

// max size of item list {dlb}:
#define MAX_ITEMS 2000
// non-item -- (ITEMS + 1) {dlb}
#define NON_ITEM 27000
#define ITEM_IN_INVENTORY (coord_def(-1, -1))
#define ITEM_IN_MONSTER_INVENTORY (coord_def(-2, -2))
#define ITEM_IN_SHOP 32767
// NON_ITEM + mindex + 1 is used as the item link for monster inventory;
// make sure we're not colliding with that.
COMPILE_CHECK(ITEM_IN_SHOP > NON_ITEM + MAX_MONSTERS);

#if NON_ITEM <= MAX_ITEMS
#error NON_ITEM must be > MAX_ITEMS
#endif

// max x-bound for level generation {dlb}
#define GXM 80
// max y-bound for level generation {dlb}
#define GYM 70

const int INFINITE_DISTANCE = 30000;
// max distance on a map
#define GDM 105

// this is the size of the border around the playing area (see in_bounds())
#define BOUNDARY_BORDER         1

// This is the border that must be left around the map. I'm not sure why it's
// necessary, beyond hysterical raisins.
const int MAPGEN_BORDER    = 2;

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

// maximal LOS radius.
// XXX: uses of this should be replaced depending on the intended behaviour,
// with LOS_DEFAULT_RANGE or LOS_MAX_RANGE or possibly you.current_vision
#define LOS_RADIUS 8
// LOS radius for 'normal' characters
#define LOS_DEFAULT_RANGE 7

// maximal horizontal or vertical LOS range:
//   a quadrant needs to fit inside an 2D array with
//     0 <= x, y <= LOS_MAX_RANGE
#define LOS_MAX_RANGE LOS_RADIUS
#define ENV_SHOW_OFFSET LOS_MAX_RANGE
#define ENV_SHOW_DIAMETER (ENV_SHOW_OFFSET * 2 + 1)

#define VIEW_BASE_WIDTH 33
#define VIEW_MIN_WIDTH  ENV_SHOW_DIAMETER
#define VIEW_MIN_HEIGHT ENV_SHOW_DIAMETER
#define MSG_MIN_HEIGHT  5

// max shops randomly generated in a level.
// changing this affects the total number of shops in a game
#define MAX_RANDOM_SHOPS  5

// range of overflow temples
#define MIN_OVERFLOW_LEVEL 3
#define MAX_OVERFLOW_LEVEL 10

#define MAX_BRANCH_DEPTH 27
COMPILE_CHECK(MAX_BRANCH_DEPTH < 256); // 8 bits

// This value is used to make test_hit checks always succeed
#define AUTOMATIC_HIT           1500

const int MAX_SKILL_LEVEL = 27;
const int MAX_EXP_TOTAL = 8999999;
const int EXERCISE_QUEUE_SIZE = 100;

const int MIN_HIT_MISS_PERCENTAGE = 5;

const int BASELINE_DELAY  = 10;

const int ICEMAIL_MAX  = 8;
const int ICEMAIL_TIME = 30 * BASELINE_DELAY;

// This value is used to mark immune levels of WL
const int WILL_INVULN = 5000;

// This is the damage amount used to signal insta-death
const int INSTANT_DEATH = -9999;

// Maximum enchantment on weapons/secondary armours
// Note: use armour_max_enchant(item) to get the correct limit for item
const int MAX_WPN_ENCHANT = 9;
const int MAX_SEC_ENCHANT = 2;

// formula for MP from a potion of magic
#define POT_MAGIC_MP (10 + random2avg(28, 3))

const int MAX_KNOWN_SPELLS = 21;

const int INVALID_ABSDEPTH = -1000;

const int UNUSABLE_SKILL = -99;

//#define DEBUG_MIMIC
#ifdef DEBUG_MIMIC
  #define FEATURE_MIMIC_CHANCE 1
#else
  #define FEATURE_MIMIC_CHANCE 100
#endif

const int AGILITY_BONUS = 5;

#define POLAR_VORTEX_RADIUS 5

#define ZOT_ENTRY_RUNES 3

// Size of unique_items in player class
#define MAX_UNRANDARTS 150

// Haste/slow boost.
#define haste_mul(x) div_rand_round((x) * 3, 2)
#define haste_div(x) div_rand_round((x) * 2, 3)
#define berserk_mul(x) div_rand_round((x) * 3, 2)
#define berserk_div(x) div_rand_round((x) * 2, 3)

#define MAX_MONSTER_HP 10000

// colours, such pretty colours ...
// The order is important (IRGB bit patterns).
enum COLOURS
{
    COLOUR_INHERIT = -1,
    BLACK,
    COLOUR_UNDEF = BLACK,
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
    NUM_TERM_COLOURS
};

// Many, MANY places currently hard-code this to 8 bits, but we need to
// expand it. Please use colour_t in new code.
typedef uint8_t colour_t;

// Colour options... these are used as bit flags along with the colour
// value in the low byte.

// This is used to signal curses (which has seven base colours) to
// try to get a brighter version using recommissioned attribute flags.
#define COLFLAG_CURSES_BRIGHTEN          0x0008

#define COLFLAG_FRIENDLY_MONSTER         0x0100
#define COLFLAG_NEUTRAL_MONSTER          0x0200
#define COLFLAG_UNUSUAL_MASK             0x0300
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
// CTRL sequences. This is a macro because a lot of the type
// it wants to be used in case labels.
#define CONTROL(xxx)          ((xxx) - 'A' + 1)
#define UNCONTROL(xxx)        ((xxx) + 'A' - 1)
// our standard CONTROL macros are defined relative to capital letters, but
// for SDL it is useful to be more general. For [a-z] these produce the same
// result as CONTROL on [A-Z].
// 'a' == SDLK_a
#define LC_CONTROL(x) (x - 'a' + 1)
#define LC_UNCONTROL(x) (x + 'a' - 1)

#define ARRAYSZ(x) (sizeof(x) / sizeof(x[0]))
#define RANDOM_ELEMENT(x) (x[random2(ARRAYSZ(x))])

// The following keys are defined here because that was simpler
// than finding a common header for them. Please define any future
// keys in a more tightly scoped header.

const char * const MONSTER_HIT_DICE = "monster-hit-dice";
const char * const CORPSE_HEADS = "monster-number";
const char * const CORPSE_NEVER_DECAYS = "corpse-no-decay";
const char * const MONSTER_MID = "monster-mid";

const char * const NEUTRAL_BRIBE_KEY         = "gozag_bribed";
const char * const FRIENDLY_BRIBE_KEY        = "gozag_permabribed";

const char * const THUNDERBOLT_CHARGES_KEY = "thunderbolt_charges";
const char * const THUNDERBOLT_LAST_KEY    = "thunderbolt_last";
const char * const THUNDERBOLT_AIM_KEY     = "thunderbolt_aim";

#define REAPING_DAMAGE_KEY "reaping_damage"
#define REAPER_KEY "reaper"
#define BAND_LEADER_KEY "band_leader"
#define ZIN_ID_KEY "zin_id"
#define BLAME_KEY "blame"
#define NO_ANNOTATE_KEY "no_annotate"
#define BENNU_REVIVES_KEY "bennu_revives"
#define FELID_REVIVES_KEY "felid_revives"
#define SPECTRAL_WEAPON_KEY "spectral_weapon"
#define FAKE_MON_KEY "fake"
#define MMOV_KEY "mmov"
#define BATTLESPHERE_KEY "battlesphere"
#define BATTLESPHERE_FOE_KEY "bs_foe"
#define BATTLESPHERE_IS_FIRING_KEY "bs_firing"
#define BATTLESPHERE_IS_TRACKING_KEY "bs_tracking"
#define TRACKING_TARGET_KEY "tracking_target"
#define FOE_APPROACHING_KEY "foe_approaching"
#define FAUX_PAS_KEY "foe_pos"
#define SWOOP_COOLDOWN_KEY "swoop_cooldown"
#define VINE_AWAKENER_KEY "vine_awakener"
#define VINES_AWAKENED_KEY "vines_awakened"
#define OUTWARDS_KEY "outwards"
#define INWARDS_KEY "inwards"
#define BASE_POSITION_KEY "base_position"
#define SUMMON_ID_KEY "summon_id"
#define FLAY_BLOOD_KEY "flay_blood"
#define IDEAL_RANGE_KEY "ideal_range"
#define LAST_POS_KEY "last_pos"
#define BLOCKED_DEADLINE_KEY "blocked_deadline"
#define BROTHERS_KEY "brothers_count"
#define OLD_HEADS_KEY "old_heads"
#define ELY_WRATH_HEALED_KEY "ely_wrath_healed"
#define CAN_CLIMB_KEY "can_climb"
#define SPEECH_PREFIX_KEY "speech_prefix"
#define MERFOLK_AVATAR_CALL_KEY "merfolk_avatar_call"
#define PIKEL_BAND_KEY "pikel_band"
#define KIRKE_BAND_KEY "kirke_band"
#define EMERGENCY_CLONE_KEY "emergency_clone"
#define BINDING_SIGIL_DURATION_KEY "binding_sigil_duration"
#define BULLSEYE_TARGET_KEY "bullseye_target"
#define BOULDER_DIRECTION_KEY "boulder_direction"
#define BOULDER_POWER_KEY "boulder_power"
#define PROTEAN_TARGET_KEY "protean_target"
#define PASSWALL_ARMOUR_KEY "passwall_armour"
#define SOUL_SPLINTERED_KEY "soul_splintered"

#define HELPLESS_KEY "helpless"
#define POISONER_KEY "poisoner"
#define POISON_AUX_KEY "poison_aux"
#define STICKY_FLAMER_KEY "sticky_flame_source"
#define STICKY_FLAME_AUX_KEY "sticky_flame_aux"
#define STICKY_FLAME_POWER_KEY "sticky_flame_pow"
#define WATER_HOLDER_KEY "water_holder"
#define WATER_HOLD_SUBSTANCE_KEY "water_hold_substance"
#define CORROSION_KEY "corrosion_amount"
#define CONFUSING_TOUCH_KEY "confusing touch power"
#define NUM_SACRIFICES_KEY "num_sacrifice_muts"
#define FLAY_DAMAGE_KEY "flay_damage"
#define POLAR_VORTEX_KEY "polar_vortex_since"
#define KILLED_BORIS_KEY "killed_boris_once"

#define FORCE_MINIVAULT_KEY "force_minivault"
#define FORCE_MAP_KEY "force_map"
#define DEBUG_BUILDER_LOGS_KEY "debug_builder_logs"

#define NEEDS_AUTOPICKUP_KEY "needs_autopickup"
#define CHARGES_KEY "charges"
#define PLUS_KEY "plus"
#define IDENT_KEY "ident"
#define USEFUL_KEY "useful"
#define UNOBTAINABLE_KEY "unobtainable"
#define NO_EXCLUDE_KEY "no_exclude"
#define NO_PICKUP_KEY "no_pickup"
#define DBNAME_KEY "dbname"
#define ITEM_TILE_NAME_KEY "item_tile_name"
#define WORN_TILE_NAME_KEY "worn_tile_name"
#define ITEM_TILE_KEY "item_tile"
#define WORN_TILE_KEY "worn_tile"
#define MIMIC_KEY "mimic"
#define THEME_BOOK_KEY "build_themed_book"

#define MONSTER_TILE_KEY "monster_tile"
#define MONSTER_TILE_NAME_KEY "monster_tile_name"
#define ALWAYS_CORPSE_KEY "always_corpse"

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
    MOUSE_MODE_PROMPT,
    MOUSE_MODE_YESNO,
    MOUSE_MODE_MAX,
};

const int DEFAULT_VIEW_DELAY = 600;

#define PI 3.14159265359f

#ifdef __ANDROID__
#define ANDROID_ASSETS "ANDROID_ASSETS"
#endif
