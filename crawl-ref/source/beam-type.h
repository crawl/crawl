#pragma once

enum beam_type                  // bolt::flavour
{
    BEAM_NONE,

    BEAM_MISSILE,
    BEAM_MMISSILE,                //    and similarly irresistible things
    BEAM_FIRE,
    BEAM_COLD,
    BEAM_MAGIC,
    BEAM_ELECTRICITY,
    BEAM_POISON,
    BEAM_NEG,
    BEAM_ACID,
    BEAM_MIASMA,
    BEAM_WATER,

    BEAM_SPORE,
    BEAM_POISON_ARROW,
    BEAM_POISON_ERINYA,
    BEAM_DAMNATION,
    BEAM_STICKY_FLAME,
    BEAM_STEAM,
    BEAM_ENERGY,
    BEAM_HOLY,
    BEAM_FRAG,
    BEAM_LAVA,
    BEAM_ICE,
    BEAM_DEVASTATION,
    BEAM_RANDOM,                  // currently translates into FIRE..ACID
    BEAM_CHAOS,
    BEAM_UNRAVELLED_MAGIC,
    BEAM_ROD_FIRE,
    BEAM_ROD_COLD,
    BEAM_ROD_ELEC,

    // Enchantments
    BEAM_SLOW,
    BEAM_FIRST_ENCHANTMENT = BEAM_SLOW,
    BEAM_HASTE,
    BEAM_MIGHT,
    BEAM_HEALING,
    BEAM_PARALYSIS,
    BEAM_CONFUSION,
    BEAM_INVISIBILITY,
    BEAM_DIGGING,
    BEAM_TELEPORT,
    BEAM_POLYMORPH,
    BEAM_MALMUTATE,
    BEAM_ENSLAVE,
    BEAM_BANISH,
    BEAM_PAIN,
    BEAM_DISPEL_UNDEAD,
    BEAM_DISINTEGRATION,
    BEAM_BLINK,
    BEAM_BLINK_CLOSE,
    BEAM_BECKONING,
    BEAM_PETRIFY,
    BEAM_CORONA,
    BEAM_PORKALATOR,
    BEAM_HIBERNATION,
    BEAM_BERSERK,
    BEAM_SLEEP,
    BEAM_INNER_FLAME,
    BEAM_SENTINEL_MARK,
    BEAM_DIMENSION_ANCHOR,
    BEAM_VULNERABILITY,
    BEAM_MALIGN_OFFERING,
    BEAM_VIRULENCE,
    BEAM_AGILITY,
    BEAM_SAP_MAGIC,
    BEAM_DRAIN_MAGIC,
    BEAM_TUKIMAS_DANCE,
    BEAM_RESISTANCE,
    BEAM_UNRAVELLING,
    BEAM_SHARED_PAIN,
    BEAM_IRRESISTIBLE_CONFUSION,
    BEAM_INFESTATION,
    BEAM_CIGOTUVIS_PLAGUE,
    BEAM_AGONY,
    BEAM_VILE_CLUTCH,
    BEAM_LAST_ENCHANTMENT = BEAM_VILE_CLUTCH,

    BEAM_MEPHITIC,
    BEAM_POTION_POISON,
    BEAM_POTION_MIASMA,
    BEAM_POTION_STEAM,
    BEAM_POTION_FIRE,
    BEAM_POTION_COLD,
    BEAM_POTION_BLUE_SMOKE, 
    BEAM_POTION_BLACK_SMOKE,
    BEAM_POTION_GREY_SMOKE,
    BEAM_POTION_PURPLE_SMOKE,
    BEAM_POTION_RAIN,
    BEAM_POTION_MUTAGENIC,
    BEAM_POTION_RANDOM,
    BEAM_AIR,
    BEAM_PETRIFYING_CLOUD,
    BEAM_ENSNARE,
    BEAM_CRYSTAL,
    BEAM_DEATH_RATTLE,
    BEAM_LAST_REAL = BEAM_DEATH_RATTLE,

    // For getting the visual effect of a beam.
    BEAM_VISUAL,
    BEAM_BOUNCY_TRACER,           // Used for random bolt tracer (bounces as
                                  // crystal bolt, but irresistible).

    BEAM_TORMENT_DAMAGE,          // Pseudo-beam for damage flavour.
    BEAM_FIRST_PSEUDO = BEAM_TORMENT_DAMAGE,

    NUM_BEAMS
};
