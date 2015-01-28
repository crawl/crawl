/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "itemprop.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "artefact.h"
#include "art-enum.h"
#include "decks.h"
#include "describe.h"
#include "godpassive.h"
#include "invent.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h" // map_find
#include "message.h"
#include "misc.h"
#include "notes.h"
#include "options.h"
#include "religion.h"
#include "random-weight.h"
#include "shopping.h"
#include "skills.h"
#include "stringutil.h"
#include "terrain.h"
#include "xom.h"

static iflags_t _full_ident_mask(const item_def& item);

// XXX: Name strings in most of the following are currently unused!
struct armour_def
{
    armour_type         id;
    const char         *name;
    int                 ac;
    int                 ev;

    equipment_type      slot;
    size_type           fit_min;
    size_type           fit_max;
    /// Whether this armour is mundane or inherently 'special', for acq.
    bool                mundane;
    /// The resists, vulns, &c that this armour type gives when worn.
    armflags_t          flags;
};

// Note: the Little-Giant range is used to make armours which are very
// flexible and adjustable and can be worn by any player character...
// providing they also pass the shape test, of course.
static int Armour_index[NUM_ARMOURS];
static const armour_def Armour_prop[] =
{
    { ARM_ANIMAL_SKIN,          "animal skin",            2,   0,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT, true },
    { ARM_ROBE,                 "robe",                   2,   0,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_BIG, true },
    { ARM_LEATHER_ARMOUR,       "leather armour",         3,  -40,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM, true },

    { ARM_RING_MAIL,            "ring mail",              5,  -70,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM, true },
    { ARM_SCALE_MAIL,           "scale mail",             6, -100,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM, true },
    { ARM_CHAIN_MAIL,           "chain mail",             8, -150,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM, true },
    { ARM_PLATE_ARMOUR,         "plate armour",          10, -180,
        EQ_BODY_ARMOUR, SIZE_SMALL, SIZE_MEDIUM, true },
    { ARM_CRYSTAL_PLATE_ARMOUR, "crystal plate armour",  14, -230,
        EQ_BODY_ARMOUR, SIZE_SMALL, SIZE_MEDIUM, false },

#define HIDE_ARMOUR(aenum, aname, aac, aevp, henum, hname, res) \
    { henum, hname, (aac)/2, aevp,                              \
      EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT, false, res },    \
    { aenum, aname, aac, aevp,                                  \
      EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT, false, res }

#define DRAGON_ARMOUR(id, name, ac, evp, res) \
    HIDE_ARMOUR(ARM_ ## id ## _DRAGON_ARMOUR, name " dragon armour", ac, evp, \
                ARM_ ## id ## _DRAGON_HIDE, name " dragon hide", res)

    HIDE_ARMOUR(
      ARM_TROLL_LEATHER_ARMOUR, "troll leather armour",   4,  -40,
      ARM_TROLL_HIDE,           "troll hide",
        ARMF_REGENERATION
    ),

    DRAGON_ARMOUR(STEAM,       "steam",                   5,   0,
        ARMF_RES_STEAM),
    DRAGON_ARMOUR(MOTTLED,     "mottled",                 6,  -50,
        ARMF_RES_STICKY_FLAME),
    DRAGON_ARMOUR(QUICKSILVER, "quicksilver",            10,  -70,
        ARMF_RES_MAGIC),
    DRAGON_ARMOUR(SWAMP,       "swamp",                   7,  -70,
        ARMF_RES_POISON),
    DRAGON_ARMOUR(FIRE,        "fire",                    8, -110,
        ard(ARMF_RES_FIRE, 2) | ARMF_VUL_COLD),
    DRAGON_ARMOUR(ICE,         "ice",                     9, -110,
        ard(ARMF_RES_COLD, 2) | ARMF_VUL_FIRE),
    DRAGON_ARMOUR(PEARL,       "pearl",                  10, -110,
        ARMF_RES_NEG),
    DRAGON_ARMOUR(STORM,       "storm",                  10, -150,
        ARMF_RES_ELEC),
    DRAGON_ARMOUR(SHADOW,      "shadow",                 10, -150,
        ard(ARMF_STEALTH, 4)),
    DRAGON_ARMOUR(GOLD,        "gold",                   12, -230,
        ARMF_RES_FIRE | ARMF_RES_COLD | ARMF_RES_POISON),

#undef DRAGON_HIDE
#undef HIDE_ARMOUR

    { ARM_CLOAK,                "cloak",                  1,   0,
        EQ_CLOAK,       SIZE_LITTLE, SIZE_BIG, true },
    { ARM_GLOVES,               "gloves",                 1,   0,
        EQ_GLOVES,      SIZE_SMALL,  SIZE_MEDIUM, true },

    { ARM_HELMET,               "helmet",                 1,   0,
        EQ_HELMET,      SIZE_SMALL,  SIZE_MEDIUM, true },

#if TAG_MAJOR_VERSION == 34
    { ARM_CAP,                  "cap",                    0,   0,
        EQ_HELMET,      SIZE_LITTLE, SIZE_LARGE, true },
#endif

    { ARM_HAT,                  "hat",                    0,   0,
        EQ_HELMET,      SIZE_LITTLE, SIZE_LARGE, true },

    // Note that barding size is compared against torso so it currently
    // needs to fit medium, but that doesn't matter as much as race
    // and shapeshift status.
    { ARM_BOOTS,                "boots",                  1,   0,
        EQ_BOOTS,       SIZE_SMALL,  SIZE_MEDIUM, true },
    // Changed max. barding size to large to allow for the appropriate
    // monster types (monsters don't differentiate between torso and general).
    { ARM_CENTAUR_BARDING,      "centaur barding",        4,  -60,
        EQ_BOOTS,       SIZE_MEDIUM, SIZE_LARGE, true },
    { ARM_NAGA_BARDING,         "naga barding",           4,  -60,
        EQ_BOOTS,       SIZE_MEDIUM, SIZE_LARGE, true },

    // Note: shields use ac-value as sh-value, EV pen is used as the basis
    // to calculate adjusted shield penalty.
    { ARM_BUCKLER,              "buckler",                3,  -8,
        EQ_SHIELD,      SIZE_LITTLE, SIZE_MEDIUM, true },
    { ARM_SHIELD,               "shield",                 8,  -30,
        EQ_SHIELD,      SIZE_SMALL,  SIZE_BIG, true    },
    { ARM_LARGE_SHIELD,         "large shield",          13,  -50,
        EQ_SHIELD,      SIZE_MEDIUM, SIZE_GIANT, true  },
};

typedef pair<brand_type, int> brand_weight_tuple;

/// The standard properties for a given weapon type. (E.g. falchions)
struct weapon_def
{
    /// The weapon_type enum for this weapon type.
    int                 id;
    /// The name of this weapon type. (E.g. "club".)
    const char         *name;
    /// The base damage of the weapon. (Later multiplied by skill, etc)
    int                 dam;
    /// The base to-hit bonus of the weapon.
    int                 hit;
    /// The number of aut it takes to swing the weapon with 0 skill.
    int                 speed;
    /// The extent to which str is more useful than dex; ranges 0-10.
    int                 str_weight;

    /// The weapon skill corresponding to this weapon's use.
    skill_type          skill;
    /// The size of the smallest creature that can wield the weapon.
    size_type           min_2h_size;
    /// The smallest creature that can wield the weapon one-handed.
    size_type           min_1h_size;
    /// The ammo fired by the weapon; MI_NONE for non-launchers.
    missile_type        ammo;

    /// A union of vorpal_damage_type flags (slash, crush, etc)
    int                 dam_type;
    /// Used in *some* item generation code; higher = generated more often.
    int                 commonness;
    /// Used in *some* item 'acquirement' code; higher = generated more.
    int                 acquire_weight;
    /// Used in non-artefact ego item generation. If empty, default to NORMAL.
    vector<brand_weight_tuple> brand_weights;
};

/// brand weights for non-dagger shortblades (short sword & rapier)
static const vector<brand_weight_tuple> SBL_BRANDS = {
    { SPWPN_NORMAL, 33 },
    { SPWPN_VENOM, 17 },
    { SPWPN_SPEED, 10 },
    { SPWPN_DRAINING, 9 },
    { SPWPN_PROTECTION, 6 },
    { SPWPN_ELECTROCUTION, 6 },
    { SPWPN_HOLY_WRATH, 5 },
    { SPWPN_VAMPIRISM, 4 },
    { SPWPN_FLAMING, 4 },
    { SPWPN_FREEZING, 4 },
    { SPWPN_DISTORTION, 1 },
    { SPWPN_ANTIMAGIC, 1 },
};

/// brand weights for most m&f weapons
static const vector<brand_weight_tuple> M_AND_F_BRANDS = {
    { SPWPN_PROTECTION,     30 },
    { SPWPN_NORMAL,         28 },
    { SPWPN_HOLY_WRATH,     15 },
    { SPWPN_VORPAL,         14 },
    { SPWPN_DRAINING,       10 },
    { SPWPN_VENOM,           5 },
    { SPWPN_DISTORTION,      1 },
    { SPWPN_ANTIMAGIC,       1 },
    { SPWPN_PAIN,            1 },
};

/// brand weights for demon weapons (whip, blade, trident)
static const vector<brand_weight_tuple> DEMON_BRANDS = {
    { SPWPN_NORMAL,         27 },
    { SPWPN_VENOM,          19 },
    { SPWPN_ELECTROCUTION,  16 },
    { SPWPN_DRAINING,       10 },
    { SPWPN_FLAMING,         7 },
    { SPWPN_FREEZING,        7 },
    { SPWPN_VAMPIRISM,       7 },
    { SPWPN_PAIN,            4 },
    { SPWPN_ANTIMAGIC,       3 },
};

/// brand weights for long blades.
static const vector<brand_weight_tuple> LBL_BRANDS = {
    { SPWPN_HOLY_WRATH,     23 },
    { SPWPN_NORMAL,         19 },
    { SPWPN_VORPAL,         15 },
    { SPWPN_ELECTROCUTION,  10 },
    { SPWPN_PROTECTION,      8 },
    { SPWPN_FREEZING,        5 },
    { SPWPN_FLAMING,         5 },
    { SPWPN_DRAINING,        5 },
    { SPWPN_VAMPIRISM,       4 },
    { SPWPN_VENOM,           2 },
    { SPWPN_DISTORTION,      2 },
    { SPWPN_PAIN,            1 },
    { SPWPN_ANTIMAGIC,       1 },
};

/// brand weights for axes.
static const vector<brand_weight_tuple> AXE_BRANDS = {
    { SPWPN_NORMAL,         31 },
    { SPWPN_VORPAL,         16 },
    { SPWPN_ELECTROCUTION,  11 },
    { SPWPN_FLAMING,        10 },
    { SPWPN_FREEZING,       10 },
    { SPWPN_VENOM,           8 },
    { SPWPN_VAMPIRISM,       5 },
    { SPWPN_DRAINING,        3 },
    { SPWPN_DISTORTION,      2 },
    { SPWPN_ANTIMAGIC,       2 },
    { SPWPN_PAIN,            1 },
    { SPWPN_HOLY_WRATH,      1 },
};

/// brand weights for most polearms.
static const vector<brand_weight_tuple> POLEARM_BRANDS = {
    { SPWPN_NORMAL,     36 },
    { SPWPN_VENOM,      17 },
    { SPWPN_PROTECTION, 12 },
    { SPWPN_VORPAL,      9 },
    { SPWPN_FLAMING,     7 },
    { SPWPN_FREEZING,    7 },
    { SPWPN_VAMPIRISM,   5 },
    { SPWPN_DISTORTION,  2 },
    { SPWPN_PAIN,        2 },
    { SPWPN_ANTIMAGIC,   2 },
    { SPWPN_HOLY_WRATH,  1 },
};

/// brand weights for most ranged weapons.
static const vector<brand_weight_tuple> RANGED_BRANDS = {
    { SPWPN_NORMAL,   50 },
    { SPWPN_FLAMING,  24 },
    { SPWPN_FREEZING, 12 },
    { SPWPN_EVASION,   8 },
    { SPWPN_VORPAL,    6 },
};

/// brand weights for holy (TSO-blessed) weapons.
static const vector<brand_weight_tuple> HOLY_BRANDS = {
    { SPWPN_HOLY_WRATH, 100 },
};


static int Weapon_index[NUM_WEAPONS];
static const weapon_def Weapon_prop[] =
{
    // Maces & Flails
    { WPN_CLUB,              "club",                5,  3, 13,  7,
        SK_MACES_FLAILS, SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING, 10, 0, {} },
    { WPN_ROD,               "rod",                 5,  3, 13,  7,
        SK_MACES_FLAILS, SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING, 0, 0, {} },
    { WPN_WHIP,              "whip",                6,  2, 11,  2,
        SK_MACES_FLAILS, SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_SLASHING, 4, 0, {
            { SPWPN_NORMAL,        34 },
            { SPWPN_VENOM,         16 },
            { SPWPN_ELECTROCUTION, 16 },
            { SPWPN_DRAINING,       7 },
            { SPWPN_FREEZING,       6 },
            { SPWPN_FLAMING,        6 },
            { SPWPN_VAMPIRISM,      5 },
            { SPWPN_PAIN,           4 },
            { SPWPN_HOLY_WRATH,     3 },
            { SPWPN_DISTORTION,     2 },
            { SPWPN_ANTIMAGIC,      1 },
        }},
    { WPN_HAMMER,            "hammer",              7,  3, 13,  7,
        SK_MACES_FLAILS, SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING, 0, 0, M_AND_F_BRANDS },
    { WPN_MACE,              "mace",                8,  3, 14,  8,
        SK_MACES_FLAILS, SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING, 9, 10, M_AND_F_BRANDS },
    { WPN_FLAIL,             "flail",              10,  0, 14,  8,
        SK_MACES_FLAILS, SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING, 8, 10, M_AND_F_BRANDS },
    { WPN_MORNINGSTAR,       "morningstar",        13, -2, 15,  8,
        SK_MACES_FLAILS, SIZE_LITTLE,  SIZE_SMALL,  MI_NONE,
        DAMV_CRUSHING | DAM_PIERCE, 7, 10, {
            { SPWPN_PROTECTION,     30 },
            { SPWPN_NORMAL,         15 },
            { SPWPN_HOLY_WRATH,     15 },
            { SPWPN_DRAINING,       10 },
            { SPWPN_VORPAL,          9 },
            { SPWPN_VENOM,           5 },
            { SPWPN_FLAMING,         4 },
            { SPWPN_FREEZING,        4 },
            { SPWPN_DISTORTION,      2 },
            { SPWPN_ANTIMAGIC,       2 },
            { SPWPN_PAIN,            2 },
            { SPWPN_VAMPIRISM,       2 },
        }},
    { WPN_DEMON_WHIP,        "demon whip",         11,  1, 11,  2,
        SK_MACES_FLAILS, SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_SLASHING, 0, 2, DEMON_BRANDS },
    { WPN_SACRED_SCOURGE,    "sacred scourge",     12,  0, 11,  2,
        SK_MACES_FLAILS, SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_SLASHING, 0, 0, HOLY_BRANDS },
    { WPN_DIRE_FLAIL,        "dire flail",         13, -3, 13,  9,
        SK_MACES_FLAILS, SIZE_MEDIUM,  SIZE_BIG,    MI_NONE,
        DAMV_CRUSHING | DAM_PIERCE, 2, 10, M_AND_F_BRANDS },
    { WPN_EVENINGSTAR,       "eveningstar",        15, -1, 15,  8,
        SK_MACES_FLAILS, SIZE_LITTLE,  SIZE_SMALL,  MI_NONE,
        DAMV_CRUSHING | DAM_PIERCE, 0, 2, {
            { SPWPN_PROTECTION,     30 },
            { SPWPN_DRAINING,       19 },
            { SPWPN_HOLY_WRATH,     15 },
            { SPWPN_NORMAL,          8 },
            { SPWPN_VORPAL,          6 },
            { SPWPN_VENOM,           6 },
            { SPWPN_FLAMING,         6 },
            { SPWPN_FREEZING,        6 },
            { SPWPN_DISTORTION,      2 },
            { SPWPN_ANTIMAGIC,       2 },
            { SPWPN_PAIN,            2 },
            { SPWPN_VAMPIRISM,       2 },
        }},
    { WPN_GREAT_MACE,        "great mace",         17, -4, 17,  9,
        SK_MACES_FLAILS, SIZE_MEDIUM,  SIZE_BIG,    MI_NONE,
        DAMV_CRUSHING, 3, 10, M_AND_F_BRANDS },
    { WPN_GIANT_CLUB,        "giant club",         20, -6, 17, 10,
        SK_MACES_FLAILS, SIZE_LARGE, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CRUSHING, 1, 10, {} },
    { WPN_GIANT_SPIKED_CLUB, "giant spiked club",  22, -7, 18, 10,
        SK_MACES_FLAILS, SIZE_LARGE, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CRUSHING | DAM_PIERCE, 1, 10, {} },

    // Short Blades
    { WPN_DAGGER,            "dagger",              4,  6, 10,  1,
        SK_SHORT_BLADES, SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_PIERCING, 10, 10, {
            { SPWPN_VENOM,          28 },
            { SPWPN_NORMAL,         20 },
            { SPWPN_SPEED,          10 },
            { SPWPN_DRAINING,        9 },
            { SPWPN_PROTECTION,      6 },
            { SPWPN_ELECTROCUTION,   6 },
            { SPWPN_HOLY_WRATH,      5 },
            { SPWPN_VAMPIRISM,       4 },
            { SPWPN_FLAMING,         4 },
            { SPWPN_FREEZING,        4 },
            { SPWPN_PAIN,            2 },
            { SPWPN_DISTORTION,      1 },
            { SPWPN_ANTIMAGIC,       1 },
        }},
    { WPN_QUICK_BLADE,       "quick blade",         5,  6,  7,  0,
        SK_SHORT_BLADES, SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_PIERCING, 0, 2, {} },
    { WPN_SHORT_SWORD,       "short sword",         6,  4, 11,  2,
        SK_SHORT_BLADES, SIZE_LITTLE,  SIZE_LITTLE,  MI_NONE,
        DAMV_PIERCING, 8, 10, SBL_BRANDS },
    { WPN_RAPIER,           "rapier",               7,  4, 12,  2,
        SK_SHORT_BLADES, SIZE_LITTLE,  SIZE_LITTLE,  MI_NONE,
        DAMV_PIERCING, 8, 10, SBL_BRANDS },
    { WPN_CUTLASS,          "cutlass",              7,  4, 12,  2,
        SK_SHORT_BLADES, SIZE_LITTLE,  SIZE_LITTLE,  MI_NONE,
        DAMV_SLICING | DAM_PIERCE, 0, 0, {}},


    // Long Blades
    { WPN_FALCHION,              "falchion",               8,  2, 13,  3,
        SK_LONG_BLADES,  SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_SLICING, 7, 10, LBL_BRANDS }, // DAMV_CHOPPING...?
    { WPN_LONG_SWORD,            "long sword",            10,  1, 14,  3,
        SK_LONG_BLADES,  SIZE_LITTLE,  SIZE_SMALL,  MI_NONE,
        DAMV_SLICING, 7, 10, LBL_BRANDS },
    { WPN_SCIMITAR,              "scimitar",              12, -2, 14,  3,
        SK_LONG_BLADES,  SIZE_LITTLE,  SIZE_SMALL,  MI_NONE,
        DAMV_SLICING, 6, 10, LBL_BRANDS },
    { WPN_DEMON_BLADE,           "demon blade",           13, -1, 13,  3,
        SK_LONG_BLADES,  SIZE_LITTLE,  SIZE_SMALL,  MI_NONE,
        DAMV_SLICING, 0, 2, DEMON_BRANDS },
    { WPN_EUDEMON_BLADE,         "eudemon blade",         14, -2, 12,  3,
        SK_LONG_BLADES,  SIZE_LITTLE,  SIZE_SMALL,  MI_NONE,
        DAMV_SLICING, 0, 0, HOLY_BRANDS },
    { WPN_DOUBLE_SWORD,          "double sword",          15, -1, 15,  3,
        SK_LONG_BLADES,  SIZE_LITTLE,  SIZE_MEDIUM, MI_NONE,
        DAMV_SLICING, 0, 2, LBL_BRANDS },
    { WPN_GREAT_SWORD,           "great sword",           16, -3, 16,  5,
        SK_LONG_BLADES,  SIZE_MEDIUM,  NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_SLICING, 6, 10, LBL_BRANDS },
    { WPN_TRIPLE_SWORD,          "triple sword",          19, -4, 19,  5,
        SK_LONG_BLADES,  SIZE_MEDIUM,  NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_SLICING, 0, 2, LBL_BRANDS },
#if TAG_MAJOR_VERSION == 34
    { WPN_BLESSED_FALCHION,      "old falchion",         8,  2, 13,  3,
        SK_LONG_BLADES,  SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_SLICING, 0, 0, {} },
    { WPN_BLESSED_LONG_SWORD,    "old long sword",      10,  1, 14,  3,
        SK_LONG_BLADES,  SIZE_LITTLE,  SIZE_SMALL,  MI_NONE,
        DAMV_SLICING, 0, 0, {} },
    { WPN_BLESSED_SCIMITAR,      "old scimitar",        12, -2, 14,  3,
        SK_LONG_BLADES,  SIZE_LITTLE,  SIZE_SMALL,  MI_NONE,
        DAMV_SLICING, 0, 0, {} },
    { WPN_BLESSED_DOUBLE_SWORD, "old double sword",     15, -1, 15,  3,
        SK_LONG_BLADES,  SIZE_LITTLE,  SIZE_MEDIUM, MI_NONE,
        DAMV_SLICING, 0, 0, {} },
    { WPN_BLESSED_GREAT_SWORD,   "old great sword",     16, -3, 16,  5,
        SK_LONG_BLADES,  SIZE_MEDIUM,  NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_SLICING, 0, 0, {} },
    { WPN_BLESSED_TRIPLE_SWORD,      "old triple sword",19, -4, 19,  5,
        SK_LONG_BLADES,  SIZE_MEDIUM,  NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_SLICING, 0, 0, {} },
#endif

    // Axes
    { WPN_HAND_AXE,          "hand axe",            7,  3, 13,  6,
        SK_AXES,       SIZE_LITTLE,  SIZE_LITTLE, MI_NONE,
        DAMV_CHOPPING, 9, 10, AXE_BRANDS },
    { WPN_WAR_AXE,           "war axe",            11,  0, 15,  7,
        SK_AXES,       SIZE_LITTLE,  SIZE_SMALL,  MI_NONE,
        DAMV_CHOPPING, 7, 10, AXE_BRANDS },
    { WPN_BROAD_AXE,         "broad axe",          13, -2, 16,  8,
        SK_AXES,       SIZE_LITTLE,  SIZE_MEDIUM, MI_NONE,
        DAMV_CHOPPING, 4, 10, AXE_BRANDS },
    { WPN_BATTLEAXE,         "battleaxe",          15, -4, 17,  8,
        SK_AXES,       SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CHOPPING, 6, 10, AXE_BRANDS },
    { WPN_EXECUTIONERS_AXE,  "executioner's axe",  18, -6, 20,  9,
        SK_AXES,       SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CHOPPING, 0, 2, AXE_BRANDS },

    // Polearms
    { WPN_SPEAR,             "spear",               6,  4, 11,  3,
        SK_POLEARMS,     SIZE_LITTLE,  SIZE_SMALL,  MI_NONE,
        DAMV_PIERCING, 8, 10, {
            { SPWPN_NORMAL,     46 },
            { SPWPN_VENOM,      17 },
            { SPWPN_VORPAL,     12 },
            { SPWPN_FLAMING,     7 },
            { SPWPN_FREEZING,    7 },
            { SPWPN_VAMPIRISM,   5 },
            { SPWPN_DISTORTION,  2 },
            { SPWPN_PAIN,        2 },
            { SPWPN_ANTIMAGIC,   2 },
        }},
    { WPN_TRIDENT,           "trident",             9,  1, 13,  4,
        SK_POLEARMS,     SIZE_LITTLE,  SIZE_MEDIUM, MI_NONE,
        DAMV_PIERCING, 6, 10, POLEARM_BRANDS },
    { WPN_HALBERD,           "halberd",            13, -3, 15,  5,
        SK_POLEARMS,     SIZE_MEDIUM,  NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_CHOPPING | DAM_PIERCE, 5, 10, POLEARM_BRANDS },
    { WPN_SCYTHE,            "scythe",             14, -4, 20,  7,
        SK_POLEARMS,     SIZE_MEDIUM,  NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_SLICING, 2, 0, POLEARM_BRANDS },
    { WPN_DEMON_TRIDENT,     "demon trident",      12,  1, 13,  4,
        SK_POLEARMS,     SIZE_LITTLE,    SIZE_MEDIUM, MI_NONE,
        DAMV_PIERCING, 0, 2, DEMON_BRANDS },
    { WPN_TRISHULA,          "trishula",           13,  0, 13,  4,
        SK_POLEARMS,     SIZE_LITTLE,    SIZE_MEDIUM, MI_NONE,
        DAMV_PIERCING, 0, 0, HOLY_BRANDS },
    { WPN_GLAIVE,            "glaive",             15, -3, 17,  6,
        SK_POLEARMS,     SIZE_MEDIUM,    NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_CHOPPING, 5, 10, POLEARM_BRANDS },
    { WPN_BARDICHE,          "bardiche",           18, -6, 20,  8,
        SK_POLEARMS,     SIZE_MEDIUM,    NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_CHOPPING, 1, 2, POLEARM_BRANDS },

    // Staves
    // WPN_STAFF is for weapon stats for magical staves only.
    { WPN_STAFF,             "staff",               5,  5, 12,  3,
        SK_STAVES,       SIZE_LITTLE,  SIZE_MEDIUM, MI_NONE,
        DAMV_CRUSHING, 0, 0, {} },
    { WPN_QUARTERSTAFF,      "quarterstaff",        10, 3, 13,  3,
        SK_STAVES,       SIZE_MEDIUM,  NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_CRUSHING, 8, 10, {
            { SPWPN_NORMAL,     50 },
            { SPWPN_PROTECTION, 18 },
            { SPWPN_DRAINING,    8 },
            { SPWPN_VORPAL,      8 },
            { SPWPN_SPEED,       8 },
            { SPWPN_DISTORTION,  2 },
            { SPWPN_PAIN,        2 },
            { SPWPN_HOLY_WRATH,  2 },
            { SPWPN_ANTIMAGIC,   2 },
        }},
    { WPN_LAJATANG,          "lajatang",            16,-3, 14,  3,
        SK_STAVES,       SIZE_MEDIUM,  NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_SLICING, 2, 2, {
            { SPWPN_NORMAL,         34 },
            { SPWPN_SPEED,          12 },
            { SPWPN_ELECTROCUTION,  12 },
            { SPWPN_VAMPIRISM,      12 },
            { SPWPN_PROTECTION,      9 },
            { SPWPN_VENOM,           7 },
            { SPWPN_PAIN,            7 },
            { SPWPN_ANTIMAGIC,       4 },
            { SPWPN_DISTORTION,      3 },
        }},

    // Range weapons
    { WPN_BLOWGUN,           "blowgun",             0,  2, 10,  0,
        SK_THROWING,     SIZE_LITTLE,  SIZE_LITTLE, MI_NEEDLE,
        DAMV_NON_MELEE, 5, 0, {
            { SPWPN_EVASION,  3 },
            { SPWPN_NORMAL,  97 },
        }},

    { WPN_HUNTING_SLING,     "hunting sling",       5,  2, 12,  1,
        SK_SLINGS,       SIZE_LITTLE,  SIZE_LITTLE, MI_STONE,
        DAMV_NON_MELEE, 8, 10, RANGED_BRANDS },
    { WPN_GREATSLING,        "greatsling",          8, -1, 14,  1,
        SK_SLINGS,       SIZE_LITTLE,  SIZE_SMALL, MI_STONE,
        DAMV_NON_MELEE, 2, 2, RANGED_BRANDS },

    { WPN_HAND_CROSSBOW,     "hand crossbow",      12,  5, 15,  5,
        SK_CROSSBOWS,    SIZE_LITTLE, SIZE_LITTLE, MI_BOLT,
        DAMV_NON_MELEE, 7, 10, RANGED_BRANDS },
    { WPN_ARBALEST,          "arbalest",           18,  2, 19,  8,
        SK_CROSSBOWS,    SIZE_LITTLE, NUM_SIZE_LEVELS, MI_BOLT,
        DAMV_NON_MELEE, 5, 10, RANGED_BRANDS },
    { WPN_TRIPLE_CROSSBOW,   "triple crossbow",    22,  0, 23,  9,
        SK_CROSSBOWS,    SIZE_SMALL,  NUM_SIZE_LEVELS, MI_BOLT,
        DAMV_NON_MELEE, 0, 2, RANGED_BRANDS },

    { WPN_SHORTBOW,          "shortbow",            9,  2, 13,  2,
        SK_BOWS,         SIZE_LITTLE,  NUM_SIZE_LEVELS, MI_ARROW,
        DAMV_NON_MELEE, 8, 10, RANGED_BRANDS },
    { WPN_LONGBOW,           "longbow",            15,  0, 17,  3,
        SK_BOWS,         SIZE_MEDIUM,  NUM_SIZE_LEVELS, MI_ARROW,
        DAMV_NON_MELEE, 2, 10, RANGED_BRANDS },
};

struct missile_def
{
    int         id;
    const char *name;
    int         dam;
    int         mulch_rate;
    bool        throwable;
};

static int Missile_index[NUM_MISSILES];
static const missile_def Missile_prop[] =
{
#if TAG_MAJOR_VERSION == 34
    { MI_DART,          "dart",          2, 1,  true  },
#endif
    { MI_NEEDLE,        "needle",        0, 12, false },
    { MI_STONE,         "stone",         2, 8,  true  },
    { MI_ARROW,         "arrow",         0, 8,  false },
    { MI_BOLT,          "bolt",          0, 8,  false },
    { MI_LARGE_ROCK,    "large rock",   20, 25, true  },
    { MI_SLING_BULLET,  "sling bullet",  4, 8,  false },
    { MI_JAVELIN,       "javelin",      10, 20, true  },
    { MI_THROWING_NET,  "throwing net",  0, 0,  true  },
    { MI_TOMAHAWK,      "tomahawk",      6, 20, true  },
};

struct food_def
{
    int         id;
    const char *name;
    int         value;
    int         carn_mod;
    int         herb_mod;
    int         turns;
};

static int Food_index[NUM_FOODS];
static const food_def Food_prop[] =
{
    { FOOD_MEAT_RATION,  "meat ration",  5000,   500, -1500,  3 },
    { FOOD_CHUNK,        "chunk",        1000,   100,  -500,  3 },
    { FOOD_BEEF_JERKY,   "beef jerky",   1500,   200,  -200,  1 },

    { FOOD_BREAD_RATION, "bread ration", 4400, -1000,   500,  3 },

    { FOOD_FRUIT,        "fruit",         850,  -100,    50,  1 },

    { FOOD_ROYAL_JELLY,  "royal jelly",  2000,     0,     0,  3 },
    { FOOD_PIZZA,        "pizza",        1500,     0,     0,  1 },

#if TAG_MAJOR_VERSION == 34
    { FOOD_UNUSED,       "buggy",           0,     0,     0,  1 },
    { FOOD_AMBROSIA,     "buggy",           0,     0,     0,  1 },
    { FOOD_ORANGE,       "buggy",        1000,  -300,   300,  1 },
    { FOOD_BANANA,       "buggy",        1000,  -300,   300,  1 },
    { FOOD_LEMON,        "buggy",        1000,  -300,   300,  1 },
    { FOOD_PEAR,         "buggy",         700,  -200,   200,  1 },
    { FOOD_APPLE,        "buggy",         700,  -200,   200,  1 },
    { FOOD_APRICOT,      "buggy",         700,  -200,   200,  1 },
    { FOOD_CHOKO,        "buggy",         600,  -200,   200,  1 },
    { FOOD_RAMBUTAN,     "buggy",         600,  -200,   200,  1 },
    { FOOD_LYCHEE,       "buggy",         600,  -200,   200,  1 },
    { FOOD_STRAWBERRY,   "buggy",         200,   -50,    50,  1 },
    { FOOD_GRAPE,        "buggy",         100,   -20,    20,  1 },
    { FOOD_SULTANA,      "buggy",          70,   -20,    20,  1 },
    { FOOD_CHEESE,       "buggy",        1200,     0,     0,  1 },
    { FOOD_SAUSAGE,      "buggy",        1200,   150,  -400,  1 },
#endif
};

// Must call this functions early on so that the above tables can
// be accessed correctly.
void init_properties()
{
    // The compiler would complain about too many initializers but not
    // about too few, so we check this ourselves.
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
    return _full_ident_mask(item) & ISFLAG_KNOW_CURSE
           && item_ident(item, ISFLAG_KNOW_CURSE) && item.cursed();
}

// Curses a random player inventory item.
bool curse_an_item(bool ignore_holy_wrath)
{
    // allowing these would enable mummy scumming
    if (you_worship(GOD_ASHENZARI))
    {
        mprf(MSGCH_GOD, "The curse is absorbed by %s.",
             god_name(GOD_ASHENZARI).c_str());
        return false;
    }

    int count = 0;
    int item  = ENDOFPACK;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (!you.inv[i].defined())
            continue;

        if (is_weapon(you.inv[i])
            || you.inv[i].base_type == OBJ_ARMOUR
            || you.inv[i].base_type == OBJ_JEWELLERY)
        {
            if (you.inv[i].cursed())
                continue;

            if (ignore_holy_wrath && you.inv[i].base_type == OBJ_WEAPONS
                && get_weapon_brand(you.inv[i]) == SPWPN_HOLY_WRATH)
            {
                continue;
            }

            // Item is valid for cursing, so we'll give it a chance.
            count++;
            if (one_chance_in(count))
                item = i;
        }
    }

    // Any item to curse?
    if (item == ENDOFPACK)
        return false;

    do_curse_item(you.inv[item], false);

    return true;
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
            const bool was_known = is_artefact(item)
                                 ? artefact_known_property(item, ARTP_BRAND)
                                 : item_ident(item, ISFLAG_KNOW_TYPE);
            mprf("Your %s glows black briefly, but repels the curse.",
                 item.name(DESC_PLAIN).c_str());
            if (is_artefact(item))
                artefact_learn_prop(item, ARTP_BRAND);
            else
                set_ident_flags(item, ISFLAG_KNOW_TYPE);

            if (!was_known)
                mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
        }
        return;
    }

    if (!quiet)
    {
        mprf("Your %s glows black for a moment.",
             item.name(DESC_PLAIN).c_str());

        // If we get the message, we know the item is cursed now.
        item.flags |= ISFLAG_KNOW_CURSE;
    }

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
        if (in_inventory(item))
            item.flags |= ISFLAG_KNOW_CURSE;
        return;
    }

    if (no_ash && you_worship(GOD_ASHENZARI))
    {
        simple_god_message(" preserves the curse.");
        return;
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

    if (check_bondage)
        ash_check_bondage();
}

/**
 * Make a net stationary (because it currently traps a victim).
 *
 * @param item The net item.
*/
void set_net_stationary(item_def &item)
{
    if (item.is_type(OBJ_MISSILES, MI_THROWING_NET))
        item.net_placed = true;
}

/**
 * Is the item stationary (unmovable)?
 *
 * Currently only carrion and nets with a trapped victim are stationary.
 * @param item The item.
 * @return  True iff the item is stationary.
*/
bool item_is_stationary(const item_def &item)
{
    return item.base_type == OBJ_CORPSES || item_is_stationary_net(item);
}

/**
 * Is the item a stationary net?
 *
 * @param item The item.
 * @return  True iff the item is a stationary net.
*/
bool item_is_stationary_net(const item_def &item)
{
    return item.is_type(OBJ_MISSILES, MI_THROWING_NET) && item.net_placed;
}

/**
 * Get the actor held in a stationary net.
 *
 * @param net A stationary net item.
 * @return  A pointer to the actor in the net, guaranteed to be non-null.
 */
actor *net_holdee(const item_def &net)
{
    ASSERT(item_is_stationary_net(net));
    // Stationary nets should not be in inventory etc.
    ASSERT_IN_BOUNDS(net.pos);
    actor * const a = actor_at(net.pos);
    ASSERTM(a, "No actor in stationary net at (%d,%d)", net.pos.x, net.pos.y);
    return a;
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
    if (is_shop_item(item))
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
    return (item.flags & flags) == flags;
}

void set_ident_flags(item_def &item, iflags_t flags)
{
    preserve_quiver_slots p;
    if ((item.flags & flags) != flags)
    {
        item.flags |= flags;
        request_autoinscribe();

        if (in_inventory(item))
        {
            shopping_list.cull_identical_items(item);
            item_skills(item, you.start_train);
        }
    }

    if (fully_identified(item))
    {
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
#if TAG_MAJOR_VERSION == 34
        if (item.sub_type == BOOK_BUGGY_DESTRUCTION)
        {
            flagset = 0;
            break;
        }
        // Intentional fall-through.
#endif
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
// Equipment description:
//
iflags_t get_equip_desc(const item_def &item)
{
    return item.flags & ISFLAG_COSMETIC_MASK;
}

void set_equip_desc(item_def &item, iflags_t flags)
{
    ASSERT((flags & ~ISFLAG_COSMETIC_MASK) == 0);

    item.flags &= ~ISFLAG_COSMETIC_MASK; // delete previous
    item.flags |= flags;
}

bool is_helmet(const item_def& item)
{
    return item.base_type == OBJ_ARMOUR && get_armour_slot(item) == EQ_HELMET;
}

bool is_hard_helmet(const item_def &item)
{
    return item.is_type(OBJ_ARMOUR, ARM_HELMET);
}

//
// Ego item functions:
//

/**
 * For a given weapon type, randomly choose an appropriate brand.
 *
 * @param wpn_type  The type of weapon in question.
 * @return          An appropriate brand. (e.g. fire, pain, venom, etc)
 *                  May be SPWPN_NORMAL.
 */
brand_type choose_weapon_brand(weapon_type wpn_type)
{
    const vector<brand_weight_tuple> weights
        = Weapon_prop[ Weapon_index[wpn_type] ].brand_weights;
    if (!weights.size())
        return SPWPN_NORMAL;

    const brand_type *brand = random_choose_weighted(weights);
    ASSERT(brand);
    return *brand;
}

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
        return static_cast<brand_type>(artefact_property(item, ARTP_BRAND));

    return static_cast<brand_type>(item.brand);
}

special_missile_type get_ammo_brand(const item_def &item)
{
    // No artefact arrows yet. -- bwr
    if (item.base_type != OBJ_MISSILES || is_artefact(item))
        return SPMSL_NORMAL;

    return static_cast<special_missile_type>(item.brand);
}

special_armour_type get_armour_ego_type(const item_def &item)
{
    // Armour ego types are "brands", so we do the randart lookup here.
    if (item.base_type != OBJ_ARMOUR)
        return SPARM_NORMAL;

    if (is_artefact(item))
    {
        return static_cast<special_armour_type>(
                   artefact_property(item, ARTP_BRAND));
    }

    return static_cast<special_armour_type>(item.brand);
}

/// A map between monster species & their hides.
static map<monster_type, armour_type> _monster_hides = {
    { MONS_TROLL,               ARM_TROLL_HIDE },
    { MONS_DEEP_TROLL,          ARM_TROLL_HIDE },
    { MONS_IRON_TROLL,          ARM_TROLL_HIDE },

    { MONS_FIRE_DRAGON,         ARM_FIRE_DRAGON_HIDE },
    { MONS_ICE_DRAGON,          ARM_ICE_DRAGON_HIDE },
    { MONS_STEAM_DRAGON,        ARM_STEAM_DRAGON_HIDE },
    { MONS_MOTTLED_DRAGON,      ARM_MOTTLED_DRAGON_HIDE },
    { MONS_STORM_DRAGON,        ARM_STORM_DRAGON_HIDE },
    { MONS_GOLDEN_DRAGON,       ARM_GOLD_DRAGON_HIDE },
    { MONS_SWAMP_DRAGON,        ARM_SWAMP_DRAGON_HIDE },
    { MONS_PEARL_DRAGON,        ARM_PEARL_DRAGON_HIDE },
    { MONS_SHADOW_DRAGON,       ARM_SHADOW_DRAGON_HIDE },
    { MONS_QUICKSILVER_DRAGON,  ARM_QUICKSILVER_DRAGON_HIDE },
};

/**
 * If a monster of the given type is butchered, what kind of hide can it leave?
 *
 * @param mc    The class of monster in question.
 * @return      The armour_type of the given monster's hide, or NUM_ARMOURS if
 *              the monster does not leave a hide.
 */
armour_type hide_for_monster(monster_type mc)
{
    return lookup(_monster_hides, mons_species(mc), NUM_ARMOURS);
}

// in principle, you can imagine specifying something that would generate this
// & _monster_hides from a set of { monster_type, hide_type, armour_type }
// triples. possibly loading from a file? ideally in a way that's nicer than
// the horror that is art-data.*

/// A map between hide & armour types.
static map<armour_type, armour_type> _hide_armours = {
    { ARM_TROLL_HIDE,               ARM_TROLL_LEATHER_ARMOUR },
    { ARM_FIRE_DRAGON_HIDE,         ARM_FIRE_DRAGON_ARMOUR },
    { ARM_ICE_DRAGON_HIDE,          ARM_ICE_DRAGON_ARMOUR },
    { ARM_STEAM_DRAGON_HIDE,        ARM_STEAM_DRAGON_ARMOUR },
    { ARM_MOTTLED_DRAGON_HIDE,      ARM_MOTTLED_DRAGON_ARMOUR },
    { ARM_STORM_DRAGON_HIDE,        ARM_STORM_DRAGON_ARMOUR },
    { ARM_GOLD_DRAGON_HIDE,         ARM_GOLD_DRAGON_ARMOUR },
    { ARM_SWAMP_DRAGON_HIDE,        ARM_SWAMP_DRAGON_ARMOUR },
    { ARM_PEARL_DRAGON_HIDE,        ARM_PEARL_DRAGON_ARMOUR },
    { ARM_SHADOW_DRAGON_HIDE,       ARM_SHADOW_DRAGON_ARMOUR },
    { ARM_QUICKSILVER_DRAGON_HIDE,  ARM_QUICKSILVER_DRAGON_ARMOUR },
};

/**
 * If a hide of the given type is enchanted, what kind of armour will it turn
 * into?
 *
 * @param hide_type     The type of hide armour in question.
 * @return              The corresponding enchanted armour, or NUM_ARMOURS if
 *                      the given armour does not change types when enchanted.
 */
armour_type armour_for_hide(armour_type hide_type)
{
    return lookup(_hide_armours, hide_type, NUM_ARMOURS);
}

// Armour information and checking functions.

/**
 * Attempt to turn a piece of armour into a new type upon enchanting it.
 *
 * @param item      The armour being enchanted.
 * @return          Whether the armour was transformed.
 */
bool hide2armour(item_def &item)
{
    if (item.base_type != OBJ_ARMOUR)
        return false;

    const armour_type new_type = armour_for_hide(static_cast<armour_type>
                                                 (item.sub_type));
    if (new_type == NUM_ARMOURS)
        return false;

    item.sub_type = new_type;
    return true;
}

/**
 * Return the enchantment limit of a piece of armour.
 *
 * @param item      The item being considered.
 * @return          The maximum enchantment the item can hold.
 */
int armour_max_enchant(const item_def &item)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    if (item.sub_type == ARM_QUICKSILVER_DRAGON_ARMOUR)
        return 0;

    const int eq_slot = get_armour_slot(item);

    int max_plus = MAX_SEC_ENCHANT;
    if (eq_slot == EQ_BODY_ARMOUR
        || item.sub_type == ARM_CENTAUR_BARDING
        || item.sub_type == ARM_NAGA_BARDING)
    {
        max_plus = property(item, PARM_AC);
    }
    else if (eq_slot == EQ_SHIELD)
        // 3 / 5 / 8 for bucklers/shields/lg. shields
        max_plus = (property(item, PARM_AC) - 3)/2 + 3;

    return max_plus;
}

/**
 * Find the set of all armours made from hides.
 */
static set<armour_type> _make_hide_armour_set()
{
    set<armour_type> _hide_armour_set;
    // iter over armours created from hides
    for (auto it: _hide_armours)
        _hide_armour_set.insert(it.second);
    return _hide_armour_set;
}
static set<armour_type> _hide_armour_set = _make_hide_armour_set();

/**
 * Is the given armour a type that changes when enchanted (i.e. dragon or troll
 * hide?
 *
 * @param item      The armour in question.
 * @param inc_made  Whether to also accept armour that has already been
 *                  enchanted & transformed (e.g. fda in addition to fire
 *                  dragon hides, etc)
 * @return          Whether the given item is (or was?) a hide.
 *                  (Note that ARM_ANIMAL_SKIN cannot be enchanted & so doesn't
 *                  count.)
 */
bool armour_is_hide(const item_def &item, bool inc_made)
{
    ASSERT(item.base_type == OBJ_ARMOUR);
    return armour_type_is_hide(static_cast<armour_type>(item.sub_type),
                               inc_made);
}

/**
 * Is the given armour a type that changes when enchanted (i.e. dragon or troll
 * hide?
 *
 * @param type      The armour_type of armour in question.
 * @param inc_made  Whether to also accept armour that has already been
 *                  enchanted & transformed (e.g. fda in addition to fire
 *                  dragon hides, etc)
 * @return          Whether the given item type is (or was?) a hide.
 *                  (Note that ARM_ANIMAL_SKIN cannot be enchanted & so doesn't
 *                  count.)
 */
bool armour_type_is_hide(int _type, bool inc_made)
{
    const armour_type type = static_cast<armour_type>(_type);
    // actual hides?
    if (_hide_armours.count(type))
        return true;
    // armour made from hides?
    return inc_made && _hide_armour_set.count(type);
}

/**
 * Generate a map from hides to the monsters that source them.
 */
static map<armour_type, monster_type> _make_hide_monster_map()
{
    map<armour_type, monster_type> hide_to_mons;
    for (auto monster_data : _monster_hides)
    {
        const armour_type hide = monster_data.second;
        const monster_type mon = monster_data.first;
        hide_to_mons[hide] = mon;

        const armour_type *arm = map_find(_hide_armours, hide);
        ASSERT(arm);
        hide_to_mons[*arm] = mon;
    }

    // troll hides are generated by multiple monsters, so set a canonical troll
    // by hand
    hide_to_mons[ARM_TROLL_HIDE] = MONS_TROLL;
    hide_to_mons[ARM_TROLL_LEATHER_ARMOUR] = MONS_TROLL;

    return hide_to_mons;
}

static map<armour_type, monster_type> hide_to_mons = _make_hide_monster_map();

/**
 * Find the monster that the given type of hide/dragon/troll armour came from.
 *
 * @param arm   The type of armour in question.
 * @return      The corresponding monster type; e.g. MONS_FIRE_DRAGON for
 *              ARM_FIRE_DRAGON_HIDE or ARM_FIRE_DRAGON_ARMOUR, MONS_TROLL
 *              for ARM_TROLL_HIDE or ARM_TROLL_LEATHER_ARMOUR...
 */
monster_type monster_for_hide(armour_type arm)
{
    monster_type *mon = map_find(hide_to_mons, arm);
    ASSERT(mon);
    return *mon;
}

/**
 * Is this armour considered inherently 'special' for acquirement?
 *
 * @param arm   The armour item in question.
 * @return      Whether that type of armour is considered 'special'.
 */
bool armour_is_special(const item_def &item)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    return !Armour_prop[ Armour_index[item.sub_type] ].mundane;
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

    return jewellery_is_amulet(item.sub_type);
}

bool jewellery_is_amulet(int sub_type)
{
    return sub_type >= AMU_FIRST_AMULET;
}

// Returns number of sizes off (0 if fitting).
int fit_armour_size(const item_def &item, size_type size)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    const size_type min = Armour_prop[ Armour_index[item.sub_type] ].fit_min;
    const size_type max = Armour_prop[ Armour_index[item.sub_type] ].fit_max;

    if (size < min)
        return min - size;    // -'ve means levels too small
    else if (size > max)
        return max - size;    // +'ve means levels too large

    return 0;
}

// Returns true if armour fits size (shape needs additional verification).
bool check_armour_size(const item_def &item, size_type size)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    return fit_armour_size(item, size) == 0;
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
            && it.charges >= wand_max_charges(it.sub_type))
        {
            return false;
        }
        return true;
    }
    else if (it.base_type == OBJ_RODS)
    {
        if (!hide_charged)
            return true;

        if (item_ident(it, ISFLAG_KNOW_PLUSES))
        {
            return it.charge_cap < MAX_ROD_CHARGE * ROD_CHARGE_MULT
                   || it.charges < it.charge_cap
                   || it.rod_plus < MAX_WPN_ENCHANT;
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

    case WAND_FLAME:
    case WAND_FROST:
    case WAND_MAGIC_DARTS:
    case WAND_SLOWING:
    case WAND_CONFUSION:
    case WAND_RANDOM_EFFECTS:
        return 16;
    }
}

int wand_max_charges(int type)
{
    return wand_charge_value(type) * 3;
}

/**
 * Is the given item a wand which is both empty & known to be empty?
 *
 * @param item  The item in question.
 * @return      Whether the wand is charge-id'd and empty, or at least known
 *              {empty}.
 */
bool is_known_empty_wand(const item_def &item)
{
    if (item.base_type != OBJ_WANDS)
        return false;

    // not charge-ID'd, but known empty (probably through hard experience)
    if (item.used_count == ZAPCOUNT_EMPTY)
        return true;

    return item_ident(item, ISFLAG_KNOW_PLUSES) && item.charges <= 0;
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

    // If we don't know the plusses, assume enchanting is possible.
    if (unknown && !is_artefact(arm) && !item_ident(arm, ISFLAG_KNOW_PLUSES))
        return true;

    // Artefacts or highly enchanted armour cannot be enchanted, only
    // uncursed.
    if (is_artefact(arm) || arm.plus >= armour_max_enchant(arm))
    {
        if (!uncurse || you_worship(GOD_ASHENZARI))
            return false;
        if (unknown && !item_ident(arm, ISFLAG_KNOW_CURSE))
            return true;
        return arm.cursed();
    }

    return true;
}

//
// Weapon information and checking functions:
//

// Checks how rare a weapon is. Many of these have special routines for
// placement, especially those with a rarity of zero. Chance is out of 10.
// ^^^ vvv "rarity" is exactly the wrong term - inverted...
int weapon_rarity(int w_type)
{
    return Weapon_prop[Weapon_index[w_type]].commonness;
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
    return get_damage_type(item) & dam_type;
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

// Give hands required to wield weapon for a torso of "size".
// Not adjusted by species or anything, which is why it's "basic".
hands_reqd_type basic_hands_reqd(const item_def &item, size_type size)
{
    const int wpn_type = OBJ_WEAPONS == item.base_type ? item.sub_type :
                         OBJ_RODS == item.base_type    ? WPN_ROD :
                         OBJ_STAVES == item.base_type  ? WPN_STAFF :
                                                         WPN_UNKNOWN;

    // Non-weapons.
    if (wpn_type == WPN_UNKNOWN)
        return HANDS_ONE;
    if (is_unrandom_artefact(item, UNRAND_GYRE))
        return HANDS_TWO;
    return size >= Weapon_prop[Weapon_index[wpn_type]].min_1h_size ? HANDS_ONE
                                                                   : HANDS_TWO;
}

hands_reqd_type hands_reqd(const actor* ac, object_class_type base_type, int sub_type)
{
    item_def item;
    item.base_type = base_type;
    item.sub_type  = sub_type;
    return ac->hands_reqd(item);
}

/**
 * Is the provided type a kind of giant club?
 *
 * @param wpn_type  The weapon_type under consideration.
 * @return          Whether it's a kind of giant club.
 */
bool is_giant_club_type(int wpn_type)
{
    return wpn_type == WPN_GIANT_CLUB
           || wpn_type == WPN_GIANT_SPIKED_CLUB;
}

/**
 * Is the provided type a kind of ranged weapon?
 *
 * @param wpn_type  The weapon_type under consideration.
 * @return          Whether it's a kind of launcher.
 */
bool is_ranged_weapon_type(int wpn_type)
{
    return Weapon_prop[Weapon_index[wpn_type]].ammo != MI_NONE;
}

/**
 * Is the provided type a kind of demon weapon?
 *
 * @param wpn_type  The weapon_type under consideration.
 * @return          Whether it's a kind of demon weapon.
 */
bool is_demonic_weapon_type(int wpn_type)
{
    return wpn_type == WPN_DEMON_BLADE
           || wpn_type == WPN_DEMON_WHIP
           || wpn_type == WPN_DEMON_TRIDENT;
}

/**
 * Is the provided type a kind of blessed weapon?
 *
 * @param wpn_type  The weapon_type under consideration.
 * @return          Whether it's a kind of blessed weapon.
 */
bool is_blessed_weapon_type(int wpn_type)
{
    return wpn_type == WPN_EUDEMON_BLADE
           || wpn_type == WPN_SACRED_SCOURGE
           || wpn_type == WPN_TRISHULA;
}

/**
 * Is the weapon type provided magical (& can't be generated in a usual way)?
 * (I.e., magic staffs & rods.)
 *
 * @param wpn_type  The weapon_type under consideration.
 * @return          Whether it's a magic staff or rod.
 */
bool is_magic_weapon_type(int wpn_type)
{
    return wpn_type == WPN_STAFF || wpn_type == WPN_ROD;
}


bool is_melee_weapon(const item_def &weapon)
{
    return is_weapon(weapon) && !is_range_weapon(weapon);
}

/**
 * Is the provided item a demon weapon?
 *
 * @param item  The item under consideration.
 * @return      Whether the given item is a demon weapon.
 */
bool is_demonic(const item_def &item)
{
    return item.base_type == OBJ_WEAPONS
           && is_demonic_weapon_type(item.sub_type);
}

/**
 * Is the provided item a blessed weapon? (Not just holy wrath.)
 *
 * @param item  The item under consideration.
 * @return      Whether the given item is a blessed weapon.
 */
bool is_blessed(const item_def &item)
{
    return item.base_type == OBJ_WEAPONS
            && is_blessed_weapon_type(item.sub_type);
}

bool is_blessed_convertible(const item_def &item)
{
    return !is_artefact(item)
           && (item.base_type == OBJ_WEAPONS
               && (is_demonic(item)
                   || is_blessed(item)));
}

bool convert2good(item_def &item)
{
    if (item.base_type != OBJ_WEAPONS)
        return false;

    switch (item.sub_type)
    {
    default: return false;

    case WPN_DEMON_BLADE:   item.sub_type = WPN_EUDEMON_BLADE; break;
    case WPN_DEMON_WHIP:    item.sub_type = WPN_SACRED_SCOURGE; break;
    case WPN_DEMON_TRIDENT: item.sub_type = WPN_TRISHULA; break;
    }

    return true;
}

bool convert2bad(item_def &item)
{
    if (item.base_type != OBJ_WEAPONS)
        return false;

    switch (item.sub_type)
    {
    default: return false;

    case WPN_EUDEMON_BLADE:        item.sub_type = WPN_DEMON_BLADE; break;
    case WPN_SACRED_SCOURGE:       item.sub_type = WPN_DEMON_WHIP; break;
    case WPN_TRISHULA:             item.sub_type = WPN_DEMON_TRIDENT; break;
    }

    return true;
}

bool is_brandable_weapon(const item_def &wpn, bool allow_ranged, bool divine)
{
    if (wpn.base_type != OBJ_WEAPONS)
        return false;

    if (is_artefact(wpn))
        return false;

    if (!allow_ranged && is_range_weapon(wpn) || wpn.sub_type == WPN_BLOWGUN)
        return false;

    // Only gods can rebrand blessed weapons, and they revert back to their
    // old base type in the process.
    if (is_blessed(wpn) && !divine)
        return false;

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

/**
 * Returns the skill used by the given item to attack.
 *
 * @param item  The item under consideration.
 * @return      The skill used to attack with the given item; defaults to
 *              SK_FIGHTING if no melee or ranged skill applies.
 */
skill_type item_attack_skill(const item_def &item)
{
    if (item.base_type == OBJ_WEAPONS)
        return Weapon_prop[ Weapon_index[item.sub_type] ].skill;
    else if (item.base_type == OBJ_RODS)
        return SK_MACES_FLAILS; // Rods are short and stubby
    else if (item.base_type == OBJ_STAVES)
        return SK_STAVES;
    else if (item.base_type == OBJ_MISSILES && !has_launcher(item))
        return SK_THROWING;

    // This is used to mark that only fighting applies.
    return SK_FIGHTING;
}

/**
 * Returns the skill used by the given item type to attack.
 *
 * @param wclass  The item base type under consideration.
 * @param wtype   The item subtype under consideration.
 * @return      The skill used to attack with the given item type; defaults to
 *              SK_FIGHTING if no melee skill applies.
 */
skill_type item_attack_skill(object_class_type wclass, int wtype)
{
    item_def    wpn;

    wpn.base_type = wclass;
    wpn.sub_type = wtype;

    return item_attack_skill(wpn);
}

// True if item is a staff that deals extra damage based on Evocations skill.
static bool _staff_uses_evocations(const item_def &item)
{
    if (is_unrandom_artefact(item, UNRAND_ELEMENTAL_STAFF))
        return true;

    if (!item_type_known(item) || item.base_type != OBJ_STAVES)
        return false;

    switch (item.sub_type)
    {
    case STAFF_FIRE:
    case STAFF_COLD:
    case STAFF_POISON:
    case STAFF_DEATH:
    case STAFF_SUMMONING:
    case STAFF_AIR:
    case STAFF_EARTH:
        return true;
    default:
        return false;
    }
}

bool item_skills(const item_def &item, set<skill_type> &skills)
{
    if (item.is_type(OBJ_BOOKS, BOOK_MANUAL))
    {
        const skill_type skill = static_cast<skill_type>(item.plus);
        if (training_restricted(skill))
            skills.insert(skill);
    }

    // Jewellery with evokable abilities, wands and similar unwielded
    // evokers allow training.
    if (item_is_evokable(item, false, false, false, false, true)
        || item.base_type == OBJ_JEWELLERY && gives_ability(item))
    {
        skills.insert(SK_EVOCATIONS);
    }

    // Shields and abilities on armours allow training as long as your species
    // can wear them.
    if (item.base_type == OBJ_ARMOUR && can_wear_armour(item, false, true))
    {
        if (is_shield(item))
            skills.insert(SK_SHIELDS);

        if (gives_ability(item))
            skills.insert(SK_EVOCATIONS);
    }

    // Weapons, staves and rods allow training as long as your species can
    // wield them.
    if (!you.could_wield(item, true, true))
        return !skills.empty();

    if (item_is_evokable(item, false, false, false, false, false)
        || _staff_uses_evocations(item)
        || item.base_type == OBJ_WEAPONS && gives_ability(item))
    {
        skills.insert(SK_EVOCATIONS);
    }

    skill_type sk = item_attack_skill(item);
    if (sk != SK_FIGHTING && sk != SK_THROWING)
        skills.insert(sk);

    return !skills.empty();
}

/**
 * Checks if the provided weapon is wieldable by a creature of the given size.
 *
 * @param item      The weapon in question.
 * @param size      The size of the creature trying to wield the weapon.
 * @return          Whether a creature of the given size can wield the weapon.
 */
bool is_weapon_wieldable(const item_def &item, size_type size)
{
    ASSERT(is_weapon(item));

    // Staves and rods are currently wieldable for everyone just to be nice.
    if (item.base_type == OBJ_STAVES || item.base_type == OBJ_RODS
        || item_attack_skill(item) == SK_STAVES)
    {
        return true;
    }

    return Weapon_prop[Weapon_index[item.sub_type]].min_2h_size <= size;
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
    return is_weapon(item) && is_ranged_weapon_type(item.sub_type);
}

const char *ammo_name(missile_type ammo)
{
    return ammo < 0 || ammo >= NUM_MISSILES ? "eggplant"
           : Missile_prop[ Missile_index[ammo] ].name;
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
    return ammo.sub_type != MI_LARGE_ROCK
#if TAG_MAJOR_VERSION == 34
           && ammo.sub_type != MI_DART
#endif
           && ammo.sub_type != MI_JAVELIN
           && ammo.sub_type != MI_TOMAHAWK
           && ammo.sub_type != MI_THROWING_NET;
}

// Returns true if item can be reasonably thrown without a launcher.
bool is_throwable(const actor *actor, const item_def &wpn, bool force)
{
    if (wpn.base_type != OBJ_MISSILES)
        return false;

    const size_type bodysize = actor->body_size();

    if (!force)
    {
        if (wpn.sub_type == MI_LARGE_ROCK)
            return actor->can_throw_large_rocks();

        if (bodysize < SIZE_MEDIUM
            && wpn.sub_type == MI_JAVELIN)
        {
            return false;
        }
    }

    return Missile_prop[Missile_index[wpn.sub_type]].throwable;
}

// Decide if something is launched or thrown.
launch_retval is_launched(const actor *actor, const item_def *launcher,
                          const item_def &missile)
{
    if (missile.base_type != OBJ_MISSILES)
        return LRET_FUMBLED;

    if (launcher && missile.launched_by(*launcher))
        return LRET_LAUNCHED;

    return is_throwable(actor, missile) ? LRET_THROWN : LRET_FUMBLED;
}


/**
 * Returns whether a given missile will always destroyed on impact.
 *
 * @param missile      The missile in question.
 * @return             Whether the missile should always be destroyed on
 *                     impact.
 */
bool ammo_always_destroyed(const item_def &missile)
{
    const int brand = get_ammo_brand(missile);
    return brand == SPMSL_CHAOS
           || brand == SPMSL_DISPERSAL
           || brand == SPMSL_EXPLODING;
}

/**
 * Returns whether a given missile will never destroyed on impact.
 *
 * @param missile      The missile in question.
 * @return             Whether the missile should never be destroyed on impact.
 */
bool ammo_never_destroyed(const item_def &missile)
{
    return missile.sub_type == MI_THROWING_NET;
}

/**
 * Returns the one_chance_in for a missile type for be destroyed on impact.
 *
 * @param missile_type      The missile type to get the mulch chance for.
 * @return                  The inverse of the missile type's mulch chance.
 */
int ammo_type_destroy_chance(int missile_type)
{
    return Missile_prop[ Missile_index[missile_type] ].mulch_rate;
}

/**
 * Returns the base damage of a given item type.
 *
 * @param missile_type      The missile type to get the damage for.
 * @return                  The base damage of the given missile type.
 */
int ammo_type_damage(int missile_type)
{
    return Missile_prop[ Missile_index[missile_type] ].dam;
}


//
// Reaching functions:
//
reach_type weapon_reach(const item_def &item)
{
    if (item_attack_skill(item) == SK_POLEARMS)
        return REACH_TWO;
    return REACH_NONE;
}

//
// Macguffins
//
bool item_is_rune(const item_def &item, rune_type which_rune)
{
    return item.is_type(OBJ_MISCELLANY, MISC_RUNE_OF_ZOT)
           && (which_rune == NUM_RUNE_TYPES || item.plus == which_rune);
}

bool item_is_unique_rune(const item_def &item)
{
    return item_is_rune(item)
           && item.plus != RUNE_DEMONIC
           && item.plus != RUNE_ABYSSAL;
}

bool item_is_orb(const item_def &item)
{
    return item.is_type(OBJ_ORBS, ORB_ZOT);
}

bool item_is_horn_of_geryon(const item_def &item)
{
    return item.is_type(OBJ_MISCELLANY, MISC_HORN_OF_GERYON);
}

bool item_is_spellbook(const item_def &item)
{
    return item.base_type == OBJ_BOOKS
#if TAG_MAJOR_VERSION == 34
           && item.sub_type != BOOK_BUGGY_DESTRUCTION
#endif
           && item.sub_type != BOOK_MANUAL;
}

//
// Ring functions:

// Returns whether jewellery has plusses.
bool ring_has_pluses(const item_def &item)
{
    ASSERT(item.base_type == OBJ_JEWELLERY);

    // not known -> no plusses
    if (!item_type_known(item))
        return false;

    switch (item.sub_type)
    {
    case RING_SLAYING:
    case RING_PROTECTION:
    case RING_EVASION:
    case RING_STRENGTH:
    case RING_INTELLIGENCE:
    case RING_DEXTERITY:
        return true;

    default:
        break;
    }

    return false;
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
    case RING_STEALTH:
    case RING_LOUDNESS:
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

    return item.sub_type == POT_BLOOD
#if TAG_MAJOR_VERSION == 34
           || item.sub_type == POT_BLOOD_COAGULATED
#endif
            ;
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
    ASSERT(item.defined() && item.base_type == OBJ_FOOD);

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
    ASSERT(item.defined() && item.base_type == OBJ_FOOD);
    return Food_prop[Food_index[item.sub_type]].turns;
}

bool can_cut_meat(const item_def &item)
{
    return _does_damage_type(item, DAM_SLICE);
}

bool is_fruit(const item_def & item)
{
    return item.is_type(OBJ_FOOD, FOOD_FRUIT);
}

//
// Generic item functions:
//
int get_armour_res_fire(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    res += armour_type_prop(arm.sub_type, ARMF_RES_FIRE);

    // check ego resistance
    const int ego = get_armour_ego_type(arm);
    if (ego == SPARM_FIRE_RESISTANCE || ego == SPARM_RESISTANCE)
        res += 1;

    if (check_artp && is_artefact(arm))
        res += artefact_property(arm, ARTP_FIRE);

    return res;
}

int get_armour_res_cold(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    res += armour_type_prop(arm.sub_type, ARMF_RES_COLD);

    // check ego resistance
    const int ego = get_armour_ego_type(arm);
    if (ego == SPARM_COLD_RESISTANCE || ego == SPARM_RESISTANCE)
        res += 1;

    if (check_artp && is_artefact(arm))
        res += artefact_property(arm, ARTP_COLD);

    return res;
}

int get_armour_res_poison(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    res += armour_type_prop(arm.sub_type, ARMF_RES_POISON);

    // check ego resistance
    if (get_armour_ego_type(arm) == SPARM_POISON_RESISTANCE)
        res += 1;

    if (check_artp && is_artefact(arm))
        res += artefact_property(arm, ARTP_POISON);

    return res;
}

int get_armour_res_elec(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    res += armour_type_prop(arm.sub_type, ARMF_RES_ELEC);

    if (check_artp && is_artefact(arm))
        res += artefact_property(arm, ARTP_ELECTRICITY);

    return res;
}

int get_armour_life_protection(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    res += armour_type_prop(arm.sub_type, ARMF_RES_NEG);

    // check for ego resistance
    if (get_armour_ego_type(arm) == SPARM_POSITIVE_ENERGY)
        res += 1;

    if (check_artp && is_artefact(arm))
        res += artefact_property(arm, ARTP_NEGATIVE_ENERGY);

    return res;
}

int get_armour_res_magic(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    res += armour_type_prop(arm.sub_type, ARMF_RES_MAGIC) * MR_PIP;

    // check for ego resistance
    if (get_armour_ego_type(arm) == SPARM_MAGIC_RESISTANCE)
        res += MR_PIP;

    if (check_artp && is_artefact(arm))
        res += MR_PIP * artefact_property(arm, ARTP_MAGIC);

    return res;
}

bool get_armour_see_invisible(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    // check for ego resistance
    if (get_armour_ego_type(arm) == SPARM_SEE_INVISIBLE)
        return true;

    if (check_artp && is_artefact(arm))
        return artefact_property(arm, ARTP_EYESIGHT);

    return false;
}

int get_armour_res_sticky_flame(const item_def &arm)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    // intrinsic armour abilities
    return armour_type_prop(arm.sub_type, ARMF_RES_STICKY_FLAME);
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
        res += artefact_property(ring, ARTP_FIRE);

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
        res += artefact_property(ring, ARTP_COLD);

    return res;
}

int get_jewellery_res_poison(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    int res = 0;

    if (ring.sub_type == RING_POISON_RESISTANCE)
        res += 1;

    if (check_artp && is_artefact(ring))
        res += artefact_property(ring, ARTP_POISON);

    return res;
}

int get_jewellery_res_elec(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    int res = 0;

    if (check_artp && is_artefact(ring))
        res += artefact_property(ring, ARTP_ELECTRICITY);

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
        res += artefact_property(ring, ARTP_NEGATIVE_ENERGY);

    return res;
}

int get_jewellery_res_magic(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    int res = 0;

    if (ring.sub_type == RING_PROTECTION_FROM_MAGIC)
        res += 40;

    if (check_artp && is_artefact(ring))
        res += 40 * artefact_property(ring, ARTP_MAGIC);

    return res;
}

bool get_jewellery_see_invisible(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    if (ring.sub_type == RING_SEE_INVISIBLE)
        return true;

    if (check_artp && is_artefact(ring))
        return artefact_property(ring, ARTP_EYESIGHT);

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
                return Weapon_prop[ Weapon_index[item.sub_type] ].dam
                       + artefact_property(item, ARTP_BASE_DAM);
            case PWPN_HIT:
                return Weapon_prop[ Weapon_index[item.sub_type] ].hit
                       + artefact_property(item, ARTP_BASE_ACC);
            case PWPN_SPEED:
                return Weapon_prop[ Weapon_index[item.sub_type] ].speed
                       + artefact_property(item, ARTP_BASE_DELAY);
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

        if (ego == SPARM_INVISIBILITY || ego == SPARM_FLYING)
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
        if (artefact_property(item, static_cast<artefact_prop_type>(rap)))
            return true;

#if TAG_MAJOR_VERSION == 34
    if (artefact_property(item, ARTP_FOG))
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

        if (artefact_property(item, static_cast<artefact_prop_type>(rap)))
            return true;
    }

    return false;
}

bool is_item_jelly_edible(const item_def &item)
{
    // Don't eat artefacts.
    if (is_artefact(item))
        return false;

    // Don't eat mimics.
    if (item.flags & ISFLAG_MIMIC)
        return false;

    // Shouldn't eat stone things
    //   - but what about wands and rings?
    if (item.base_type == OBJ_MISSILES
        && (item.sub_type == MI_STONE || item.sub_type == MI_LARGE_ROCK))
    {
        return false;
    }

    // Don't eat special game items.
    if (item.base_type == OBJ_ORBS
        || (item.base_type == OBJ_MISCELLANY
            && (item.sub_type == MISC_RUNE_OF_ZOT
                || item.sub_type == MISC_HORN_OF_GERYON)))
    {
        return false;
    }

    return true;
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
        return jewellery_is_amulet(sub_type) ? EQ_AMULET : EQ_RINGS;

    default:
        break;
    }

    return EQ_NONE;
}

bool is_shield(const item_def &item)
{
    return item.base_type == OBJ_ARMOUR
           && get_armour_slot(item) == EQ_SHIELD;
}

// Returns true if the given item cannot be wielded _by you_ with the given shield.
// The currently equipped shield is used if no shield is passed in.
bool is_shield_incompatible(const item_def &weapon, const item_def *shield)
{
    // If there's no shield, there's no problem.
    if (!shield && !(shield = you.shield()))
        return false;

    hands_reqd_type hand = you.hands_reqd(weapon);
    return hand == HANDS_TWO;
}

bool shield_reflects(const item_def &shield)
{
    ASSERT(is_shield(shield));

    return get_armour_ego_type(shield) == SPARM_REFLECTION;
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

void remove_whitespace(string &str)
{
    str.erase(remove_if(str.begin(), str.end(),
        static_cast<int(*)(int)>(isspace)), str.end());
}

/**
 * Try to find a weapon, given the weapon's name without whitespace.
 *
 * @param name_nospace  The weapon's base name, with all whitespace removed.
 * @return              The id of the weapon, or WPN_UNKNOWN if nothing matches.
 */
weapon_type name_nospace_to_weapon(string name_nospace)
{
    for (const weapon_def &wpn : Weapon_prop)
    {
        string weap_nospace = wpn.name;
        remove_whitespace(weap_nospace);

        if (name_nospace == weap_nospace)
            return (weapon_type) wpn.id;
    }

    // No match found
    return WPN_UNKNOWN;
}

void seen_item(const item_def &item)
{
    if (!is_artefact(item) && _is_affordable(item))
    {
        // Known brands will be set in set_item_flags().
        if (item.base_type == OBJ_WEAPONS)
            you.seen_weapon[item.sub_type] |= 1U << SP_UNKNOWN_BRAND;
        if (item.base_type == OBJ_ARMOUR)
            you.seen_armour[item.sub_type] |= 1U << SP_UNKNOWN_BRAND;
        if (item.base_type == OBJ_MISCELLANY
            && !is_deck(item))
        {
            you.seen_misc.set(item.sub_type);
        }
    }

    // major hack.  Deconstify should be safe here, but it's still repulsive.
    ((item_def*)&item)->flags |= ISFLAG_SEEN;
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

/// Map of xp evokers to you.props[] xp debt keys.
static const map<int, const char*> debt_map = {
    { MISC_FAN_OF_GALES,        "fan_debt" },
    { MISC_LAMP_OF_FIRE,        "lamp_debt" },
    { MISC_STONE_OF_TREMORS,    "stone_debt" },
    { MISC_PHIAL_OF_FLOODS,     "phial_debt" },
    { MISC_HORN_OF_GERYON,      "horn_debt" },
};

/**
 * Is the given item an xp-charged evocable? (That is, one that recharges as
 * the player gains xp.)
 *
 * @param item      The item in question.
 * @return          Whether the given item is an xp evocable. (One of the
 *                  elemental evocables or the Horn of Geryon.)
 */
bool is_xp_evoker(const item_def &item)
{
    return item.base_type == OBJ_MISCELLANY
           && map_find(debt_map, item.sub_type);
}

/**
 * Return the xp debt corresponding to the given type of evoker.
 * Asserts that the given evoker type actually corresponds to an xp evoker.
 *
 * @param evoker_type       The misc_item_type of the evoker in question.
 * @return                  The level of xp debt the given evoker type has
 *                          before it can be used again.
 */
int &evoker_debt(int evoker_type)
{
    const char* const *prop_name = map_find(debt_map, evoker_type);
    ASSERT(prop_name);
    return you.props[*prop_name].get_int();
}

bool evoker_is_charged(const item_def &item)
{
    return evoker_debt(item.sub_type) == 0;
}


/// witchcraft. copied from mon-util.h's get_resist
static inline int _get_armour_flag(armflags_t all, armour_flag res)
{
    if (res > ARMF_LAST_MULTI)
        return all & res ? 1 : 0;
    int v = (all / res) & 7;
    if (v > 4)
        return v - 8;
    return v;
}

/**
 * What inherent special properties does the given armour type have?
 *
 * @param arm   The given armour type.
 * @return      A bitfield of special properties.
 */
static armflags_t _armour_type_flags(const uint8_t arm)
{
    // Ugly hack
    if (you.species == SP_TROLL && (arm == ARM_TROLL_HIDE
                                    || arm ==  ARM_TROLL_LEATHER_ARMOUR))
    {
        return ARMF_NO_FLAGS;
    }
    else
        return Armour_prop[ Armour_index[arm] ].flags;
}

/**
 * What value does the given armour type have for the innate special property?
 *
 * @param arm   The given armour type.
 * @param prop  The property in question.
 * @return      A value for that property; ranges -3 to 4.
 */
int armour_type_prop(const uint8_t arm, const armour_flag prop)
{
    return _get_armour_flag(_armour_type_flags(arm), prop);
}
