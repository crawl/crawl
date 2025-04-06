#pragma once

#include "tag-version.h"

// This list must match the enchant_names array in mon-ench.cc
// Enchantments that imply other enchantments should come first
// to avoid timeout message confusion. Currently:
//     berserk -> haste, might; fatigue -> slow
enum enchant_type
{
    ENCH_NONE = 0,
    ENCH_BERSERK,
    ENCH_HASTE,
    ENCH_MIGHT,
    ENCH_FATIGUE,        // Post-berserk fatigue.
    ENCH_SLOW,
    ENCH_FEAR,
    ENCH_CONFUSION,
    ENCH_INVIS,
    ENCH_POISON,
#if TAG_MAJOR_VERSION == 34
    ENCH_ROT,
#endif
    ENCH_SUMMON,
    ENCH_SUMMON_TIMER,
    ENCH_CORONA,
    ENCH_CHARM,
    ENCH_STICKY_FLAME,
    ENCH_GLOWING_SHAPESHIFTER,
    ENCH_SHAPESHIFTER,
    ENCH_TP,
    ENCH_SLEEP_WARY,
#if TAG_MAJOR_VERSION == 34
    ENCH_SUBMERGED,
    ENCH_SHORT_LIVED,
#endif
    ENCH_PARALYSIS,
    ENCH_SICK,
#if TAG_MAJOR_VERSION == 34
    ENCH_SLEEPY,         //   Monster can't wake until this wears off.
#endif
    ENCH_HELD,           //   Caught in a net.
#if TAG_MAJOR_VERSION == 34
    ENCH_OLD_BATTLE_FRENZY,
    ENCH_TEMP_PACIF,
#endif
    ENCH_PETRIFYING,
    ENCH_PETRIFIED,
    ENCH_LOWERED_WL,
    ENCH_SOUL_RIPE,
    ENCH_SLOWLY_DYING,
#if TAG_MAJOR_VERSION == 34
    ENCH_EAT_ITEMS,
#endif
    ENCH_AQUATIC_LAND,   // Water monsters lose hp while on land.
#if TAG_MAJOR_VERSION == 34
    ENCH_SPORE_PRODUCTION,
    ENCH_SLOUCH,
#endif
    ENCH_SWIFT,
    ENCH_TIDE,
    ENCH_FRENZIED,         // Berserk + changed attitude.
    ENCH_SILENCE,
    ENCH_AWAKEN_FOREST,
    ENCH_EXPLODING,
#if TAG_MAJOR_VERSION == 34
    ENCH_BLEED,
#endif
    ENCH_PORTAL_TIMER,
    ENCH_SEVERED,
    ENCH_ANTIMAGIC,
#if TAG_MAJOR_VERSION == 34
    ENCH_FADING_AWAY,
    ENCH_PREPARING_RESURRECT,
#endif
    ENCH_REGENERATION,
    ENCH_STRONG_WILLED,
    ENCH_MIRROR_DAMAGE,
#if TAG_MAJOR_VERSION == 34
    ENCH_MAGIC_ARMOUR,
    ENCH_FEAR_INSPIRING,
#endif
    ENCH_PORTAL_PACIFIED,
#if TAG_MAJOR_VERSION == 34
    ENCH_WITHDRAWN,
    ENCH_ATTACHED,
    ENCH_LIFE_TIMER,     // Minimum time demonic guardian must exist.
#endif
    ENCH_FLIGHT,
    ENCH_LIQUEFYING,
    ENCH_POLAR_VORTEX,
#if TAG_MAJOR_VERSION == 34
    ENCH_FAKE_ABJURATION,
#endif
    ENCH_DAZED,          // Dazed - less chance of acting each turn.
    ENCH_MUTE,           // Silenced.
    ENCH_BLIND,          // Blind (everything is invisible).
    ENCH_DUMB,           // Stupefied (paralysis by a different name).
    ENCH_MAD,            // Confusion by another name.
    ENCH_SILVER_CORONA,  // Zin's silver light.
    ENCH_RECITE_TIMER,   // Was recited against.
    ENCH_INNER_FLAME,
#if TAG_MAJOR_VERSION == 34
    ENCH_OLD_ROUSED,
#endif
    ENCH_BREATH_WEAPON,  // timer for breathweapon/similar spam
#if TAG_MAJOR_VERSION == 34
    ENCH_DEATHS_DOOR,
#endif
    ENCH_ROLLING,        // Boulder Beetle in ball form
#if TAG_MAJOR_VERSION == 34
    ENCH_OZOCUBUS_ARMOUR,
#endif
    ENCH_WRETCHED,       // An abstract placeholder for monster mutations
    ENCH_SCREAMED,       // Starcursed scream timer
    ENCH_WORD_OF_RECALL, // Chanting word of recall
    ENCH_INJURY_BOND,
    ENCH_WATER_HOLD,     // Silence and asphyxiation damage
    ENCH_FLAYED,
    ENCH_HAUNTING,
#if TAG_MAJOR_VERSION == 34
    ENCH_RETCHING,
#endif
    ENCH_WEAK,
    ENCH_DIMENSION_ANCHOR,
#if TAG_MAJOR_VERSION == 34
    ENCH_AWAKEN_VINES,
    ENCH_CONTROL_WINDS,
    ENCH_WIND_AIDED,
    ENCH_SUMMON_CAPPED,  // Abjuring quickly because a summon cap was hit
#endif
    ENCH_TOXIC_RADIANCE,
#if TAG_MAJOR_VERSION == 34
    ENCH_GRASPING_ROOTS_SOURCE, // Not actually entangled, but entangling others
#endif
    ENCH_GRASPING_ROOTS,
    ENCH_SPELL_CHARGED,
    ENCH_FIRE_VULN,
    ENCH_POLAR_VORTEX_COOLDOWN,
    ENCH_MERFOLK_AVATAR_SONG,
    ENCH_BARBS,
#if TAG_MAJOR_VERSION == 34
    ENCH_BUILDING_CHARGE,
#endif
    ENCH_POISON_VULN,
#if TAG_MAJOR_VERSION == 34
    ENCH_ICEMAIL,
#endif
    ENCH_AGILE,
    ENCH_FROZEN,
#if TAG_MAJOR_VERSION == 34
    ENCH_EPHEMERAL_INFUSION,
#endif
    ENCH_SIGN_OF_RUIN,
#if TAG_MAJOR_VERSION == 34
    ENCH_GRAND_AVATAR,
#endif
    ENCH_SAP_MAGIC,
#if TAG_MAJOR_VERSION == 34
    ENCH_SHROUD,
#endif
    ENCH_PHANTOM_MIRROR,
    ENCH_NEUTRAL_BRIBED,
    ENCH_FRIENDLY_BRIBED,
    ENCH_CORROSION,
    ENCH_GOLD_LUST,
    ENCH_DRAINED,
    ENCH_REPEL_MISSILES,
#if TAG_MAJOR_VERSION == 34
    ENCH_DEFLECT_MISSILES,
    ENCH_NEGATIVE_VULN,
    ENCH_CONDENSATION_SHIELD,
#endif
    ENCH_RESISTANCE,
    ENCH_HEXED,
#if TAG_MAJOR_VERSION == 34
    ENCH_BONE_ARMOUR,
    ENCH_CHANT_FIRE_STORM, // chanting the fire storm spell
    ENCH_CHANT_WORD_OF_ENTROPY, // chanting word of entropy
    ENCH_BRILLIANCE_AURA, // emanating a brilliance aura
#endif
    ENCH_EMPOWERED_SPELLS, // affected by above
    ENCH_GOZAG_INCITE,
    ENCH_PAIN_BOND, // affected by above
    ENCH_IDEALISED,
    ENCH_BOUND_SOUL,
    ENCH_INFESTATION,
    ENCH_STILL_WINDS,
    ENCH_RING_OF_THUNDER,
#if TAG_MAJOR_VERSION == 34
    ENCH_WHIRLWIND_PINNED,
    ENCH_VORTEX,
    ENCH_VORTEX_COOLDOWN,
#endif
    ENCH_VILE_CLUTCH,
    ENCH_WATERLOGGED,
    ENCH_RING_OF_FLAMES,
    ENCH_RING_OF_CHAOS,
    ENCH_RING_OF_MUTATION,
    ENCH_RING_OF_FOG,
    ENCH_RING_OF_ICE,
    ENCH_RING_OF_MISERY,
    ENCH_RING_OF_ACID,
    ENCH_RING_OF_MIASMA,
    ENCH_CONCENTRATE_VENOM,
    ENCH_FIRE_CHAMPION,
    ENCH_ANGUISH,
#if TAG_MAJOR_VERSION == 34
    ENCH_SIMULACRUM,
    ENCH_NECROTISE,
#endif
    ENCH_CONTAM,
#if TAG_MAJOR_VERSION == 34
    ENCH_PURSUING,
#endif
    ENCH_BOUND,
    ENCH_BULLSEYE_TARGET,
    ENCH_VITRIFIED,
    ENCH_INSTANT_CLEAVE,
    ENCH_PROTEAN_SHAPESHIFTING,
    ENCH_SIMULACRUM_SCULPTING,
    ENCH_CURSE_OF_AGONY,
    ENCH_CHANNEL_SEARING_RAY,
    ENCH_TOUCH_OF_BEOGH,
    ENCH_VENGEANCE_TARGET,
    ENCH_RIMEBLIGHT,
    ENCH_MAGNETISED,
    ENCH_ARMED,
    ENCH_MISDIRECTED,
    ENCH_CHANGED_APPEARANCE,  // Visual change for player shadow during Shadowslip
    ENCH_SHADOWLESS,
    ENCH_DOUBLED_HEALTH,
    ENCH_KINETIC_GRAPNEL,
    ENCH_TEMPERED,
    ENCH_HATCHING,
    ENCH_BLINKITIS,
    ENCH_CHAOS_LACE,
    ENCH_VEXED,
    ENCH_DEEP_SLEEP,
    ENCH_DROWSY,
    ENCH_VAMPIRE_THRALL,
    ENCH_PYRRHIC_RECOLLECTION,
    // Update enchant_names[] in mon-ench.cc when adding or removing
    // enchantments.
    NUM_ENCHANTMENTS
};
