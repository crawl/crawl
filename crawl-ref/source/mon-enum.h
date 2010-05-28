/*
 * Summary:   Monster-related enums.
 *
 * Extracted from mon-util.h to cut down on mon-util.h
 * dependencies.
 */

#ifndef MON_ENUM_H
#define MON_ENUM_H

#include "tag-version.h"

enum corpse_effect_type
{
    CE_NOCORPSE,        //    0
    CE_CLEAN,           //    1
    CE_CONTAMINATED,    //    2
    CE_POISONOUS,       //    3
    CE_HCL,             //    4
    CE_MUTAGEN_RANDOM,  //    5
    CE_MUTAGEN_GOOD,    //    6 - may be worth implementing {dlb}
    CE_MUTAGEN_BAD,     //    7 - may be worth implementing {dlb}
    CE_RANDOM,          //    8 - not used, but may be worth implementing {dlb}
    CE_ROTTEN = 50      //   50 - must remain at 50 for now {dlb}
};

enum gender_type
{
    GENDER_NEUTER,
    GENDER_MALE,
    GENDER_FEMALE
};

enum mon_attack_type
{
    AT_NONE,
    AT_HIT,         // Including weapon attacks.
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

    AT_SHOOT,       // Attack representing missile damage for M_ARCHER.
    AT_WEAP_ONLY,   // Ranged weap: shoot point-blank like AT_SHOOT, melee weap:
                    //   use it, no weapon: stand there doing nothing.
    AT_RANDOM       // Anything but AT_SHOOT and AT_WEAP_ONLY.
};

enum mon_attack_flavour
{
    AF_PLAIN,
    AF_ACID,
    AF_BLINK,
    AF_COLD,
    AF_CONFUSE,
    AF_DISEASE,
    AF_DRAIN_STR,
    AF_DRAIN_DEX,
    AF_DRAIN_XP,
    AF_ELEC,
    AF_FIRE,
    AF_HUNGER,
    AF_MUTATE,
    AF_PARALYSE,
    AF_POISON,
    AF_POISON_NASTY,
    AF_POISON_MEDIUM,
    AF_POISON_STRONG,
    AF_POISON_STR,
    AF_POISON_INT,
    AF_POISON_DEX,
    AF_POISON_STAT,
    AF_ROT,
    AF_VAMPIRIC,
    AF_KLOWN,
    AF_DISTORT,
    AF_RAGE,
    AF_NAPALM,
    AF_CHAOS,
    AF_STEAL,
    AF_STEAL_FOOD,
    AF_CRUSH,
    AF_REACH
};

// Non-spell "summoning" types to give to monsters::mark_summoned(), or
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
    MON_SUMM_AID      // Divine aid
};

// properties of the monster class (other than resists/vulnerabilities)
enum mons_class_flags
{
    M_NO_FLAGS          = 0,

    M_SPELLCASTER       = (1<< 0),        // any non-physical-attack powers,
    M_ACTUAL_SPELLS     = (1<< 1),        // monster is a wizard
                                          // (hated by Trog, affected by silence)
    M_PRIEST            = (1<< 2),        // monster is a priest
    M_FIGHTER           = (1<< 3),        // monster is skilled fighter

    M_NO_WAND           = (1<< 4),        // do not give (unique) a wand
    M_NO_HT_WAND        = (1<< 5),        // do not give a high tier wand

    M_INVIS             = (1<< 6),        // is created invis
    M_SEE_INVIS         = (1<< 7),        // can see invis
    M_SENSE_INVIS       = (1<< 8),        // can sense invisible things
    M_SPEAKS            = (1<< 9),        // uses talking code
    M_CONFUSED          = (1<<10),        // monster is perma-confused,
    M_BATTY             = (1<<11),        // monster is batty
    M_SPLITS            = (1<<12),        // monster can split
    M_GLOWS             = (1<<13),        // monster glows
    M_STATIONARY        = (1<<14),        // monster is stationary
    M_BLOOD_SCENT       = (1<<15),        // monster can smell blood
    M_COLD_BLOOD        = (1<<16),        // susceptible to cold,
                                          // drainable by vampires
    M_WARM_BLOOD        = (1<<17),        // no effect currently,
                                          // drainable by vampires
    //                    (1<<18)         // XXX: unused
    M_BURROWS           = (1<<19),        // monster digs through rock

    M_SUBMERGES         = (1<<20),        // monster can submerge

    M_UNIQUE            = (1<<21),        // monster is a unique
    M_ACID_SPLASH       = (1<<22),        // Passive acid splash when hit.

    M_ARCHER            = (1<<23),        // gets various archery boosts

    M_INSUBSTANTIAL     = (1<<24),        // is insubstantial

    M_TWOWEAPON         = (1<<25),        // wields two weapons at once

    M_SPECIAL_ABILITY   = (1<<26),        // XXX: eventually make these spells?
    M_NO_REGEN          = (1<<27),        // cannot regenerate

    M_SPELL_NO_SILENT   = (1<<28),        // cannot cast spells when silenced,
                                          // even though it's not a priest or
                                          // wizard

    M_NOISY_SPELLS      = (1<<29),        // can cast spells when silenced, but
                                          // casting makes noise when not
                                          // silenced

    M_NO_SKELETON       = (1<<30),        // boneless corpses
    M_NO_EXP_GAIN       = (1<<31)         // worth 0 xp
};

enum mon_intel_type             // Must be in increasing intelligence order
{
    I_PLANT = 0,
    I_INSECT,
    I_ANIMAL,
    I_NORMAL,
    I_HIGH
};

enum habitat_type
{
    // Flying monsters will appear in all categories except rock walls
    HT_LAND = 0,         // Land critters
    HT_AMPHIBIOUS,       // Amphibious creatures
    HT_WATER,            // Water critters
    HT_LAVA,             // Lava critters
    HT_ROCK,             // Rock critters

    NUM_HABITATS
};

// order of these is important:
enum mon_itemuse_type
{
    MONUSE_NOTHING,
    MONUSE_OPEN_DOORS,
    MONUSE_STARTING_EQUIPMENT,
    MONUSE_WEAPONS_ARMOUR,
    MONUSE_MAGIC_ITEMS,

    NUM_MONUSE
};

enum mon_itemeat_type
{
    MONEAT_NOTHING,
    MONEAT_ITEMS,
    MONEAT_CORPSES,
    MONEAT_FOOD,

    NUM_MONEAT
};

// now saved in an unsigned long.
enum mon_resist_flags
{
    MR_NO_FLAGS          = 0,

    // resistances
    // Notes:
    // - negative energy is mostly handled via monsters::res_negative_energy()
    MR_RES_ELEC          = (1<< 0),
    MR_RES_POISON        = (1<< 1),
    MR_RES_FIRE          = (1<< 2),
    MR_RES_HELLFIRE      = (1<< 3),
    MR_RES_COLD          = (1<< 4),
    MR_RES_ASPHYX        = (1<< 5),
    MR_RES_ACID          = (1<< 6),

    // vulnerabilities
    MR_VUL_ELEC          = (1<< 7),
    MR_VUL_POISON        = (1<< 8),
    MR_VUL_FIRE          = (1<< 9),
    MR_VUL_COLD          = (1<<10),

    // Melee armour resists/vulnerabilities.
    // XXX: how to do combos (bludgeon/slice, bludgeon/pierce)
    MR_RES_PIERCE        = (1<<11),
    MR_RES_SLICE         = (1<<12),
    MR_RES_BLUDGEON      = (1<<13),

    MR_VUL_PIERCE        = (1<<14),
    MR_VUL_SLICE         = (1<<15),
    MR_VUL_BLUDGEON      = (1<<16),

    // Immune to stickiness of sticky flame.
    MR_RES_STICKY_FLAME  = (1<<17),

    // Immune to rotting.
    MR_RES_ROTTING       = (1<<18),

    MR_RES_STEAM         = (1<<19)
};

enum shout_type
{
    S_SILENT,               // silent
    S_SHOUT,                // shout
    S_BARK,                 // bark
    S_SHOUT2,               // shout twice (e.g. two-headed ogres)
    S_ROAR,                 // roar
    S_SCREAM,               // scream
    S_BELLOW,               // bellow (yaks)
    S_SCREECH,              // screech
    S_BUZZ,                 // buzz
    S_MOAN,                 // moan
    S_GURGLE,               // gurgle
    S_WHINE,                // irritating whine (mosquitos)
    S_CROAK,                // frog croak
    S_GROWL,                // for bears
    S_HISS,                 // for snakes and lizards
    S_DEMON_TAUNT,          // for pandemonium lords
    NUM_SHOUTS,

    // Loudness setting for shouts that are only defined in dat/shout.txt
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
    Z_BIG
};

enum mon_body_shape
{
    MON_SHAPE_HUMANOID,
    MON_SHAPE_HUMANOID_WINGED,
    MON_SHAPE_HUMANOID_TAILED,
    MON_SHAPE_HUMANOID_WINGED_TAILED,
    MON_SHAPE_CENTAUR,
    MON_SHAPE_NAGA,
    MON_SHAPE_QUADRUPED,
    MON_SHAPE_QUADRUPED_TAILLESS,
    MON_SHAPE_QUADRUPED_WINGED,
    MON_SHAPE_BAT,
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
    MON_SHAPE_MISC
};

#endif

