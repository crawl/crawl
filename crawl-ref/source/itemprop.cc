/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "itemname.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "externs.h"

#include "artefact.h"
#include "decks.h"
#include "describe.h"
#include "food.h"
#include "godpassive.h"
#include "invent.h"
#include "items.h"
#include "itemprop.h"
#include "libutil.h"
#include "misc.h"
#include "mon-util.h"
#include "mon-stuff.h"
#include "notes.h"
#include "options.h"
#include "player.h"
#include "religion.h"
#include "skills.h"
#include "spl-book.h"
#include "quiver.h"
#include "random.h"
#include "shopping.h"
#include "xom.h"

static iflags_t _full_ident_mask(const item_def& item);

// XXX: Name strings in most of the following are currently unused!
struct armour_def
{
    armour_type         id;
    const char         *name;
    int                 ac;
    int                 ev;
    int                 mass;

    equipment_type      slot;
    size_type           fit_min;
    size_type           fit_max;
};

// Note: the Little-Giant range is used to make armours which are very
// flexible and adjustable and can be worn by any player character...
// providing they also pass the shape test, of course.
static int Armour_index[NUM_ARMOURS];
static const armour_def Armour_prop[NUM_ARMOURS] =
{
    { ARM_ANIMAL_SKIN,          "animal skin",            2,   0,  100,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_ROBE,                 "robe",                   2,   0,   60,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_BIG },
    { ARM_LEATHER_ARMOUR,       "leather armour",         3,  -4,  150,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM },

    { ARM_RING_MAIL,            "ring mail",              5,  -7,  250,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM },
    { ARM_SCALE_MAIL,           "scale mail",             6, -11,  350,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM },
    { ARM_CHAIN_MAIL,           "chain mail",             8, -15,  400,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM },
    { ARM_PLATE_ARMOUR,         "plate armour",          10, -19,  650,
        EQ_BODY_ARMOUR, SIZE_SMALL, SIZE_MEDIUM },
    { ARM_CRYSTAL_PLATE_ARMOUR, "crystal plate armour",  14, -24, 1200,
        EQ_BODY_ARMOUR, SIZE_SMALL, SIZE_MEDIUM },

    { ARM_TROLL_HIDE,           "troll hide",             2,  -4,  220,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_TROLL_LEATHER_ARMOUR, "troll leather armour",   4,  -4,  220,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_STEAM_DRAGON_HIDE,    "steam dragon hide",      2,   0,  120,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_STEAM_DRAGON_ARMOUR,  "steam dragon armour",    5,   0,  120,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_MOTTLED_DRAGON_HIDE,  "mottled dragon hide",    3,  -4,  150,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_MOTTLED_DRAGON_ARMOUR,"mottled dragon armour",  6,  -4,  150,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_SWAMP_DRAGON_HIDE,    "swamp dragon hide",      3,  -7,  200,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_SWAMP_DRAGON_ARMOUR,  "swamp dragon armour",    7,  -7,  200,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_FIRE_DRAGON_HIDE,     "fire dragon hide",       3, -11,  350,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_FIRE_DRAGON_ARMOUR,   "fire dragon armour",     8, -11,  350,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_ICE_DRAGON_HIDE,      "ice dragon hide",        4, -11,  350,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_ICE_DRAGON_ARMOUR,    "ice dragon armour",      9, -11,  350,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_PEARL_DRAGON_HIDE,    "pearl dragon hide",      3, -11,  400,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_PEARL_DRAGON_ARMOUR,  "pearl dragon armour",   10, -11,  400,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_STORM_DRAGON_HIDE,    "storm dragon hide",      4, -11,  600,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_STORM_DRAGON_ARMOUR,  "storm dragon armour",   10, -17,  600,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_GOLD_DRAGON_HIDE,     "gold dragon hide",       4, -17, 1100,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },
    { ARM_GOLD_DRAGON_ARMOUR,   "gold dragon armour",    12, -27, 1100,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT },

    { ARM_CLOAK,                "cloak",                  1,   0,   40,
        EQ_CLOAK,       SIZE_LITTLE, SIZE_BIG },
    { ARM_GLOVES,               "gloves",                 1,   0,   20,
        EQ_GLOVES,      SIZE_SMALL,  SIZE_MEDIUM },

    { ARM_HELMET,               "helmet",                 1,   0,   80,
        EQ_HELMET,      SIZE_SMALL,  SIZE_MEDIUM },

    { ARM_CAP,                  "cap",                    0,   0,   40,
        EQ_HELMET,      SIZE_LITTLE, SIZE_LARGE },

    { ARM_WIZARD_HAT,           "wizard hat",             0,   0,   40,
        EQ_HELMET,      SIZE_LITTLE, SIZE_LARGE },

    // Note that barding size is compared against torso so it currently
    // needs to fit medium, but that doesn't matter as much as race
    // and shapeshift status.
    { ARM_BOOTS,                "boots",                  1,   0,   30,
        EQ_BOOTS,       SIZE_SMALL,  SIZE_MEDIUM },
    // Changed max. barding size to large to allow for the appropriate
    // monster types (monsters don't differentiate between torso and general).
    { ARM_CENTAUR_BARDING,      "centaur barding",        4,  -6,  100,
        EQ_BOOTS,       SIZE_MEDIUM, SIZE_LARGE },
    { ARM_NAGA_BARDING,         "naga barding",           4,  -6,  100,
        EQ_BOOTS,       SIZE_MEDIUM, SIZE_LARGE },

    // Note: shields use ac-value as sh-value, EV pen is used as the basis
    // to calculate adjusted shield penalty.
    { ARM_BUCKLER,              "buckler",                3,  -1,   90,
        EQ_SHIELD,      SIZE_LITTLE, SIZE_MEDIUM },
    { ARM_SHIELD,               "shield",                 8,  -3,  150,
        EQ_SHIELD,      SIZE_SMALL,  SIZE_BIG    },
    { ARM_LARGE_SHIELD,         "large shield",          13,  -5,  230,
        EQ_SHIELD,      SIZE_MEDIUM, SIZE_GIANT  },
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
    size_type           fit_size;
    missile_type        ammo;         // MI_NONE for non-launchers
    bool                throwable;

    int                 dam_type;
    int                 acquire_weight;
};

static int Weapon_index[NUM_WEAPONS];
static const weapon_def Weapon_prop[NUM_WEAPONS] =
{
    // Maces & Flails
    { WPN_CLUB,              "club",                5,  3, 13,  50,  7,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_LITTLE, MI_NONE, true,
        DAMV_CRUSHING, 0 },
    { WPN_ROD,               "rod",                 5,  3, 13,  50,  7,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_LITTLE, MI_NONE, true,
        DAMV_CRUSHING, 0 },
    { WPN_WHIP,              "whip",                6,  2, 11,  30,  2,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_SLASHING, 0 },
    { WPN_HAMMER,            "hammer",              7,  3, 13,  90,  7,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_CRUSHING, 0 },
    { WPN_MACE,              "mace",                8,  3, 14, 120,  8,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_CRUSHING, 10 },
    { WPN_FLAIL,             "flail",              10,  0, 14, 130,  8,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_CRUSHING, 10 },
    { WPN_MORNINGSTAR,       "morningstar",        13, -2, 15, 140,  8,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_CRUSHING | DAM_PIERCE, 10 },
    { WPN_DEMON_WHIP,        "demon whip",         11,  1, 11,  30,  2,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_SLASHING, 2 },
    { WPN_SACRED_SCOURGE,    "sacred scourge",     12,  0, 11,  30,  2,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_SLASHING, 0 },
    { WPN_DIRE_FLAIL,        "dire flail",         13, -3, 13, 240,  9,
        SK_MACES_FLAILS, HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CRUSHING | DAM_PIERCE, 10 },
    { WPN_EVENINGSTAR,       "eveningstar",        15, -1, 15, 180,  8,
        SK_MACES_FLAILS, HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_CRUSHING | DAM_PIERCE, 2 },
    { WPN_GREAT_MACE,        "great mace",         18, -4, 17, 270,  9,
        SK_MACES_FLAILS, HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CRUSHING, 10 },
    { WPN_GIANT_CLUB,        "giant club",         20, -6, 17, 330, 10,
        SK_MACES_FLAILS, HANDS_TWO,    SIZE_BIG,    MI_NONE, false,
        DAMV_CRUSHING, 10 },
    { WPN_GIANT_SPIKED_CLUB, "giant spiked club",  22, -7, 18, 350, 10,
        SK_MACES_FLAILS, HANDS_TWO,    SIZE_BIG,    MI_NONE, false,
        DAMV_CRUSHING | DAM_PIERCE, 10 },

    // Short Blades
    { WPN_DAGGER,            "dagger",              4,  6, 10,  20,  1,
        SK_SHORT_BLADES, HANDS_ONE,    SIZE_LITTLE, MI_NONE, true,
        DAMV_STABBING | DAM_SLICE, 10 },
    { WPN_QUICK_BLADE,       "quick blade",         5,  6,  7,  50,  0,
        SK_SHORT_BLADES, HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_STABBING | DAM_SLICE, 2 },
    { WPN_SHORT_SWORD,       "short sword",         6,  4, 11,  80,  2,
        SK_SHORT_BLADES, HANDS_ONE,    SIZE_LITTLE,  MI_NONE, false,
        DAMV_SLICING | DAM_PIERCE, 10 },
    { WPN_SABRE,             "sabre",               7,  4, 12,  90,  2,
        SK_SHORT_BLADES, HANDS_ONE,    SIZE_LITTLE,  MI_NONE, false,
        DAMV_SLICING | DAM_PIERCE, 10 },

    // Long Blades
    { WPN_FALCHION,              "falchion",               8,  2, 13, 170,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_SLICING, 10 },      // or perhaps DAMV_CHOPPING is more apt?
    { WPN_BLESSED_FALCHION,      "blessed falchion",       9,  2, 12, 170,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_LITTLE, MI_NONE, false,
        DAMV_SLICING, 0 },       // or perhaps DAMV_CHOPPING is more apt?
    { WPN_LONG_SWORD,            "long sword",            10,  1, 14, 160,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_SLICING, 10 },
    { WPN_BLESSED_LONG_SWORD,    "blessed long sword",    11,  0, 13, 160,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_SLICING, 0 },
    { WPN_SCIMITAR,              "scimitar",              12, -2, 14, 170,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_SLICING, 10 },
    { WPN_BLESSED_SCIMITAR,      "blessed scimitar",      13, -3, 13, 170,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_SLICING, 0 },
    { WPN_DEMON_BLADE,           "demon blade",           13, -1, 13, 200,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_SLICING, 2 },
    { WPN_EUDEMON_BLADE,         "eudemon blade",         14, -2, 12, 200,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_SLICING, 0 },
    { WPN_DOUBLE_SWORD,          "double sword",          15, -1, 15, 220,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 2 },
    { WPN_BLESSED_DOUBLE_SWORD,  "blessed double sword",  16, -2, 14, 220,  3,
        SK_LONG_BLADES,  HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_SLICING, 0 },
    { WPN_GREAT_SWORD,           "great sword",           16, -3, 16, 250,  5,
        SK_LONG_BLADES,  HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 10 },
    { WPN_BLESSED_GREAT_SWORD,   "blessed great sword",   17, -4, 15, 250,  5,
        SK_LONG_BLADES,  HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 0 },
    { WPN_TRIPLE_SWORD,          "triple sword",          19, -4, 19, 260,  5,
        SK_LONG_BLADES,  HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 2 },
    { WPN_BLESSED_TRIPLE_SWORD,  "blessed triple sword",  20, -5, 18, 260,  5,
        SK_LONG_BLADES,  HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 0 },

    // Axes
    { WPN_HAND_AXE,          "hand axe",            7,  3, 13,  80,  6,
        SK_AXES,         HANDS_ONE,    SIZE_LITTLE, MI_NONE, true,
        DAMV_CHOPPING, 10 },
    { WPN_WAR_AXE,           "war axe",            11,  0, 15, 180,  7,
        SK_AXES,         HANDS_ONE,    SIZE_SMALL,  MI_NONE, false,
        DAMV_CHOPPING, 10 },
    { WPN_BROAD_AXE,         "broad axe",          13, -2, 16, 230,  8,
        SK_AXES,         HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_CHOPPING, 10 },
    { WPN_BATTLEAXE,         "battleaxe",          15, -4, 17, 250,  8,
        SK_AXES,         HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CHOPPING, 10 },
    { WPN_EXECUTIONERS_AXE,  "executioner's axe",  18, -6, 20, 280,  9,
        SK_AXES,         HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CHOPPING, 2 },

    // Polearms
    { WPN_SPEAR,             "spear",               6,  4, 11,  50,  3,
        SK_POLEARMS,     HANDS_ONE,    SIZE_SMALL,  MI_NONE, true,
        DAMV_PIERCING, 10 },
    { WPN_TRIDENT,           "trident",             9,  1, 13, 160,  4,
        SK_POLEARMS,     HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_PIERCING, 10 },
    { WPN_HALBERD,           "halberd",            13, -3, 15, 200,  5,
        SK_POLEARMS,     HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CHOPPING | DAM_PIERCE, 10 },
    { WPN_SCYTHE,            "scythe",             14, -4, 20, 220,  7,
        SK_POLEARMS,     HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 0 },
    { WPN_DEMON_TRIDENT,     "demon trident",      12,  1, 13, 160,  4,
        SK_POLEARMS,     HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_PIERCING, 2 },
    { WPN_TRISHULA,          "trishula",           13,  0, 13, 160,  4,
        SK_POLEARMS,     HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_PIERCING, 0 },
    { WPN_GLAIVE,            "glaive",             15, -3, 17, 200,  6,
        SK_POLEARMS,     HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CHOPPING, 10 },
    { WPN_BARDICHE,          "bardiche",           18, -6, 20, 200,  8,
        SK_POLEARMS,     HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CHOPPING, 2 },

    // Staves
    // WPN_STAFF is for weapon stats for magical staves only.
    { WPN_STAFF,             "staff",               5,  5, 12, 150,  3,
        SK_STAVES,       HANDS_ONE,    SIZE_MEDIUM, MI_NONE, false,
        DAMV_CRUSHING, 0 },
    { WPN_QUARTERSTAFF,      "quarterstaff",        10, 3, 13, 180,  3,
        SK_STAVES,       HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_CRUSHING, 10 },
    { WPN_LAJATANG,          "lajatang",            16,-3, 14, 200,  3,
        SK_STAVES,       HANDS_TWO,    SIZE_LARGE,  MI_NONE, false,
        DAMV_SLICING, 2 },

    // Range weapons
    // Notes:
    // - damage field is used for bonus strength damage (string tension)
    // - slings get a bonus from dex, not str (as tension is meaningless)
    // - str weight is used for speed and applying dex to skill
    { WPN_BLOWGUN,           "blowgun",             0,  2, 10,  20,  0,
        SK_THROWING,     HANDS_ONE,    SIZE_LITTLE, MI_NEEDLE, false,
        DAMV_NON_MELEE, 0 },
    { WPN_SLING,             "sling",               0,  2, 11,  20,  1,
        SK_SLINGS,       HANDS_ONE,    SIZE_LITTLE, MI_STONE, false,
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
static const missile_def Missile_prop[NUM_MISSILES] =
{
    { MI_NEEDLE,        "needle",        0,    1, false },
    { MI_STONE,         "stone",         4,    6, true  },
    { MI_DART,          "dart",          2,    3, true  },
    { MI_ARROW,         "arrow",         7,    5, false },
    { MI_BOLT,          "bolt",          9,    5, false },
    { MI_LARGE_ROCK,    "large rock",   20,  600, true  },
    { MI_SLING_BULLET,  "sling bullet",  6,    4, false },
    { MI_JAVELIN,       "javelin",      10,   80, true  },
    { MI_THROWING_NET,  "throwing net",  0,   30, true  },
#if TAG_MAJOR_VERSION == 34
    { MI_PIE,           "pie",           2,    6, true  },
#endif
};

enum food_flag_type
{
    FFL_NONE  = 0x00,
    FFL_FRUIT = 0x01,
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
    uint32_t    flags;
};

// NOTE: Any food with special random messages or side effects
// currently only takes one turn to eat (except ghouls and chunks)...
// If this changes then those items will have to have special code
// (like ghoul chunks) to guarantee that the special thing is only
// done once.  See the ghoul eating code over in food.cc.
static int Food_index[NUM_FOODS];
static const food_def Food_prop[NUM_FOODS] =
{
    { FOOD_MEAT_RATION,  "meat ration",  5000,   500, -1500,  80, 4, FFL_NONE },
    { FOOD_SAUSAGE,      "sausage",      1200,   150,  -400,  40, 2, FFL_NONE },
    { FOOD_CHUNK,        "chunk",        1000,   100,  -500, 100, 3, FFL_NONE },
    { FOOD_BEEF_JERKY,   "beef jerky",   1500,   200,  -200,  20, 2, FFL_NONE },

    { FOOD_BREAD_RATION, "bread ration", 4400, -1000,   500,  80, 4, FFL_NONE },

    { FOOD_SNOZZCUMBER,  "snozzcumber",  1500,  -500,   500,  50, 2, FFL_FRUIT},
    { FOOD_ORANGE,       "orange",       1000,  -300,   300,  20, 2, FFL_FRUIT},
    { FOOD_BANANA,       "banana",       1000,  -300,   300,  20, 2, FFL_FRUIT},
    { FOOD_LEMON,        "lemon",        1000,  -300,   300,  20, 2, FFL_FRUIT},
    { FOOD_PEAR,         "pear",          700,  -200,   200,  20, 2, FFL_FRUIT},
    { FOOD_APPLE,        "apple",         700,  -200,   200,  20, 2, FFL_FRUIT},
    { FOOD_APRICOT,      "apricot",       700,  -200,   200,  15, 2, FFL_FRUIT},
    { FOOD_CHOKO,        "choko",         600,  -200,   200,  30, 2, FFL_FRUIT},
    { FOOD_RAMBUTAN,     "rambutan",      600,  -200,   200,  10, 2, FFL_FRUIT},
    { FOOD_LYCHEE,       "lychee",        600,  -200,   200,  10, 2, FFL_FRUIT},
    { FOOD_STRAWBERRY,   "strawberry",    200,   -50,    50,   5, 2, FFL_FRUIT},
    { FOOD_GRAPE,        "grape",         100,   -20,    20,   2, 1, FFL_FRUIT},
    { FOOD_SULTANA,      "sultana",        70,   -20,    20,   1, 1, FFL_FRUIT},

    { FOOD_ROYAL_JELLY,  "royal jelly",  5000,     0,     0,  55, 2, FFL_NONE },
    { FOOD_HONEYCOMB,    "honeycomb",    2000,     0,     0,  40, 2, FFL_NONE },
    { FOOD_PIZZA,        "pizza",        1500,     0,     0,  40, 2, FFL_NONE },
    { FOOD_CHEESE,       "cheese",       1200,     0,     0,  40, 2, FFL_NONE },
    { FOOD_AMBROSIA,     "ambrosia",     2500,     0,     0,  40, 1, FFL_NONE },
};

// Must call this functions early on so that the above tables can
// be accessed correctly.
void init_properties()
{
    // The compiler would complain about too many initializers but not
    // about too few, check it by hand:
    COMPILE_CHECK(NUM_ARMOURS  == ARRAYSZ(Armour_prop));
    COMPILE_CHECK(NUM_WEAPONS  == ARRAYSZ(Weapon_prop));
    COMPILE_CHECK(NUM_MISSILES == ARRAYSZ(Missile_prop));
    COMPILE_CHECK(NUM_FOODS    == ARRAYSZ(Food_prop));

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
// gets changed again. - bwr

//
// Item cursed status functions:
//
bool item_known_cursed(const item_def &item)
{
    return (_full_ident_mask(item) & ISFLAG_KNOW_CURSE
            && item_ident(item, ISFLAG_KNOW_CURSE) && item.cursed());
}

static bool _item_known_uncursed(const item_def &item)
{
    return (!(_full_ident_mask(item) & ISFLAG_KNOW_CURSE)
            || (item_ident(item, ISFLAG_KNOW_CURSE) && !item.cursed()));
}

void do_curse_item(item_def &item, bool quiet)
{
    // Already cursed?
    if (item.flags & ISFLAG_CURSED)
        return;

    if (!is_weapon(item) && item.base_type != OBJ_ARMOUR
        && item.base_type != OBJ_JEWELLERY)
    {
        return;
    }

    // Holy wrath weapons cannot be cursed.
    if (item.base_type == OBJ_WEAPONS
        && get_weapon_brand(item) == SPWPN_HOLY_WRATH)
    {
        if (!quiet)
        {
            mprf("Your %s glows black briefly, but repels the curse.",
                 item.name(DESC_PLAIN).c_str());
        }
        return;
    }

    if (!quiet)
    {
        mprf("Your %s glows black for a moment.",
             item.name(DESC_PLAIN).c_str());

        // If we get the message, we know the item is cursed now.
        item.flags |= ISFLAG_KNOW_CURSE;
        item.flags |= ISFLAG_SEEN_CURSED;
    }

    // This might prevent a carried item from allowing training.
    maybe_change_train(item, false);

    // If the item isn't equipped, you might not be able to use it for training.
    if (!item_is_equipped(item))
        item_skills(item, you.stop_train);

    item.flags |= ISFLAG_CURSED;

    // Xom is amused by the player's items being cursed, especially if
    // they're worn/equipped.
    if (in_inventory(item))
    {
        int amusement = 50;

        if (item_is_equipped(item))
        {
            amusement *= 2;

            // Cursed cloaks prevent you from removing body armour,
            // gloves from switching rings.
            if (item.base_type == OBJ_ARMOUR
                && (get_armour_slot(item) == EQ_CLOAK
                    || get_armour_slot(item) == EQ_GLOVES))
            {
                amusement *= 2;
            }
            else if (you.equip[EQ_WEAPON] == item.link)
            {
                // Redraw the weapon.
                you.wield_change = true;
            }

            ash_check_bondage();
            auto_id_inventory();
        }

        xom_is_stimulated(amusement);
    }
}

void do_uncurse_item(item_def &item, bool inscribe, bool no_ash,
                     bool check_bondage)
{
    if (!item.cursed())
    {
        item.flags &= ~ISFLAG_SEEN_CURSED;
        if (in_inventory(item))
            item.flags |= ISFLAG_KNOW_CURSE;
        return;
    }

    if (no_ash && you_worship(GOD_ASHENZARI))
    {
        simple_god_message(" preserves the curse.");
        return;
    }

    if (inscribe && Options.autoinscribe_cursed
        && item.inscription.find("was cursed") == string::npos
        && !item_ident(item, ISFLAG_SEEN_CURSED)
        && !fully_identified(item))
    {
        add_inscription(item, "was cursed");
    }

    if (in_inventory(item))
    {
        if (you.equip[EQ_WEAPON] == item.link)
        {
            // Redraw the weapon.
            you.wield_change = true;
        }
        item.flags |= ISFLAG_KNOW_CURSE;
    }
    item.flags &= (~ISFLAG_CURSED);
    item.flags &= (~ISFLAG_SEEN_CURSED);

    // This might allow training for a carried item.
    maybe_change_train(item, true);

    if (check_bondage)
        ash_check_bondage();
}

// Is item stationary (cannot be picked up)?
void set_item_stationary(item_def &item)
{
    if (item.base_type == OBJ_MISSILES && item.sub_type == MI_THROWING_NET)
        item.plus2 = 1;
}

void remove_item_stationary(item_def &item)
{
    if (item.base_type == OBJ_MISSILES && item.sub_type == MI_THROWING_NET)
        item.plus2 = 0;
}

bool item_is_stationary(const item_def &item)
{
    return (item.base_type == OBJ_MISSILES
            && item.sub_type == MI_THROWING_NET
            && item.plus2);
}

static bool _in_shop(const item_def &item)
{
    // yay the shop hack...
    return (item.pos.x == 0 && item.pos.y >= 5);
}

static bool _is_affordable(const item_def &item)
{
    // Temp items never count.
    if (item.flags & ISFLAG_SUMMONED)
        return false;

    // Already in our grubby mitts.
    if (in_inventory(item))
        return true;

    // Disregard shop stuff above your reach.
    if (_in_shop(item))
        return (int)item_value(item) <= you.gold;

    // Explicitly marked by a vault.
    if (item.flags & ISFLAG_UNOBTAINABLE)
        return false;

    // On the ground or a monster has it.  Violence is the answer.
    return true;
}

//
// Item identification status:
//
bool item_ident(const item_def &item, iflags_t flags)
{
    return ((item.flags & flags) == flags);
}

void set_ident_flags(item_def &item, iflags_t flags)
{
    preserve_quiver_slots p;
    if ((item.flags & flags) != flags)
    {
        bool was_randapp = is_randapp_artefact(item);
        item.flags |= flags;
        if (was_randapp && (flags & ISFLAG_KNOW_TYPE))
            reveal_randapp_artefact(item);
        request_autoinscribe();

        if (in_inventory(item))
        {
            shopping_list.cull_identical_items(item);
            item_skills(item, you.start_train);
        }
    }

    if (fully_identified(item))
    {
        // Clear "was cursed" inscription once the item is identified.
        if (Options.autoinscribe_cursed
            && item.inscription.find("was cursed") != string::npos)
        {
            item.inscription = replace_all(item.inscription, ", was cursed", "");
            item.inscription = replace_all(item.inscription, "was cursed, ", "");
            item.inscription = replace_all(item.inscription, "was cursed", "");
            trim_string(item.inscription);
        }

        if (notes_are_active() && !(item.flags & ISFLAG_NOTED_ID)
            && get_ident_type(item) != ID_KNOWN_TYPE
            && is_interesting_item(item))
        {
            // Make a note of it.
            take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_A).c_str(),
                           origin_desc(item).c_str()));

            // Sometimes (e.g. shops) you can ID an item before you get it;
            // don't note twice in those cases.
            item.flags |= (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
        }
    }

    if (item.flags & ISFLAG_KNOW_TYPE && !is_artefact(item)
        && _is_affordable(item))
    {
        if (item.base_type == OBJ_WEAPONS)
            you.seen_weapon[item.sub_type] |= 1 << item.special;
        if (item.base_type == OBJ_ARMOUR)
            you.seen_armour[item.sub_type] |= 1 << item.special;
        if (item.base_type == OBJ_MISCELLANY)
            you.seen_misc.set(item.sub_type);
    }
}

void unset_ident_flags(item_def &item, iflags_t flags)
{
    preserve_quiver_slots p;
    item.flags &= (~flags);
}

// Returns the mask of interesting identify bits for this item
// (e.g., scrolls don't have know-cursedness).
static iflags_t _full_ident_mask(const item_def& item)
{
    // KNOW_PROPERTIES is only relevant for artefacts, handled later.
    iflags_t flagset = ISFLAG_IDENT_MASK & ~ISFLAG_KNOW_PROPERTIES;
    switch (item.base_type)
    {
    case OBJ_FOOD:
    case OBJ_CORPSES:
    case OBJ_MISSILES:
    case OBJ_ORBS:
        flagset = 0;
        break;
    case OBJ_BOOKS:
        if (item.sub_type == BOOK_DESTRUCTION)
        {
            flagset = 0;
            break;
        }
        // Intentional fall-through.
    case OBJ_SCROLLS:
    case OBJ_POTIONS:
        flagset = ISFLAG_KNOW_TYPE;
        break;
    case OBJ_STAVES:
        flagset = ISFLAG_KNOW_TYPE | ISFLAG_KNOW_CURSE;
        break;
    case OBJ_WANDS:
        flagset = (ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PLUSES);
        break;
    case OBJ_JEWELLERY:
        flagset = (ISFLAG_KNOW_CURSE | ISFLAG_KNOW_TYPE);
        if (ring_has_pluses(item))
            flagset |= ISFLAG_KNOW_PLUSES;
        break;
    case OBJ_MISCELLANY:
        if (is_deck(item))
            flagset = ISFLAG_KNOW_TYPE;
        else
            flagset = 0;
        break;
    case OBJ_RODS:
    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
        // All flags necessary for full identification.
    default:
        break;
    }

    if (item_type_known(item.base_type, item.sub_type) && !is_artefact(item))
        flagset &= (~ISFLAG_KNOW_TYPE);

    if (is_artefact(item))
        flagset |= ISFLAG_KNOW_PROPERTIES;

    return flagset;
}

bool fully_identified(const item_def& item)
{
    return item_ident(item, _full_ident_mask(item));
}

//
// Equipment race and description:
//
iflags_t get_equip_race(const item_def &item)
{
    return (item.flags & ISFLAG_RACIAL_MASK);
}

iflags_t get_equip_desc(const item_def &item)
{
    return (item.flags & ISFLAG_COSMETIC_MASK);
}

void set_equip_race(item_def &item, iflags_t flags)
{
    ASSERT((flags & ~ISFLAG_RACIAL_MASK) == 0);

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
            if (item.sub_type == ARM_PLATE_ARMOUR || is_hard_helmet(item))
                return;
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
                || item.sub_type == WPN_LONGBOW)
            {
                return;
            }
            break;
        case OBJ_ARMOUR:
            if (get_armour_slot(item) == EQ_HELMET && !is_hard_helmet(item)
                && item.sub_type != ARM_WIZARD_HAT)
            {
                return;
            }
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

void set_equip_desc(item_def &item, iflags_t flags)
{
    ASSERT((flags & ~ISFLAG_COSMETIC_MASK) == 0);

    item.flags &= ~ISFLAG_COSMETIC_MASK; // delete previous
    item.flags |= flags;
}

iflags_t get_species_race(species_type sp)
{
    return you.species == SP_DEEP_DWARF ? ISFLAG_DWARVEN :
           player_genus(GENPC_ELVEN)    ? ISFLAG_ELVEN :
           player_genus(GENPC_ORCISH)   ? ISFLAG_ORCISH
                                        : 0;
}

//
// These functions handle the description and subtypes for helmets/caps.
//
short get_helmet_desc(const item_def &item)
{
    ASSERT(is_helmet(item));

    return item.plus2;
}

bool is_helmet(const item_def& item)
{
    return (item.base_type == OBJ_ARMOUR && get_armour_slot(item) == EQ_HELMET);
}

bool is_hard_helmet(const item_def &item)
{
    return (item.base_type == OBJ_ARMOUR && item.sub_type == ARM_HELMET);
}

void set_helmet_random_desc(item_def &item)
{
    ASSERT(is_helmet(item));

    if (is_hard_helmet(item))
        item.plus2 = random2(THELM_NUM_DESCS);
    else
        item.plus2 = random2(THELM_DESC_MAX_SOFT + 1);
}

short get_gloves_desc(const item_def &item)
{
    ASSERT(item.base_type == OBJ_ARMOUR);
    ASSERT(item.sub_type == ARM_GLOVES);
    return item.plus2;
}

void set_gloves_random_desc(item_def &item)
{
    ASSERT(item.base_type == OBJ_ARMOUR);
    ASSERT(item.sub_type == ARM_GLOVES);

    item.plus2 = coinflip() ? TGLOV_DESC_GLOVES : TGLOV_DESC_GAUNTLETS;
    if (get_armour_ego_type(item) == SPARM_ARCHERY)
    {
        item.plus2 = TGLOV_DESC_BRACERS;
        set_ident_flags(item, ISFLAG_KNOW_TYPE);
    }
}

//
// Ego item functions:
//
bool set_item_ego_type(item_def &item, object_class_type item_type,
                       int ego_type)
{
    if (item.base_type == item_type && !is_artefact(item))
    {
        item.special = ego_type;
        return true;
    }

    return false;
}

brand_type get_weapon_brand(const item_def &item)
{
    // Weapon ego types are "brands", so we do the randart lookup here.

    // Staves "brands" handled specially
    if (item.base_type != OBJ_WEAPONS)
        return SPWPN_NORMAL;

    if (is_artefact(item))
        return static_cast<brand_type>(artefact_wpn_property(item, ARTP_BRAND));

    return static_cast<brand_type>(item.special);
}

special_missile_type get_ammo_brand(const item_def &item)
{
    // No artefact arrows yet. -- bwr
    if (item.base_type != OBJ_MISSILES || is_artefact(item))
        return SPMSL_NORMAL;

    return static_cast<special_missile_type>(item.special);
}

special_armour_type get_armour_ego_type(const item_def &item)
{
    // Armour ego types are "brands", so we do the randart lookup here.
    if (item.base_type != OBJ_ARMOUR)
        return SPARM_NORMAL;

    if (is_artefact(item))
    {
        return static_cast<special_armour_type>(
                   artefact_wpn_property(item, ARTP_BRAND));
    }

    return static_cast<special_armour_type>(item.special);
}

// Armour information and checking functions.
bool hide2armour(item_def &item)
{
    if (item.base_type != OBJ_ARMOUR)
        return false;

    switch (item.sub_type)
    {
    default:
        return false;

    case ARM_FIRE_DRAGON_HIDE:
        item.sub_type = ARM_FIRE_DRAGON_ARMOUR;
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

    case ARM_PEARL_DRAGON_HIDE:
        item.sub_type = ARM_PEARL_DRAGON_ARMOUR;
        break;
    }

    return true;
}

// Return the enchantment limit of a piece of armour.
int armour_max_enchant(const item_def &item)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    const int eq_slot = get_armour_slot(item);

    int max_plus = MAX_SEC_ENCHANT;
    if (eq_slot == EQ_BODY_ARMOUR
        || item.sub_type == ARM_CENTAUR_BARDING
        || item.sub_type == ARM_NAGA_BARDING)
        max_plus = property(item, PARM_AC);
    else if (eq_slot == EQ_SHIELD)
        max_plus = 3;

    return max_plus;
}

// Doesn't include animal skin (only skins we can make and enchant).
bool armour_is_hide(const item_def &item, bool inc_made)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    switch (item.sub_type)
    {
    case ARM_TROLL_LEATHER_ARMOUR:
    case ARM_FIRE_DRAGON_ARMOUR:
    case ARM_ICE_DRAGON_ARMOUR:
    case ARM_STEAM_DRAGON_ARMOUR:
    case ARM_MOTTLED_DRAGON_ARMOUR:
    case ARM_STORM_DRAGON_ARMOUR:
    case ARM_GOLD_DRAGON_ARMOUR:
    case ARM_SWAMP_DRAGON_ARMOUR:
    case ARM_PEARL_DRAGON_ARMOUR:
        return inc_made;

    case ARM_TROLL_HIDE:
    case ARM_FIRE_DRAGON_HIDE:
    case ARM_ICE_DRAGON_HIDE:
    case ARM_STEAM_DRAGON_HIDE:
    case ARM_MOTTLED_DRAGON_HIDE:
    case ARM_STORM_DRAGON_HIDE:
    case ARM_GOLD_DRAGON_HIDE:
    case ARM_SWAMP_DRAGON_HIDE:
    case ARM_PEARL_DRAGON_HIDE:
        return true;

    default:
        break;
    }

    return false;
}

equipment_type get_armour_slot(const item_def &item)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    return Armour_prop[ Armour_index[item.sub_type] ].slot;
}

equipment_type get_armour_slot(armour_type arm)
{
    return Armour_prop[ Armour_index[arm] ].slot;
}

bool jewellery_is_amulet(const item_def &item)
{
    ASSERT(item.base_type == OBJ_JEWELLERY);

    return (item.sub_type >= AMU_RAGE);
}

bool jewellery_is_amulet(int sub_type)
{
    return (sub_type >= AMU_RAGE);
}

// Returns number of sizes off (0 if fitting).
int fit_armour_size(const item_def &item, size_type size)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    const size_type min = Armour_prop[ Armour_index[item.sub_type] ].fit_min;
    const size_type max = Armour_prop[ Armour_index[item.sub_type] ].fit_max;

    if (size < min)
        return (min - size);    // -'ve means levels too small
    else if (size > max)
        return (max - size);    // +'ve means levels too large

    return 0;
}

// Returns true if armour fits size (shape needs additional verification).
bool check_armour_size(const item_def &item, size_type size)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    return (fit_armour_size(item, size) == 0);
}

// Returns whether a wand or rod can be charged.
// If hide_charged is true, wands known to be full will return false.
// (This distinction is necessary because even full wands/rods give a message.)
bool item_is_rechargeable(const item_def &it, bool hide_charged)
{
    // These are obvious...
    if (it.base_type == OBJ_WANDS)
    {
        if (!hide_charged)
            return true;

        // Don't offer wands already maximally charged.
        if (item_ident(it, ISFLAG_KNOW_PLUSES)
            && it.plus >= wand_max_charges(it.sub_type))
        {
            return false;
        }
        return true;
    }
    else if (it.base_type == OBJ_RODS)
    {
        if (item_is_melded(it))
            return false;

        if (!hide_charged)
            return true;

        if (item_ident(it, ISFLAG_KNOW_PLUSES))
        {
            return (it.plus2 < MAX_ROD_CHARGE * ROD_CHARGE_MULT
                    || it.plus < it.plus2
                    || it.special < MAX_WPN_ENCHANT);
        }
        return true;
    }

    return false;
}

int wand_charge_value(int type)
{
    switch (type)
    {
    case WAND_INVISIBILITY:
    case WAND_FIREBALL:
    case WAND_TELEPORTATION:
    case WAND_HEAL_WOUNDS:
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

int wand_max_charges(int type)
{
    return wand_charge_value(type) * 3;
}

bool is_offensive_wand(const item_def& item)
{
    switch (item.sub_type)
    {
    // Monsters don't use those, so no need to warn the player about them.
    case WAND_ENSLAVEMENT:
    case WAND_RANDOM_EFFECTS:
    case WAND_DIGGING:

    // Monsters will use them on themselves.
    case WAND_HASTING:
    case WAND_HEAL_WOUNDS:
    case WAND_INVISIBILITY:
        return false;

    case WAND_FLAME:
    case WAND_FROST:
    case WAND_SLOWING:
    case WAND_MAGIC_DARTS:
    case WAND_PARALYSIS:
    case WAND_FIRE:
    case WAND_COLD:
    case WAND_CONFUSION:
    case WAND_FIREBALL:
    case WAND_TELEPORTATION:
    case WAND_LIGHTNING:
    case WAND_POLYMORPH:
    case WAND_DRAINING:
    case WAND_DISINTEGRATION:
        return true;
    }
    return false;
}

// Returns whether a piece of armour can be enchanted further.
// If unknown is true, unidentified armour will return true.
bool is_enchantable_armour(const item_def &arm, bool uncurse, bool unknown)
{
    if (arm.base_type != OBJ_ARMOUR)
        return false;

    // Melded armour cannot be enchanted.
    if (item_is_melded(arm))
        return false;

    // If we don't know the plusses, assume enchanting is possible.
    if (unknown && !is_known_artefact(arm)
        && !item_ident(arm, ISFLAG_KNOW_PLUSES))
    {
        return true;
    }

    // Artefacts or highly enchanted armour cannot be enchanted, only
    // uncursed.
    if (is_artefact(arm) || arm.plus >= armour_max_enchant(arm))
        return (uncurse && arm.cursed() && !you_worship(GOD_ASHENZARI));

    return true;
}

//
// Weapon information and checking functions:
//

// Checks how rare a weapon is. Many of these have special routines for
// placement, especially those with a rarity of zero. Chance is out of 10.
int weapon_rarity(int w_type)
{
    switch (w_type)
    {
    case WPN_CLUB:
    case WPN_DAGGER:
        return 10;

    case WPN_HAND_AXE:
    case WPN_MACE:
        return 9;

    case WPN_BOW:
    case WPN_FLAIL:
    case WPN_SABRE:
    case WPN_SHORT_SWORD:
    case WPN_SLING:
    case WPN_SPEAR:
    case WPN_QUARTERSTAFF:
        return 8;

    case WPN_FALCHION:
    case WPN_LONG_SWORD:
    case WPN_MORNINGSTAR:
    case WPN_WAR_AXE:
        return 7;

    case WPN_BATTLEAXE:
    case WPN_CROSSBOW:
    case WPN_GREAT_SWORD:
    case WPN_SCIMITAR:
    case WPN_TRIDENT:
        return 6;

    case WPN_GLAIVE:
    case WPN_HALBERD:
    case WPN_BLOWGUN:
        return 5;

    case WPN_BROAD_AXE:
    case WPN_WHIP:
        return 4;

    case WPN_GREAT_MACE:
        return 3;

    case WPN_DIRE_FLAIL:
    case WPN_SCYTHE:
    case WPN_LONGBOW:
    case WPN_LAJATANG:
        return 2;

    case WPN_GIANT_CLUB:
    case WPN_GIANT_SPIKED_CLUB:
    case WPN_BARDICHE:
        return 1;

    case WPN_DOUBLE_SWORD:
    case WPN_EVENINGSTAR:
    case WPN_EXECUTIONERS_AXE:
    case WPN_QUICK_BLADE:
    case WPN_TRIPLE_SWORD:
    case WPN_DEMON_WHIP:
    case WPN_DEMON_BLADE:
    case WPN_DEMON_TRIDENT:
    case WPN_BLESSED_FALCHION:
    case WPN_BLESSED_LONG_SWORD:
    case WPN_BLESSED_SCIMITAR:
    case WPN_EUDEMON_BLADE:
    case WPN_BLESSED_DOUBLE_SWORD:
    case WPN_BLESSED_GREAT_SWORD:
    case WPN_BLESSED_TRIPLE_SWORD:
    case WPN_SACRED_SCOURGE:
    case WPN_TRISHULA:
    case WPN_STAFF:
    case WPN_ROD:
    case WPN_HAMMER:
        // Zero value weapons must be placed specially -- see make_item() {dlb}
        return 0;

    default:
        break;
    }

    return 0;
}

int get_vorpal_type(const item_def &item)
{
    int ret = DVORP_NONE;

    if (item.base_type == OBJ_WEAPONS)
        ret = (Weapon_prop[Weapon_index[item.sub_type]].dam_type & DAMV_MASK);

    return ret;
}

int get_damage_type(const item_def &item)
{
    int ret = DAM_BASH;

    if (item.base_type == OBJ_RODS)
        ret = DAM_BLUDGEON;
    if (item.base_type == OBJ_WEAPONS)
        ret = (Weapon_prop[Weapon_index[item.sub_type]].dam_type & DAM_MASK);

    return ret;
}

static bool _does_damage_type(const item_def &item, int dam_type)
{
    return (get_damage_type(item) & dam_type);
}

int single_damage_type(const item_def &item)
{
    int ret = get_damage_type(item);

    if (ret > 0)
    {
        int count = 0;

        for (int i = 1; i <= DAM_MAX_TYPE; i <<= 1)
        {
            if (!_does_damage_type(item, i))
                continue;

            if (one_chance_in(++count))
                ret = i;
        }
    }

    return ret;
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
hands_reqd_type hands_reqd(const item_def &item, size_type size)
{
    hands_reqd_type ret = HANDS_ONE;

    switch (item.base_type)
    {
    case OBJ_STAVES:
        if (size < SIZE_MEDIUM)
            ret = HANDS_TWO;
        else
            ret = HANDS_ONE;
        break;

    case OBJ_WEAPONS:
        ret = Weapon_prop[ Weapon_index[item.sub_type] ].hands;
        // Adjust handedness only for small races using melee weapons
        // that are larger than they are.
        if (!is_range_weapon(item)
            && size < SIZE_MEDIUM
            && Weapon_prop[Weapon_index[item.sub_type]].fit_size > size)
        {
            ret = HANDS_TWO;
        }
        break;

    case OBJ_CORPSES:   // unwieldy
        ret = HANDS_TWO;
        break;

    case OBJ_ARMOUR:    // Bardings and body armours are unwieldy.
        if (item.sub_type == ARM_NAGA_BARDING
            || item.sub_type == ARM_CENTAUR_BARDING
            || get_armour_slot(item) == EQ_BODY_ARMOUR)
        {
            ret = HANDS_TWO;
        }
        break;

    default:
        break;
    }
    return ret;
}

bool is_giant_club_type(int wpn_type)
{
    return (wpn_type == WPN_GIANT_CLUB
            || wpn_type == WPN_GIANT_SPIKED_CLUB);
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
            return true;

        default:
            break;
        }
    }

    return false;
}

bool is_blessed(const item_def &item)
{
    if (item.base_type == OBJ_WEAPONS)
    {
        switch (item.sub_type)
        {
        case WPN_BLESSED_FALCHION:
        case WPN_BLESSED_LONG_SWORD:
        case WPN_BLESSED_SCIMITAR:
        case WPN_EUDEMON_BLADE:
        case WPN_BLESSED_DOUBLE_SWORD:
        case WPN_BLESSED_GREAT_SWORD:
        case WPN_BLESSED_TRIPLE_SWORD:
        case WPN_SACRED_SCOURGE:
        case WPN_TRISHULA:
            return true;

        default:
            break;
        }
    }

    return false;
}

bool is_blessed_convertible(const item_def &item)
{
    return (!is_artefact(item)
            && (item.base_type == OBJ_WEAPONS
                && (is_demonic(item)
                    || item.sub_type == WPN_SACRED_SCOURGE
                    || item.sub_type == WPN_TRISHULA
                    || weapon_skill(item) == SK_LONG_BLADES)));
}

bool convert2good(item_def &item)
{
    if (item.base_type != OBJ_WEAPONS)
        return false;

    switch (item.sub_type)
    {
    default: return false;

    case WPN_FALCHION:      item.sub_type = WPN_BLESSED_FALCHION; break;
    case WPN_LONG_SWORD:    item.sub_type = WPN_BLESSED_LONG_SWORD; break;
    case WPN_SCIMITAR:      item.sub_type = WPN_BLESSED_SCIMITAR; break;
    case WPN_DEMON_BLADE:   item.sub_type = WPN_EUDEMON_BLADE; break;
    case WPN_DOUBLE_SWORD:  item.sub_type = WPN_BLESSED_DOUBLE_SWORD; break;
    case WPN_GREAT_SWORD:   item.sub_type = WPN_BLESSED_GREAT_SWORD; break;
    case WPN_TRIPLE_SWORD:  item.sub_type = WPN_BLESSED_TRIPLE_SWORD; break;
    case WPN_DEMON_WHIP:    item.sub_type = WPN_SACRED_SCOURGE; break;
    case WPN_DEMON_TRIDENT: item.sub_type = WPN_TRISHULA; break;
    }

    if (is_blessed(item))
        item.flags &= ~ISFLAG_RACIAL_MASK;

    return true;
}

bool convert2bad(item_def &item)
{
    if (item.base_type != OBJ_WEAPONS)
        return false;

    switch (item.sub_type)
    {
    default: return false;

    case WPN_BLESSED_FALCHION:     item.sub_type = WPN_FALCHION; break;
    case WPN_BLESSED_LONG_SWORD:   item.sub_type = WPN_LONG_SWORD; break;
    case WPN_BLESSED_SCIMITAR:     item.sub_type = WPN_SCIMITAR; break;
    case WPN_EUDEMON_BLADE:        item.sub_type = WPN_DEMON_BLADE; break;
    case WPN_BLESSED_DOUBLE_SWORD: item.sub_type = WPN_DOUBLE_SWORD; break;
    case WPN_BLESSED_GREAT_SWORD:  item.sub_type = WPN_GREAT_SWORD; break;
    case WPN_BLESSED_TRIPLE_SWORD: item.sub_type = WPN_TRIPLE_SWORD; break;
    case WPN_SACRED_SCOURGE:       item.sub_type = WPN_DEMON_WHIP; break;
    case WPN_TRISHULA:             item.sub_type = WPN_DEMON_TRIDENT; break;
    }

    return true;
}

bool is_brandable_weapon(const item_def &wpn, bool allow_ranged)
{
    if (wpn.base_type != OBJ_WEAPONS)
        return false;

    if (is_artefact(wpn))
        return false;

    if (!allow_ranged && is_range_weapon(wpn)
        || wpn.sub_type == WPN_BLOWGUN)
    {
        return false;
    }

    return true;
}

int weapon_str_weight(const item_def &wpn)
{
    if (!is_weapon(wpn))
        return 5;

    if (wpn.base_type == OBJ_STAVES)
        return Weapon_prop[ Weapon_index[WPN_STAFF] ].str_weight;

    if (wpn.base_type == OBJ_RODS)
        return Weapon_prop[ Weapon_index[WPN_ROD] ].str_weight;

    return Weapon_prop[ Weapon_index[wpn.sub_type] ].str_weight;
}

// Returns melee skill of item.
skill_type weapon_skill(const item_def &item)
{
    if (item.base_type == OBJ_WEAPONS && !is_range_weapon(item))
        return Weapon_prop[ Weapon_index[item.sub_type] ].skill;
    else if (item.base_type == OBJ_RODS)
        return SK_MACES_FLAILS; // Rods are short and stubby
    else if (item.base_type == OBJ_STAVES)
        return SK_STAVES;

    // This is used to mark that only fighting applies.
    return SK_FIGHTING;
}

// Front function for the above when we don't have a physical item to check.
skill_type weapon_skill(object_class_type wclass, int wtype)
{
    item_def    wpn;

    wpn.base_type = wclass;
    wpn.sub_type = wtype;

    return weapon_skill(wpn);
}

// Returns range skill of the item.
skill_type range_skill(const item_def &item)
{
    if (item.base_type == OBJ_WEAPONS && is_range_weapon(item))
        return Weapon_prop[ Weapon_index[item.sub_type] ].skill;
    else if (item.base_type == OBJ_MISSILES)
    {
        if (!has_launcher(item))
            return SK_THROWING;
    }

    return SK_THROWING;
}

// Front function for the above when we don't have a physical item to check.
skill_type range_skill(object_class_type wclass, int wtype)
{
    item_def    wpn;

    wpn.base_type = wclass;
    wpn.sub_type = wtype;

    return range_skill(wpn);
}

// Check whether an item can be easily and quickly equipped. This needs to
// know which slot we're considering for cases like where we're already
// wielding a cursed non-weapon.
static bool _item_is_swappable(const item_def &item, equipment_type slot, bool swap_in)
{
    if (get_item_slot(item) != slot)
        return true;

    if (item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES
        || item.base_type == OBJ_RODS)
    {
        return true;
    }

    if (item.base_type == OBJ_ARMOUR || !_item_known_uncursed(item))
        return false;

    if (item.base_type == OBJ_JEWELLERY)
    {
        if (item.sub_type == AMU_FAITH && !you_worship(GOD_NO_GOD))
            return false;
        return !((item.sub_type == AMU_THE_GOURMAND && !swap_in)
                || item.sub_type == AMU_GUARDIAN_SPIRIT
                || (item.sub_type == RING_MAGICAL_POWER && !swap_in));
    }
    return true;
}

static bool _item_is_swappable(const item_def &item, bool swap_in)
{
    return _item_is_swappable(item, get_item_slot(item), swap_in);
}

// Check whether the equipment slot of an item is occupied by an item which
// cannot be quickly removed.
static bool _slot_blocked(const item_def &item)
{
    const equipment_type eq = get_item_slot(item);
    if (eq == EQ_NONE)
        return false;

    if (eq == EQ_RINGS)
    {
        if (you.equip[EQ_GLOVES] >= 0 && you.inv[you.equip[EQ_GLOVES]].cursed())
            return true;

        equipment_type eq_from = EQ_LEFT_RING;
        equipment_type eq_to = EQ_RIGHT_RING;
        if (you.species == SP_OCTOPODE)
        {
            eq_from = EQ_RING_ONE;
            eq_to = EQ_RING_EIGHT;
        }

        for (int i = eq_from; i <= eq_to; ++i)
        {
            if (you.equip[i] == -1
                || _item_is_swappable(you.inv[you.equip[i]], false))
            {
                return false;
            }
        }

        // No free slot found.
        return true;
    }

    if (eq == EQ_WEAPON && you.equip[EQ_SHIELD] >= 0
        && you.inv[you.equip[EQ_SHIELD]].cursed()
        && is_shield_incompatible(item, &you.inv[you.equip[EQ_SHIELD]]))
    {
        return true;
    }

    return (you.equip[eq] != -1
            && !_item_is_swappable(you.inv[you.equip[eq]], eq, false));
}

bool item_skills(const item_def &item, set<skill_type> &skills)
{
    const bool equipped = item_is_equipped(item);

    if (item.base_type == OBJ_BOOKS && item.sub_type == BOOK_MANUAL)
    {
        const skill_type skill = static_cast<skill_type>(item.plus);
        if (training_restricted(skill))
            skills.insert(skill);
    }

    // Evokables that don't need to be equipped.
    if (item_is_evokable(item, false, false, false, false, true))
        skills.insert(SK_EVOCATIONS);

    // Item doesn't have to be equipped, but if it isn't, it needs to be easy
    // and quick to equip. This means:
    // - known to be uncursed
    // - quick to equip (no armour)
    // - no effect that suffer from swapping (distortion, vampirism, faith,...)
    // - slot easily accessible (item in slot needs to meet the same conditions)
    if (!equipped && (!_item_is_swappable(item, true) || _slot_blocked(item)))
        return !skills.empty();

    // Evokables that need to be equipped to be evoked. They can train
    // evocations just by being carried, but they need to pass the equippable
    // check first.
    if (gives_ability(item)
        || item_is_evokable(item, false, false, false, false, false))
    {
        skills.insert(SK_EVOCATIONS);
    }

    skill_type sk = weapon_skill(item);
    if (sk != SK_FIGHTING)
        skills.insert(sk);

    sk = range_skill(item);
    if (sk != SK_THROWING)
        skills.insert(sk);

    return !skills.empty();
}

void maybe_change_train(const item_def& item, bool start)
{
    const equipment_type eq = get_item_slot(item);
    if (eq == EQ_NONE)
        return;

    for (int i = 0; i < ENDOFPACK; ++i)
        if (item.link != i && you.inv[i].defined())
        {
            equipment_type islot = get_item_slot(you.inv[i]);
            if (islot == eq
                || (eq == EQ_GLOVES && islot == EQ_RINGS)
                || (eq == EQ_SHIELD && islot == EQ_WEAPON))
            {
                item_skills(you.inv[i], start ? you.start_train : you.stop_train);
            }
        }
}

// Returns number of sizes away from being a usable weapon.
static int _fit_weapon_wieldable_size(const item_def &item, size_type size)
{
    const int fit = Weapon_prop[Weapon_index[item.sub_type]].fit_size - size;

    return ((fit < -2) ? fit + 2 :
            (fit >  1) ? fit - 1 : 0);
}

// Returns true if weapon is usable as a weapon.
bool check_weapon_wieldable_size(const item_def &item, size_type size)
{
    ASSERT(is_weapon(item));

    // Staves and rods are currently wieldable for everyone just to be nice.
    if (item.base_type == OBJ_STAVES || item.base_type == OBJ_RODS
        || weapon_skill(item) == SK_STAVES)
    {
        return true;
    }

    int fit = _fit_weapon_wieldable_size(item, size);

    // Adjust fit for size.
    if (size < SIZE_SMALL && fit > 0)
        fit--;
    else if (size > SIZE_LARGE && fit < 0)
        fit++;

    return (fit == 0);
}

//
// Launcher and ammo functions:
//
missile_type fires_ammo_type(const item_def &item)
{
    if (item.base_type != OBJ_WEAPONS)
        return MI_NONE;

    return Weapon_prop[Weapon_index[item.sub_type]].ammo;
}

bool is_range_weapon(const item_def &item)
{
    return (fires_ammo_type(item) != MI_NONE);
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
    return (ammo.sub_type != MI_DART
            && ammo.sub_type != MI_LARGE_ROCK
            && ammo.sub_type != MI_JAVELIN
#if TAG_MAJOR_VERSION == 34
            && ammo.sub_type != MI_PIE
#endif
            && ammo.sub_type != MI_THROWING_NET);
}

// Returns true if item can be reasonably thrown without a launcher.
bool is_throwable(const actor *actor, const item_def &wpn, bool force)
{
    const size_type bodysize = actor->body_size();

    if (wpn.base_type == OBJ_WEAPONS)
        return Weapon_prop[Weapon_index[wpn.sub_type]].throwable;
    else if (wpn.base_type == OBJ_MISSILES)
    {
        if (!force)
        {
            if ((bodysize < SIZE_LARGE
                    || !actor->can_throw_large_rocks())
                && wpn.sub_type == MI_LARGE_ROCK)
            {
                return false;
            }

            if (bodysize < SIZE_MEDIUM
                && (wpn.sub_type == MI_JAVELIN
                    || wpn.sub_type == MI_THROWING_NET))
            {
                return false;
            }
        }

        return Missile_prop[Missile_index[wpn.sub_type]].throwable;
    }

    return false;
}

// Decide if something is launched or thrown.
launch_retval is_launched(const actor *actor, const item_def *launcher,
                          const item_def &missile)
{
    if (missile.base_type == OBJ_MISSILES
        && launcher
        && missile.launched_by(*launcher))
    {
        return LRET_LAUNCHED;
    }

    return is_throwable(actor, missile) ? LRET_THROWN : LRET_FUMBLED;
}

bool is_melee_weapon(const item_def &weapon)
{
    return is_weapon(weapon) && !is_range_weapon(weapon);
}

//
// Reaching functions:
//
reach_type weapon_reach(const item_def &item)
{
    if (weapon_skill(item) == SK_POLEARMS)
        return REACH_TWO;
    if (get_weapon_brand(item) == SPWPN_REACHING)
        return REACH_TWO;
    return REACH_NONE;
}

//
// Macguffins
//
bool item_is_rune(const item_def &item, rune_type which_rune)
{
    return (item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_RUNE_OF_ZOT
            && (which_rune == NUM_RUNE_TYPES || item.plus == which_rune));
}

bool item_is_unique_rune(const item_def &item)
{
    return (item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_RUNE_OF_ZOT
            && item.plus != RUNE_DEMONIC
            && item.plus != RUNE_ABYSSAL);
}

bool item_is_orb(const item_def &item)
{
    return (item.base_type == OBJ_ORBS && item.sub_type == ORB_ZOT);
}

bool item_is_horn_of_geryon(const item_def &item)
{
    return (item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_HORN_OF_GERYON);
}

bool item_is_spellbook(const item_def &item)
{
    return (item.base_type == OBJ_BOOKS && item.sub_type != BOOK_MANUAL
            && item.sub_type != BOOK_DESTRUCTION);
}

//
// Ring functions:

// Returns number of pluses on jewellery (always none for amulets yet).
int ring_has_pluses(const item_def &item)
{
    ASSERT(item.base_type == OBJ_JEWELLERY);

    // not known -> no pluses
    if (!item_type_known(item))
        return 0;

    switch (item.sub_type)
    {
    case RING_SLAYING:
        return 2;

    case RING_PROTECTION:
    case RING_EVASION:
    case RING_STRENGTH:
    case RING_INTELLIGENCE:
    case RING_DEXTERITY:
        return 1;

    default:
        break;
    }

    return 0;
}

// Returns true if having two rings of the same type on at the same
// has more effect than just having one on.
bool ring_has_stackable_effect(const item_def &item)
{
    ASSERT(item.base_type == OBJ_JEWELLERY);
    ASSERT(!jewellery_is_amulet(item));

    if (!item_type_known(item))
        return false;

    if (ring_has_pluses(item))
        return true;

    switch (item.sub_type)
    {
    case RING_PROTECTION_FROM_FIRE:
    case RING_PROTECTION_FROM_COLD:
    case RING_LIFE_PROTECTION:
    case RING_SUSTENANCE:
    case RING_WIZARDRY:
    case RING_FIRE:
    case RING_ICE:
        return true;

    default:
        break;
    }

    return false;
}

//
// Food functions:
//
bool is_blood_potion(const item_def &item)
{
    if (item.base_type != OBJ_POTIONS)
        return false;

    return (item.sub_type == POT_BLOOD
            || item.sub_type == POT_BLOOD_COAGULATED);
}

bool food_is_meaty(int food_type)
{
    ASSERTM(food_type >= 0 && food_type < NUM_FOODS,
            "Bad food type %d (NUM_FOODS = %d)",
            food_type, NUM_FOODS);

    return Food_prop[Food_index[food_type]].carn_mod > 0;
}

bool food_is_meaty(const item_def &item)
{
    if (item.base_type != OBJ_FOOD)
        return false;

    return food_is_meaty(item.sub_type);
}

bool food_is_veggie(int food_type)
{
    ASSERTM(food_type >= 0 && food_type < NUM_FOODS,
            "Bad food type %d (NUM_FOODS = %d)",
            food_type, NUM_FOODS);

    return Food_prop[Food_index[food_type]].herb_mod > 0;
}

bool food_is_veggie(const item_def &item)
{
    if (item.base_type != OBJ_FOOD)
        return false;

    return food_is_veggie(item.sub_type);
}

int food_value(const item_def &item)
{
    ASSERT(item.defined());

    if (item.base_type != OBJ_FOOD) // TRAN_JELLY
        return max(1, item_mass(item) * 5);

    const int herb = player_mutation_level(MUT_HERBIVOROUS);
    const int carn = player_mutation_level(MUT_CARNIVOROUS);

    const food_def &food = Food_prop[Food_index[item.sub_type]];

    int ret = food.value;

    ret += carn * food.carn_mod;
    ret += herb * food.herb_mod;

    return ret;
}

int food_turns(const item_def &item)
{
    ASSERT(item.defined());
    if (item.base_type == OBJ_FOOD)
        return Food_prop[Food_index[item.sub_type]].turns;
    return max(1, item_mass(item) / 100); // TRAN_JELLY
}

bool can_cut_meat(const item_def &item)
{
    return _does_damage_type(item, DAM_SLICE);
}

bool is_fruit(const item_def & item)
{
    if (item.base_type != OBJ_FOOD)
        return false;

    return (Food_prop[Food_index[item.sub_type]].flags & FFL_FRUIT);
}

bool food_is_rotten(const item_def &item)
{
    return (item.special <= ROTTING_CORPSE)
                                    && (item.base_type == OBJ_CORPSES
                                       && item.sub_type == CORPSE_BODY
                                    || item.base_type == OBJ_FOOD
                                       && item.sub_type == FOOD_CHUNK);
}

//
// Generic item functions:
//
int get_armour_res_fire(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    switch (arm.sub_type)
    {
    case ARM_FIRE_DRAGON_ARMOUR:
    case ARM_FIRE_DRAGON_HIDE:
        res += 2;
        break;
    case ARM_GOLD_DRAGON_ARMOUR:
    case ARM_GOLD_DRAGON_HIDE:
        res += 1;
        break;
    case ARM_ICE_DRAGON_ARMOUR:
    case ARM_ICE_DRAGON_HIDE:
        res -= 1;
        break;
    default:
        break;
    }

    // check ego resistance
    const int ego = get_armour_ego_type(arm);
    if (ego == SPARM_FIRE_RESISTANCE || ego == SPARM_RESISTANCE)
        res += 1;

    if (check_artp && is_artefact(arm))
        res += artefact_wpn_property(arm, ARTP_FIRE);

    return res;
}

int get_armour_res_cold(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    switch (arm.sub_type)
    {
    case ARM_ICE_DRAGON_ARMOUR:
    case ARM_ICE_DRAGON_HIDE:
        res += 2;
        break;
    case ARM_GOLD_DRAGON_ARMOUR:
    case ARM_GOLD_DRAGON_HIDE:
        res += 1;
        break;
    case ARM_FIRE_DRAGON_ARMOUR:
    case ARM_FIRE_DRAGON_HIDE:
        res -= 1;
        break;
    default:
        break;
    }

    // check ego resistance
    const int ego = get_armour_ego_type(arm);
    if (ego == SPARM_COLD_RESISTANCE || ego == SPARM_RESISTANCE)
        res += 1;

    if (check_artp && is_artefact(arm))
        res += artefact_wpn_property(arm, ARTP_COLD);

    return res;
}

int get_armour_res_poison(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    switch (arm.sub_type)
    {
    case ARM_SWAMP_DRAGON_ARMOUR:
    case ARM_SWAMP_DRAGON_HIDE:
        res += 1;
        break;
    case ARM_GOLD_DRAGON_ARMOUR:
    case ARM_GOLD_DRAGON_HIDE:
        res += 1;
        break;
    default:
        break;
    }

    // check ego resistance
    if (get_armour_ego_type(arm) == SPARM_POISON_RESISTANCE)
        res += 1;

    if (check_artp && is_artefact(arm))
        res += artefact_wpn_property(arm, ARTP_POISON);

    return res;
}

int get_armour_res_elec(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    switch (arm.sub_type)
    {
    case ARM_STORM_DRAGON_ARMOUR:
    case ARM_STORM_DRAGON_HIDE:
        res += 1;
        break;
    default:
        break;
    }

    if (check_artp && is_artefact(arm))
        res += artefact_wpn_property(arm, ARTP_ELECTRICITY);

    return res;
}

int get_armour_life_protection(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // Pearl dragon armour grants rN+.
    if (arm.sub_type == ARM_PEARL_DRAGON_ARMOUR)
        res += 1;

    // check for ego resistance
    if (get_armour_ego_type(arm) == SPARM_POSITIVE_ENERGY)
        res += 1;

    if (check_artp && is_artefact(arm))
        res += artefact_wpn_property(arm, ARTP_NEGATIVE_ENERGY);

    return res;
}

int get_armour_res_magic(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // check for ego resistance
    if (get_armour_ego_type(arm) == SPARM_MAGIC_RESISTANCE)
        res += 30;

    if (check_artp && is_artefact(arm))
        res += artefact_wpn_property(arm, ARTP_MAGIC);

    return res;
}

bool get_armour_see_invisible(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    // check for ego resistance
    if (get_armour_ego_type(arm) == SPARM_POSITIVE_ENERGY)
        return true;

    if (check_artp && is_artefact(arm))
        return artefact_wpn_property(arm, ARTP_EYESIGHT);

    return false;
}

int get_armour_res_sticky_flame(const item_def &arm)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    // intrinsic armour abilities
    switch (arm.sub_type)
    {
    case ARM_MOTTLED_DRAGON_ARMOUR:
    case ARM_MOTTLED_DRAGON_HIDE:
        return 1;
    default:
        return 0;
    }
}

int get_jewellery_res_fire(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    int res = 0;

    // intrinsic jewellery abilities
    switch (ring.sub_type)
    {
    case RING_PROTECTION_FROM_FIRE:
    case RING_FIRE:
        res += 1;
        break;
    case RING_ICE:
        res -= 1;
        break;
    default:
        break;
    }

    if (check_artp && is_artefact(ring))
        res += artefact_wpn_property(ring, ARTP_FIRE);

    return res;
}

int get_jewellery_res_cold(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    int res = 0;

    // intrinsic jewellery abilities
    switch (ring.sub_type)
    {
    case RING_PROTECTION_FROM_COLD:
    case RING_ICE:
        res += 1;
        break;
    case RING_FIRE:
        res -= 1;
        break;
    default:
        break;
    }

    if (check_artp && is_artefact(ring))
        res += artefact_wpn_property(ring, ARTP_COLD);

    return res;
}

int get_jewellery_res_poison(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    int res = 0;

    if (ring.sub_type == RING_POISON_RESISTANCE)
        res += 1;

    if (check_artp && is_artefact(ring))
        res += artefact_wpn_property(ring, ARTP_POISON);

    return res;
}

int get_jewellery_res_elec(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    int res = 0;

    if (check_artp && is_artefact(ring))
        res += artefact_wpn_property(ring, ARTP_ELECTRICITY);

    return res;
}

int get_jewellery_life_protection(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    int res = 0;

    // check for ego resistance
    if (ring.sub_type == RING_LIFE_PROTECTION)
        res += 1;

    if (check_artp && is_artefact(ring))
        res += artefact_wpn_property(ring, ARTP_NEGATIVE_ENERGY);

    return res;
}

int get_jewellery_res_magic(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    int res = 0;

    if (ring.sub_type == RING_PROTECTION_FROM_MAGIC)
        res += 40;

    if (check_artp && is_artefact(ring))
        res += artefact_wpn_property(ring, ARTP_MAGIC);

    return res;
}

bool get_jewellery_see_invisible(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    if (ring.sub_type == RING_SEE_INVISIBLE)
        return true;

    if (check_artp && is_artefact(ring))
        return artefact_wpn_property(ring, ARTP_EYESIGHT);

    return false;
}

int property(const item_def &item, int prop_type)
{
    weapon_type weapon_sub;

    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        if (prop_type == PARM_AC)
            return Armour_prop[ Armour_index[item.sub_type] ].ac;
        else if (prop_type == PARM_EVASION)
            return Armour_prop[ Armour_index[item.sub_type] ].ev;
        break;

    case OBJ_WEAPONS:
        if (is_unrandom_artefact(item))
        {
            switch (prop_type)
            {
            case PWPN_DAMAGE:
                return (Weapon_prop[ Weapon_index[item.sub_type] ].dam
                        + artefact_wpn_property(item, ARTP_BASE_DAM));
            case PWPN_HIT:
                return (Weapon_prop[ Weapon_index[item.sub_type] ].hit
                        + artefact_wpn_property(item, ARTP_BASE_ACC));
            case PWPN_SPEED:
                return (Weapon_prop[ Weapon_index[item.sub_type] ].speed
                        + artefact_wpn_property(item, ARTP_BASE_DELAY));
            }
        }
        if (prop_type == PWPN_DAMAGE)
            return Weapon_prop[ Weapon_index[item.sub_type] ].dam;
        else if (prop_type == PWPN_HIT)
            return Weapon_prop[ Weapon_index[item.sub_type] ].hit;
        else if (prop_type == PWPN_SPEED)
            return Weapon_prop[ Weapon_index[item.sub_type] ].speed;
        else if (prop_type == PWPN_ACQ_WEIGHT)
            return Weapon_prop[ Weapon_index[item.sub_type] ].acquire_weight;
        break;

    case OBJ_MISSILES:
        if (prop_type == PWPN_DAMAGE)
            return Missile_prop[ Missile_index[item.sub_type] ].dam;
        break;

    case OBJ_STAVES:
    case OBJ_RODS:
        weapon_sub = (item.base_type == OBJ_RODS) ? WPN_ROD : WPN_STAFF;

        if (prop_type == PWPN_DAMAGE)
            return Weapon_prop[ Weapon_index[weapon_sub] ].dam;
        else if (prop_type == PWPN_HIT)
            return Weapon_prop[ Weapon_index[weapon_sub] ].hit;
        else if (prop_type == PWPN_SPEED)
            return Weapon_prop[ Weapon_index[weapon_sub] ].speed;
        break;

    default:
        break;
    }

    return 0;
}

// Returns true if item is evokable.
bool gives_ability(const item_def &item)
{
    if (!item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        break;
    case OBJ_JEWELLERY:
        if (item.sub_type == RING_TELEPORTATION
            || item.sub_type == RING_FLIGHT
            || item.sub_type == RING_INVISIBILITY
            || item.sub_type == RING_TELEPORT_CONTROL
            || item.sub_type == AMU_RAGE)
        {
            return true;
        }
        break;
    case OBJ_ARMOUR:
    {
        const equipment_type eq = get_armour_slot(item);
        if (eq == EQ_NONE)
            return false;
        const special_armour_type ego = get_armour_ego_type(item);

        if (ego == SPARM_DARKNESS || ego == SPARM_FLYING)
            return true;
        break;
    }
    default:
        return false;
    }

    if (!is_artefact(item))
        return false;

    // Check for evokable randart properties.
    for (int rap = ARTP_INVISIBLE; rap <= ARTP_BERSERK; rap++)
        if (artefact_wpn_property(item, static_cast<artefact_prop_type>(rap)))
            return true;

#if TAG_MAJOR_VERSION == 34
    if (artefact_wpn_property(item, ARTP_FOG))
        return true;
#endif

    return false;
}

// Returns true if the item confers an intrinsic that is shown on the % screen.
bool gives_resistance(const item_def &item)
{
    if (!item_type_known(item))
        return false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        break;
    case OBJ_JEWELLERY:
        if (!jewellery_is_amulet(item))
        {
            if (item.sub_type >= RING_PROTECTION_FROM_FIRE
                   && item.sub_type <= RING_PROTECTION_FROM_COLD
                || item.sub_type == RING_SEE_INVISIBLE
                || item.sub_type >= RING_LIFE_PROTECTION
                   && item.sub_type <= RING_TELEPORT_CONTROL
                || item.sub_type == RING_SUSTAIN_ABILITIES)
            {
                return true;
            }
        }
        else
        {
            if (item.sub_type != AMU_RAGE && item.sub_type != AMU_INACCURACY)
                return true;
        }
        break;
    case OBJ_ARMOUR:
    {
        const equipment_type eq = get_armour_slot(item);
        if (eq == EQ_NONE)
            return false;

        const int ego = get_armour_ego_type(item);
        if (ego >= SPARM_FIRE_RESISTANCE && ego <= SPARM_SEE_INVISIBLE
            || ego == SPARM_RESISTANCE || ego == SPARM_POSITIVE_ENERGY)
        {
            return true;
        }
        break;
    }
    case OBJ_STAVES:
        if (item.sub_type >= STAFF_FIRE && item.sub_type <= STAFF_POISON
            || item.sub_type == STAFF_AIR
            || item.sub_type == STAFF_DEATH)
        {
            return true;
        }
        return false;
    default:
        return false;
    }

    if (!is_artefact(item))
        return false;

    // Check for randart resistances.
    for (int rap = ARTP_FIRE; rap <= ARTP_BERSERK; rap++)
    {
        if (rap == ARTP_MAGIC || rap >= ARTP_INVISIBLE)
            continue;

        if (artefact_wpn_property(item, static_cast<artefact_prop_type>(rap)))
            return true;
    }

    return false;
}

int item_mass(const item_def &item)
{
    int unit_mass = 0;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        unit_mass = Weapon_prop[ Weapon_index[item.sub_type] ].mass;
        break;

    case OBJ_ARMOUR:
        unit_mass = Armour_prop[ Armour_index[item.sub_type] ].mass;

        if (get_equip_race(item) == ISFLAG_ELVEN)
        {
            const int reduc = (unit_mass >= 25) ? unit_mass / 5 : 5;

            // Truncate to the nearest 5 and reduce the item mass:
            unit_mass -= ((reduc / 5) * 5);
            unit_mass = max(unit_mass, 5);
        }
        break;

    case OBJ_MISSILES:
    {
        unit_mass = Missile_prop[ Missile_index[item.sub_type] ].mass;
        int brand = get_ammo_brand(item);

        if (brand == SPMSL_SILVER)
            unit_mass *= 2;
        else if (brand == SPMSL_STEEL)
            unit_mass *= 3;
        break;
    }

    case OBJ_FOOD:
        unit_mass = Food_prop[ Food_index[item.sub_type] ].mass;
        break;

    case OBJ_WANDS:
        unit_mass = 100;
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

    case OBJ_BOOKS:
        unit_mass = 70;
        break;

    case OBJ_STAVES:
    case OBJ_RODS:
        unit_mass = 130;
        break;

    case OBJ_ORBS:
        unit_mass = 600;
        break;

    case OBJ_MISCELLANY:
        if (is_deck(item))
        {
            unit_mass = 50;
            break;
        }
        switch (item.sub_type)
        {
        case MISC_BOTTLED_EFREET:
        case MISC_CRYSTAL_BALL_OF_ENERGY:
            unit_mass = 150;
            break;

        case MISC_RUNE_OF_ZOT:
            unit_mass = 0;
            break;

        default:
            unit_mass = 100;
            break;
        }
        break;

    case OBJ_CORPSES:
        unit_mass = mons_weight(item.mon_type);

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

equipment_type get_item_slot(const item_def& item)
{
    return get_item_slot(item.base_type, item.sub_type);
}

equipment_type get_item_slot(object_class_type type, int sub_type)
{
    switch (type)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
    case OBJ_RODS:
    case OBJ_MISCELLANY:
        return EQ_WEAPON;

    case OBJ_ARMOUR:
        return get_armour_slot(static_cast<armour_type>(sub_type));

    case OBJ_JEWELLERY:
        return (jewellery_is_amulet(sub_type) ? EQ_AMULET : EQ_RINGS);

    default:
        break;
    }

    return EQ_NONE;
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
    if (!shield && !(shield = you.shield()))
        return false;

    hands_reqd_type hand = hands_reqd(weapon, you.body_size());
    return (hand == HANDS_TWO && !is_range_weapon(weapon));
}

bool shield_reflects(const item_def &shield)
{
    ASSERT(is_shield(shield));

    return (get_armour_ego_type(shield) == SPARM_REFLECTION);
}

void ident_reflector(item_def *item)
{
    if (!is_artefact(*item))
        set_ident_flags(*item, ISFLAG_KNOW_TYPE);
}

string item_base_name(const item_def &item)
{
    return item_base_name(item.base_type, item.sub_type);
}

string item_base_name(object_class_type type, int sub_type)
{
    switch (type)
    {
    case OBJ_WEAPONS:
        return Weapon_prop[Weapon_index[sub_type]].name;
    case OBJ_MISSILES:
        return Missile_prop[Missile_index[sub_type]].name;
    case OBJ_ARMOUR:
        return Armour_prop[Armour_index[sub_type]].name;
    case OBJ_JEWELLERY:
        return jewellery_is_amulet(sub_type) ? "amulet" : "ring";
    default:
        return "";
    }
}

string food_type_name(int sub_type)
{
    return Food_prop[Food_index[sub_type]].name;
}

const char* weapon_base_name(weapon_type subtype)
{
    return Weapon_prop[Weapon_index[subtype]].name;
}

void seen_item(const item_def &item)
{
    if (!is_artefact(item) && _is_affordable(item))
    {
        // Known brands will be set in set_item_flags().
        if (item.base_type == OBJ_WEAPONS)
            you.seen_weapon[item.sub_type] |= 1 << SP_UNKNOWN_BRAND;
        if (item.base_type == OBJ_ARMOUR)
            you.seen_armour[item.sub_type] |= 1 << SP_UNKNOWN_BRAND;
        if (item.base_type == OBJ_MISCELLANY
            && !is_deck(item))
        {
            you.seen_misc.set(item.sub_type);
        }
    }

    // major hack.  Deconstify should be safe here, but it's still repulsive.
    if (you_worship(GOD_ASHENZARI))
        ((item_def*)&item)->flags |= ISFLAG_KNOW_CURSE;
    if (item.base_type == OBJ_GOLD && !item.plus)
        ((item_def*)&item)->plus = (you_worship(GOD_ZIN)) ? 2 : 1;

    if (item_type_has_ids(item.base_type) && !is_artefact(item)
        && item_ident(item, ISFLAG_KNOW_TYPE)
        && you.type_ids[item.base_type][item.sub_type] != ID_KNOWN_TYPE)
    {
        // Can't cull shop items here -- when called from view, we shouldn't
        // access the UI.  Old ziggurat prompts are a very minor case of what
        // could go wrong.
        set_ident_type(item.base_type, item.sub_type, ID_KNOWN_TYPE);
    }
}

bool is_elemental_evoker(const item_def &item)
{
    return (item.base_type == OBJ_MISCELLANY
            && (item.sub_type == MISC_LAMP_OF_FIRE
                || item.sub_type == MISC_STONE_OF_TREMORS
                || item.sub_type == MISC_FAN_OF_GALES
                || item.sub_type == MISC_PHIAL_OF_FLOODS));
}

bool evoker_is_charged(const item_def &item)
{
    return (item.plus2 == 0);
}
