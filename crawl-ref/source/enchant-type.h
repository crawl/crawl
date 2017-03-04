#pragma once

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
    ENCH_ABJ,
    ENCH_CORONA,
    ENCH_CHARM,
    ENCH_STICKY_FLAME,
    ENCH_GLOWING_SHAPESHIFTER,
    ENCH_SHAPESHIFTER,
    ENCH_TP,
    ENCH_SLEEP_WARY,
    ENCH_SUBMERGED,
    ENCH_SHORT_LIVED,
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
    ENCH_LOWERED_MR,
    ENCH_SOUL_RIPE,
    ENCH_SLOWLY_DYING,
#if TAG_MAJOR_VERSION == 34
    ENCH_EAT_ITEMS,
#endif
    ENCH_AQUATIC_LAND,   // Water monsters lose hp while on land.
    ENCH_SPORE_PRODUCTION,
#if TAG_MAJOR_VERSION == 34
    ENCH_SLOUCH,
#endif
    ENCH_SWIFT,
    ENCH_TIDE,
    ENCH_INSANE,         // Berserk + changed attitude.
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
    ENCH_RAISED_MR,
    ENCH_MIRROR_DAMAGE,
#if TAG_MAJOR_VERSION == 34
    ENCH_MAGIC_ARMOUR,
#endif
    ENCH_FEAR_INSPIRING,
    ENCH_PORTAL_PACIFIED,
#if TAG_MAJOR_VERSION == 34
    ENCH_WITHDRAWN,
    ENCH_ATTACHED,
#endif
    ENCH_LIFE_TIMER,     // Minimum time demonic guardian must exist.
    ENCH_FLIGHT,
    ENCH_LIQUEFYING,
    ENCH_TORNADO,
    ENCH_FAKE_ABJURATION,
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
    ENCH_ROLLING,        // Boulder Beetle in ball form
#endif
    ENCH_OZOCUBUS_ARMOUR,
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
    ENCH_AWAKEN_VINES,   // Is presently animating snaplasher vines
#if TAG_MAJOR_VERSION == 34
    ENCH_CONTROL_WINDS,
    ENCH_WIND_AIDED,
#endif
    ENCH_SUMMON_CAPPED,  // Abjuring quickly because a summon cap was hit
    ENCH_TOXIC_RADIANCE,
    ENCH_GRASPING_ROOTS_SOURCE, // Not actually entangled, but entangling others
    ENCH_GRASPING_ROOTS,
    ENCH_SPELL_CHARGED,
    ENCH_FIRE_VULN,
    ENCH_TORNADO_COOLDOWN,
    ENCH_MERFOLK_AVATAR_SONG,
    ENCH_BARBS,
#if TAG_MAJOR_VERSION == 34
    ENCH_BUILDING_CHARGE,
#endif
    ENCH_POISON_VULN,
    ENCH_ICEMAIL,
    ENCH_AGILE,
    ENCH_FROZEN,
#if TAG_MAJOR_VERSION == 34
    ENCH_EPHEMERAL_INFUSION,
#endif
    ENCH_BLACK_MARK,
#if TAG_MAJOR_VERSION == 34
    ENCH_GRAND_AVATAR,
#endif
    ENCH_SAP_MAGIC,
    ENCH_SHROUD,
    ENCH_PHANTOM_MIRROR,
    ENCH_NEUTRAL_BRIBED,
    ENCH_FRIENDLY_BRIBED,
    ENCH_CORROSION,
    ENCH_GOLD_LUST,
    ENCH_DRAINED,
    ENCH_REPEL_MISSILES,
    ENCH_DEFLECT_MISSILES,
#if TAG_MAJOR_VERSION == 34
    ENCH_NEGATIVE_VULN,
    ENCH_CONDENSATION_SHIELD,
#endif
    ENCH_RESISTANCE,
    ENCH_HEXED,
    ENCH_BONE_ARMOUR,
#if TAG_MAJOR_VERSION == 34
    ENCH_CHANT_FIRE_STORM, // chanting the fire storm spell
    ENCH_CHANT_WORD_OF_ENTROPY, // chanting word of entropy
#endif
    ENCH_BRILLIANCE_AURA, // emanating a brilliance aura
    ENCH_EMPOWERED_SPELLS, // affected by above
    ENCH_GOZAG_INCITE,
    ENCH_PAIN_BOND, // affected by above
    ENCH_IDEALISED,
    ENCH_BOUND_SOUL,
    ENCH_INFESTATION,
    ENCH_STILL_WINDS,
    ENCH_RING_OF_THUNDER,
    ENCH_DISTRACTED_ACROBATICS,
    // Update enchant_names[] in mon-ench.cc when adding or removing
    // enchantments.
    NUM_ENCHANTMENTS
};
