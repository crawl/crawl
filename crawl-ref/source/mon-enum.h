/**
 * @file
 * @brief Monster-related enums.
 *
 * Extracted from mon-util.h to cut down on mon-util.h
 * dependencies.
**/

#pragma once

#define HERD_COMFORT_RANGE 6

enum corpse_effect_type
{
    CE_NOCORPSE,
    CE_CLEAN,
    CE_NOXIOUS,
};

// TODO: Unify this and a player_equivalent (if applicable)
// and move into attack.h
enum attack_type
{
    AT_NONE,
    AT_HIT,         // Including weapon attacks.
    AT_FIRST_ATTACK = AT_HIT,
    AT_BITE,
    AT_STING,
    AT_SPORE,
    AT_TOUCH,
    AT_ENGULF,
    AT_CLAW,
    AT_PECK,
    AT_HEADBUTT,
    AT_PUNCH,
    AT_KICK,
    AT_TENTACLE_SLAP,
    AT_TAIL_SLAP,
    AT_GORE,
    AT_CONSTRICT,
    AT_TRAMPLE,
    AT_TRUNK_SLAP,
#if TAG_MAJOR_VERSION == 34
    AT_SNAP,
    AT_SPLASH,
#endif
    AT_POUNCE,
#if TAG_MAJOR_VERSION == 34
    AT_REACH_STING,
    AT_LAST_REAL_ATTACK = AT_REACH_STING,
#else
    AT_LAST_REAL_ATTACK = AT_POUNCE,
#endif

    AT_CHERUB,
#if TAG_MAJOR_VERSION == 34
    AT_SHOOT,
#endif
    AT_WEAP_ONLY,   // AT_HIT if wielding a melee weapon, AT_NONE otherwise
    AT_RANDOM,      // Anything but AT_SHOOT and AT_WEAP_ONLY.
    NUM_ATTACK_TYPES,
};

// When adding an attack flavour, give it a short description in
// _describe_attack_flavour() in describe.cc.
enum attack_flavour
{
    AF_PLAIN,
    AF_ACID,
    AF_BLINK,
    AF_COLD,
    AF_CONFUSE,
#if TAG_MAJOR_VERSION == 34
    AF_DISEASE,
#endif
    AF_DRAIN_STR,
    AF_DRAIN_INT,
    AF_DRAIN_DEX,
    AF_DRAIN_STAT,
    AF_DRAIN_XP,
    AF_ELEC,
    AF_FIRE,
    AF_HUNGER,
    AF_MUTATE,
    AF_POISON_PARALYSE,
    AF_POISON,
#if TAG_MAJOR_VERSION == 34
    AF_POISON_NASTY,
    AF_POISON_MEDIUM,
#endif
    AF_POISON_STRONG,
#if TAG_MAJOR_VERSION == 34
    AF_POISON_STR,
    AF_POISON_INT,
    AF_POISON_DEX,
    AF_POISON_STAT,
#endif
    AF_ROT,
    AF_VAMPIRIC,
    AF_KLOWN,
    AF_DISTORT,
    AF_RAGE,
    AF_STICKY_FLAME,
    AF_CHAOTIC,
    AF_STEAL,
#if TAG_MAJOR_VERSION == 34
    AF_STEAL_FOOD,
#endif
    AF_CRUSH,
    AF_REACH,
    AF_HOLY,
    AF_ANTIMAGIC,
    AF_PAIN,
    AF_ENSNARE,
    AF_ENGULF,
    AF_PURE_FIRE,
    AF_DRAIN_SPEED,
    AF_VULN,
#if TAG_MAJOR_VERSION == 34
    AF_PLAGUE,
#endif
    AF_REACH_STING,
    AF_SHADOWSTAB,
    AF_DROWN,
#if TAG_MAJOR_VERSION == 34
    AF_FIREBRAND,
#endif
    AF_CORRODE,
    AF_SCARAB,
    AF_KITE,  // Hops backwards if attacking with a polearm.
    AF_SWOOP, // Swoops in to perform a melee attack if far away.
    AF_TRAMPLE, // Trampling effect.
    AF_WEAKNESS,
#if TAG_MAJOR_VERSION == 34
    AF_MIASMATA,
#endif
};

// Non-spell "summoning" types to give to monster::mark_summoned(), or
// as the fourth parameter of mgen_data's constructor.
//
// Negative values since spells are non-negative.
enum mon_summon_type
{
    MON_SUMM_CLONE = -10000, // Cloned from another monster
    MON_SUMM_ANIMATE, // Item/feature/substance which was animated
    MON_SUMM_CHAOS,   // Was made from pure chaos
    MON_SUMM_MISCAST, // Spell miscast
    MON_SUMM_ZOT,     // Zot trap
    MON_SUMM_WRATH,   // Divine wrath
    MON_SUMM_AID,     // Divine aid
    MON_SUMM_SCROLL,  // Scroll of summoning
    MON_SUMM_SHADOW,  // Shadow trap
#if TAG_MAJOR_VERSION == 34
    MON_SUMM_LANTERN, // Lantern of shadows
#endif
};

#include "mon-flags.h"

enum mon_intel_type             // Must be in increasing intelligence order
{
    I_BRAINLESS = 0,
    I_ANIMAL,
    I_HUMAN,
};

enum habitat_type
{
    // Flying monsters will appear in all categories except rock walls
    HT_LAND = 0,         // Land critters
    HT_AMPHIBIOUS,       // Amphibious creatures
    HT_WATER,            // Water critters
    HT_LAVA,             // Lava critters
    HT_AMPHIBIOUS_LAVA,  // Amphibious w/ lava (salamanders)

    NUM_HABITATS
};

// order of these is important:
enum mon_itemuse_type
{
    MONUSE_NOTHING,
    MONUSE_OPEN_DOORS,
    MONUSE_STARTING_EQUIPMENT,
    MONUSE_WEAPONS_ARMOUR,

    NUM_MONUSE
};

typedef uint32_t resists_t;
#define mrd(res, lev) (resists_t)((res) * ((lev) & 7))

enum mon_resist_flags
{
    MR_NO_FLAGS          = 0,

    // resistances
    // Notes:
    // - negative energy is mostly handled via monster::res_negative_energy()
    MR_RES_ELEC          = 1 << 0,
    MR_RES_POISON        = 1 << 3,
    MR_RES_FIRE          = 1 << 6,
    MR_RES_COLD          = 1 << 9,
    MR_RES_NEG           = 1 << 12,
    MR_RES_ROTTING       = 1 << 15,
    MR_RES_ACID          = 1 << 18,

    MR_LAST_MULTI, // must be >= any multi, < any boolean, exact value doesn't matter

    MR_RES_TORMENT       = 1 << 22,
    MR_RES_PETRIFY       = 1 << 23,
    MR_RES_DAMNATION     = 1 << 24,
#if TAG_MAJOR_VERSION == 34
    MR_OLD_RES_ACID      = 1 << 25,
#else
    // unused 1 << 25,
#endif
    MR_RES_STICKY_FLAME  = 1 << 26,
    MR_RES_TORNADO       = 1 << 27,
    MR_RES_STEAM         = 1 << 28,

    // vulnerabilities
    MR_VUL_WATER         = 1 << 29,
    MR_VUL_ELEC          = mrd(MR_RES_ELEC, -1),
    MR_VUL_POISON        = mrd(MR_RES_POISON, -1),
    MR_VUL_FIRE          = mrd(MR_RES_FIRE, -1),
    MR_VUL_COLD          = mrd(MR_RES_COLD, -1),
};

enum shout_type
{
    S_SILENT,               // silent
    S_SHOUT,                // shout
    S_BARK,                 // bark
    S_HOWL,                 // howl
    S_SHOUT2,               // shout twice (e.g. two-headed ogres)
    S_ROAR,                 // roar
    S_SCREAM,               // scream
    S_BELLOW,               // bellow (yaks)
    S_BLEAT,                // bleat (sheep)
    S_TRUMPET,              // trumpets (elephants)
    S_SCREECH,              // screech
    S_BUZZ,                 // buzz
    S_MOAN,                 // moan
    S_GURGLE,               // gurgle
    S_CROAK,                // frog croak
    S_GROWL,                // for bears
    S_HISS,                 // for reptiles & arachnids. quiet!
    S_DEMON_TAUNT,          // for pandemonium lords
    S_CHERUB,               // for cherubs
    S_SQUEAL,               // pigs
    S_LOUD_ROAR,            // dragons, &c. loud!
    NUM_SHOUTS,

    // Loudness setting for shouts that are only defined in dat/shout.txt
    // Only used for the verb/volume of random demon taunts
    S_VERY_SOFT,
    S_SOFT,
    S_NORMAL,
    S_LOUD,
    S_VERY_LOUD,

    NUM_LOUDNESS,
    S_RANDOM
};

enum zombie_size_type
{
    Z_NOZOMBIE = 0,
    Z_SMALL,
    Z_BIG,
};

/**
 * Monster body shapes. Set for each monster in the monster definitions of
 * mon-data.h, the monster's shape affects various casting and melee attack
 * messages.
 */
enum mon_body_shape
{
    MON_SHAPE_BUGGY,
    MON_SHAPE_HUMANOID,
    MON_SHAPE_FIRST_HUMANOID = MON_SHAPE_HUMANOID,
    MON_SHAPE_HUMANOID_WINGED,
    MON_SHAPE_HUMANOID_TAILED,
    MON_SHAPE_HUMANOID_WINGED_TAILED,
    MON_SHAPE_CENTAUR,
    MON_SHAPE_NAGA,
    MON_SHAPE_LAST_HUMANOID = MON_SHAPE_NAGA,
    // Everything before this should have at least a humanoid upper body
    MON_SHAPE_QUADRUPED,
    MON_SHAPE_QUADRUPED_TAILLESS,
    MON_SHAPE_QUADRUPED_WINGED,
    MON_SHAPE_BAT,
    MON_SHAPE_BIRD,
    MON_SHAPE_SNAKE, // Including eels and worms
    MON_SHAPE_FISH,
    MON_SHAPE_INSECT,
    MON_SHAPE_INSECT_WINGED,
    MON_SHAPE_ARACHNID,
    MON_SHAPE_CENTIPEDE,
    MON_SHAPE_SNAIL,
    MON_SHAPE_PLANT,
    MON_SHAPE_FUNGUS,
    MON_SHAPE_ORB,
    MON_SHAPE_BLOB,
    MON_SHAPE_MISC,
};
