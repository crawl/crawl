/**
 * @file
 * @brief Monster-related enums.
 *
 * Extracted from mon-util.h to cut down on mon-util.h
 * dependencies.
**/

#pragma once

#include "tag-version.h"

#define HERD_COMFORT_RANGE 6

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
    AF_DRAIN_STR,
    AF_DRAIN_INT,
    AF_DRAIN_DEX,
    AF_DRAIN_STAT,
#endif
    AF_DRAIN,
    AF_ELEC,
    AF_FIRE,
#if TAG_MAJOR_VERSION == 34
    AF_HUNGER,
    AF_MUTATE,
#endif
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
    AF_ROT,
#endif
    AF_VAMPIRIC,
#if TAG_MAJOR_VERSION == 34
    AF_KLOWN,
#endif
    AF_DISTORT,
    AF_RAGE,
#if TAG_MAJOR_VERSION == 34
    AF_STICKY_FLAME,
#endif
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
#if TAG_MAJOR_VERSION == 34
    AF_KITE,  // Hops backwards if attacking with a polearm.
#endif
    AF_SWOOP, // Swoops in to perform a melee attack if far away.
    AF_TRAMPLE, // Trampling effect.
    AF_WEAKNESS,
#if TAG_MAJOR_VERSION == 34
    AF_MIASMATA,
#endif
    AF_REACH_TONGUE,
    AF_BLINK_WITH,
    AF_SEAR,
    AF_BARBS,
    AF_SPIDER,
    AF_RIFT,
    AF_BLOODZERK,
    AF_SLEEP,
    AF_MINIPARA,
    AF_FLANK,
    AF_DRAG,
    AF_FOUL_FLAME,
    AF_HELL_HUNT,
    AF_SWARM,
    AF_ALEMBIC,
    AF_BOMBLET,
    AF_AIRSTRIKE,
    AF_TRICKSTER,
};

// Non-spell "summoning" types to give to monster::mark_summoned(), or
// as the second parameter of mgen_data::set_summoned().
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
#if TAG_MAJOR_VERSION == 34
    MON_SUMM_SHADOW,  // Shadow trap
    MON_SUMM_LANTERN, // Lantern of shadows
#endif
    MON_SUMM_BUTTERFLIES, // Scroll of butterflies
    MON_SUMM_YRED_REAP, // Yred's reaping passive
    MON_SUMM_WPN_REAP,  // Reaping brand reaping
    MON_SUMM_CACOPHONY, // Poltergeist ability
    MON_SUMM_THRALL,    // Vampiric thralls
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
    MR_RES_MIASMA        = 1 << 15,
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
    // unused 1 << 26,
    // unused 1 << 27,
    MR_RES_STEAM         = 1 << 28,

    // vulnerabilities
#if TAG_MAJOR_VERSION == 34
    MR_VUL_WATER         = 1 << 29,
#endif
};

const mon_resist_flags ALL_MON_RESISTS[] = {
    MR_RES_ELEC,
    MR_RES_POISON,
    MR_RES_FIRE,
    MR_RES_COLD,
    MR_RES_NEG,
    MR_RES_ACID,
    MR_RES_MIASMA,
    MR_RES_TORMENT,
    MR_RES_PETRIFY,
    MR_RES_DAMNATION,
    MR_RES_STEAM,
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
    S_HISS,                 // for reptiles, quiet!
    S_SKITTER,              // medium+ arachnids and similar
    S_FAINT_SKITTER,        // little/small arachnids/insects, quiet!
    S_DEMON_TAUNT,          // for pandemonium lords
    S_CHERUB,               // for cherubs
    S_SQUEAL,               // pigs
    S_LOUD_ROAR,            // dragons, &c. loud!
    S_RUSTLE,               // books
    S_SQUEAK,               // rats and similar
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
