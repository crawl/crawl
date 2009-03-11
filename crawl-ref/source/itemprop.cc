/*
 *  File:       itemprop.h
 *  Summary:    Misc functions.
 *  Written by: Brent Ross
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "itemname.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef DOS
 #include <conio.h>
#endif

#include "externs.h"

#include "decks.h"
#include "food.h"
#include "invent.h"
#include "items.h"
#include "itemprop.h"
#include "it_use2.h"
#include "macro.h"
#include "mon-util.h"
#include "monstuff.h"
#include "notes.h"
#include "player.h"
#include "quiver.h"
#include "randart.h"
#include "skills2.h"
#include "stuff.h"
#include "transfor.h"
#include "view.h"
#include "xom.h"


// XXX: Name strings in most of the following are currently unused!
struct armour_def
{
    armour_type         id;
    const char         *name;
    int                 ac;
    int                 ev;
    int                 mass;

    bool                light;
    equipment_type      slot;
    size_type           fit_min;
    size_type           fit_max;
};

// Note: the Little-Giant range is used to make armours which are very
// flexible and adjustable and can be worn by any player character...
// providing they also pass the shape test, of course.
static int Armour_index[NUM_ARMOURS];
static armour_def Armour_prop[NUM_ARMOURS] =
{
    { ARM_ANIMAL_SKIN,          "animal skin",            2,  0,  100,
        true,  EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_ROBE,                 "robe",                   2,  0,   60,
        true,  EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_BIG },
    { ARM_LEATHER_ARMOUR,       "leather armour",         3, -1,  150,
        true,  EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM },

    { ARM_RING_MAIL,            "ring mail",              4, -2,  250,
        false, EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM },
    { ARM_SCALE_MAIL,           "scale mail",             5, -3,  350,
        false, EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM },
    { ARM_CHAIN_MAIL,           "chain mail",             6, -4,  400,
        false, EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM },
    { ARM_BANDED_MAIL,          "banded mail",            7, -5,  500,
        false, EQ_BODY_ARMOUR, SIZE_MEDIUM, SIZE_MEDIUM },
    { ARM_SPLINT_MAIL,          "splint mail",            8, -5,  550,
        false, EQ_BODY_ARMOUR, SIZE_MEDIUM, SIZE_MEDIUM },
    { ARM_PLATE_MAIL,           "plate mail",            10, -6,  650,
        false, EQ_BODY_ARMOUR, SIZE_MEDIUM, SIZE_MEDIUM },
    { ARM_CRYSTAL_PLATE_MAIL,   "crystal plate mail",    14, -8, 1200,
        false, EQ_BODY_ARMOUR, SIZE_MEDIUM, SIZE_MEDIUM },

    { ARM_TROLL_HIDE,           "troll hide",             2, -1,  220,
        true,  EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_TROLL_LEATHER_ARMOUR, "troll leather armour",   4, -1,  220,
        true,  EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_STEAM_DRAGON_HIDE,    "steam dragon hide",      2,  0,  120,
        true,  EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_STEAM_DRAGON_ARMOUR,  "steam dragon armour",    4,  0,  120,
        true,  EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_MOTTLED_DRAGON_HIDE,  "mottled dragon hide",    3, -1,  150,
        true,  EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_MOTTLED_DRAGON_ARMOUR,"mottled dragon armour",  5, -1,  150,
        true,  EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },

    { ARM_SWAMP_DRAGON_HIDE,    "swamp dragon hide",      3, -2,  200,
        false, EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_SWAMP_DRAGON_ARMOUR,  "swamp dragon armour",    7, -2,  200,
        false, EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_DRAGON_HIDE,          "dragon hide",            3, -3,  350,
        false, EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_DRAGON_ARMOUR,        "dragon armour",          8, -3,  350,
        false, EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_ICE_DRAGON_HIDE,      "ice dragon hide",        4, -3,  350,
        false, EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_ICE_DRAGON_ARMOUR,    "ice dragon armour",      9, -3,  350,
        false, EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_STORM_DRAGON_HIDE,    "storm dragon hide",      4, -4,  600,
        false, EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_STORM_DRAGON_ARMOUR,  "storm dragon armour",    10, -5,  600,
        false, EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_GOLD_DRAGON_HIDE,     "gold dragon hide",       5, -5, 1100,
        false, EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_GOLD_DRAGON_ARMOUR,   "gold dragon armour",   13, -9, 1100,
        false, EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },

    { ARM_CLOAK,                "cloak",                  1,  0,   40,
        true,  EQ_CLOAK,       SIZE_LITTLE, SIZE_BIG },
    { ARM_GLOVES,               "gloves",                 1,  0,   20,
        true,  EQ_GLOVES,      SIZE_SMALL,  SIZE_MEDIUM },

    { ARM_HELMET,               "helmet",                 1,  0,   80,
        false, EQ_HELMET,      SIZE_SMALL,  SIZE_MEDIUM },

    { ARM_CAP,                  "cap",                    0,  0,   40,
        true,  EQ_HELMET,      SIZE_LITTLE, SIZE_LARGE },

    { ARM_WIZARD_HAT,           "wizard hat",             0,  0,   40,
        true,  EQ_HELMET,      SIZE_LITTLE, SIZE_LARGE },

    // Note that barding size is compared against torso so it currently
    // needs to fit medium, but that doesn't matter as much as race
    // and shapeshift status.
    { ARM_BOOTS,                "boots",                  1,  0,   30,
        true,  EQ_BOOTS,       SIZE_SMALL,  SIZE_MEDIUM },
    // Changed max. barding size to large to allow for the appropriate
    // monster types (monsters don't differentiate between torso and general).
    { ARM_CENTAUR_BARDING,      "centaur barding",        4, -2,  100,
        true,  EQ_BOOTS,       SIZE_MEDIUM, SIZE_LARGE },
    { ARM_NAGA_BARDING,         "naga barding",           4, -2,  100,
        true,  EQ_BOOTS,       SIZE_MEDIUM, SIZE_LARGE },

    // Note: shields use ac-value as sh-value, EV pen is used for heavy_shield.
    { ARM_BUCKLER,              "buckler",                3,  0,   90,
        true,  EQ_SHIELD,      SIZE_LITTLE, SIZE_MEDIUM },
    { ARM_SHIELD,               "shield",                 5, -1,  150,
        false, EQ_SHIELD,      SIZE_SMALL,  SIZE_BIG    },
    { ARM_LARGE_SHIELD,         "large shield",           7, -2,  230,
        false, EQ_SHIELD,      SIZE_MEDIUM, SIZE_GIANT  },
};

struct weapon_def
{
    int                 id;
    const char         *name;
    int                 dam;
    int                 hit;
    int                 speed;
    int                 mass;
    int                 str_weight;

    skill_type          skill;
    hands_reqd_type     hands;
    size_type           fit_size;     // actual size is one size smaller
    missile_type        ammo;         // MI_NONE for non-launchers
    bool                throwable;

    int                 dam_type;
    int                 acquire_weight;
};

static int Weapon_index[NUM_WEAPONS];
static weapon_def Weapon_prop[NUM_WEAPONS] =
{
    // Maces & Flails
    { WPN_WHIP,              "whip",                4,  2, 13,  30,  2,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLASHING, 0 },
    { WPN_CLUB,              "club",                5,  3, 13,  50,  7,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_SMALL,  MI_NONE, true,
        DAMV_CRUSHING, 0 },
    { WPN_HAMMER,            "hammer",              7,  3, 13,  90,  7,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_CRUSHING, 0 },
    { WPN_MACE,              "mace",                8,  3, 14, 120,  8,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_CRUSHING, 10 },
    { WPN_FLAIL,             "flail",               9,  2, 15, 130,  8,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_CRUSHING, 10 },
    { WPN_ANKUS,             "ankus",               9,  2, 14, 120,  8,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_CRUSHING, 10 },
    { WPN_MORNINGSTAR,       "morningstar",        10, -1, 15, 140,  8,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_PIERCING | DAM_BLUDGEON, 10 },
    { WPN_DEMON_WHIP,        "demon whip",         10,  1, 13,  30,  2,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLASHING, 2 },
    { WPN_SPIKED_FLAIL,      "spiked flail",       12, -2, 16, 190,  8,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_PIERCING | DAM_BLUDGEON, 10 },
    { WPN_EVENINGSTAR,       "eveningstar",        12, -1, 15, 180,  8,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_PIERCING | DAM_BLUDGEON, 2 },
    { WPN_DIRE_FLAIL,        "dire flail",         13, -3, 14, 240,  9,
        SK_MACES_FLAILS, HANDS_DOUBLE, SIZE_LARGE,  MI_NONE, false,
        DAMV_PIERCING | DAM_BLUDGEON, 10 },
    { WPN_GREAT_MACE,        "great mace",         16, -4, 18, 270,  9,
        SK_MACES_FLAILS, HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CRUSHING, 10 },
    { WPN_GIANT_CLUB,        "giant club",         18, -6, 19, 330, 10,
        SK_MACES_FLAILS, HANDS_TWO,    SIZE_BIG,    MI_NONE, false,
        DAMV_CRUSHING, 0 },
    { WPN_GIANT_SPIKED_CLUB, "giant spiked club",  20, -7, 20, 350, 10,
        SK_MACES_FLAILS, HANDS_TWO,    SIZE_BIG,    MI_NONE, false,
        DAMV_PIERCING | DAM_BLUDGEON, 0 },

    // Short blades
    { WPN_KNIFE,             "knife",               3,  5, 10,  10,  1,
        SK_SHORT_BLADES, HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_STABBING | DAM_SLICE, 0 },
    { WPN_DAGGER,            "dagger",              4,  6, 10,  20,  1,
        SK_SHORT_BLADES, HANDS_ONE,    SIZE_LITTLE, MI_NONE, true,
        DAMV_STABBING | DAM_SLICE, 10 },
    { WPN_QUICK_BLADE,       "quick blade",         5,  6,  7,  50,  0,
        SK_SHORT_BLADES, HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_STABBING | DAM_SLICE, 2 },
    { WPN_SHORT_SWORD,       "short sword",         6,  4, 11,  80,  2,
        SK_SHORT_BLADES, HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_SLICING | DAM_PIERCE, 10 },
    { WPN_SABRE,             "sabre",               7,  4, 12,  90,  2,
        SK_SHORT_BLADES, HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_SLICING | DAM_PIERCE, 10 },

    // Long blades
    { WPN_FALCHION,              "falchion",               8,  2, 13, 170,  4,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_SMALL, MI_NONE, false,
        DAMV_SLICING, 10 },      // or perhaps DAMV_CHOPPING is more apt?
    { WPN_BLESSED_FALCHION,      "blessed falchion",      10,  2, 11, 170,  4,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_SMALL, MI_NONE, false,
        DAMV_SLICING, 10 },      // or perhaps DAMV_CHOPPING is more apt?
    { WPN_LONG_SWORD,            "long sword",            10,  1, 14, 160,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 10 },
    { WPN_BLESSED_LONG_SWORD,    "blessed long sword",    12,  0, 13, 160,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 10 },
    { WPN_SCIMITAR,              "scimitar",              11, -1, 14, 170,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 10 },
    { WPN_BLESSED_SCIMITAR,      "blessed scimitar",      12, -1, 12, 170,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 10 },
    { WPN_KATANA,                "katana",                13,  2, 13, 160,  3,
        SK_LONG_BLADES,  HANDS_HALF,   SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 2 },
    { WPN_BLESSED_KATANA,        "blessed katana",        14,  1, 13, 160,  3,
        SK_LONG_BLADES,  HANDS_HALF,   SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 2 },
    { WPN_DEMON_BLADE,           "demon blade",           13, -1, 15, 200,  4,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 2 },
    { WPN_BLESSED_EUDEMON_BLADE, "blessed eudemon blade", 14, -2, 14, 200,  4,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 0 },
    { WPN_DOUBLE_SWORD,          "double sword",          15, -2, 16, 220,  5,
        SK_LONG_BLADES,  HANDS_HALF,   SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 2 },
    { WPN_BLESSED_DOUBLE_SWORD,  "blessed double sword",  15, -2, 15, 220,  5,
        SK_LONG_BLADES,  HANDS_HALF,   SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 2 },
    { WPN_GREAT_SWORD,           "great sword",           16, -3, 17, 250,  6,
        SK_LONG_BLADES,  HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 10 },
    { WPN_BLESSED_GREAT_SWORD,   "blessed great sword",   17, -5, 17, 250,  6,
        SK_LONG_BLADES,  HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 10 },
    { WPN_TRIPLE_SWORD,          "triple sword",          19, -4, 19, 260,  6,
        SK_LONG_BLADES,  HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 2 },
    { WPN_BLESSED_TRIPLE_SWORD,  "blessed triple sword",  19, -5, 18, 260,  6,
        SK_LONG_BLADES,  HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 2 },

    // Axes
    { WPN_HAND_AXE,          "hand axe",            7,  3, 13,  80,  6,
        SK_AXES,         HANDS_ONE,    SIZE_SMALL,  MI_NONE, true,
        DAMV_CHOPPING, 10 },
    { WPN_WAR_AXE,           "war axe",            11,  0, 16, 180,  7,
        SK_AXES,         HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_CHOPPING, 10 },
    { WPN_BROAD_AXE,         "broad axe",          14, -2, 17, 230,  8,
        SK_AXES,         HANDS_HALF,   SIZE_MEDIUM, MI_NONE, false,
        DAMV_CHOPPING, 10 },
    { WPN_BATTLEAXE,         "battleaxe",          17, -4, 18, 250,  8,
        SK_AXES,         HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CHOPPING, 10 },
    { WPN_EXECUTIONERS_AXE,  "executioner\'s axe", 20, -6, 20, 280,  9,
        SK_AXES,         HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CHOPPING, 2 },

    // Polearms
    { WPN_SPEAR,             "spear",               6,  3, 12,  50,  3,
        SK_POLEARMS,     HANDS_HALF,   SIZE_SMALL,  MI_NONE, true,
        DAMV_PIERCING, 10 },
    { WPN_TRIDENT,           "trident",             9,  2, 14, 160,  4,
        SK_POLEARMS,     HANDS_HALF,   SIZE_MEDIUM, MI_NONE, false,
        DAMV_PIERCING, 10 },
    { WPN_DEMON_TRIDENT,     "demon trident",      13,  0, 14, 160,  4,
        SK_POLEARMS,     HANDS_HALF,   SIZE_MEDIUM, MI_NONE, false,
        DAMV_PIERCING, 2 },
    { WPN_HALBERD,           "halberd",            13, -3, 16, 200,  5,
        SK_POLEARMS,     HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CHOPPING | DAM_PIERCE, 10 },
    { WPN_SCYTHE,            "scythe",             14, -4, 20, 220,  7,
        SK_POLEARMS,     HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 10 },
    { WPN_GLAIVE,            "glaive",             15, -3, 18, 200,  6,
        SK_POLEARMS,     HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CHOPPING, 10 },
    { WPN_BARDICHE,          "bardiche",           18, -6, 20, 200,  8,
        SK_POLEARMS,     HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CHOPPING, 2 },

    // Staves
    { WPN_QUARTERSTAFF,      "quarterstaff",        7,  6, 12, 180,  7,
        SK_STAVES,       HANDS_DOUBLE, SIZE_LARGE,  MI_NONE, false,
        DAMV_CRUSHING, 10 },
    { WPN_LAJATANG,          "lajatang",           14, -3, 14, 200,  3,
        SK_STAVES,       HANDS_DOUBLE, SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 2 },

    // Range weapons
    // Notes:
    // - HANDS_HALF means a reloading time penalty if using shield
    // - damage field is used for bonus strength damage (string tension)
    // - slings get a bonus from dex, not str (as tension is meaningless)
    // - str weight is used for speed and applying dex to skill
    { WPN_BLOWGUN,           "blowgun",             0,  2, 10,  20,  0,
        SK_DARTS,        HANDS_HALF,   SIZE_LITTLE, MI_NEEDLE, false,
        DAMV_NON_MELEE, 0 },
    { WPN_SLING,             "sling",               0,  2, 11,  20,  1,
        SK_SLINGS,       HANDS_HALF,   SIZE_LITTLE, MI_STONE, false,
        DAMV_NON_MELEE, 10 },
    { WPN_HAND_CROSSBOW,     "hand crossbow",       3,  4, 15,  70,  5,
        SK_CROSSBOWS,    HANDS_HALF,   SIZE_SMALL,  MI_DART, false,
        DAMV_NON_MELEE, 10 },
    { WPN_CROSSBOW,          "crossbow",            5,  4, 15, 150,  8,
        SK_CROSSBOWS,    HANDS_TWO,    SIZE_MEDIUM, MI_BOLT, false,
        DAMV_NON_MELEE, 10 },
    { WPN_BOW,               "bow",                 3,  1, 11,  90,  2,
        SK_BOWS,         HANDS_TWO,    SIZE_MEDIUM, MI_ARROW, false,
        DAMV_NON_MELEE, 10 },
    { WPN_LONGBOW,           "longbow",             6,  0, 12, 120,  3,
        SK_BOWS,         HANDS_TWO,    SIZE_LARGE,  MI_ARROW, false,
        DAMV_NON_MELEE, 10 },
};

struct missile_def
{
    int         id;
    const char *name;
    int         dam;
    int         mass;
    bool        throwable;
};

static int Missile_index[NUM_MISSILES];
static missile_def Missile_prop[NUM_MISSILES] =
{
    { MI_NEEDLE,        "needle",        0,    1, false },
    { MI_STONE,         "stone",         4,    2, true  },
    { MI_DART,          "dart",          5,    3, true  },
    { MI_ARROW,         "arrow",         7,    5, false },
    { MI_BOLT,          "bolt",          9,    5, false },
    { MI_LARGE_ROCK,    "large rock",   20,  600, true  },
    { MI_SLING_BULLET,  "sling bullet",  6,    4, false },
    { MI_JAVELIN,       "javelin",      10,   80, true  },
    { MI_THROWING_NET,  "throwing net",  0,   30, true  },
};

struct food_def
{
    int         id;
    const char *name;
    int         value;
    int         carn_mod;
    int         herb_mod;
    int         mass;
    int         turns;
};

// NOTE: Any food with special random messages or side effects
// currently only takes one turn to eat (except ghouls and chunks)...
// If this changes then those items will have to have special code
// (like ghoul chunks) to guarantee that the special thing is only
// done once.  See the ghoul eating code over in food.cc.
static int Food_index[NUM_FOODS];
static food_def Food_prop[NUM_FOODS] =
{
    { FOOD_MEAT_RATION,  "meat ration",  5000,   500, -1500,  80, 4 },
    { FOOD_SAUSAGE,      "sausage",      1500,   150,  -400,  40, 1 },
    { FOOD_CHUNK,        "chunk",        1000,   100,  -500, 100, 3 },
    { FOOD_BEEF_JERKY,   "beef jerky",    800,   100,  -250,  20, 1 },

    { FOOD_BREAD_RATION, "bread ration", 4400, -1500,   750,  80, 4 },
    { FOOD_SNOZZCUMBER,  "snozzcumber",  1500,  -500,   500,  50, 1 },
    { FOOD_ORANGE,       "orange",       1000,  -350,   400,  20, 1 },
    { FOOD_BANANA,       "banana",       1000,  -350,   400,  20, 1 },
    { FOOD_LEMON,        "lemon",        1000,  -350,   400,  20, 1 },
    { FOOD_PEAR,         "pear",          700,  -250,   300,  20, 1 },
    { FOOD_APPLE,        "apple",         700,  -250,   300,  20, 1 },
    { FOOD_APRICOT,      "apricot",       700,  -250,   300,  15, 1 },
    { FOOD_CHOKO,        "choko",         600,  -200,   250,  30, 1 },
    { FOOD_RAMBUTAN,     "rambutan",      600,  -200,   250,  10, 1 },
    { FOOD_LYCHEE,       "lychee",        600,  -200,   250,  10, 1 },
    { FOOD_STRAWBERRY,   "strawberry",    200,   -80,   100,   5, 1 },
    { FOOD_GRAPE,        "grape",         100,   -40,    50,   2, 1 },
    { FOOD_SULTANA,      "sultana",        70,   -30,    30,   1, 1 },

    { FOOD_ROYAL_JELLY,  "royal jelly",  4000,     0,     0,  55, 1 },
    { FOOD_HONEYCOMB,    "honeycomb",    2000,     0,     0,  40, 1 },
    { FOOD_PIZZA,        "pizza",        1500,     0,     0,  40, 1 },
    { FOOD_CHEESE,       "cheese",       1200,     0,     0,  40, 1 },
};

// Must call this functions early on so that the above tables can
// be accessed correctly.
void init_properties()
{
    // Compare with enum comments, to catch changes.
    COMPILE_CHECK(NUM_ARMOURS  == 37, c1);
    COMPILE_CHECK(NUM_WEAPONS  == 55, c2);
    COMPILE_CHECK(NUM_MISSILES ==  9, c3);
    COMPILE_CHECK(NUM_FOODS    == 22, c4);

    int i;

    for (i = 0; i < NUM_ARMOURS; i++)
        Armour_index[ Armour_prop[i].id ] = i;

    for (i = 0; i < NUM_WEAPONS; i++)
        Weapon_index[ Weapon_prop[i].id ] = i;

    for (i = 0; i < NUM_MISSILES; i++)
        Missile_index[ Missile_prop[i].id ] = i;

    for (i = 0; i < NUM_FOODS; i++)
        Food_index[ Food_prop[i].id ] = i;
}


// Some convenient functions to hide the bit operations and create
// an interface layer between the code and the data in case this
// gets changed again. -- bwr

//
// Item cursed status functions:
//
bool item_cursed( const item_def &item )
{
    return (item.flags & ISFLAG_CURSED);
}

bool item_known_cursed( const item_def &item )
{
    return ((item.flags & ISFLAG_KNOW_CURSE) && (item.flags & ISFLAG_CURSED));
}

bool item_known_uncursed( const item_def &item )
{
    return ((item.flags & ISFLAG_KNOW_CURSE) && !(item.flags & ISFLAG_CURSED));
}

void do_curse_item( item_def &item, bool quiet )
{
    // Already cursed?
    if (item.flags & ISFLAG_CURSED)
        return;

    // Xom is amused by the player's items being cursed, especially
    // if they're worn/equipped.
    if (in_inventory(item))
    {
        int amusement = 64;

        if (item_is_equipped(item))
        {
            amusement *= 2;

            // Cursed cloaks prevent you from removing body armour.
            if (item.base_type == OBJ_ARMOUR
                && get_armour_slot(item) == EQ_CLOAK)
            {
                amusement *= 2;
            }
            else if (you.equip[EQ_WEAPON] == item.link)
            {
                // Redraw the weapon.
                you.wield_change = true;
            }
        }
        xom_is_stimulated(amusement);
    }

    if (!quiet)
    {
        mprf("Your %s glows black for a moment.",
             item.name(DESC_PLAIN).c_str());
    }

    item.flags |= ISFLAG_CURSED;
}

void do_uncurse_item( item_def &item )
{
    if (in_inventory(item) && you.equip[EQ_WEAPON] == item.link)
    {
        // Redraw the weapon.
        you.wield_change = true;
    }
    item.flags &= (~ISFLAG_CURSED);
}

// Is item stationary (cannot be picked up)?
void set_item_stationary( item_def &item )
{
    if (item.base_type == OBJ_MISSILES && item.sub_type == MI_THROWING_NET)
        item.plus2 = 1;
}

void remove_item_stationary( item_def &item )
{
    if (item.base_type == OBJ_MISSILES && item.sub_type == MI_THROWING_NET)
        item.plus2 = 0;
}

bool item_is_stationary( const item_def &item )
{
    return (item.base_type == OBJ_MISSILES
            && item.sub_type == MI_THROWING_NET
            && item.plus2);
}

//
// Item identification status:
//
bool item_ident( const item_def &item, unsigned long flags )
{
    return ((item.flags & flags) == flags);
}

// The Orb of Zot and unique runes are considered critical.
bool item_is_critical(const item_def &item)
{
    if (!is_valid_item(item))
        return (false);

    if (item.base_type == OBJ_ORBS)
        return (true);

    return (item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_RUNE_OF_ZOT
            && item.plus != RUNE_DEMONIC
            && item.plus != RUNE_ABYSSAL);
}

// Is item something that no one would bother enchanting?
bool item_is_mundane(const item_def &item)
{
    bool retval = false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        retval = (item.sub_type == WPN_CLUB
                     || item.sub_type == WPN_GIANT_CLUB
                     || item.sub_type == WPN_GIANT_SPIKED_CLUB
                     || item.sub_type == WPN_KNIFE);
        break;
    default:
        break;
    }

    return (retval);
}

void set_ident_flags( item_def &item, unsigned long flags )
{
    preserve_quiver_slots p;
    if ((item.flags & flags) != flags)
    {
        item.flags |= flags;
        request_autoinscribe();
    }

    if (notes_are_active() && !(item.flags & ISFLAG_NOTED_ID)
        && get_ident_type(item) != ID_KNOWN_TYPE
        && fully_identified(item) && is_interesting_item(item))
    {
        // Make a note of it.
        take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_NOCAP_A).c_str(),
                       origin_desc(item).c_str()));

        // Sometimes (e.g. shops) you can ID an item before you get it;
        // don't note twice in those cases.
        item.flags |= (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
    }
}

void unset_ident_flags( item_def &item, unsigned long flags )
{
    preserve_quiver_slots p;
    item.flags &= (~flags);
}

// Returns the mask of interesting identify bits for this item
// (e.g., scrolls don't have know-cursedness).
unsigned long full_ident_mask( const item_def& item )
{
    unsigned long flagset = ISFLAG_IDENT_MASK;
    switch (item.base_type)
    {
    case OBJ_FOOD:
    case OBJ_CORPSES:
        flagset = 0;
        break;
    case OBJ_MISCELLANY:
        if (item.sub_type == MISC_RUNE_OF_ZOT)
            flagset = 0;
        else
            flagset = ISFLAG_KNOW_TYPE;
        break;
    case OBJ_BOOKS:
    case OBJ_ORBS:
    case OBJ_SCROLLS:
    case OBJ_POTIONS:
    case OBJ_STAVES:
        flagset = ISFLAG_KNOW_TYPE;
        break;
    case OBJ_WANDS:
        flagset = (ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PLUSES);
        break;
    case OBJ_JEWELLERY:
        flagset = (ISFLAG_KNOW_CURSE | ISFLAG_KNOW_TYPE);
        if (ring_has_pluses(item))
            flagset |= ISFLAG_KNOW_PLUSES;
        break;
    case OBJ_MISSILES:
        flagset = ISFLAG_KNOW_PLUSES | ISFLAG_KNOW_TYPE;
        if (get_ammo_brand(item) == SPMSL_NORMAL)
            flagset &= ~ISFLAG_KNOW_TYPE;
        break;
    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
        // All flags necessary for full identification.
    default:
        break;
    }

    if (item_type_known(item))
        flagset &= (~ISFLAG_KNOW_TYPE);

    if (is_artefact(item))
        flagset |= ISFLAG_KNOW_PROPERTIES;

    return flagset;
}

bool fully_identified( const item_def& item )
{
    return item_ident(item, full_ident_mask(item));
}

//
// Equipment race and description:
//
unsigned long get_equip_race( const item_def &item )
{
    return (item.flags & ISFLAG_RACIAL_MASK);
}

unsigned long get_equip_desc( const item_def &item )
{
    return (item.flags & ISFLAG_COSMETIC_MASK);
}

void set_equip_race( item_def &item, unsigned long flags )
{
    ASSERT( (flags & ~ISFLAG_RACIAL_MASK) == 0 );

    // first check for base-sub pairs that can't ever have racial types
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (item.sub_type > WPN_MAX_RACIAL)
            return;
        break;

    case OBJ_ARMOUR:
        if (item.sub_type > ARM_MAX_RACIAL)
            return;
        break;

    case OBJ_MISSILES:
        if (item.sub_type > MI_MAX_RACIAL)
            return;
        break;

    default:
        return;
    }

    // check that item is appropriate for racial type
    switch (flags)
    {
    case ISFLAG_ELVEN:
        switch (item.base_type)
        {
        case OBJ_WEAPONS:
            if ((weapon_skill(item) == SK_MACES_FLAILS
                    && item.sub_type != WPN_WHIP)
                || (weapon_skill(item) == SK_LONG_BLADES
                    && item.sub_type != WPN_FALCHION
                    && item.sub_type != WPN_LONG_SWORD
                    && item.sub_type != WPN_SCIMITAR)
                || weapon_skill(item) == SK_AXES
                || (weapon_skill(item) == SK_POLEARMS
                    && item.sub_type != WPN_SPEAR
                    && item.sub_type != WPN_TRIDENT)
                || item.sub_type == WPN_CROSSBOW)
            {
                return;
            }
            break;
        case OBJ_ARMOUR:
            if (item.sub_type == ARM_SPLINT_MAIL
                || item.sub_type == ARM_BANDED_MAIL
                || item.sub_type == ARM_PLATE_MAIL
                || is_hard_helmet(item))
            {
                return;
            }
            break;
        case OBJ_MISSILES:
            if (item.sub_type == MI_BOLT)
                return;
            break;
        default:
            break;
        }
        break;

    case ISFLAG_DWARVEN:
        switch (item.base_type)
        {
        case OBJ_WEAPONS:
            if (item.sub_type == WPN_WHIP
                || item.sub_type == WPN_CLUB
                || item.sub_type == WPN_QUICK_BLADE
                || (weapon_skill(item) == SK_LONG_BLADES
                    && item.sub_type != WPN_FALCHION
                    && item.sub_type != WPN_LONG_SWORD)
                || weapon_skill(item) == SK_POLEARMS
                || item.sub_type == WPN_BLOWGUN
                || item.sub_type == WPN_HAND_CROSSBOW
                || item.sub_type == WPN_BOW
                || item.sub_type == WPN_LONGBOW)
            {
                return;
            }
            break;
        case OBJ_ARMOUR:
            if (item.sub_type == ARM_ROBE
                || item.sub_type == ARM_LEATHER_ARMOUR
                || get_armour_slot(item) == EQ_HELMET && !is_hard_helmet(item))
            {
                return;
            }
            break;
        case OBJ_MISSILES:
            if (item.sub_type == MI_NEEDLE
                || item.sub_type == MI_ARROW
                || item.sub_type == MI_JAVELIN)
            {
                return;
            }
            break;
        default:
            break;
        }
        break;

    case ISFLAG_ORCISH:
        switch (item.base_type)
        {
        case OBJ_WEAPONS:
            if (item.sub_type == WPN_QUICK_BLADE
                || item.sub_type == WPN_HAND_CROSSBOW
                || item.sub_type == WPN_LONGBOW)
                return;
            break;
        case OBJ_ARMOUR:
            if (get_armour_slot(item) == EQ_HELMET && !is_hard_helmet(item))
                return;
            break;
        default:
            break;
        }

    default:
        break;
    }

    item.flags &= ~ISFLAG_RACIAL_MASK; // delete previous
    item.flags |= flags;
}

void set_equip_desc( item_def &item, unsigned long flags )
{
    ASSERT( (flags & ~ISFLAG_COSMETIC_MASK) == 0 );

    item.flags &= ~ISFLAG_COSMETIC_MASK; // delete previous
    item.flags |= flags;
}

//
// These functions handle the description and subtypes for helmets/caps.
//
short get_helmet_desc( const item_def &item )
{
    ASSERT( is_helmet(item) );

    return item.plus2;
}

void set_helmet_desc( item_def &item, helmet_desc_type type )
{
    ASSERT( is_helmet(item) );

    if (!is_hard_helmet(item) && type > THELM_DESC_MAX_SOFT)
        type = THELM_DESC_PLAIN;

    item.plus2 = type;
}

bool is_helmet(const item_def& item)
{
    return (item.base_type == OBJ_ARMOUR && get_armour_slot(item) == EQ_HELMET);
}

bool is_hard_helmet(const item_def &item)
{
    return (item.base_type == OBJ_ARMOUR && item.sub_type == ARM_HELMET);
}

void set_helmet_random_desc( item_def &item )
{
    ASSERT( is_helmet(item) );

    if (is_hard_helmet(item))
        item.plus2 = random2(THELM_NUM_DESCS);
    else
        item.plus2 = random2(THELM_DESC_MAX_SOFT + 1);
}

//
// Ego item functions:
//
bool set_item_ego_type( item_def &item, int item_type, int ego_type )
{
    if (item.base_type == item_type
        && !is_random_artefact( item )
        && !is_fixed_artefact( item ))
    {
        item.special = ego_type;
        return (true);
    }

    return (false);
}

int get_weapon_brand( const item_def &item )
{
    // Weapon ego types are "brands", so we do the randart lookup here.

    // Staves "brands" handled specially
    if (item.base_type != OBJ_WEAPONS)
        return (SPWPN_NORMAL);

    if (is_fixed_artefact( item ))
    {
        switch (item.special)
        {
        case SPWPN_SWORD_OF_CEREBOV:
            return (SPWPN_FLAMING);

        case SPWPN_STAFF_OF_OLGREB:
            return (SPWPN_VENOM);

        case SPWPN_VAMPIRES_TOOTH:
            return (SPWPN_VAMPIRICISM);

        default:
            return (SPWPN_NORMAL);
        }
    }
    else if (is_random_artefact( item ))
    {
        return (randart_wpn_property( item, RAP_BRAND ));
    }

    return (item.special);
}

int get_ammo_brand( const item_def &item )
{
    // No artefact arrows yet. -- bwr
    if (item.base_type != OBJ_MISSILES || is_random_artefact( item ))
        return (SPMSL_NORMAL);

    return (item.special);
}

special_armour_type get_armour_ego_type( const item_def &item )
{
    // Artefact armours have no ego type, must look up powers separately.
    if (item.base_type != OBJ_ARMOUR
        || (is_random_artefact( item ) && !is_unrandom_artefact( item )))
    {
        return (SPARM_NORMAL);
    }

    return (static_cast<special_armour_type>(item.special));
}

//
// Armour information and checking functions.
//
bool hide2armour( item_def &item )
{
    if (item.base_type != OBJ_ARMOUR)
        return (false);

    switch (item.sub_type)
    {
    default:
        return (false);

    case ARM_DRAGON_HIDE:
        item.sub_type = ARM_DRAGON_ARMOUR;
        break;

    case ARM_TROLL_HIDE:
        item.sub_type = ARM_TROLL_LEATHER_ARMOUR;
        break;

    case ARM_ICE_DRAGON_HIDE:
        item.sub_type = ARM_ICE_DRAGON_ARMOUR;
        break;

    case ARM_MOTTLED_DRAGON_HIDE:
        item.sub_type = ARM_MOTTLED_DRAGON_ARMOUR;
        break;

    case ARM_STORM_DRAGON_HIDE:
        item.sub_type = ARM_STORM_DRAGON_ARMOUR;
        break;

    case ARM_GOLD_DRAGON_HIDE:
        item.sub_type = ARM_GOLD_DRAGON_ARMOUR;
        break;

    case ARM_SWAMP_DRAGON_HIDE:
        item.sub_type = ARM_SWAMP_DRAGON_ARMOUR;
        break;

    case ARM_STEAM_DRAGON_HIDE:
        item.sub_type = ARM_STEAM_DRAGON_ARMOUR;
        break;
    }

    return (true);
}

// Return the enchantment limit of a piece of armour.
int armour_max_enchant( const item_def &item )
{
    ASSERT( item.base_type == OBJ_ARMOUR );

    const int eq_slot = get_armour_slot( item );

    int max_plus = MAX_SEC_ENCHANT;
    if (eq_slot == EQ_BODY_ARMOUR || item.sub_type == ARM_CENTAUR_BARDING
        || item.sub_type == ARM_NAGA_BARDING)
    {
        max_plus = MAX_ARM_ENCHANT;
    }

    return (max_plus);
}

// Doesn't include animal skin (only skins we can make and enchant).
bool armour_is_hide( const item_def &item, bool inc_made )
{
    ASSERT( item.base_type == OBJ_ARMOUR );

    switch (item.sub_type)
    {
    case ARM_TROLL_LEATHER_ARMOUR:
    case ARM_DRAGON_ARMOUR:
    case ARM_ICE_DRAGON_ARMOUR:
    case ARM_STEAM_DRAGON_ARMOUR:
    case ARM_MOTTLED_DRAGON_ARMOUR:
    case ARM_STORM_DRAGON_ARMOUR:
    case ARM_GOLD_DRAGON_ARMOUR:
    case ARM_SWAMP_DRAGON_ARMOUR:
        return (inc_made);

    case ARM_TROLL_HIDE:
    case ARM_DRAGON_HIDE:
    case ARM_ICE_DRAGON_HIDE:
    case ARM_STEAM_DRAGON_HIDE:
    case ARM_MOTTLED_DRAGON_HIDE:
    case ARM_STORM_DRAGON_HIDE:
    case ARM_GOLD_DRAGON_HIDE:
    case ARM_SWAMP_DRAGON_HIDE:
        return (true);

    default:
        break;
    }

    return (false);
}

// Used to distinguish shiny and embroidered.
bool armour_not_shiny( const item_def &item )
{
    ASSERT( item.base_type == OBJ_ARMOUR );

    switch (item.sub_type)
    {
    case ARM_ROBE:
    case ARM_CLOAK:
    case ARM_GLOVES:
    case ARM_BOOTS:
    case ARM_NAGA_BARDING:
    case ARM_CENTAUR_BARDING:
    case ARM_LEATHER_ARMOUR:
    case ARM_ANIMAL_SKIN:
    case ARM_TROLL_HIDE:
    case ARM_TROLL_LEATHER_ARMOUR:
    case ARM_CAP:
    case ARM_WIZARD_HAT:
        return (true);
    }

    return (false);
}

int armour_str_required( const item_def &arm )
{
    ASSERT (arm.base_type == OBJ_ARMOUR );

    int ret = 0;

    const equipment_type  slot = get_armour_slot( arm );
    const int             mass = item_mass( arm );

    switch (slot)
    {
    case EQ_BODY_ARMOUR:
        ret = mass / 35;
        break;

    case EQ_SHIELD:
        ret = mass / 15;
        break;

    default:
        break;
    }

    return ((ret < STR_REQ_THRESHOLD) ? 0 : ret);
}

equipment_type get_armour_slot( const item_def &item )
{
    ASSERT( item.base_type == OBJ_ARMOUR );

    return (Armour_prop[ Armour_index[item.sub_type] ].slot);
}

equipment_type get_armour_slot( armour_type arm )
{
    return (Armour_prop[ Armour_index[arm] ].slot);
}

bool jewellery_is_amulet( const item_def &item )
{
    ASSERT( item.base_type == OBJ_JEWELLERY );

    return (item.sub_type >= AMU_RAGE);
}

bool check_jewellery_size( const item_def &item, size_type size )
{
    ASSERT( item.base_type == OBJ_JEWELLERY );

    // Currently assuming amulets are always wearable (only needs
    // to be held over head or heart... giants can strap it on with
    // a bit of binder twine).  However, rings need to actually fit
    // around the ring finger to work, and so the big cannot use them.
    return (size <= SIZE_LARGE || jewellery_is_amulet( item ));
}

// Returns the basic light status of an armour, ignoring things like the
// elven bonus... you probably want is_light_armour() most times.
bool base_armour_is_light( const item_def &item )
{
    ASSERT( item.base_type == OBJ_ARMOUR );

    return (Armour_prop[ Armour_index[item.sub_type] ].light);
}

// Returns number of sizes off (0 if fitting).
int fit_armour_size( const item_def &item, size_type size )
{
    ASSERT( item.base_type == OBJ_ARMOUR );

    const size_type min = Armour_prop[ Armour_index[item.sub_type] ].fit_min;
    const size_type max = Armour_prop[ Armour_index[item.sub_type] ].fit_max;

    if (size < min)
        return (min - size);    // -'ve means levels too small
    else if (size > max)
        return (max - size);    // +'ve means levels too large

    return (0);
}

// Returns true if armour fits size (shape needs additional verification).
bool check_armour_size( const item_def &item, size_type size )
{
    ASSERT( item.base_type == OBJ_ARMOUR );

    return (fit_armour_size( item, size ) == 0);
}

// Note that this function is used to check validity of equipment
// coming out of transformations... so it shouldn't contain any
// wield/unwield only checks like two-handed weapons and shield.
bool check_armour_shape( const item_def &item, bool quiet )
{
    ASSERT( item.base_type == OBJ_ARMOUR );

    const int slot = get_armour_slot( item );

    if (!player_is_shapechanged())
    {
        switch (slot)
        {
        case EQ_BOOTS:

            if (player_mutation_level(MUT_HOOVES))
            {
                if (!quiet)
                    mpr("You can't wear boots with hooves!");

                return (false);
            }

            if (player_mutation_level(MUT_TALONS))
            {
                if (!quiet)
                    mpr("Boots don't fit your talons!");

                return (false);
            }

            switch (you.species)
            {
            case SP_NAGA:
                if (item.sub_type != ARM_NAGA_BARDING)
                {
                    if (!quiet)
                        mpr( "You can't wear that!" );

                    return (false);
                }
                break;

            case SP_CENTAUR:
                if (item.sub_type != ARM_CENTAUR_BARDING)
                {
                    if (!quiet)
                        mpr( "You can't wear that!" );

                    return (false);
                }
                break;

            case SP_MERFOLK:
                if (player_in_water() && item.sub_type == ARM_BOOTS)
                {
                    if (!quiet)
                        mpr( "You don't currently have feet!" );

                    return (false);
                }
                // intentional fall-through
            default:
                if (item.sub_type == ARM_NAGA_BARDING
                    || item.sub_type == ARM_CENTAUR_BARDING)
                {
                    if (!quiet)
                        mpr( "You can't wear barding!" );

                    return (false);
                }
                break;
            }
            break;

        case EQ_HELMET:
            if (!is_hard_helmet(item))
                break;

            if (player_mutation_level(MUT_HORNS))
            {
                if (!quiet)
                    mpr( "You can't wear that with your horns!" );

                return (false);
            }

            if (player_mutation_level(MUT_BEAK))
            {
                if (!quiet)
                    mpr( "You can't wear that with your beak!" );

                return (false);
            }
            break;

        case EQ_GLOVES:
            if (you.has_claws(false) >= 3)
            {
                if (!quiet)
                    mpr( "You can't wear gloves with your huge claws!" );

                return (false);
            }
            break;

        case EQ_BODY_ARMOUR:
            // Cannot swim in heavy armour.
            if (player_is_swimming() && !is_light_armour( item ))
            {
                if (!quiet)
                   mpr("You can't swim in that!");

                return (false);
            }

            // Draconians are human-sized, but have wings that cause problems
            // with most body armours (only very flexible fit allowed).
            if (player_genus( GENPC_DRACONIAN )
                && !check_armour_size( item, SIZE_BIG ))
            {
                if (!quiet)
                   mpr( "That armour doesn't fit your wings." );

                return (false);
            }
            break;

        default:
            break;
        }
    }
    else
    {
        // Note: Some transformations include all of the above as well.
        if (item.sub_type == ARM_NAGA_BARDING
            || item.sub_type == ARM_CENTAUR_BARDING)
        {
            if (!quiet)
               mpr( "You can't wear barding in your current form!" );

            return (false);
        }
    }

    // Note: This need to be checked after all the special cases
    // above, and in addition to shapechanged or not.  This is
    // a simple check against the armour types that are forced off.

    // FIXME FIXME FIXME
    /*
    if (!transform_can_equip_type( slot ))
    {
        if (!quiet)
           mpr( "You can't wear that in your current form!" );

        return (false);
    }
    */

    return (true);
}

// Returns whether a wand or rod can be charged, or a weapon of electrocution
// enchanted.
// If unknown is true, wands with unknown charges and weapons with unknown
// brand will also return true.
// If hide_charged is true, wands known to be full will return false.
// (This distinction is necessary because even full wands/rods give a message.)
bool item_is_rechargeable(const item_def &it, bool unknown, bool hide_charged)
{
    // These are obvious...
    if (it.base_type == OBJ_WANDS)
    {
        if (unknown && !hide_charged)
            return (true);

        // Don't offer wands already maximally charged.
        if (it.plus2 == ZAPCOUNT_MAX_CHARGED
            || item_ident(it, ISFLAG_KNOW_PLUSES)
               && it.plus >= 3 * wand_charge_value(it.sub_type))
        {
            return (false);
        }
        return (true);
    }
    else if (item_is_rod(it))
    {
        if (unknown && !hide_charged)
            return (true);

        if (item_ident(it, ISFLAG_KNOW_PLUSES))
        {
            return (it.plus2 < MAX_ROD_CHARGE * ROD_CHARGE_MULT
                    || it.plus < it.plus2);
        }
        return (true);
    }
    else if (it.base_type == OBJ_WEAPONS)
    {
        if (unknown && !item_type_known(it)) // Could be electrocution.
            return (true);

        // Weapons of electrocution can get +1 to-dam this way.
        if (!is_artefact(it)
            && get_weapon_brand(it) == SPWPN_ELECTROCUTION
            && item_type_known(it)
            && (unknown && !item_ident(it, ISFLAG_KNOW_PLUSES )
                || item_ident(it, ISFLAG_KNOW_PLUSES )
                   && it.plus2 < MAX_WPN_ENCHANT))
        {
            return (true);
        }
    }

    return (false);
}

// Max. charges are 3 times this value.
int wand_charge_value(int type)
{
    switch (type)
    {
    case WAND_INVISIBILITY:
    case WAND_FIREBALL:
    case WAND_TELEPORTATION:
    case WAND_HEALING:
    case WAND_HASTING:
        return 3;

    case WAND_LIGHTNING:
    case WAND_DRAINING:
        return 4;

    case WAND_FIRE:
    case WAND_COLD:
        return 5;

    default:
        return 8;
    }
}

bool is_enchantable_weapon(const item_def &wpn, bool uncurse)
{
    if (wpn.base_type != OBJ_WEAPONS && wpn.base_type != OBJ_MISSILES)
        return (false);

    // Artefacts or highly enchanted weapons cannot be enchanted,
    // only uncursed.
    if (wpn.base_type == OBJ_WEAPONS)
    {
        if (is_artefact(wpn)
            || wpn.plus >= MAX_WPN_ENCHANT && wpn.plus2 >= MAX_WPN_ENCHANT)
        {
            return (uncurse && item_cursed(wpn));
        }
    }
    // Highly enchanted missiles, which have only one stat, cannot be
    // enchanted or uncursed, since missiles cannot be artefacts or
    // cursed.
    else if (wpn.plus >= MAX_WPN_ENCHANT)
        return (false);

    return (true);
}

// Returns whether a piece of armour can be enchanted further.
// If unknown is true, unidentified armour will return true.
bool is_enchantable_armour(const item_def &arm, bool uncurse, bool unknown)
{
    if (arm.base_type != OBJ_ARMOUR)
        return (false);

    // Melded armour cannot be enchanted.
    if (!you_tran_can_wear(arm) && item_is_equipped(arm))
        return (false);

    // If we don't know the plusses, assume enchanting is possible.
    if (unknown && !is_known_artefact(arm)
        && !item_ident(arm, ISFLAG_KNOW_PLUSES ))
    {
        return (true);
    }

    // Artefacts or highly enchanted armour cannot be enchanted, only
    // uncursed.
    if (is_artefact(arm) || arm.plus >= armour_max_enchant(arm))
        return (uncurse && item_cursed(arm));

    return (true);
}

//
// Weapon information and checking functions:
//

// Checks how rare a weapon is. Many of these have special routines for
// placement, especially those with a rarity of zero. Chance is out of 10.
int weapon_rarity( int w_type )
{
    switch (w_type)
    {
    case WPN_CLUB:
    case WPN_DAGGER:
        return (10);

    case WPN_HAND_AXE:
    case WPN_MACE:
    case WPN_QUARTERSTAFF:
        return (9);

    case WPN_BOW:
    case WPN_FLAIL:
    case WPN_HAMMER:
    case WPN_SABRE:
    case WPN_SHORT_SWORD:
    case WPN_SLING:
    case WPN_SPEAR:
        return (8);

    case WPN_FALCHION:
    case WPN_LONG_SWORD:
    case WPN_MORNINGSTAR:
    case WPN_WAR_AXE:
        return (7);

    case WPN_BATTLEAXE:
    case WPN_CROSSBOW:
    case WPN_GREAT_SWORD:
    case WPN_SCIMITAR:
    case WPN_TRIDENT:
        return (6);

    case WPN_GLAIVE:
    case WPN_HALBERD:
    case WPN_BLOWGUN:
        return (5);

    case WPN_BROAD_AXE:
    case WPN_HAND_CROSSBOW:
    case WPN_SPIKED_FLAIL:
    case WPN_WHIP:
        return (4);

    case WPN_GREAT_MACE:
        return (3);

    case WPN_ANKUS:
    case WPN_DIRE_FLAIL:
    case WPN_SCYTHE:
    case WPN_LONGBOW:
    case WPN_LAJATANG:
        return (2);

    case WPN_GIANT_CLUB:
    case WPN_GIANT_SPIKED_CLUB:
    case WPN_BARDICHE:
        return (1);

    case WPN_DOUBLE_SWORD:
    case WPN_EVENINGSTAR:
    case WPN_EXECUTIONERS_AXE:
    case WPN_KATANA:
    case WPN_KNIFE:
    case WPN_QUICK_BLADE:
    case WPN_TRIPLE_SWORD:
    case WPN_DEMON_WHIP:
    case WPN_DEMON_BLADE:
    case WPN_DEMON_TRIDENT:
    case WPN_BLESSED_FALCHION:
    case WPN_BLESSED_LONG_SWORD:
    case WPN_BLESSED_SCIMITAR:
    case WPN_BLESSED_KATANA:
    case WPN_BLESSED_EUDEMON_BLADE:
    case WPN_BLESSED_DOUBLE_SWORD:
    case WPN_BLESSED_GREAT_SWORD:
    case WPN_BLESSED_TRIPLE_SWORD:
        // Zero value weapons must be placed specially -- see make_item() {dlb}
        return (0);

    default:
        break;
    }

    return (0);
}

int get_vorpal_type( const item_def &item )
{
    int ret = DVORP_NONE;

    if (item.base_type == OBJ_WEAPONS)
        ret = (Weapon_prop[ Weapon_index[item.sub_type] ].dam_type & DAMV_MASK);

    return (ret);
}

int get_damage_type( const item_def &item )
{
    int ret = DAM_BASH;

    if (item.base_type == OBJ_WEAPONS)
        ret = (Weapon_prop[ Weapon_index[item.sub_type] ].dam_type & DAM_MASK);

    return (ret);
}

bool does_damage_type( const item_def &item, int dam_type )
{
    return (get_damage_type( item ) & dam_type);
}


hands_reqd_type hands_reqd(object_class_type base_type, int sub_type,
                           size_type size)
{
    item_def item;
    item.base_type = base_type;
    item.sub_type  = sub_type;
    return hands_reqd(item, size);
}

// Give hands required to wield weapon for a torso of "size".
hands_reqd_type hands_reqd( const item_def &item, size_type size )
{
    int         ret = HANDS_ONE;
    int         fit;
    bool        doub = false;

    switch (item.base_type)
    {
    case OBJ_STAVES:
    case OBJ_WEAPONS:
        // Merging staff with magical staves for consistency... doing
        // as a special case because we want to be very flexible with
        // these useful objects (we want spriggans and ogre magi to
        // be able to use them).
        if (item.base_type == OBJ_STAVES || weapon_skill(item) == SK_STAVES)
        {
            if (size < SIZE_SMALL)
                ret = HANDS_TWO;
            else if (size > SIZE_LARGE)
                ret = HANDS_ONE;
            else
                ret = HANDS_HALF;
            break;
        }

        ret = Weapon_prop[ Weapon_index[item.sub_type] ].hands;

        // Size is the level where we can use one hand for one end.
        if (ret == HANDS_DOUBLE)
        {
            doub = true;
            ret = HANDS_TWO; // HANDS_HALF once double-ended is implemented.
        }

        // Adjust handedness for size only for non-whip melee weapons.
        if (!is_range_weapon(item)
            && item.sub_type != WPN_WHIP
            && item.sub_type != WPN_DEMON_WHIP)
        {
            fit = cmp_weapon_size(item, size);

            // Adjust handedness for non-medium races:
            // (XX values don't matter, see fit_weapon_wieldable_size)
            //
            //         Spriggan Kobold  Human   Ogre    Big     Giant
            // Little      0       0      0      XX     XX      XX
            // Small      +1       0      0      -2     XX      XX
            // Medium     XX      +1      0      -1     -2      XX
            // Large      XX      XX      0       0     -1      -2
            // Big        XX      XX     XX       0      0      -1
            // Giant      XX      XX     XX      XX      0       0

            // Note the stretching of double weapons for larger characters
            // by one level since they tend to be larger weapons.
            if (size < SIZE_MEDIUM && fit > 0)
                ret += fit;
            else if (size > SIZE_MEDIUM && fit < 0)
                ret += (fit + doub);
        }
        break;

    case OBJ_CORPSES:   // unwieldy
        ret = HANDS_TWO;
        break;

    case OBJ_ARMOUR:    // Bardings and body armours are unwieldy.
        if (item.sub_type == ARM_NAGA_BARDING
            || item.sub_type == ARM_CENTAUR_BARDING
            || get_armour_slot( item ) == EQ_BODY_ARMOUR)
        {
            ret = HANDS_TWO;
        }
        break;

    default:
        break;
    }

    if (ret > HANDS_TWO)
        ret = HANDS_TWO;
    else if (ret < HANDS_ONE)
        ret = HANDS_ONE;

    return (static_cast< hands_reqd_type >( ret ));
}

bool is_double_ended( const item_def &item )
{
    if (item.base_type == OBJ_STAVES)
        return (true);
    else if (item.base_type != OBJ_WEAPONS)
        return (false);

    return (Weapon_prop[ Weapon_index[item.sub_type] ].hands == HANDS_DOUBLE);
}

int double_wpn_awkward_speed( const item_def &item )
{
    ASSERT( is_double_ended( item ) );

    const int base = property( item, PWPN_SPEED );

    return ((base * 30 + 10) / 20 + 2);
}

bool is_demonic(const item_def &item)
{
    if (item.base_type == OBJ_WEAPONS)
    {
        switch (item.sub_type)
        {
        case WPN_DEMON_BLADE:
        case WPN_DEMON_WHIP:
        case WPN_DEMON_TRIDENT:
            return (true);

        default:
            break;
        }
    }

    return (false);
}

bool is_blessed_blade(const item_def &item)
{
    if (item.base_type == OBJ_WEAPONS)
    {
        switch (item.sub_type)
        {
        case WPN_BLESSED_FALCHION:
        case WPN_BLESSED_LONG_SWORD:
        case WPN_BLESSED_SCIMITAR:
        case WPN_BLESSED_KATANA:
        case WPN_BLESSED_EUDEMON_BLADE:
        case WPN_BLESSED_DOUBLE_SWORD:
        case WPN_BLESSED_GREAT_SWORD:
        case WPN_BLESSED_TRIPLE_SWORD:
            return (true);

        default:
            break;
        }
    }

    return (false);
}

bool is_blessed_blade_convertible(const item_def &item)
{
    return (!is_artefact(item)
        && (item.base_type == OBJ_WEAPONS
            && (is_demonic(item)
                || weapon_skill(item) == SK_LONG_BLADES)));
}

bool convert2good(item_def &item, bool allow_blessed)
{
    if (item.base_type != OBJ_WEAPONS)
        return (false);

    switch (item.sub_type)
    {
    default:
        return (false);

    case WPN_FALCHION:
        if (!allow_blessed)
            return (false);
        item.sub_type = WPN_BLESSED_FALCHION;
        break;

    case WPN_LONG_SWORD:
        if (!allow_blessed)
            return (false);
        item.sub_type = WPN_BLESSED_LONG_SWORD;
        break;

    case WPN_SCIMITAR:
        if (!allow_blessed)
            return (false);
        item.sub_type = WPN_BLESSED_SCIMITAR;
        break;

    case WPN_DEMON_BLADE:
        if (!allow_blessed)
            item.sub_type = WPN_SCIMITAR;
        else
            item.sub_type = WPN_BLESSED_EUDEMON_BLADE;
        break;

    case WPN_KATANA:
        if (!allow_blessed)
            return (false);
        item.sub_type = WPN_BLESSED_KATANA;
        break;

    case WPN_DOUBLE_SWORD:
        if (!allow_blessed)
            return (false);
        item.sub_type = WPN_BLESSED_DOUBLE_SWORD;
        break;

    case WPN_GREAT_SWORD:
        if (!allow_blessed)
            return (false);
        item.sub_type = WPN_BLESSED_GREAT_SWORD;
        break;

    case WPN_TRIPLE_SWORD:
        if (!allow_blessed)
            return (false);
        item.sub_type = WPN_BLESSED_TRIPLE_SWORD;
        break;

    case WPN_DEMON_WHIP:
        item.sub_type = WPN_WHIP;
        break;

    case WPN_DEMON_TRIDENT:
        item.sub_type = WPN_TRIDENT;
        break;
    }

    if (is_blessed_blade(item))
        item.flags &= ~ISFLAG_RACIAL_MASK;

    return (true);
}

bool convert2bad(item_def &item)
{
    if (item.base_type != OBJ_WEAPONS)
        return (false);

    switch (item.sub_type)
    {
    default:
        return (false);

    case WPN_BLESSED_FALCHION:
        item.sub_type = WPN_FALCHION;
        break;

    case WPN_BLESSED_LONG_SWORD:
        item.sub_type = WPN_LONG_SWORD;
        break;

    case WPN_BLESSED_SCIMITAR:
        item.sub_type = WPN_SCIMITAR;
        break;

    case WPN_BLESSED_EUDEMON_BLADE:
        item.sub_type = WPN_DEMON_BLADE;
        break;

    case WPN_BLESSED_KATANA:
        item.sub_type = WPN_KATANA;
        break;

    case WPN_BLESSED_DOUBLE_SWORD:
        item.sub_type = WPN_DOUBLE_SWORD;
        break;

    case WPN_BLESSED_GREAT_SWORD:
        item.sub_type = WPN_GREAT_SWORD;
        break;

    case WPN_BLESSED_TRIPLE_SWORD:
        item.sub_type = WPN_TRIPLE_SWORD;
        break;
    }

    return (true);
}

int weapon_str_weight( const item_def &wpn )
{
    ASSERT (wpn.base_type == OBJ_WEAPONS || wpn.base_type == OBJ_STAVES);

    if (wpn.base_type == OBJ_STAVES)
        return (Weapon_prop[ Weapon_index[WPN_QUARTERSTAFF] ].str_weight);

    return (Weapon_prop[ Weapon_index[wpn.sub_type] ].str_weight);
}

int weapon_dex_weight( const item_def &wpn )
{
    return (10 - weapon_str_weight( wpn ));
}

int weapon_impact_mass( const item_def &wpn )
{
    ASSERT (wpn.base_type == OBJ_WEAPONS || wpn.base_type == OBJ_STAVES);

    return ((weapon_str_weight( wpn ) * item_mass( wpn ) + 5) / 10);
}

int weapon_str_required( const item_def &wpn, bool hand_half )
{
    ASSERT (wpn.base_type == OBJ_WEAPONS || wpn.base_type == OBJ_STAVES);

    const int req = weapon_impact_mass( wpn ) / ((hand_half) ? 11 : 10);

    return ((req < STR_REQ_THRESHOLD) ? 0 : req);
}

// Returns melee skill of item.
skill_type weapon_skill( const item_def &item )
{
    if (item.base_type == OBJ_WEAPONS && !is_range_weapon( item ))
        return (Weapon_prop[ Weapon_index[item.sub_type] ].skill);
    else if (item.base_type == OBJ_STAVES)
        return (SK_STAVES);

    // This is used to mark that only fighting applies.
    return (SK_FIGHTING);
}

// Front function for the above when we don't have a physical item to check.
skill_type weapon_skill( object_class_type wclass, int wtype )
{
    item_def    wpn;

    wpn.base_type = wclass;
    wpn.sub_type = wtype;

    return (weapon_skill( wpn ));
}

// Returns range skill of the item.
skill_type range_skill( const item_def &item )
{
    if (item.base_type == OBJ_WEAPONS && is_range_weapon( item ))
        return (Weapon_prop[ Weapon_index[item.sub_type] ].skill);
    else if (item.base_type == OBJ_MISSILES)
    {
        switch (item.sub_type)
        {
        case MI_DART:    return (SK_DARTS);
        case MI_JAVELIN: return (SK_POLEARMS);
        default:         break;
        }
    }

    return (SK_THROWING);
}

// Front function for the above when we don't have a physical item to check.
skill_type range_skill( object_class_type wclass, int wtype )
{
    item_def    wpn;

    wpn.base_type = wclass;
    wpn.sub_type = wtype;

    return (range_skill( wpn ));
}


// Calculate the bonus to melee EV for using "wpn", with "skill" and "dex"
// to protect a body of size "body".
int weapon_ev_bonus( const item_def &wpn, int skill, size_type body, int dex,
                     bool hide_hidden )
{
    ASSERT( wpn.base_type == OBJ_WEAPONS || wpn.base_type == OBJ_STAVES );

    int ret = 0;

    // Note: ret currently measured in halves (see skill factor).
    if (wpn.sub_type == WPN_WHIP || wpn.sub_type == WPN_DEMON_WHIP)
        ret = 3 + (dex / 5);
    else if (weapon_skill( wpn ) == SK_POLEARMS)
        ret = 2 + (dex / 5);

    // Weapons of reaching are naturally a bit longer/flexier.
    if (!hide_hidden || item_type_known( wpn ))
    {
        if (get_weapon_brand( wpn ) == SPWPN_REACHING)
            ret += 1;
    }

    // Only consider additional modifications if we have a positive base:
    if (ret > 0)
    {
        // Size factors:
        // - large characters can't cover their flanks as well
        // - note that not all weapons are available to small characters
        if (body > SIZE_LARGE)
            ret -= (4 * (body - SIZE_LARGE) - 2);
        else if (body < SIZE_MEDIUM)
            ret += 1;

        // apply skill (and dividing by 2)
        ret = (ret * (skill + 10)) / 20;

        // Make sure things can't get too insane.
        if (ret > 8)
            ret = 8 + (ret - 8) / 2;
    }

    // Note: this is always a bonus.
    return ((ret > 0) ? ret : 0);
}

static size_type weapon_size( const item_def &item )
{
    ASSERT (item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES);

    if (item.base_type == OBJ_STAVES)
        return (Weapon_prop[ Weapon_index[WPN_QUARTERSTAFF] ].fit_size);

    return (Weapon_prop[ Weapon_index[item.sub_type] ].fit_size);
}

// Returns number of sizes off.
int cmp_weapon_size( const item_def &item, size_type size )
{
    ASSERT( item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES );

    return (weapon_size( item ) - size);
}

// Returns number of sizes away from being a usable weapon.
int fit_weapon_wieldable_size( const item_def &item, size_type size )
{
    ASSERT( item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES );

    const int fit = cmp_weapon_size( item, size );

    return ((fit < -2) ? fit + 2 :
            (fit >  1) ? fit - 1 : 0);
}

// Returns number of sizes away from being throwable... the window
// is currently [size - 5, size - 1].
int fit_item_throwable_size( const item_def &item, size_type size )
{
    int ret = item_size( item ) - size;

    return ((ret >= 0) ? ret + 1 :
            (ret > -6) ? 0
                       : ret + 5);
}

// Returns true if weapon is usable as a tool.
// Note that we assume that tool usable >= wieldable.
bool check_weapon_tool_size( const item_def &item, size_type size )
{
    ASSERT( item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES );

    // Staves are currently usable for everyone just to be nice.
    if (item.base_type == OBJ_STAVES || weapon_skill(item) == SK_STAVES)
        return (true);

    const int fit = cmp_weapon_size( item, size );

    return (fit >= -3 && fit <= 1);
}

// Returns true if weapon is usable as a weapon.
bool check_weapon_wieldable_size( const item_def &item, size_type size )
{
    ASSERT( item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES );

    // Staves are currently wieldable for everyone just to be nice.
    if (item.base_type == OBJ_STAVES || weapon_skill(item) == SK_STAVES)
        return (true);

    int fit = fit_weapon_wieldable_size( item, size );

    // Adjust fit for size.
    if (size < SIZE_SMALL && fit > 0)
        fit--;
    else if (size > SIZE_LARGE && fit < 0)
        fit++;

    return (fit == 0);
}

// Note that this function is used to check validity of equipment
// coming out of transformations... so it shouldn't contain any
// wield/unwield only checks like two-handed weapons and shield.
// check_id is only used for descriptions, where we don't want to
// give away any information the player doesn't have yet.
bool check_weapon_shape( const item_def &item, bool quiet, bool check_id )
{
    const int brand = get_weapon_brand( item );

    if ((!check_id || item_type_known( item ))
        && ((item.base_type == OBJ_WEAPONS
                && is_blessed_blade(item))
            || brand == SPWPN_HOLY_WRATH)
        && player_is_unholy())
    {
        if (!quiet)
            mpr( "This weapon will not allow you to wield it." );

        return (false);
    }

    // Note: this should always be done last, see check_armour_shape()
    // FIXME FIXME FIXME
    /*
    if (!transform_can_equip_type( EQ_WEAPON ))
    {
        if (!quiet)
           mpr( "You can't wield anything in your current form!" );

        return (false);
    }
    */

    return (true);
}

// Returns the you.inv[] index of our wielded weapon or -1 (no item, not wield).
int get_inv_wielded( void )
{
    return (player_weapon_wielded() ? you.equip[EQ_WEAPON] : -1);
}

// Returns the you.inv[] index of our hand tool or -1 (no item, not usable)
// Note that we don't count armour and such as "tools" here because
// this function is used to judge if the item will sticky curse to
// our hands.
int get_inv_hand_tool( void )
{
    const int tool = you.equip[EQ_WEAPON];

    // FIXME
    /*
    if (tool == -1 || !is_tool( you.inv[tool] ))
        return (-1);

    if (you.inv[tool].base_type == OBJ_WEAPONS
        || you.inv[tool].base_type == OBJ_STAVES)
    {
        // assuring that bad "shape" weapons aren't in hand
        ASSERT( check_weapon_shape( you.inv[tool], false ) );

        if (!check_weapon_tool_size( you.inv[tool], player_size() ))
            return (-1);
    }
    */

    return (tool);
}

// Returns the you.inv[] index of the thing in our hand... this is provided
// as a service to specify that both of the above are irrelevant.
// Do not use this for low level functions dealing with wielding directly.
int get_inv_in_hand( void )
{
    return (you.equip[EQ_WEAPON]);
}

//
// Launcher and ammo functions:
//
missile_type fires_ammo_type(const item_def &item)
{
    if (item.base_type != OBJ_WEAPONS)
        return (MI_NONE);

    return (Weapon_prop[Weapon_index[item.sub_type]].ammo);
}

missile_type fires_ammo_type(weapon_type wtype)
{
    item_def wpn;
    wpn.base_type = OBJ_WEAPONS;
    wpn.sub_type = wtype;

    return (fires_ammo_type(wpn));
}

bool is_range_weapon(const item_def &item)
{
    return (fires_ammo_type(item) != MI_NONE);
}

bool is_range_weapon_type(weapon_type wtype)
{
    item_def wpn;
    wpn.base_type = OBJ_WEAPONS;
    wpn.sub_type = wtype;

    return (is_range_weapon(wpn));
}

const char *ammo_name(missile_type ammo)
{
    return (ammo < 0 || ammo >= NUM_MISSILES ? "eggplant"
            : Missile_prop[ Missile_index[ammo] ].name);
}

const char *ammo_name(const item_def &bow)
{
    ASSERT(is_range_weapon(bow));
    return ammo_name(fires_ammo_type(bow));
}

// Returns true if item has an associated launcher.
bool has_launcher(const item_def &ammo)
{
    ASSERT(ammo.base_type == OBJ_MISSILES);
    return (ammo.sub_type != MI_LARGE_ROCK
            && ammo.sub_type != MI_JAVELIN
            && ammo.sub_type != MI_THROWING_NET);
}

// Returns true if item can be reasonably thrown without a launcher.
bool is_throwable(const actor *actor, const item_def &wpn, bool force)
{
    size_type bodysize = actor->body_size();

    if (wpn.base_type == OBJ_WEAPONS)
        return (Weapon_prop[Weapon_index[wpn.sub_type]].throwable);
    else if (wpn.base_type == OBJ_MISSILES)
    {
        if (!force)
        {
            if ((bodysize < SIZE_LARGE
                    || !actor->can_throw_large_rocks())
                && wpn.sub_type == MI_LARGE_ROCK)
            {
                return (false);
            }

            if (bodysize < SIZE_MEDIUM
                && (wpn.sub_type == MI_JAVELIN
                    || wpn.sub_type == MI_THROWING_NET))
            {
                return (false);
            }
        }

        return (Missile_prop[Missile_index[wpn.sub_type]].throwable);
    }

    return (false);
}

// Decide if something is launched or thrown.
launch_retval is_launched(const actor *actor, const item_def *launcher,
                          const item_def &missile)
{
    if (missile.base_type == OBJ_MISSILES
        && launcher
        && missile.launched_by(*launcher))
    {
        return (LRET_LAUNCHED);
    }

    return (is_throwable(actor, missile) ? LRET_THROWN : LRET_FUMBLED);
}

//
// Staff/rod functions:
//
bool item_is_rod( const item_def &item )
{
    return (item.base_type == OBJ_STAVES && item.sub_type >= STAFF_FIRST_ROD);
}

bool item_is_staff( const item_def &item )
{
    return (item.base_type == OBJ_STAVES && !item_is_rod(item));
}

//
// Ring functions:
//
// Returns number of pluses on jewellery (always none for amulets yet).
int ring_has_pluses( const item_def &item )
{
    ASSERT (item.base_type == OBJ_JEWELLERY);

    // not known -> no pluses
    if (!item_type_known(item))
        return (0);

    switch (item.sub_type)
    {
    case RING_SLAYING:
        return (2);

    case RING_PROTECTION:
    case RING_EVASION:
    case RING_STRENGTH:
    case RING_INTELLIGENCE:
    case RING_DEXTERITY:
        return (1);

    default:
        break;
    }

    return (0);
}

//
// Food functions:
//
bool food_is_meat( const item_def &item )
{
    ASSERT( is_valid_item( item ) && item.base_type == OBJ_FOOD );

    return (Food_prop[ Food_index[item.sub_type] ].carn_mod > 0);
}

bool food_is_veg( const item_def &item )
{
    ASSERT( is_valid_item( item ) && item.base_type == OBJ_FOOD );

    return (Food_prop[ Food_index[item.sub_type] ].herb_mod > 0);
}

bool is_blood_potion( const item_def &item)
{
    if (item.base_type != OBJ_POTIONS)
        return (false);

    return (item.sub_type == POT_BLOOD
            || item.sub_type == POT_BLOOD_COAGULATED);
}

// Returns food value for one turn of eating.
int food_value( const item_def &item )
{
    ASSERT( is_valid_item( item ) && item.base_type == OBJ_FOOD );

    const int herb = player_mutation_level(MUT_HERBIVOROUS);

    // XXX: This needs to be better merged with the mutation system.
    const int carn = player_mutation_level(MUT_CARNIVOROUS);

    const food_def &food = Food_prop[ Food_index[item.sub_type] ];

    int ret = food.value;

    ret += (carn * food.carn_mod);
    ret += (herb * food.herb_mod);

    return ((ret > 0) ? div_rand_round( ret, food.turns ) : 0);
}

int food_turns( const item_def &item )
{
    ASSERT( is_valid_item( item ) && item.base_type == OBJ_FOOD );

    return (Food_prop[ Food_index[item.sub_type] ].turns);
}

bool can_cut_meat( const item_def &item )
{
    return (does_damage_type( item, DAM_SLICE ));
}

bool food_is_rotten( const item_def &item )
{
    return (item.special < 100) && (item.base_type == OBJ_CORPSES
                                       && item.sub_type == CORPSE_BODY
                                    || item.base_type == OBJ_FOOD
                                       && item.sub_type == FOOD_CHUNK);
}

int corpse_freshness( const item_def &item )
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(item.special <= FRESHEST_CORPSE);
    return (item.special);
}

// Returns true if item counts as a tool for tool size comparisons and msgs.
bool is_tool( const item_def &item )
{
    // Currently using OBJ_WEAPONS instead of can_cut_meat() as almost
    // any weapon might be an evocable artefact.
    return (item.base_type == OBJ_WEAPONS
            || item.base_type == OBJ_STAVES
            || (item.base_type == OBJ_MISCELLANY
                && item.sub_type != MISC_RUNE_OF_ZOT));
}


//
// Generic item functions:
//
int property( const item_def &item, int prop_type )
{
    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        if (prop_type == PARM_AC)
            return (Armour_prop[ Armour_index[item.sub_type] ].ac);
        else if (prop_type == PARM_EVASION)
            return (Armour_prop[ Armour_index[item.sub_type] ].ev);
        break;

    case OBJ_WEAPONS:
        if (prop_type == PWPN_DAMAGE)
            return (Weapon_prop[ Weapon_index[item.sub_type] ].dam);
        else if (prop_type == PWPN_HIT)
            return (Weapon_prop[ Weapon_index[item.sub_type] ].hit);
        else if (prop_type == PWPN_SPEED)
            return (Weapon_prop[ Weapon_index[item.sub_type] ].speed);
        else if (prop_type == PWPN_ACQ_WEIGHT)
            return (Weapon_prop[ Weapon_index[item.sub_type] ].acquire_weight);
        break;

    case OBJ_MISSILES:
        if (prop_type == PWPN_DAMAGE)
            return (Missile_prop[ Missile_index[item.sub_type] ].dam);
        break;

    case OBJ_STAVES:
        if (prop_type == PWPN_DAMAGE)
            return (Weapon_prop[ Weapon_index[WPN_QUARTERSTAFF] ].dam);
        else if (prop_type == PWPN_HIT)
            return (Weapon_prop[ Weapon_index[WPN_QUARTERSTAFF] ].hit);
        else if (prop_type == PWPN_SPEED)
            return (Weapon_prop[ Weapon_index[WPN_QUARTERSTAFF] ].speed);
        break;

    default:
        break;
    }

    return (0);
}

// Returns true if item is evokable.
bool gives_ability(const item_def &item)
{
    if (!item_type_known(item))
        return (false);

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    {
        // unwielded weapon
        item_def *weap = you.slot_item(EQ_WEAPON);
        if (!weap || weap->slot != item.slot)
            return (false);
        break;
    }
    case OBJ_JEWELLERY:
    {
        if (!jewellery_is_amulet(item))
        {
            // unworn ring
            item_def *lring = you.slot_item(EQ_LEFT_RING);
            item_def *rring = you.slot_item(EQ_RIGHT_RING);
            if ((!lring || lring->slot != item.slot)
                && (!rring || rring->slot != item.slot))
            {
                return (false);
            }

            if (item.sub_type == RING_TELEPORTATION
                || item.sub_type == RING_LEVITATION
                || item.sub_type == RING_INVISIBILITY)
            {
                return (true);
            }
        }
        else
        {
            // unworn amulet
            item_def *amul = you.slot_item(EQ_AMULET);
            if (!amul || amul->slot != item.slot)
                return (false);

            if (item.sub_type == AMU_RAGE)
                return (true);
        }
        break;
    }
    case OBJ_ARMOUR:
    {
        const equipment_type eq = get_armour_slot(item);
        if (eq == EQ_NONE)
            return (false);

        // unworn armour
        item_def *arm = you.slot_item(eq);
        if (!arm || arm->slot != item.slot)
            return (false);
        break;
    }
    default:
        return (false);
    }

    if (!is_random_artefact(item))
        return (false);

    // Check for evokable randart properties.
    for (int rap = RAP_INVISIBLE; rap <= RAP_MAPPING; rap++)
        if (randart_wpn_property( item, static_cast<randart_prop_type>(rap) ))
            return (true);

    return (false);
}

// Returns true if the item confers an intrinsic that is shown on the % screen.
bool gives_resistance(const item_def &item)
{
    if (!item_type_known(item))
        return (false);

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    {
        // unwielded weapon
        item_def *weap = you.slot_item(EQ_WEAPON);
        if (!weap || weap->slot != item.slot)
            return (false);
        break;
    }
    case OBJ_JEWELLERY:
    {
        if (!jewellery_is_amulet(item))
        {
            // unworn ring
            const item_def *lring = you.slot_item(EQ_LEFT_RING);
            const item_def *rring = you.slot_item(EQ_RIGHT_RING);
            if ((!lring || lring->slot != item.slot)
                && (!rring || rring->slot != item.slot))
            {
                return (false);
            }

            if (item.sub_type >= RING_PROTECTION_FROM_FIRE
                   && item.sub_type <= RING_PROTECTION_FROM_COLD
                || item.sub_type == RING_SEE_INVISIBLE
                || item.sub_type >= RING_LIFE_PROTECTION
                   && item.sub_type <= RING_TELEPORT_CONTROL
                || item.sub_type == RING_SUSTAIN_ABILITIES)
            {
                return (true);
            }
        }
        else
        {
            // unworn amulet
            const item_def *amul = you.slot_item(EQ_AMULET);
            if (!amul || amul->slot != item.slot)
                return (false);

            if (item.sub_type != AMU_RAGE && item.sub_type != AMU_INACCURACY)
                return (true);
        }
        break;
    }
    case OBJ_ARMOUR:
    {
        const equipment_type eq = get_armour_slot(item);
        if (eq == EQ_NONE)
            return (false);

        // unworn armour
        item_def *arm = you.slot_item(eq);
        if (!arm || arm->slot != item.slot)
            return (false);
        break;

        const int ego = get_armour_ego_type(item);
        if (ego >= SPARM_FIRE_RESISTANCE && ego <= SPARM_SEE_INVISIBLE
            || ego == SPARM_RESISTANCE || ego == SPARM_POSITIVE_ENERGY)
        {
            return (true);
        }
    }
    case OBJ_STAVES:
    {
        // unwielded staff
        item_def *weap = you.slot_item(EQ_WEAPON);
        if (!weap || weap->slot != item.slot)
            return (false);

        if (item.sub_type >= STAFF_FIRE && item.sub_type <= STAFF_POISON
            || item.sub_type == STAFF_AIR)
        {
            return (true);
        }
        return (false);
    }
    default:
        return (false);
    }

    if (!is_random_artefact(item))
        return (false);

    // Check for randart resistances.
    for (int rap = RAP_FIRE; rap <= RAP_CAN_TELEPORT; rap++)
    {
        if (rap == RAP_MAGIC || rap >= RAP_INVISIBLE && rap != RAP_CAN_TELEPORT)
            continue;

        if (randart_wpn_property( item, static_cast<randart_prop_type>(rap) ))
            return (true);
    }

    return (false);
}

int item_mass( const item_def &item )
{
    int unit_mass = 0;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        unit_mass = Weapon_prop[ Weapon_index[item.sub_type] ].mass;
        break;

    case OBJ_ARMOUR:
        unit_mass = Armour_prop[ Armour_index[item.sub_type] ].mass;

        if (get_equip_race( item ) == ISFLAG_ELVEN)
        {
            const int reduc = (unit_mass >= 25) ? unit_mass / 5 : 5;

            // Truncate to the nearest 5 and reduce the item mass:
            unit_mass -= ((reduc / 5) * 5);
            unit_mass = std::max(unit_mass, 5);
        }
        break;

    case OBJ_MISSILES:
    {
        unit_mass = Missile_prop[ Missile_index[item.sub_type] ].mass;
        int brand = get_ammo_brand(item);

        if (brand == SPMSL_SILVER)
            unit_mass *= 2;
        else if(brand == SPMSL_STEEL)
            unit_mass *= 3;
        break;
    }

    case OBJ_FOOD:
        unit_mass = Food_prop[ Food_index[item.sub_type] ].mass;
        break;

    case OBJ_WANDS:
        unit_mass = 100;
        break;

    case OBJ_UNKNOWN_I:
        unit_mass = 200;        // labeled "books"
        break;

    case OBJ_SCROLLS:
        unit_mass = 20;
        break;

    case OBJ_JEWELLERY:
        unit_mass = 10;
        break;

    case OBJ_POTIONS:
        unit_mass = 40;
        break;

    case OBJ_UNKNOWN_II:
        unit_mass = 5;          // labeled "gems"
        break;

    case OBJ_BOOKS:
        unit_mass = 70;
        break;

    case OBJ_STAVES:
        unit_mass = 130;
        break;

    case OBJ_ORBS:
        unit_mass = 300;
        break;

    case OBJ_MISCELLANY:
        if ( is_deck(item) )
        {
            unit_mass = 50;
            break;
        }
        switch (item.sub_type)
        {
        case MISC_BOTTLED_EFREET:
        case MISC_CRYSTAL_BALL_OF_SEEING:
        case MISC_CRYSTAL_BALL_OF_ENERGY:
        case MISC_CRYSTAL_BALL_OF_FIXATION:
            unit_mass = 150;
            break;

        default:
            unit_mass = 100;
            break;
        }
        break;

    case OBJ_CORPSES:
        unit_mass = mons_weight( item.plus );

        if (item.sub_type == CORPSE_SKELETON)
            unit_mass /= 10;
        break;

    default:
    case OBJ_GOLD:
        unit_mass = 0;
        break;
    }

    return ((unit_mass > 0) ? unit_mass : 0);
}

// Note that this function, and item sizes in general aren't quite on the
// same scale as PCs and monsters.
size_type item_size( const item_def &item )
{
    int size = SIZE_TINY;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
        size = Weapon_prop[ Weapon_index[item.sub_type] ].fit_size - 1;
        break;

    case OBJ_ARMOUR:
        size = SIZE_MEDIUM;

        switch (item.sub_type)
        {
        case ARM_GLOVES:
        case ARM_HELMET:
        case ARM_CAP:
        case ARM_WIZARD_HAT:
        case ARM_BOOTS:
        case ARM_BUCKLER:
            // tiny armour
            size = SIZE_TINY;
            break;

        case ARM_SHIELD:
            size = SIZE_LITTLE;
            break;

        case ARM_LARGE_SHIELD:
            size = SIZE_SMALL;
            break;

        default:        // Body armours and bardings.
            size = SIZE_MEDIUM;
            break;
        }
        break;

    case OBJ_MISSILES:
        if (item.sub_type == MI_LARGE_ROCK)
            size = SIZE_SMALL;
        break;

    case OBJ_MISCELLANY:
        break;

    case OBJ_CORPSES:
        // FIXME: This should depend on the original monster's size!
        size = SIZE_SMALL;
        break;

    default:            // sundry tiny items
        break;
    }

    if (size < SIZE_TINY)
        size = SIZE_TINY;
    else if (size > SIZE_HUGE)
        size = SIZE_HUGE;

    return (static_cast<size_type>( size ));
}

// Returns true if we might be interested in dumping the colour.
bool is_colourful_item( const item_def &item )
{
    bool ret = false;

    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        if (item.sub_type == ARM_ROBE
            || item.sub_type == ARM_CLOAK
            || item.sub_type == ARM_CAP
            || item.sub_type == ARM_NAGA_BARDING
            || item.sub_type == ARM_CENTAUR_BARDING)
        {
            ret = true;
        }
        break;

    default:
        break;
    }

    return (ret);
}

bool is_shield(const item_def &item)
{
    return (item.base_type == OBJ_ARMOUR
            && get_armour_slot(item) == EQ_SHIELD);
}

// Returns true if the given item cannot be wielded with the given shield.
// The currently equipped shield is used if no shield is passed in.
bool is_shield_incompatible(const item_def &weapon, const item_def *shield)
{
    // If there's no shield, there's no problem.
    if (!shield && !(shield = player_shield()))
        return (false);

    hands_reqd_type hand = hands_reqd(weapon, player_size());
    return (hand == HANDS_TWO
            && !item_is_rod(weapon)
            && !is_range_weapon(weapon));
}

bool shield_reflects(const item_def &shield)
{
    ASSERT(is_shield(shield));

    return (get_armour_ego_type(shield) == SPARM_REFLECTION);
}

std::string item_base_name(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        return Weapon_prop[Weapon_index[item.sub_type]].name;
    case OBJ_MISSILES:
        return Missile_prop[Missile_index[item.sub_type]].name;
    case OBJ_ARMOUR:
        return Armour_prop[Armour_index[item.sub_type]].name;
    case OBJ_JEWELLERY:
        return (jewellery_is_amulet(item) ? "amulet" : "ring");
    default:
        return "";
    }
}

const char* weapon_base_name(unsigned char subtype)
{
    return Weapon_prop[Weapon_index[subtype]].name;
}
