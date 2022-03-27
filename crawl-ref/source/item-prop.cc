/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "item-prop.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "artefact.h"
#include "art-enum.h"
#include "describe.h"
#include "god-passive.h"
#include "invent.h"
#include "items.h"
#include "item-status-flag-type.h"
#include "item-use.h"
#include "libutil.h" // map_find
#include "message.h"
#include "notes.h"
#include "orb-type.h"
#include "potion-type.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "xom.h"
#include "xp-evoker-data.h"

static iflags_t _full_ident_mask(const item_def& item);

// XXX: Name strings in most of the following are currently unused!
struct armour_def
{
    /// The armour_type enum of this armour type.
    armour_type         id;
    /// The name of the armour. (E.g. "robe".)
    const char         *name;
    /// The base AC value provided by the armour, before skill & enchant.
    int                 ac;
    /// The base EV penalty of the armour; used for EV, stealth, spell %, &c.
    int                 ev;
    /// The base price of the item in shops.
    int                 price;

    /// The slot the armour is equipped into; e.g. EQ_BOOTS.
    equipment_type      slot;
    /// The smallest size creature the armour will fit.
    size_type           fit_min;
    /// The largest size creature the armour will fit.
    size_type           fit_max;
    /// Whether this armour is mundane or inherently 'special', for acq.
    bool                mundane; // (special armour doesn't need egos etc)
    /// The resists, vulns, &c that this armour type gives when worn.
    armflags_t          flags;
    /// Used in body armour 'acquirement' code; higher = generated more.
    int                 acquire_weight;
};

// would be nice to lookup the name from monster_for_armour, but that
// leads to static initialization races (plus 'gold' special case)
#if TAG_MAJOR_VERSION == 34
#define DRAGON_ARMOUR(id, name, ac, evp, prc, res)                          \
    { ARM_ ## id ## _DRAGON_HIDE, "removed " name " dragon hide", 0, 0, 0,  \
      EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT, false, res, 0 },             \
    { ARM_ ## id ## _DRAGON_ARMOUR, name " dragon scales",  ac, evp, prc,   \
      EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT, false, res, 25 }
#else
#define DRAGON_ARMOUR(id, name, ac, evp, prc, res)                          \
    { ARM_ ## id ## _DRAGON_ARMOUR, name " dragon scales",  ac, evp, prc,   \
      EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT, false, res, 25 }
#endif

// Note: the Little-Giant range is used to make armours which are very
// flexible and adjustable and can be worn by any player character...
// providing they also pass the shape test, of course.
static int Armour_index[NUM_ARMOURS];
static const armour_def Armour_prop[] =
{
    { ARM_ANIMAL_SKIN,          "animal skin",            2,   0,     3,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT, true, ARMF_NO_FLAGS, 333 },
    { ARM_ROBE,                 "robe",                   2,   0,     7,
        EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_LARGE, true, ARMF_NO_FLAGS, 1000 },
    { ARM_LEATHER_ARMOUR,       "leather armour",         3,  -40,   20,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM, true },

    { ARM_RING_MAIL,            "ring mail",              5,  -70,   40,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM, true, ARMF_NO_FLAGS, 1000 },
    { ARM_SCALE_MAIL,           "scale mail",             6, -100,   40,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM, true, ARMF_NO_FLAGS, 1000 },
    { ARM_CHAIN_MAIL,           "chain mail",             8, -150,   45,
        EQ_BODY_ARMOUR, SIZE_SMALL,  SIZE_MEDIUM, true, ARMF_NO_FLAGS, 1000 },
    { ARM_PLATE_ARMOUR,         "plate armour",          10, -180,   230,
        EQ_BODY_ARMOUR, SIZE_SMALL, SIZE_MEDIUM, true, ARMF_NO_FLAGS, 1000 },
    { ARM_CRYSTAL_PLATE_ARMOUR, "crystal plate armour",  14, -230,   800,
        EQ_BODY_ARMOUR, SIZE_SMALL, SIZE_MEDIUM, false, ARMF_NO_FLAGS, 500 },

#if TAG_MAJOR_VERSION == 34
    { ARM_TROLL_HIDE, "removed troll hide",              0,    0,      0,
       EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT, false, ARMF_REGENERATION, 0 },
#endif
    { ARM_TROLL_LEATHER_ARMOUR, "troll leather armour",  4,  -40,    150,
       EQ_BODY_ARMOUR, SIZE_LITTLE, SIZE_GIANT, false, ARMF_REGENERATION, 50 },

    { ARM_CLOAK,                "cloak",                  1,   0,   45,
        EQ_CLOAK,       SIZE_LITTLE, SIZE_LARGE, true },
    { ARM_SCARF,                "scarf",                  0,   0,   50,
        EQ_CLOAK,       SIZE_LITTLE, SIZE_LARGE, true },

    { ARM_GLOVES,               "gloves",                 1,   0,   45,
        EQ_GLOVES,      SIZE_SMALL,  SIZE_MEDIUM, true },

    { ARM_HELMET,               "helmet",                 1,   0,   45,
        EQ_HELMET,      SIZE_SMALL,  SIZE_MEDIUM, true },

#if TAG_MAJOR_VERSION == 34
    { ARM_CAP,                  "cap",                    0,   0,   45,
        EQ_HELMET,      SIZE_LITTLE, SIZE_LARGE, true },
#endif

    { ARM_HAT,                  "hat",                    0,   0,   40,
        EQ_HELMET,      SIZE_TINY, SIZE_LARGE, true },

    // Note that barding size is compared against torso so it currently
    // needs to fit medium, but that doesn't matter as much as race
    // and shapeshift status.
    { ARM_BOOTS,                "boots",                  1,   0,   45,
        EQ_BOOTS,       SIZE_SMALL,  SIZE_MEDIUM, true },
    // Changed max. barding size to large to allow for the appropriate
    // monster types (monsters don't differentiate between torso and general).
#if TAG_MAJOR_VERSION == 34
    { ARM_CENTAUR_BARDING,      "centaur barding",        4,  -60,  230,
        EQ_BOOTS,       SIZE_MEDIUM, SIZE_LARGE, true },
#endif
    { ARM_BARDING,         "barding",           4,  -60,  230,
        EQ_BOOTS,       SIZE_MEDIUM, SIZE_LARGE, true },

    // Note: shields use ac-value as sh-value, EV pen is used as the basis
    // to calculate adjusted shield penalty.
    { ARM_ORB,                 "orb",                     0,   0,   90,
        EQ_SHIELD,      SIZE_LITTLE, SIZE_GIANT, true },
    { ARM_BUCKLER,             "buckler",                 3,  -50,  45,
        EQ_SHIELD,      SIZE_LITTLE, SIZE_MEDIUM, true },
    { ARM_KITE_SHIELD,         "kite shield",             8, -100,  45,
        EQ_SHIELD,      SIZE_SMALL,  SIZE_LARGE, true },
    { ARM_TOWER_SHIELD,        "tower shield",           13, -150,  45,
        EQ_SHIELD,      SIZE_MEDIUM, SIZE_GIANT, true },

    // Following all ARM_ entries for the benefit of util/gather_items
    DRAGON_ARMOUR(STEAM,       "steam",                   5,   0,   400,
        ARMF_RES_STEAM),
    DRAGON_ARMOUR(ACID,        "acid",                    6,  -50,  400,
        ARMF_RES_CORR),
    DRAGON_ARMOUR(QUICKSILVER, "quicksilver",             9,  -70,  600,
        ARMF_WILLPOWER),
    DRAGON_ARMOUR(SWAMP,       "swamp",                   7,  -70,  500,
        ARMF_RES_POISON),
    DRAGON_ARMOUR(FIRE,        "fire",                    8, -110,  600,
        ard(ARMF_RES_FIRE, 2) | ARMF_VUL_COLD),
    DRAGON_ARMOUR(ICE,         "ice",                     9, -110,  600,
        ard(ARMF_RES_COLD, 2) | ARMF_VUL_FIRE),
    DRAGON_ARMOUR(PEARL,       "pearl",                  10, -110, 1000,
        ARMF_RES_NEG),
    DRAGON_ARMOUR(STORM,       "storm",                  10, -150,  800,
        ARMF_RES_ELEC),
    DRAGON_ARMOUR(SHADOW,      "shadow",                 11, -150,  800,
        ard(ARMF_STEALTH, 4)),
    DRAGON_ARMOUR(GOLD,        "gold",                   12, -230,  800,
        ARMF_RES_FIRE | ARMF_RES_COLD | ARMF_RES_POISON),

#undef DRAGON_ARMOUR
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
    /// Base pricing for shops, before egos, enchantment, etc.
    int                 price;
    /// Used in non-artefact ego item generation. If empty, default to NORMAL.
    vector<brand_weight_tuple> brand_weights;
};

/**
  * "Why do we have all these ridiculous brand tables?"

  1) The main purpose of weapon brand distribution varying across weapon type
     is to help to balance the different weapon skills against each other -
     staves and short blades getting better brands as partial compensation for
     their other drawbacks, for instance. It is true that we have other knobs
     that we also use to balance different weapon types, but they don't all
     affect things in the same way. For instance, lajatangs having very good
     brands on average partially compensates for the rarity of good staves in a
     different way from how raising their base damage would - it means that
     finding a really great staff is of more comparable rarity to finding a
     really great axe. (This is important because finding a really great weapon
     like a lajatang of speed or elec or pain is one of the ways that players
     decide to use a weapon type in the first place.) Having this knob isn't
     redundant with having base damage and delay to modify - it is similar to
     being able to adjust the rarity of different base types of weapons.

 2)  The secondary purpose of varying weapon brand distribution is to give
     different weapon skills more individual feel. For instance, if you play a
     lot of maces chars in a row, then you will get used to using a lot of
     protection weapons and you'll never see vamp except on rare randarts, and
     then when you switch to axes for a few games you'll actually find vamp
     axes with some regularity and use them and be excited about that.

     This isn't a particularly strong effect with the current distributions -
     among the four "normal" weapon skills (axes/maces/polearms/longblades),
     only the m&f distribution is particularly distinctive. But it is
     definitely a noticeable effect if you play 5 non-maces games in a row and
     follow up with 5 maces games, and it contributes to making maces feel more
     distinct.

     They could probably be simplified to a certain extent (only one set of
     brands per weapon skill, for example), but there is a reason not to
     simplify them down to just one table.
 */

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

/// brand weights for club-type weapons
static const vector<brand_weight_tuple> CLUB_BRANDS = {
    { SPWPN_NORMAL,          9 },
    { SPWPN_SPECTRAL,        1 },
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
    { SPWPN_NORMAL,   58 },
    { SPWPN_FLAMING,  16 },
    { SPWPN_FREEZING, 16 },
    { SPWPN_VORPAL,   10 },
};

/// brand weights for holy (TSO-blessed) weapons.
static const vector<brand_weight_tuple> HOLY_BRANDS = {
    { SPWPN_HOLY_WRATH, 100 },
};


static int Weapon_index[NUM_WEAPONS];
static const weapon_def Weapon_prop[] =
{
    // Maces & Flails
    { WPN_CLUB,              "club",                5,  3, 13,
        SK_MACES_FLAILS, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING, 10, 0, 10, CLUB_BRANDS },
#if TAG_MAJOR_VERSION == 34
    { WPN_SPIKED_FLAIL,      "spiked flail",        5,  3, 13,
        SK_MACES_FLAILS, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING, 0, 0, 0, {} },
#endif
    { WPN_WHIP,              "whip",                6,  2, 11,
        SK_MACES_FLAILS, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLASHING, 4, 0, 25, {
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
#if TAG_MAJOR_VERSION == 34
    { WPN_HAMMER,            "hammer",              7,  3, 13,
        SK_MACES_FLAILS, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING, 0, 0, 0, M_AND_F_BRANDS },
#endif
    { WPN_MACE,              "mace",                8,  3, 14,
        SK_MACES_FLAILS, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING, 9, 10, 30, M_AND_F_BRANDS },
    { WPN_FLAIL,             "flail",              10,  0, 14,
        SK_MACES_FLAILS, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING, 8, 10, 35, M_AND_F_BRANDS },
    { WPN_MORNINGSTAR,       "morningstar",        13, -2, 15,
        SK_MACES_FLAILS, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING | DAM_PIERCE, 7, 10, 40, {
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
    { WPN_DEMON_WHIP,        "demon whip",         11,  1, 11,
        SK_MACES_FLAILS, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLASHING, 0, 2, 150, DEMON_BRANDS },
    { WPN_SACRED_SCOURGE,    "sacred scourge",     12,  0, 11,
        SK_MACES_FLAILS, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLASHING, 0, 0, 200, HOLY_BRANDS },
    { WPN_DIRE_FLAIL,        "dire flail",         13, -3, 13,
        SK_MACES_FLAILS, SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CRUSHING | DAM_PIERCE, 2, 10, 40, M_AND_F_BRANDS },
    { WPN_EVENINGSTAR,       "eveningstar",        15, -1, 15,
        SK_MACES_FLAILS, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_CRUSHING | DAM_PIERCE, 0, 2, 150, {
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
    { WPN_GREAT_MACE,        "great mace",         17, -4, 17,
        SK_MACES_FLAILS, SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CRUSHING, 3, 10, 65, M_AND_F_BRANDS },
    { WPN_GIANT_CLUB,        "giant club",         20, -6, 16,
        SK_MACES_FLAILS, SIZE_LARGE, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CRUSHING, 1, 10, 17, CLUB_BRANDS },
    { WPN_GIANT_SPIKED_CLUB, "giant spiked club",  22, -7, 18,
        SK_MACES_FLAILS, SIZE_LARGE, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CRUSHING | DAM_PIERCE, 1, 10, 19, CLUB_BRANDS },

    // Short Blades
    { WPN_DAGGER,            "dagger",              4,  6, 10,
        SK_SHORT_BLADES, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_PIERCING, 10, 10, 20, {
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
    { WPN_QUICK_BLADE,       "quick blade",         5,  6,  7,
        SK_SHORT_BLADES, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_PIERCING, 0, 2, 150, {} },
    { WPN_SHORT_SWORD,       "short sword",         6,  4, 11,
        SK_SHORT_BLADES, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_PIERCING, 8, 10, 30, SBL_BRANDS },
    { WPN_RAPIER,           "rapier",               8,  4, 12,
        SK_SHORT_BLADES, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_PIERCING, 8, 10, 40, SBL_BRANDS },
#if TAG_MAJOR_VERSION == 34
    { WPN_CUTLASS,          "cutlass",              8,  4, 12,
        SK_SHORT_BLADES, SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLICING | DAM_PIERCE, 0, 0, 0, {}},
#endif


    // Long Blades
    { WPN_FALCHION,              "falchion",               8,  2, 13,
        SK_LONG_BLADES,  SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLICING, 7, 10, 30, LBL_BRANDS }, // DAMV_CHOPPING...?
    { WPN_LONG_SWORD,            "long sword",            10,  1, 14,
        SK_LONG_BLADES,  SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLICING, 7, 10, 35, LBL_BRANDS },
    { WPN_SCIMITAR,              "scimitar",              12, 0, 14,
        SK_LONG_BLADES,  SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLICING, 6, 10, 40, LBL_BRANDS },
    { WPN_DEMON_BLADE,           "demon blade",           13, -1, 13,
        SK_LONG_BLADES,  SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLICING, 0, 2, 150, DEMON_BRANDS },
    { WPN_EUDEMON_BLADE,         "eudemon blade",         14, -2, 12,
        SK_LONG_BLADES,  SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLICING, 0, 0, 200, HOLY_BRANDS },
    { WPN_DOUBLE_SWORD,          "double sword",          15, -1, 15,
        SK_LONG_BLADES,  SIZE_LITTLE, SIZE_MEDIUM, MI_NONE,
        DAMV_SLICING, 0, 2, 150, LBL_BRANDS },
    { WPN_GREAT_SWORD,           "great sword",           17, -3, 17,
        SK_LONG_BLADES,  SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_SLICING, 6, 10, 65, LBL_BRANDS },
    { WPN_TRIPLE_SWORD,          "triple sword",          19, -4, 18,
        SK_LONG_BLADES,  SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_SLICING, 0, 2, 100, LBL_BRANDS },
#if TAG_MAJOR_VERSION == 34
    { WPN_BLESSED_FALCHION,      "old falchion",         8,  2, 13,
        SK_LONG_BLADES,  SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLICING, 0, 0, 0, {} },
    { WPN_BLESSED_LONG_SWORD,    "old long sword",      10,  1, 14,
        SK_LONG_BLADES,  SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLICING, 0, 0, 0, {} },
    { WPN_BLESSED_SCIMITAR,      "old scimitar",        12, -2, 14,
        SK_LONG_BLADES,  SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_SLICING, 0, 0, 0, {} },
    { WPN_BLESSED_DOUBLE_SWORD, "old double sword",     15, -1, 15,
        SK_LONG_BLADES,  SIZE_LITTLE, SIZE_MEDIUM, MI_NONE,
        DAMV_SLICING, 0, 0, 0, {} },
    { WPN_BLESSED_GREAT_SWORD,   "old great sword",     17, -3, 16,
        SK_LONG_BLADES,  SIZE_MEDIUM, NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_SLICING, 0, 0, 0, {} },
    { WPN_BLESSED_TRIPLE_SWORD,      "old triple sword", 19, -4, 18,
        SK_LONG_BLADES,  SIZE_MEDIUM, NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_SLICING, 0, 0, 0, {} },
#endif

    // Axes
    { WPN_HAND_AXE,          "hand axe",            7,  3, 13,
        SK_AXES,       SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_CHOPPING, 9, 10, 30, AXE_BRANDS },
    { WPN_WAR_AXE,           "war axe",            11,  0, 15,
        SK_AXES,       SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_CHOPPING, 7, 10, 35, AXE_BRANDS },
    { WPN_BROAD_AXE,         "broad axe",          13, -2, 16,
        SK_AXES,       SIZE_LITTLE, SIZE_MEDIUM, MI_NONE,
        DAMV_CHOPPING, 4, 10, 40, AXE_BRANDS },
    { WPN_BATTLEAXE,         "battleaxe",          15, -4, 17,
        SK_AXES,       SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CHOPPING, 6, 10, 65, AXE_BRANDS },
    { WPN_EXECUTIONERS_AXE,  "executioner's axe",  18, -6, 19,
        SK_AXES,       SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CHOPPING, 0, 2, 100, AXE_BRANDS },

    // Polearms
    { WPN_SPEAR,             "spear",               6,  4, 11,
        SK_POLEARMS,     SIZE_LITTLE, SIZE_LITTLE, MI_NONE,
        DAMV_PIERCING, 8, 10, 30, {
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
    { WPN_TRIDENT,           "trident",             9,  1, 13,
        SK_POLEARMS,     SIZE_LITTLE, SIZE_MEDIUM, MI_NONE,
        DAMV_PIERCING, 6, 10, 35, POLEARM_BRANDS },
    { WPN_HALBERD,           "halberd",            13, -3, 15,
        SK_POLEARMS,     SIZE_MEDIUM, NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_CHOPPING | DAM_PIERCE, 5, 10, 40, POLEARM_BRANDS },
    { WPN_SCYTHE,            "scythe",             14, -4, 20,
        SK_POLEARMS,     SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_SLICING, 2, 0, 30, POLEARM_BRANDS },
    { WPN_DEMON_TRIDENT,     "demon trident",      12,  1, 13,
        SK_POLEARMS,     SIZE_LITTLE, SIZE_MEDIUM, MI_NONE,
        DAMV_PIERCING, 0, 2, 150, DEMON_BRANDS },
    { WPN_TRISHULA,          "trishula",           13,  0, 13,
        SK_POLEARMS,     SIZE_LITTLE, SIZE_MEDIUM, MI_NONE,
        DAMV_PIERCING, 0, 0, 200, HOLY_BRANDS },
    { WPN_GLAIVE,            "glaive",             15, -3, 17,
        SK_POLEARMS,     SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CHOPPING, 5, 10, 65, POLEARM_BRANDS },
    { WPN_BARDICHE,          "bardiche",           18, -6, 19,
        SK_POLEARMS,     SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_CHOPPING, 1, 2, 90, POLEARM_BRANDS },

    // Staves
    // WPN_STAFF is for weapon stats for magical staves only.
    { WPN_STAFF,             "staff",               5,  5, 12,
        SK_STAVES,       SIZE_LITTLE, SIZE_MEDIUM, MI_NONE,
        DAMV_CRUSHING, 0, 0, 15, {} },
    { WPN_QUARTERSTAFF,      "quarterstaff",        10, 3, 13,
        SK_STAVES,       SIZE_LITTLE, NUM_SIZE_LEVELS,  MI_NONE,
        DAMV_CRUSHING, 8, 10, 40, {
            { SPWPN_NORMAL,     50 },
            { SPWPN_SPECTRAL,   18 },
            { SPWPN_DRAINING,    8 },
            { SPWPN_VORPAL,      8 },
            { SPWPN_SPEED,       8 },
            { SPWPN_DISTORTION,  2 },
            { SPWPN_PAIN,        2 },
            { SPWPN_HOLY_WRATH,  2 },
            { SPWPN_ANTIMAGIC,   2 },
        }},
    { WPN_LAJATANG,          "lajatang",            16,-3, 14,
        SK_STAVES,       SIZE_LITTLE, NUM_SIZE_LEVELS, MI_NONE,
        DAMV_SLICING, 2, 2, 150, {
            { SPWPN_NORMAL,         34 },
            { SPWPN_SPEED,          12 },
            { SPWPN_ELECTROCUTION,  12 },
            { SPWPN_VAMPIRISM,      12 },
            { SPWPN_SPECTRAL,        9 },
            { SPWPN_VENOM,           7 },
            { SPWPN_PAIN,            7 },
            { SPWPN_ANTIMAGIC,       4 },
            { SPWPN_DISTORTION,      3 },
        }},

    // Range weapons
#if TAG_MAJOR_VERSION == 34
    { WPN_BLOWGUN,           "blowgun",             0,  2, 10,
        SK_THROWING,     SIZE_LITTLE, SIZE_LITTLE, MI_NEEDLE,
        DAMV_NON_MELEE, 0, 0, 0, {}, },
#endif

    { WPN_HUNTING_SLING,     "hunting sling",       7,  0, 15,
        SK_SLINGS,       SIZE_LITTLE, SIZE_LITTLE, MI_SLING_BULLET,
        DAMV_NON_MELEE, 8, 10, 15, RANGED_BRANDS },
    { WPN_FUSTIBALUS,        "fustibalus",         10, -2, 16,
        SK_SLINGS,       SIZE_LITTLE, SIZE_SMALL, MI_SLING_BULLET,
        DAMV_NON_MELEE, 2, 99, 150, RANGED_BRANDS },

    { WPN_HAND_CROSSBOW,     "hand crossbow",       8,  3, 15,
        SK_CROSSBOWS,    SIZE_LITTLE, SIZE_LITTLE, MI_BOLT,
        DAMV_NON_MELEE, 7, 10, 35, RANGED_BRANDS },
    { WPN_ARBALEST,          "arbalest",           15,  0, 18,
        SK_CROSSBOWS,    SIZE_LITTLE, NUM_SIZE_LEVELS, MI_BOLT,
        DAMV_NON_MELEE, 5, 10, 45, RANGED_BRANDS },
    { WPN_TRIPLE_CROSSBOW,   "triple crossbow",    21, -2, 23,
        SK_CROSSBOWS,    SIZE_SMALL, NUM_SIZE_LEVELS, MI_BOLT,
        DAMV_NON_MELEE, 0, 2, 100, RANGED_BRANDS },

    { WPN_SHORTBOW,          "shortbow",            9,  2, 15,
        SK_BOWS,         SIZE_LITTLE, NUM_SIZE_LEVELS, MI_ARROW,
        DAMV_NON_MELEE, 8, 10, 30, RANGED_BRANDS },
    { WPN_LONGBOW,           "longbow",            13,  0, 18,
        SK_BOWS,         SIZE_MEDIUM, NUM_SIZE_LEVELS, MI_ARROW,
        DAMV_NON_MELEE, 2, 10, 45, RANGED_BRANDS },
};

struct missile_def
{
    int         id;
    const char *name;
    int         dam;
    int         mulch_rate;
    int         price;
};

static int Missile_index[NUM_MISSILES];
static const missile_def Missile_prop[] =
{
    { MI_DART,          "dart",          0, 12, 2  },
#if TAG_MAJOR_VERSION == 34
    { MI_NEEDLE,        "needle",        0, 12, 2  },
#endif
    { MI_STONE,         "stone",         2, 8,  1  },
    { MI_ARROW,         "arrow",         0, 1,  2  },
    { MI_BOLT,          "bolt",          0, 1,  2  },
    { MI_LARGE_ROCK,    "large rock",   20, 25, 7  },
    { MI_SLING_BULLET,  "sling bullet",  0, 1,  5  },
    { MI_JAVELIN,       "javelin",      10, 20, 8  },
    { MI_THROWING_NET,  "throwing net",  0, 0,  30 },
    { MI_BOOMERANG,     "boomerang",     6, 20, 5  },
};

#if TAG_MAJOR_VERSION == 34
struct food_def
{
    int         id;
    const char *name;
    int         normal_nutr;
    int         carn_nutr;
    int         herb_nutr;
};

static int Food_index[NUM_FOODS];
static const food_def Food_prop[] =
{
    { FOOD_RATION,       "buggy ration", 3400,  1900,  1900 },
    { FOOD_CHUNK,        "buggy chunk",        1000,  1300,     0 },
    { FOOD_UNUSED,       "buggy pizza",     0,     0,     0 },
    { FOOD_ROYAL_JELLY,  "buggy jelly",  2000,  2000,  2000 },
    { FOOD_BREAD_RATION, "buggy ration", 4400,     0,  5900 },
    { FOOD_FRUIT,        "buggy fruit",   850,     0,  1000 },
    { FOOD_AMBROSIA,     "buggy fruit",     0,     0,     0 },
    { FOOD_ORANGE,       "buggy fruit",  1000,  -300,   300 },
    { FOOD_BANANA,       "buggy fruit",  1000,  -300,   300 },
    { FOOD_LEMON,        "buggy fruit",  1000,  -300,   300 },
    { FOOD_PEAR,         "buggy fruit",   700,  -200,   200 },
    { FOOD_APPLE,        "buggy fruit",   700,  -200,   200 },
    { FOOD_APRICOT,      "buggy fruit",   700,  -200,   200 },
    { FOOD_CHOKO,        "buggy fruit",   600,  -200,   200 },
    { FOOD_RAMBUTAN,     "buggy fruit",   600,  -200,   200 },
    { FOOD_LYCHEE,       "buggy fruit",   600,  -200,   200 },
    { FOOD_STRAWBERRY,   "buggy fruit",   200,   -50,    50 },
    { FOOD_GRAPE,        "buggy fruit",   100,   -20,    20 },
    { FOOD_SULTANA,      "buggy fruit",    70,   -20,    20 },
    { FOOD_CHEESE,       "buggy fruit",  1200,     0,     0 },
    { FOOD_SAUSAGE,      "buggy fruit",  1200,   150,  -400 },
    { FOOD_BEEF_JERKY,   "buggy fruit",  1500,   200,  -200 },
    { FOOD_PIZZA,        "buggy fruit",  1500,     0,     0 },
};
#endif

// Must call this functions early on so that the above tables can
// be accessed correctly.
void init_properties()
{
    // The compiler would complain about too many initializers but not
    // about too few, so we check this ourselves.
    COMPILE_CHECK(NUM_ARMOURS  == ARRAYSZ(Armour_prop));
    COMPILE_CHECK(NUM_WEAPONS  == ARRAYSZ(Weapon_prop));
    COMPILE_CHECK(NUM_MISSILES == ARRAYSZ(Missile_prop));
#if TAG_MAJOR_VERSION == 34
    COMPILE_CHECK(NUM_FOODS    == ARRAYSZ(Food_prop));
#endif

    for (int i = 0; i < NUM_ARMOURS; i++)
        Armour_index[ Armour_prop[i].id ] = i;

    for (int i = 0; i < NUM_WEAPONS; i++)
        Weapon_index[ Weapon_prop[i].id ] = i;

    for (int i = 0; i < NUM_MISSILES; i++)
        Missile_index[ Missile_prop[i].id ] = i;

#if TAG_MAJOR_VERSION == 34
    for (int i = 0; i < NUM_FOODS; i++)
        Food_index[ Food_prop[i].id ] = i;
#endif
}

const set<pair<object_class_type, int> > removed_items =
{
#if TAG_MAJOR_VERSION == 34
    { OBJ_JEWELLERY, AMU_CONTROLLED_FLIGHT },
    { OBJ_JEWELLERY, AMU_CONSERVATION },
    { OBJ_JEWELLERY, AMU_THE_GOURMAND },
    { OBJ_JEWELLERY, AMU_HARM },
    { OBJ_JEWELLERY, AMU_RAGE },
    { OBJ_JEWELLERY, AMU_INACCURACY },
    { OBJ_JEWELLERY, RING_REGENERATION },
    { OBJ_JEWELLERY, RING_SUSTAIN_ATTRIBUTES },
    { OBJ_JEWELLERY, RING_TELEPORT_CONTROL },
    { OBJ_JEWELLERY, RING_TELEPORTATION },
    { OBJ_JEWELLERY, RING_ATTENTION },
    { OBJ_JEWELLERY, RING_STEALTH },
    { OBJ_STAVES,    STAFF_ENCHANTMENT },
    { OBJ_STAVES,    STAFF_CHANNELING },
    { OBJ_STAVES,    STAFF_POWER },
    { OBJ_STAVES,    STAFF_ENERGY },
    { OBJ_STAVES,    STAFF_SUMMONING },
    { OBJ_STAVES,    STAFF_WIZARDRY },
    { OBJ_POTIONS,   POT_GAIN_STRENGTH },
    { OBJ_POTIONS,   POT_GAIN_DEXTERITY },
    { OBJ_POTIONS,   POT_GAIN_INTELLIGENCE },
    { OBJ_POTIONS,   POT_WATER },
    { OBJ_POTIONS,   POT_STRONG_POISON },
    { OBJ_POTIONS,   POT_BLOOD_COAGULATED },
    { OBJ_POTIONS,   POT_BLOOD },
    { OBJ_POTIONS,   POT_PORRIDGE },
    { OBJ_POTIONS,   POT_SLOWING },
    { OBJ_POTIONS,   POT_DECAY },
    { OBJ_POTIONS,   POT_POISON },
    { OBJ_POTIONS,   POT_RESTORE_ABILITIES },
    { OBJ_POTIONS,   POT_CURE_MUTATION },
    { OBJ_POTIONS,   POT_BENEFICIAL_MUTATION },
    { OBJ_POTIONS,   POT_DUMMY_AGILITY },
    { OBJ_BOOKS,     BOOK_WIZARDRY },
    { OBJ_BOOKS,     BOOK_CONTROL },
    { OBJ_BOOKS,     BOOK_BUGGY_DESTRUCTION },
    { OBJ_BOOKS,     BOOK_ENVENOMATIONS },
    { OBJ_BOOKS,     BOOK_AKASHIC_RECORD },
    { OBJ_BOOKS,     BOOK_BATTLE },
    { OBJ_BOOKS,     BOOK_STONE },
    { OBJ_BOOKS,     BOOK_PAIN },
    { OBJ_RODS,      ROD_VENOM },
    { OBJ_RODS,      ROD_WARDING },
    { OBJ_RODS,      ROD_DESTRUCTION },
    { OBJ_RODS,      ROD_SWARM },
    { OBJ_RODS,      ROD_LIGHTNING },
    { OBJ_RODS,      ROD_IGNITION },
    { OBJ_RODS,      ROD_CLOUDS },
    { OBJ_RODS,      ROD_INACCURACY },
    { OBJ_RODS,      ROD_SHADOWS },
    { OBJ_RODS,      ROD_IRON },
    { OBJ_SCROLLS,   SCR_ENCHANT_WEAPON_II },
    { OBJ_SCROLLS,   SCR_ENCHANT_WEAPON_III },
    { OBJ_SCROLLS,   SCR_RECHARGING },
    { OBJ_SCROLLS,   SCR_CURSE_WEAPON },
    { OBJ_SCROLLS,   SCR_CURSE_ARMOUR },
    { OBJ_SCROLLS,   SCR_CURSE_JEWELLERY },
    { OBJ_SCROLLS,   SCR_REMOVE_CURSE },
    { OBJ_SCROLLS,   SCR_RANDOM_USELESSNESS },
    { OBJ_SCROLLS,   SCR_HOLY_WORD },
    { OBJ_WANDS,     WAND_MAGIC_DARTS_REMOVED },
    { OBJ_WANDS,     WAND_FROST_REMOVED },
    { OBJ_WANDS,     WAND_FIRE_REMOVED },
    { OBJ_WANDS,     WAND_COLD_REMOVED },
    { OBJ_WANDS,     WAND_INVISIBILITY_REMOVED },
    { OBJ_WANDS,     WAND_HEAL_WOUNDS_REMOVED },
    { OBJ_WANDS,     WAND_HASTING_REMOVED },
    { OBJ_WANDS,     WAND_TELEPORTATION_REMOVED },
    { OBJ_WANDS,     WAND_SLOWING_REMOVED },
    { OBJ_WANDS,     WAND_CONFUSION_REMOVED },
    { OBJ_WANDS,     WAND_LIGHTNING_REMOVED },
    { OBJ_WANDS,     WAND_SCATTERSHOT_REMOVED },
    { OBJ_WANDS,     WAND_CLOUDS_REMOVED },
    { OBJ_WANDS,     WAND_RANDOM_EFFECTS_REMOVED },
    { OBJ_FOOD,      FOOD_CHUNK},
    { OBJ_FOOD,      FOOD_BREAD_RATION },
    { OBJ_FOOD,      FOOD_ROYAL_JELLY },
    { OBJ_FOOD,      FOOD_UNUSED },
    { OBJ_FOOD,      FOOD_FRUIT },
    { OBJ_FOOD,      FOOD_RATION },
    { OBJ_MISSILES,  MI_ARROW },
    { OBJ_MISSILES,  MI_BOLT },
    { OBJ_MISSILES,  MI_SLING_BULLET },
#endif
    { OBJ_JEWELLERY, AMU_NOTHING }, // These should only spawn as uniques
};

bool item_type_removed(object_class_type base, int subtype)
{
    return removed_items.count({ base, subtype }) != 0;
}

// If item is a new unrand, takes a note of it and returns true.
// Otherwise, takes no action and returns false.
static bool _maybe_note_found_unrand(const item_def &item)
{
    if (is_unrandom_artefact(item) && !(item.flags & ISFLAG_SEEN))
    {
        take_note(Note(NOTE_FOUND_UNRAND, 0, 0, item.name(DESC_THE),
                       origin_desc(item)));
        return true;
    }
    return false;
}

/**
 * Is the provided item cursable?
 *
 * @param item  The item under consideration.
 * @return      Whether the given item is cursable.
 */
bool item_is_cursable(const item_def &item)
{
    if (!item_type_has_curses(item.base_type))
        return false;
    if (item.cursed())
        return false;
    return true;
}

void auto_id_inventory()
{
    for (int slot = 0; slot < ENDOFPACK; ++slot)
    {
        item_def &item = you.inv[slot];
        if (item.defined() && !fully_identified(item))
        {
            god_id_item(item, false);
            item_def * moved = auto_assign_item_slot(item);
            // We moved the item to later in the pack, so don't
            // miss what we swapped with.
            if (moved != nullptr && moved->link > slot)
                --slot;
        }
    }
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

    // On the ground or a monster has it. Violence is the answer.
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
    if ((item.flags & flags) != flags)
    {
        item.flags |= flags;
        request_autoinscribe();

        if (in_inventory(item))
        {
            shopping_list.cull_identical_items(item);
            item_skills(item, you.skills_to_show);
        }
    }

    if (fully_identified(item))
    {
        if (notes_are_active() && !(item.flags & ISFLAG_NOTED_ID)
            && !get_ident_type(item)
            && is_interesting_item(item))
        {
            if (!_maybe_note_found_unrand(item))
            {
                // Make a note of this non-unrand item
                take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_A),
                               origin_desc(item)));
            }

            // Sometimes (e.g. shops) you can ID an item before you get it;
            // don't note twice in those cases.
            item.flags |= (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
        }
    }

    if (item.flags & ISFLAG_KNOW_TYPE && !is_artefact(item)
        && _is_affordable(item))
    {
        if (item.base_type == OBJ_MISCELLANY)
            you.seen_misc.set(item.sub_type);
    }
}

void unset_ident_flags(item_def &item, iflags_t flags)
{
    item.flags &= (~flags);
}

// Returns the mask of interesting identify bits for this item
// (e.g., missiles don't have know-type).
static iflags_t _full_ident_mask(const item_def& item)
{
    // KNOW_PROPERTIES is only relevant for artefacts, handled later.
    iflags_t flagset = ISFLAG_IDENT_MASK & ~ISFLAG_KNOW_PROPERTIES;
    switch (item.base_type)
    {
    case OBJ_CORPSES:
    case OBJ_MISSILES:
    case OBJ_ORBS:
    case OBJ_RUNES:
    case OBJ_GOLD:
    case OBJ_BOOKS:
#if TAG_MAJOR_VERSION == 34
    case OBJ_FOOD:
    case OBJ_RODS:
#endif
        flagset = 0;
        break;
    case OBJ_SCROLLS:
    case OBJ_POTIONS:
    case OBJ_WANDS:
    case OBJ_STAVES:
    case OBJ_JEWELLERY:
        flagset = ISFLAG_KNOW_TYPE;
        break;
    case OBJ_MISCELLANY:
        flagset = 0;
        break;
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
        item.brand = ego_type;
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
    { MONS_TROLL,               ARM_TROLL_LEATHER_ARMOUR },
    { MONS_DEEP_TROLL,          ARM_TROLL_LEATHER_ARMOUR },
    { MONS_IRON_TROLL,          ARM_TROLL_LEATHER_ARMOUR },

    { MONS_FIRE_DRAGON,         ARM_FIRE_DRAGON_ARMOUR },
    { MONS_ICE_DRAGON,          ARM_ICE_DRAGON_ARMOUR },
    { MONS_STEAM_DRAGON,        ARM_STEAM_DRAGON_ARMOUR },
    { MONS_ACID_DRAGON,         ARM_ACID_DRAGON_ARMOUR },
    { MONS_STORM_DRAGON,        ARM_STORM_DRAGON_ARMOUR },
    { MONS_GOLDEN_DRAGON,       ARM_GOLD_DRAGON_ARMOUR },
    { MONS_SWAMP_DRAGON,        ARM_SWAMP_DRAGON_ARMOUR },
    { MONS_PEARL_DRAGON,        ARM_PEARL_DRAGON_ARMOUR },
    { MONS_SHADOW_DRAGON,       ARM_SHADOW_DRAGON_ARMOUR },
    { MONS_QUICKSILVER_DRAGON,  ARM_QUICKSILVER_DRAGON_ARMOUR },
};

/**
 * If a monster of the given type dies, what kind of armour can it leave?
 *
 * @param mc    The class of monster in question.
 * @return      The armour_type of the given monster's armour, or NUM_ARMOURS
 *              if the monster does not leave armour.
 */
armour_type hide_for_monster(monster_type mc)
{
    return lookup(_monster_hides, mons_species(mc), NUM_ARMOURS);
}

/**
 * Return whether a piece of armour is enchantable.
 *
 * This function ignores the current enchantment level, so is still
 * true for maximally-enchanted items.
 *
 * @param item      The item being considered.
 * @return          True if the armour can have a +X enchantment.
 */
bool armour_is_enchantable(const item_def &item)
{
    ASSERT(item.base_type == OBJ_ARMOUR);
    return item.sub_type != ARM_QUICKSILVER_DRAGON_ARMOUR
        && item.sub_type != ARM_SCARF
        && item.sub_type != ARM_ORB;
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

    // Unenchantables.
    if (!armour_is_enchantable(item))
        return 0;

    const int eq_slot = get_armour_slot(item);

    int max_plus = MAX_SEC_ENCHANT;
    if (eq_slot == EQ_BODY_ARMOUR || item.sub_type == ARM_BARDING)
        max_plus = property(item, PARM_AC);
    else if (eq_slot == EQ_SHIELD)
        // 3 / 5 / 8 for bucklers/shields/lg. shields
        max_plus = (property(item, PARM_AC) - 3)/2 + 3;

    return max_plus;
}

/**
 * Find the set of all armours made from monsters.
 */
static set<armour_type> _make_hide_armour_set()
{
    set<armour_type> _hide_armour_set;
    // iter over armours created from monsters
    for (auto it: _monster_hides)
        _hide_armour_set.insert(it.second);
    return _hide_armour_set;
}
static set<armour_type> _hide_armour_set = _make_hide_armour_set();

/**
 * Is the given armour a type that drops from dead monsters (i.e. dragon or
 * troll armour)?
 *
 * @param item      The armour in question.
 * @return          Whether the given item comes from a monster corpse.
 *                  (Note that ARM_ANIMAL_SKIN doesn't come from in-game
*                    monsters (these days) and so doesn't count.)
 */
bool armour_is_hide(const item_def &item)
{
    return item.base_type == OBJ_ARMOUR
           && armour_type_is_hide(static_cast<armour_type>(item.sub_type));
}

/**
 * Is the given armour a type that drops from dead monsters (i.e. dragon or
 * troll armour)?
 *
 * @param type      The armour_type in question.
 * @return          Whether the given item comes from a monster corpse.
 *                  (Note that ARM_ANIMAL_SKIN doesn't come from in-game
 *                   monsters (these days) and so doesn't count.)
 */
bool armour_type_is_hide(armour_type type)
{
    return _hide_armour_set.count(type);
}

/**
 * Generate a map from monster armours to the monsters that drop them.
 */
static map<armour_type, monster_type> _make_hide_monster_map()
{
    map<armour_type, monster_type> hide_to_mons;
    for (auto monster_data : _monster_hides)
    {
        const armour_type hide = monster_data.second;
        const monster_type mon = monster_data.first;
        hide_to_mons[hide] = mon;
    }

    // troll hides are generated by multiple monsters, so set a canonical troll
    // by hand
    hide_to_mons[ARM_TROLL_LEATHER_ARMOUR] = MONS_TROLL;

    return hide_to_mons;
}

static map<armour_type, monster_type> hide_to_mons = _make_hide_monster_map();

/**
 * Find the monster that the given type of dragon/troll armour came from.
 *
 * @param arm   The type of armour in question.
 * @return      The corresponding monster type; e.g. MONS_FIRE_DRAGON for
 *              ARM_FIRE_DRAGON_ARMOUR,
 *              MONS_TROLL for ARM_TROLL_LEATHER_ARMOUR...
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

/**
 * What weight does this armour type have for acquirement? (Higher = appears
 * more often.)
 *
 * @param arm   The armour item in question.
 * @return      The base weight the armour should have in acquirement, before
 *              skills & other effects are factored in.
 */
int armour_acq_weight(const armour_type armour)
{
    return Armour_prop[ Armour_index[armour] ].acquire_weight;
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

/**
 * How many size categories away is the given type of armour from fitting a
 * creature of the given size - and in what direction?
 *
 * @param sub_type  The type of armour in question.
 * @param size      The size of the creature in question.
 * @return          The difference between the size of the creature and the
 *                  closest size of creature that might wear the armour.
 *                  Negative if the creature is too small.
 *                  Positive if the creature is too large.
 *                  0 if the creature is just right.
 */
static int _fit_armour_size(armour_type sub_type, size_type size)
{
    const size_type min = Armour_prop[ Armour_index[sub_type] ].fit_min;
    const size_type max = Armour_prop[ Armour_index[sub_type] ].fit_max;

    if (size < min)
        return min - size;    // negative means levels too small
    else if (size > max)
        return max - size;    // positive means levels too large

    return 0;
}

/**
 * How many size categories away is the given armour from fitting a creature of
 * the given size - and in what direction?
 *
 * @param item      The armour in question.
 * @param size      The size of the creature in question.
 * @return          The difference between the size of the creature and the
 *                  closest size of creature that might wear the armour.
 *                  Negative if the creature is too small.
 *                  Positive if the creature is too large.
 *                  0 if the creature is just right.
 */
int fit_armour_size(const item_def &item, size_type size)
{
    ASSERT(item.base_type == OBJ_ARMOUR);
    return _fit_armour_size(static_cast<armour_type>(item.sub_type), size);
}

/**
 * Does the given type of armour fit a creature of the given size?
 *
 * @param sub_type  The type of armour in question.
 * @param size      The size of the creature that might wear the armour.
 * @return          Whether the armour fits based on size. (It might still not
 *                  fit for other reasons, e.g. mutations...)
 */
bool check_armour_size(armour_type sub_type, size_type size)
{
    return _fit_armour_size(sub_type, size) == 0;
}

/**
 * Does the given armour fit a creature of the given size?
 *
 * @param item      The armour in question.
 * @param size      The size of the creature that might wear the armour.
 * @return          Whether the armour fits based on size. (It might still not
 *                  fit for other reasons, e.g. mutations...)
 */
bool check_armour_size(const item_def &item, size_type size)
{
    ASSERT(item.base_type == OBJ_ARMOUR);

    return check_armour_size(static_cast<armour_type>(item.sub_type), size);
}

int wand_charge_value(int type, int item_level)
{
    switch (type)
    {
    case WAND_DIGGING:
        return 9;

    // Decrease charge generation later on so that players get wands to play
    // with early, but aren't totally flooded with charges by late game.
    case WAND_ICEBLAST:
    case WAND_ACID:
    case WAND_CHARMING:
    case WAND_PARALYSIS:
    case WAND_POLYMORPH:
        return item_level > 7 ? 8 : 15;

    default:
        return item_level > 7 ? 12 : 24;

    case WAND_FLAME:
        return item_level > 7 ? 16 : 32;
    }
}


#if TAG_MAJOR_VERSION == 34
/**
 * Is the given item a wand which is empty? Wands are normally destroyed when
 * their charges are exhausted, but empty wands can still happen through
 * transfered games.
 *
 * @param item  The item in question.
 * @return      Whether the wand is empty.
 */
bool is_known_empty_wand(const item_def &item)
{
    if (item.base_type != OBJ_WANDS)
        return false;

    return item_ident(item, ISFLAG_KNOW_TYPE) && item.charges <= 0;
}
#endif

/**
 * What wands could a monster use to directly harm the player?
 *
 * @param item      The wand to be examined.
 * @return          True if the wand could harm the player, false otherwise.
 */
bool is_offensive_wand(const item_def& item)
{
    switch (item.sub_type)
    {
    // Monsters use it, but it's not an offensive wand
    case WAND_DIGGING:
        return false;

    case WAND_ACID:
    case WAND_MINDBURST:
    case WAND_CHARMING:
    case WAND_FLAME:
    case WAND_ICEBLAST:
    case WAND_PARALYSIS:
    case WAND_POLYMORPH:
        return true;
    }
    return false;
}

// Returns whether a piece of armour can be enchanted further.
// If unknown is true, unidentified armour will return true.
bool is_enchantable_armour(const item_def &arm, bool unknown)
{
    if (arm.base_type != OBJ_ARMOUR)
        return false;

    // Armour types that can never be enchanted.
    if (!armour_is_enchantable(arm))
        return false;

    // If we don't know the plusses, assume enchanting is possible.
    if (unknown && !is_artefact(arm) && !item_ident(arm, ISFLAG_KNOW_PLUSES))
        return true;

    // Artefacts or highly enchanted armour cannot be enchanted.
    if (is_artefact(arm) || arm.plus >= armour_max_enchant(arm))
        return false;

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
    if (item.base_type == OBJ_WEAPONS)
        return Weapon_prop[Weapon_index[item.sub_type]].dam_type & DAM_MASK;

    return DAM_BASH;
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
    const auto wpn_type = [&item]() {
        if (item.base_type == OBJ_WEAPONS)
            return static_cast<weapon_type>(item.sub_type);
        if (item.base_type == OBJ_STAVES)
            return WPN_STAFF;
        return WPN_UNKNOWN;
    }();

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
    // This function is used for item generation only, so use the actor's
    // (player's) base size, not its current form.
    return ac->hands_reqd(item, true);
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

static map<weapon_type, weapon_type> _holy_upgrades =
{
    { WPN_DEMON_BLADE, WPN_EUDEMON_BLADE },
    { WPN_DEMON_WHIP, WPN_SACRED_SCOURGE },
    { WPN_DEMON_TRIDENT, WPN_TRISHULA },
};

bool convert2good(item_def &item)
{
    if (item.base_type != OBJ_WEAPONS)
        return false;

    if (auto repl = map_find(_holy_upgrades, (weapon_type) item.sub_type))
    {
        item.sub_type = *repl;
        return true;
    }

    return false;
}

bool convert2bad(item_def &item)
{
    if (item.base_type != OBJ_WEAPONS)
        return false;

    auto it = find_if(begin(_holy_upgrades), end(_holy_upgrades),
                      [&item](const pair<const weapon_type, weapon_type> elt)
                      {
                          return elt.second == item.sub_type;
                      });
    if  (it != end(_holy_upgrades))
    {
        item.sub_type = it->first;
        return true;
    }

    return false;
}

bool is_brandable_weapon(const item_def &wpn, bool allow_ranged, bool divine)
{
    if (wpn.base_type != OBJ_WEAPONS)
        return false;

    if (is_artefact(wpn))
        return false;

    if (!allow_ranged && is_range_weapon(wpn)
#if TAG_MAJOR_VERSION == 34
        || wpn.sub_type == WPN_BLOWGUN
#endif
       )
    {
        return false;
    }

    // Only gods can rebrand blessed weapons, and they revert back to their
    // old base type in the process.
    if (is_blessed(wpn) && !divine)
        return false;

    return true;
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
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        return Weapon_prop[ Weapon_index[item.sub_type] ].skill;
    case OBJ_STAVES:
        return SK_STAVES;
    case OBJ_MISSILES:
        return SK_THROWING;
    default:
        return SK_FIGHTING;
    }
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

// True if item is a staff that deals extra damage based on Evocations skill,
// or has an evocations-based passive effect (staff of Wucad Mu).
bool staff_uses_evocations(const item_def &item)
{
    if (is_unrandom_artefact(item, UNRAND_ELEMENTAL_STAFF)
        || is_unrandom_artefact(item, UNRAND_OLGREB)
        || is_unrandom_artefact(item, UNRAND_ASMODEUS)
        || is_unrandom_artefact(item, UNRAND_DISPATER)
        || is_unrandom_artefact(item, UNRAND_WUCAD_MU))
    {
        return true;
    }

    return item.base_type == OBJ_STAVES;
}

skill_type staff_skill(stave_type s)
{
    switch (s)
    {
    case STAFF_AIR:
        return SK_AIR_MAGIC;
    case STAFF_COLD:
        return SK_ICE_MAGIC;
    case STAFF_EARTH:
        return SK_EARTH_MAGIC;
    case STAFF_FIRE:
        return SK_FIRE_MAGIC;
    case STAFF_POISON:
        return SK_POISON_MAGIC;
    case STAFF_DEATH:
        return SK_NECROMANCY;
    case STAFF_CONJURATION:
        return SK_CONJURATIONS;
    default:
        return SK_NONE;
    }
}

bool item_skills(const item_def &item, set<skill_type> &skills)
{
    if (item.is_type(OBJ_BOOKS, BOOK_MANUAL))
    {
        const skill_type skill = static_cast<skill_type>(item.plus);
        if (!skill_default_shown(skill))
            skills.insert(skill);
    }

    // Jewellery with evokable abilities, wands and similar unwielded
    // evokers allow training.
    // XX why are gives_ability cases broken up like this
    if (item_is_evokable(item)
        || item.base_type == OBJ_JEWELLERY && gives_ability(item)
        || staff_uses_evocations(item)
        || item.base_type == OBJ_WEAPONS && gives_ability(item))
    {
        skills.insert(SK_EVOCATIONS);
    }

    // Shields and abilities on armours allow training as long as your species
    // can wear them.
    if (item.base_type == OBJ_ARMOUR && can_wear_armour(item, false, true))
    {
        if (is_shield(item))
            skills.insert(SK_SHIELDS);

        if (gives_ability(item) || get_armour_ego_type(item) == SPARM_ENERGY)
            skills.insert(SK_EVOCATIONS);
    }

    // Weapons and staves allow training as long as your species can wield them.
    if (!you.could_wield(item, true, true))
        return !skills.empty();

    if (item.base_type == OBJ_STAVES)
    {
        const skill_type staff_sk
                    = staff_skill(static_cast<stave_type>(item.sub_type));
        if (staff_sk != SK_NONE)
            skills.insert(staff_sk);
    }

    if (item.base_type == OBJ_WEAPONS && get_weapon_brand(item) == SPWPN_PAIN)
        skills.insert(SK_NECROMANCY);

    const skill_type sk = item_attack_skill(item);
    if (sk != SK_FIGHTING)
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

    const int subtype = OBJ_STAVES == item.base_type ? int{WPN_STAFF}
                                                     : item.sub_type;
    return Weapon_prop[Weapon_index[subtype]].min_2h_size <= size;
}

//
// Launcher and ammo functions:
//
bool is_range_weapon(const item_def &item)
{
    return is_weapon(item) && is_ranged_weapon_type(item.sub_type);
}

bool is_slowed_by_armour(const item_def *item)
{
    return item && is_range_weapon(*item);
}

const char *ammo_name(missile_type ammo)
{
    return ammo < 0 || ammo >= NUM_MISSILES ? "eggplant"
           : Missile_prop[ Missile_index[ammo] ].name;
}

bool is_launcher_ammo(const item_def &wpn)
{
    if (wpn.base_type != OBJ_MISSILES)
        return false;

    switch (wpn.sub_type)
    {
    case MI_ARROW:
    case MI_BOLT:
    case MI_SLING_BULLET:
        return true;
    default:
        return false;
    }
}

// Returns true if item can be reasonably thrown without a launcher.
bool is_throwable(const actor *actor, const item_def &wpn)
{
    if (wpn.base_type != OBJ_MISSILES || is_launcher_ammo(wpn))
        return false;
    if (!actor)
        return true;

    if (wpn.sub_type == MI_LARGE_ROCK)
        return actor->can_throw_large_rocks();
    return wpn.sub_type != MI_JAVELIN || actor->body_size() >= SIZE_MEDIUM;
}

// Decide if something is launched or thrown.
launch_retval is_launched(const actor *actor, const item_def &missile)
{
    return is_throwable(actor, missile) ? launch_retval::THROWN : launch_retval::FUMBLED;
}

// Sorry about this.
void populate_fake_projectile(const item_def &wep, item_def &fake_proj)
{
    ASSERT(is_weapon(wep) && is_ranged_weapon_type(wep.sub_type));
    fake_proj.base_type = OBJ_MISSILES;
    fake_proj.sub_type  = Weapon_prop[Weapon_index[wep.sub_type]].ammo;
    fake_proj.quantity  = 1;
    fake_proj.rnd       = 1;
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
    if (is_unrandom_artefact(item, UNRAND_RIFT))
        return REACH_THREE;
    if (item_attack_skill(item) == SK_POLEARMS)
        return REACH_TWO;
    return REACH_NONE;
}

//
// Macguffins
//
bool item_is_unique_rune(const item_def &item)
{
    return item.base_type == OBJ_RUNES
           && item.sub_type != RUNE_DEMONIC
           && item.sub_type != RUNE_ABYSSAL;
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
           && item.sub_type != NUM_BOOKS
           && item.sub_type != BOOK_MANUAL;
}

//
// Ring functions:

// Returns whether jewellery has plusses.
bool jewellery_has_pluses(const item_def &item)
{
    ASSERT(item.base_type == OBJ_JEWELLERY);

    // not known -> no plusses
    if (!item_type_known(item))
        return false;

    return jewellery_type_has_plusses(item.sub_type);
}

bool jewellery_type_has_plusses(int jewel_type)
{
    switch (jewel_type)
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

    if (jewellery_has_pluses(item))
        return true;

    switch (item.sub_type)
    {
    case RING_PROTECTION_FROM_FIRE:
    case RING_PROTECTION_FROM_COLD:
    case RING_LIFE_PROTECTION:
    case RING_STEALTH:
    case RING_WIZARDRY:
    case RING_FIRE:
    case RING_ICE:
    case RING_WILLPOWER:
    case RING_MAGICAL_POWER:
        return true;

    default:
        break;
    }

    return false;
}

static map<potion_type, item_rarity_type> _potion_rarity = {
    { POT_CURING,       RARITY_VERY_COMMON },
    { POT_HEAL_WOUNDS,  RARITY_COMMON },
    { POT_FLIGHT,       RARITY_UNCOMMON },
    { POT_HASTE,        RARITY_UNCOMMON },
    { POT_LIGNIFY,      RARITY_UNCOMMON },
    { POT_ATTRACTION,   RARITY_UNCOMMON },
    { POT_DEGENERATION, RARITY_UNCOMMON },
    { POT_MIGHT,        RARITY_UNCOMMON },
    { POT_BRILLIANCE,   RARITY_UNCOMMON },
    { POT_MUTATION,     RARITY_UNCOMMON },
    { POT_INVISIBILITY, RARITY_RARE },
    { POT_RESISTANCE,   RARITY_RARE },
    { POT_MAGIC,        RARITY_RARE },
    { POT_BERSERK_RAGE, RARITY_RARE },
    { POT_CANCELLATION, RARITY_RARE },
    { POT_AMBROSIA,     RARITY_RARE },
    { POT_EXPERIENCE,   RARITY_VERY_RARE },
};

static map<scroll_type, item_rarity_type> _scroll_rarity = {
    { SCR_IDENTIFY,       RARITY_VERY_COMMON },
    { SCR_TELEPORTATION,  RARITY_COMMON },
    { SCR_AMNESIA,        RARITY_UNCOMMON },
    { SCR_NOISE,          RARITY_UNCOMMON },
    { SCR_ENCHANT_ARMOUR, RARITY_UNCOMMON },
    { SCR_ENCHANT_WEAPON, RARITY_UNCOMMON },
    { SCR_MAGIC_MAPPING,  RARITY_UNCOMMON },
    { SCR_FEAR,           RARITY_UNCOMMON },
    { SCR_FOG,            RARITY_UNCOMMON },
    { SCR_BLINKING,       RARITY_UNCOMMON },
    { SCR_IMMOLATION,     RARITY_UNCOMMON },
    { SCR_POISON,         RARITY_UNCOMMON },
    { SCR_VULNERABILITY,  RARITY_UNCOMMON },
    { SCR_SUMMONING,      RARITY_RARE },
    { SCR_SILENCE,        RARITY_RARE },
    { SCR_BRAND_WEAPON,   RARITY_RARE },
    { SCR_TORMENT,        RARITY_RARE },
    { SCR_ACQUIREMENT,    RARITY_VERY_RARE },
};

item_rarity_type consumable_rarity(const item_def &item)
{
    return consumable_rarity(item.base_type, item.sub_type);
}

item_rarity_type consumable_rarity(object_class_type base_type, int sub_type)
{
    item_rarity_type *rarity = nullptr;
    if (base_type == OBJ_POTIONS)
        rarity = map_find(_potion_rarity, (potion_type) sub_type);
    else if (base_type == OBJ_SCROLLS)
        rarity = map_find(_scroll_rarity, (scroll_type) sub_type);

    if (!rarity)
        return RARITY_NONE;

    return *rarity;
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

int get_armour_willpower(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    int res = 0;

    // intrinsic armour abilities
    res += armour_type_prop(arm.sub_type, ARMF_WILLPOWER) * WL_PIP;

    // check for ego resistance
    if (get_armour_ego_type(arm) == SPARM_WILLPOWER)
        res += WL_PIP;

    if (get_armour_ego_type(arm) == SPARM_GUILE)
        res -= 2 * WL_PIP;

    if (check_artp && is_artefact(arm))
        res += WL_PIP * artefact_property(arm, ARTP_WILLPOWER);

    return res;
}

bool get_armour_see_invisible(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    // check for ego resistance
    if (get_armour_ego_type(arm) == SPARM_SEE_INVISIBLE)
        return true;

    if (check_artp && is_artefact(arm))
        return artefact_property(arm, ARTP_SEE_INVISIBLE);

    return false;
}

int get_armour_res_corr(const item_def &arm)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    // intrinsic armour abilities
    return get_armour_ego_type(arm) == SPARM_PRESERVATION
           || armour_type_prop(arm.sub_type, ARMF_RES_CORR);
}

bool get_armour_rampaging(const item_def &arm, bool check_artp)
{
    ASSERT(arm.base_type == OBJ_ARMOUR);

    if (is_unrandom_artefact(arm, UNRAND_SEVEN_LEAGUE_BOOTS))
        return true;

    // check for ego resistance
    if (get_armour_ego_type(arm) == SPARM_RAMPAGING)
        return true;

    if (check_artp && is_artefact(arm))
        return artefact_property(arm, ARTP_RAMPAGING);

    return false;
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

int get_jewellery_willpower(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    int res = 0;

    if (ring.sub_type == RING_WILLPOWER)
        res += WL_PIP;

    if (check_artp && is_artefact(ring))
        res += WL_PIP * artefact_property(ring, ARTP_WILLPOWER);

    return res;
}

bool get_jewellery_see_invisible(const item_def &ring, bool check_artp)
{
    ASSERT(ring.base_type == OBJ_JEWELLERY);

    if (ring.sub_type == RING_SEE_INVISIBLE)
        return true;

    if (check_artp && is_artefact(ring))
        return artefact_property(ring, ARTP_SEE_INVISIBLE);

    return false;
}

int property(const item_def &item, int prop_type)
{
    weapon_type weapon_sub;

    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        return armour_prop(item.sub_type, prop_type);

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
        weapon_sub = WPN_STAFF;

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

/**
 * Find a given property of a given armour_type.
 *
 * @param armour        The armour_type in question (e.g. ARM_ROBE)
 * @param prop_type     The property being requested (e.g. PARM_AC)
 * @return              The property value, if the prop_type is valid.
 *                      Otherwise, 0 (!?)
 *                      ^ hopefully never comes up...
 */
int armour_prop(int armour, int prop_type)
{
    switch (prop_type)
    {
        case PARM_AC:
            return Armour_prop[ Armour_index[armour] ].ac;
        case PARM_EVASION:
            return Armour_prop[ Armour_index[armour] ].ev;
        default:
            return 0; // !?
    }
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
    if (artefact_property(item, ARTP_INVISIBLE)
        || artefact_property(item, ARTP_BLINK)
        || is_unrandom_artefact(item, UNRAND_ASMODEUS)
        || is_unrandom_artefact(item, UNRAND_DISPATER)
        || is_unrandom_artefact(item, UNRAND_OLGREB))
    {
        return true;
    }


    return false;
}

// Returns true if the item confers a resistance that is shown on the % screen,
// for the purposes of giving information in hints mode.
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
            if (item.sub_type == RING_PROTECTION_FROM_FIRE
                || item.sub_type == RING_POISON_RESISTANCE
                || item.sub_type == RING_PROTECTION_FROM_COLD
                || item.sub_type == RING_RESIST_CORROSION
                || item.sub_type == RING_LIFE_PROTECTION
                || item.sub_type == RING_WILLPOWER
                || item.sub_type == RING_FIRE
                || item.sub_type == RING_ICE
                || item.sub_type == RING_FLIGHT)
            {
                return true;
            }
        }
        break;
    case OBJ_ARMOUR:
    {
        const equipment_type eq = get_armour_slot(item);
        if (eq == EQ_NONE)
            return false;

        const int ego = get_armour_ego_type(item);
        if (ego == SPARM_FIRE_RESISTANCE
            || ego == SPARM_COLD_RESISTANCE
            || ego == SPARM_POISON_RESISTANCE
            || ego == SPARM_WILLPOWER
            || ego == SPARM_RESISTANCE
            || ego == SPARM_PRESERVATION
            || ego == SPARM_POSITIVE_ENERGY)
        {
            return true;
        }
        break;
    }
    case OBJ_STAVES:
        if (item.sub_type == STAFF_FIRE
            || item.sub_type == STAFF_COLD
            || item.sub_type == STAFF_POISON
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
    for (int rap = 0; rap < ARTP_NUM_PROPERTIES; rap++)
    {
        if (artefact_property(item, static_cast<artefact_prop_type>(rap))
            && (rap == ARTP_FIRE
                || rap == ARTP_COLD
                || rap == ARTP_ELECTRICITY
                || rap == ARTP_POISON
                || rap == ARTP_NEGATIVE_ENERGY
                || rap == ARTP_WILLPOWER
                || rap == ARTP_RCORR
                || rap == ARTP_RMUT))
        {
            return true;
        }
    }

    return false;
}

bool item_is_jelly_edible(const item_def &item)
{
    if (item_is_stationary_net(item))
        return false;

    // Don't eat artefacts or the horn of Geryon.
    if (is_artefact(item) || item_is_horn_of_geryon(item))
        return false;

    // Don't eat zigfigs. (They're artefact-like, and Jiyvaites shouldn't worry
    // about losing them.)
    if (item.base_type == OBJ_MISCELLANY && item.sub_type == MISC_ZIGGURAT)
        return false;

    // Don't eat mimics.
    if (item.flags & ISFLAG_MIMIC)
        return false;

    // Don't eat special game items.
    if (item_is_orb(item) || item.base_type == OBJ_RUNES)
        return false;

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
#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
#endif
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
           && get_armour_slot(item) == EQ_SHIELD
           && item.sub_type != ARM_ORB;
}

bool is_offhand(const item_def &item)
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

// Returns true if the worn shield has the reflection ego. Used only for
// messaging, to distinguish between a reflective shield and ego reflection.
bool shield_reflects(const item_def &shield)
{
    return is_shield(shield) && get_armour_ego_type(shield) == SPARM_REFLECTION;
}

int guile_adjust_willpower(int wl)
{
    return max(0, wl - 2 * WL_PIP);
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

void seen_item(item_def &item)
{
    if (!is_artefact(item) && _is_affordable(item))
    {
        // XXX: the first two are unsigned integers because
        // they used to be a bitfield tracking brands as well
        // and are marshalled to the save file as such.
        if (item.base_type == OBJ_WEAPONS)
            you.seen_weapon[item.sub_type] = 1U;
        if (item.base_type == OBJ_ARMOUR)
            you.seen_armour[item.sub_type] = 1U;
        if (item.base_type == OBJ_MISCELLANY)
            you.seen_misc.set(item.sub_type);
    }

    _maybe_note_found_unrand(item);

    item.flags |= ISFLAG_SEEN;
    if (item.base_type == OBJ_GOLD && !item.tithe_state)
        item.plus = (you_worship(GOD_ZIN)) ? TS_FULL_TITHE : TS_NO_PIETY;

    if (item_type_has_ids(item.base_type) && !is_artefact(item)
        && item_ident(item, ISFLAG_KNOW_TYPE)
        && !you.type_ids[item.base_type][item.sub_type])
    {
        // Can't cull shop items here -- when called from view, we shouldn't
        // access the UI. Old ziggurat prompts are a very minor case of what
        // could go wrong.
        set_ident_type(item.base_type, item.sub_type, true);
    }
}

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
           && map_find(xp_evoker_data,
                       static_cast<misc_item_type>(item.sub_type));
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
    const evoker_data* edata = map_find(xp_evoker_data,
                                   static_cast<misc_item_type>(evoker_type));
    ASSERT(edata);
    return you.props[edata->key].get_int();
}

/**
 * How many max charges can the given XP evoker have?
 *
 * @param evoker_type The type of evoker.
 * @returns The max number of charges.
 */
int evoker_max_charges(int evoker_type)
{
    const evoker_data* edata = map_find(xp_evoker_data,
                                   static_cast<misc_item_type>(evoker_type));
    ASSERT(edata);
    return edata->max_charges;
}

/**
 * What is the XP debt of using one charge of the given XP evoker type? This
 * debt represents a cost after scaling by a level-based XP factor.
 *
 * @params evoker_type The item sub type of the evoker
 * @returns The debt of using a charge.
 */
int evoker_charge_xp_debt(int evoker_type)
{
    const evoker_data* edata = map_find(xp_evoker_data,
                                        static_cast<misc_item_type>(evoker_type));
    ASSERT(edata);
    return edata->charge_xp_debt;
}

/**
 * How many remaining charges does the given XP evoker have?
 *
 * @param evoker_type The item subtype of the evoker.
 * @returns The number of remaining charges.
 */
int evoker_charges(int evoker_type)
{
    const int max_charges = evoker_max_charges(evoker_type);
    const int charge_xp_debt = evoker_charge_xp_debt(evoker_type);
    const int debt = evoker_debt(evoker_type);
    return min(max_charges,
            max_charges - debt / charge_xp_debt - (debt % charge_xp_debt > 0));
}

void expend_xp_evoker(int evoker_type)
{
    evoker_debt(evoker_type) += evoker_charge_xp_debt(evoker_type);
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

/**
 * For store pricing purposes, how much is the given type of weapon worth,
 * before curses, egos, etc are taken into account?
 *
 * @param type      The type of weapon in question; e.g. WPN_DAGGER.
 * @return          A value in gold; e.g. 20.
 */
int weapon_base_price(weapon_type type)
{
    return Weapon_prop[ Weapon_index[type] ].price;
}

/**
 * For store pricing purposes, how much is the given type of missile worth?
 *
 * @param type      The type of missile in question; e.g. MI_ARROW.
 * @return          A value in gold for each missile; e.g. 20.
 */
int missile_base_price(missile_type type)
{
    return Missile_prop[ Missile_index[type] ].price;
}

/**
 * For store pricing purposes, how much is the given type of armour worth,
 * before curses, egos, etc are taken into account?
 *
 * @param type      The type of weapon in question; e.g. ARM_RING_MAIL, or
 *                  ARM_BUCKLER.
 * @return          A value in gold; e.g. 45.
 */
int armour_base_price(armour_type type)
{
    return Armour_prop[ Armour_index[type] ].price;
}
