#ifndef MON_SPLL_H
#define MON_SPLL_H


/* *********************************************************************

    This file determines which spells are contained in monster spellbooks.
    It is used by /util/gen-mst.pl to generate spellbook enums, which are
    listed in mon-mst.h.

    Template Format:

    {    mon_spellbook_type,
        {
             bolt spell,
             enchantment,
             self-enchantment,          // 50% tried after others fail
             misc(1) spell,
             misc(2) spell,             // SPELL_DIG must be here to work!
             emergency spell            // only when fleeing
        }
    }

********************************************************************* */


    {  MST_ORC_WIZARD_I,
      {
       SPELL_MAGIC_DART,
       SPELL_SLOW,
       SPELL_HASTE,
       SPELL_MAGIC_DART,
       SPELL_BLINK,
       SPELL_BLINK
      }
    },

    {  MST_ORC_WIZARD_II,
      {
       SPELL_THROW_FLAME,
       SPELL_CONFUSE,
       SPELL_INVISIBILITY,
       SPELL_MAGIC_DART,
       SPELL_NO_SPELL,
       SPELL_CONFUSE
      }
    },

    {  MST_ORC_WIZARD_III,
      {
       SPELL_THROW_FROST,
       SPELL_CANTRIP,
       SPELL_HASTE,
       SPELL_THROW_FLAME,
       SPELL_MAGIC_DART,
       SPELL_INVISIBILITY
      }
    },

#if TAG_MAJOR_VERSION == 34
    {  MST_DEEP_DWARF_NECROMANCER,
      {
       SPELL_PAIN,
       SPELL_DISPEL_UNDEAD,
       SPELL_SIMULACRUM,
       SPELL_VAMPIRIC_DRAINING,
       SPELL_AGONY,
       SPELL_VAMPIRIC_DRAINING
      }
    },
#endif

    {  MST_UNBORN,
      {
       SPELL_AGONY,
       SPELL_DISPEL_UNDEAD,
       SPELL_INJURY_MIRROR,
       SPELL_ANIMATE_DEAD,
       SPELL_HAUNT,
       SPELL_INJURY_MIRROR
      }
    },


    {  MST_BK_TROG,
      {
       SPELL_BROTHERS_IN_ARMS,
       SPELL_NO_SPELL,
       SPELL_TROGS_HAND,
       SPELL_BERSERKER_RAGE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_BK_YREDELEMNUL,
      {
       SPELL_NO_SPELL,
       SPELL_ANIMATE_DEAD,
       SPELL_INJURY_MIRROR,
       SPELL_NO_SPELL,
       SPELL_DRAIN_LIFE,
       SPELL_DRAIN_LIFE
      }
    },

    {  MST_GUARDIAN_SERPENT,
      {
       SPELL_TELEPORT_OTHER,
       SPELL_TELEPORT_OTHER,
       SPELL_MINOR_HEALING,
       SPELL_VENOM_BOLT,
       SPELL_SLOW,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_LICH_I,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_PARALYSE,
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_ANIMATE_DEAD,
       SPELL_IRON_SHOT,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_LICH_II,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_CONFUSE,
       SPELL_HASTE,
       SPELL_BOLT_OF_DRAINING,
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_BANISHMENT
      }
    },

    {  MST_LICH_III,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_SLOW,
       SPELL_INVISIBILITY,
       SPELL_THROW_ICICLE,
       SPELL_LEHUDIBS_CRYSTAL_SPEAR,
       SPELL_INVISIBILITY
      }
    },

    {  MST_LICH_IV,
      {
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_BOLT_OF_COLD,
       SPELL_INVISIBILITY,
       SPELL_ANIMATE_DEAD,
       SPELL_IOOD,
       SPELL_INVISIBILITY
      }
    },

    {  MST_HELLION,
      {
       SPELL_HELLFIRE_BURST,
       SPELL_HELLFIRE_BURST,
       SPELL_NO_SPELL,
       SPELL_HELLFIRE_BURST,
       SPELL_HELLFIRE_BURST,
       SPELL_HELLFIRE_BURST
      }
    },

    {  MST_VAMPIRE,
      {
       SPELL_VAMPIRIC_DRAINING,
       SPELL_CONFUSE,
       SPELL_INVISIBILITY,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_VAMPIRIC_DRAINING,
      }
    },

    {  MST_VAMPIRE_KNIGHT,
      {
       SPELL_BLINK_CLOSE,
       SPELL_PARALYSE,
       SPELL_HASTE,
       SPELL_INVISIBILITY,
       SPELL_NO_SPELL,
       SPELL_VAMPIRIC_DRAINING
      }
    },

    {  MST_VAMPIRE_MAGE,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_SUMMON_UNDEAD,
       SPELL_INVISIBILITY,
       SPELL_VAMPIRIC_DRAINING,
       SPELL_ANIMATE_DEAD,
       SPELL_INVISIBILITY
      }
    },

    {  MST_EFREET,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_FIREBALL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_BRAIN_WORM,
      {
       SPELL_BRAIN_FEED,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BRAIN_FEED,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_GIANT_ORANGE_BRAIN,
      {
       SPELL_BRAIN_FEED,
       SPELL_CAUSE_FEAR,
       SPELL_SHADOW_CREATURES,
       SPELL_CONFUSE,
       SPELL_BLINK,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_RAKSHASA,
      {
       SPELL_FAKE_RAKSHASA_SUMMON,
       SPELL_BLINK,
       SPELL_INVISIBILITY,
       SPELL_FAKE_RAKSHASA_SUMMON,
       SPELL_BLINK,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_GREAT_ORB_OF_EYES,
      {
       SPELL_PARALYSE,
       SPELL_DISINTEGRATE,
       SPELL_NO_SPELL,
       SPELL_SLOW,
       SPELL_CONFUSE,
       SPELL_TELEPORT_OTHER
      }
    },

    {  MST_KRAKEN,
      {
       SPELL_CREATE_TENTACLES,
       SPELL_CREATE_TENTACLES,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_INK_CLOUD
      }
    },

    {  MST_ORC_SORCERER,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_BOLT_OF_DRAINING,
       SPELL_SUMMON_DEMON,
       SPELL_PARALYSE,
       SPELL_ANIMATE_DEAD,
       SPELL_NO_SPELL,
      }
    },

    {  MST_STEAM_DRAGON,
      {
       SPELL_STEAM_BALL,
       SPELL_STEAM_BALL,
       SPELL_NO_SPELL,
       SPELL_STEAM_BALL,
       SPELL_STEAM_BALL,
       SPELL_NO_SPELL
      }
    },

    {  MST_HELL_KNIGHT_I,
      {
       SPELL_PAIN,
       SPELL_THROW_FLAME,
       SPELL_HASTE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_HASTE
      }
    },

    {  MST_HELL_KNIGHT_II,
      {
       SPELL_PAIN,
       SPELL_BOLT_OF_FIRE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BLINK
      }
    },

    {  MST_NECROMANCER_I,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_AGONY,
       SPELL_NO_SPELL,
       SPELL_ANIMATE_DEAD,
       SPELL_SIMULACRUM,
       SPELL_NO_SPELL
      }
    },

    {  MST_NECROMANCER_II,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_AGONY,
       SPELL_INVISIBILITY,
       SPELL_ANIMATE_DEAD,
       SPELL_ANIMATE_DEAD,
       SPELL_INVISIBILITY
      }
    },

    {  MST_WIZARD_I,
      {
       SPELL_MAGIC_DART,
       SPELL_PARALYSE,
       SPELL_HASTE,
       SPELL_LIGHTNING_BOLT,
       SPELL_CONFUSE,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_WIZARD_II,
      {
       SPELL_VENOM_BOLT,
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_INVISIBILITY,
       SPELL_CONFUSE,
       SPELL_SLOW,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_WIZARD_III,
      {
       SPELL_NO_SPELL,
       SPELL_LEHUDIBS_CRYSTAL_SPEAR,
       SPELL_BLINK,
       SPELL_BOLT_OF_FIRE,
       SPELL_BOLT_OF_COLD,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_WIZARD_IV,
      {
       SPELL_STONE_ARROW,
       SPELL_STING,
       SPELL_BLINK,
       SPELL_LIGHTNING_BOLT,
       SPELL_BANISHMENT,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_WIZARD_V,
      {
       SPELL_PARALYSE,
       SPELL_THROW_FLAME,
       SPELL_INVISIBILITY,
       SPELL_TELEPORT_OTHER,
       SPELL_FIREBALL,
       SPELL_TELEPORT_OTHER
      }
    },

    {  MST_ORC_PRIEST,
      {
       SPELL_PAIN,
       SPELL_NO_SPELL,
       SPELL_CANTRIP,
       SPELL_SMITING,
       SPELL_HEAL_OTHER,
       SPELL_NO_SPELL
      }
    },

    {  MST_ORC_HIGH_PRIEST,
      {
       SPELL_PAIN,
       SPELL_SUMMON_DEMON,
       SPELL_SUMMON_DEMON,
       SPELL_SMITING,
       SPELL_HEAL_OTHER,
       SPELL_MINOR_HEALING,
      }
    },

    {  MST_MOTTLED_DRAGON,
      {
       SPELL_STICKY_FLAME_SPLASH,
       SPELL_STICKY_FLAME_SPLASH,
       SPELL_NO_SPELL,
       SPELL_STICKY_FLAME_SPLASH,
       SPELL_STICKY_FLAME_SPLASH,
       SPELL_NO_SPELL
      }
    },

    {  MST_FIEND,
      {
       SPELL_HELLFIRE,
       SPELL_HELLFIRE,
       SPELL_NO_SPELL,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_MELEE,
       SPELL_NO_SPELL
      }
    },

    {  MST_HELL_SENTINEL,
      {
       SPELL_HELLFIRE,
       SPELL_HELLFIRE,
       SPELL_MELEE,
       SPELL_IRON_SHOT,
       SPELL_MELEE,
       SPELL_NO_SPELL
      }
    },

    {  MST_ICE_FIEND,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_BOLT_OF_COLD,
       SPELL_NO_SPELL,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_SHADOW_FIEND,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_BOLT_OF_DRAINING,
       SPELL_NO_SPELL,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_DISPEL_UNDEAD,
       SPELL_NO_SPELL
      }
    },

    {  MST_TORMENTOR,
      {
       SPELL_PAIN,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_PAIN,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_SYMBOL_OF_TORMENT
      }
    },

    {  MST_STORM_DRAGON,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_LIGHTNING_BOLT,
       SPELL_NO_SPELL,
       SPELL_LIGHTNING_BOLT,
       SPELL_LIGHTNING_BOLT,
       SPELL_NO_SPELL
      }
    },

    {  MST_WHITE_IMP,
      {
       SPELL_THROW_FROST,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_YNOXINUL,
      {
       SPELL_NO_SPELL,
       SPELL_IRON_SHOT,
       SPELL_SUMMON_UFETUBUS,
       SPELL_NO_SPELL,
       SPELL_SUMMON_UFETUBUS,
       SPELL_NO_SPELL
      }
    },

    {  MST_NEQOXEC,
      {
       SPELL_MALMUTATE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BRAIN_FEED,
       SPELL_SUMMON_MINOR_DEMON,
       SPELL_NO_SPELL
      }
    },

    {  MST_HELLWING,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_TELEPORT_OTHER,
       SPELL_ANIMATE_DEAD,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_SMOKE_DEMON,
      {
       SPELL_STICKY_FLAME_RANGE,
       SPELL_STEAM_BALL,
       SPELL_NO_SPELL,
       SPELL_SMITING,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_CACODEMON,
      {
       SPELL_ENERGY_BOLT,
       SPELL_SLOW,
       SPELL_CONFUSE,
       SPELL_MALMUTATE,
       SPELL_DIG,
       SPELL_NO_SPELL
      }
    },

    {  MST_GREEN_DEATH,
      {
       SPELL_POISON_ARROW,
       SPELL_POISONOUS_CLOUD,
       SPELL_NO_SPELL,
       SPELL_VENOM_BOLT,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_BALRUG,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_FIREBALL,
       SPELL_NO_SPELL,
       SPELL_STICKY_FLAME_RANGE,
       SPELL_SMITING,
       SPELL_NO_SPELL
      }
    },

    {  MST_BLIZZARD_DEMON,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_FREEZING_CLOUD,
       SPELL_NO_SPELL,
       SPELL_AIRSTRIKE,
       SPELL_BOLT_OF_COLD,
       SPELL_NO_SPELL
      }
    },

    {  MST_TITAN,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_NO_SPELL,
       SPELL_MINOR_HEALING,
       SPELL_AIRSTRIKE,
       SPELL_NO_SPELL,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_GOLDEN_DRAGON,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_BOLT_OF_COLD,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_POISONOUS_CLOUD,
       SPELL_NO_SPELL
      }
    },

    {  MST_DEEP_ELF_SUMMONER,
      {
       SPELL_BLINK,
       SPELL_SUMMON_VERMIN,
       SPELL_NO_SPELL,
       SPELL_SUMMON_VERMIN,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_DEEP_ELF_CONJURER,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_BOLT_OF_COLD,
       SPELL_NO_SPELL,
       SPELL_LIGHTNING_BOLT,
       SPELL_BOLT_OF_DRAINING,
       SPELL_NO_SPELL
      }
    },

    {  MST_DEEP_ELF_PRIEST,
      {
       SPELL_PAIN,
       SPELL_MALIGN_OFFERING,
       SPELL_NO_SPELL,
       SPELL_SMITING,
       SPELL_TWISTED_RESURRECTION,
       SPELL_NO_SPELL
      }
    },

    {  MST_DEEP_ELF_HIGH_PRIEST,
      {
       SPELL_SUMMON_DEMON,
       SPELL_HELLFIRE_BURST,
       SPELL_MALIGN_OFFERING,
       SPELL_SMITING,
       SPELL_TWISTED_RESURRECTION,
       SPELL_NO_SPELL
      }
    },

    {  MST_DEEP_ELF_DEMONOLOGIST,
      {
       SPELL_SUMMON_DEMON,
       SPELL_BANISHMENT,
       SPELL_SUMMON_DEMON,
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_SUMMON_MINOR_DEMON,
       SPELL_NO_SPELL
      }
    },

    {  MST_DEEP_ELF_ANNIHILATOR,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_LEHUDIBS_CRYSTAL_SPEAR,
       SPELL_BLINK,
       SPELL_IRON_SHOT,
       SPELL_POISON_ARROW,
       SPELL_BLINK
      }
    },

    {  MST_DEEP_ELF_SORCERER,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_BANISHMENT,
       SPELL_HASTE,
       SPELL_SUMMON_DEMON,
       SPELL_HELLFIRE,
       SPELL_HASTE
      }
    },

    {  MST_DEEP_ELF_DEATH_MAGE,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_BOLT_OF_DRAINING,
       SPELL_VAMPIRIC_DRAINING,
       SPELL_CALL_LOST_SOUL,
       SPELL_TWISTED_RESURRECTION,
       SPELL_VAMPIRIC_DRAINING
      }
    },

    {  MST_KOBOLD_DEMONOLOGIST,
      {
       SPELL_SUMMON_MINOR_DEMON,
       SPELL_SUMMON_DEMON,
       SPELL_CANTRIP,
       SPELL_SUMMON_MINOR_DEMON,
       SPELL_SUMMON_DEMON,
       SPELL_CANTRIP           // this should be cute -- bw
      },
    },

    {  MST_NAGA,
      {
       SPELL_SPIT_POISON,
       SPELL_SPIT_POISON,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_NAGA_MAGE,
      {
       SPELL_VENOM_BOLT,
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_HASTE,
       SPELL_POISON_ARROW,
       SPELL_TELEPORT_OTHER,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_CURSE_SKULL,
      {
       SPELL_SUMMON_UNDEAD,
       SPELL_SUMMON_UNDEAD,
       SPELL_NO_SPELL,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_SUMMON_UNDEAD,
       SPELL_NO_SPELL
      }
    },

    {  MST_SHINING_EYE,
      {
       SPELL_MALMUTATE,
       SPELL_MALMUTATE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_FROST_GIANT,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_BOLT_OF_COLD,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_ANGEL,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_MINOR_HEALING,
       SPELL_NO_SPELL,
       SPELL_MINOR_HEALING,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_DAEVA,
      {
       SPELL_SMITING,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_SMITING,
       SPELL_SMITING,
       SPELL_NO_SPELL
      }
    },

    {  MST_SHADOW_DRAGON,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_BOLT_OF_DRAINING,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BOLT_OF_DRAINING,
       SPELL_NO_SPELL
      }
    },

    {  MST_SPHINX,
      {
       SPELL_CONFUSE,
       SPELL_PARALYSE,
       SPELL_MINOR_HEALING,
       SPELL_SMITING,
       SPELL_SLOW,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_MUMMY,
      {
       SPELL_SUMMON_DEMON,
       SPELL_SMITING,
       SPELL_NO_SPELL,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_SUMMON_UNDEAD,
       SPELL_SUMMON_UNDEAD
      }
    },

    {  MST_ELECTRIC_GOLEM,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_LIGHTNING_BOLT,
       SPELL_BLINK,
       SPELL_LIGHTNING_BOLT,
       SPELL_LIGHTNING_BOLT,
       SPELL_BLINK
      }
    },

    {  MST_ORB_OF_FIRE,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_BOLT_OF_FIRE,
       SPELL_NO_SPELL,
       SPELL_MALMUTATE,
       SPELL_FIREBALL,
       SPELL_FIREBALL
      }
    },

    {  MST_SHADOW_IMP,
      {
       SPELL_PAIN,
       SPELL_NO_SPELL,
       SPELL_ANIMATE_DEAD,
       SPELL_ANIMATE_DEAD,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_HELL_HOG,
      {
       SPELL_STICKY_FLAME_SPLASH,
       SPELL_STICKY_FLAME_SPLASH,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_SWAMP_DRAGON,
      {
       SPELL_POISONOUS_CLOUD,
       SPELL_POISONOUS_CLOUD,
       SPELL_NO_SPELL,
       SPELL_POISONOUS_CLOUD,
       SPELL_POISONOUS_CLOUD,
       SPELL_NO_SPELL
      }
    },

    {  MST_SWAMP_DRAKE,
      {
       SPELL_NOXIOUS_CLOUD,
       SPELL_NOXIOUS_CLOUD,
       SPELL_NO_SPELL,
       SPELL_NOXIOUS_CLOUD,
       SPELL_NOXIOUS_CLOUD,
       SPELL_NO_SPELL
      }
    },

    {  MST_SERPENT_OF_HELL_GEHENNA,
      {
       SPELL_HELLFIRE,
       SPELL_FIRE_BREATH,
       SPELL_NO_SPELL,
       SPELL_HELLFIRE,
       SPELL_FIRE_BREATH,
       SPELL_NO_SPELL
      }
    },

    {  MST_SERPENT_OF_HELL_COCYTUS,
      {
       SPELL_OZOCUBUS_REFRIGERATION,
       SPELL_COLD_BREATH,
       SPELL_NO_SPELL,
       SPELL_OZOCUBUS_REFRIGERATION,
       SPELL_COLD_BREATH,
       SPELL_NO_SPELL
      }
    },

    {  MST_SERPENT_OF_HELL_DIS,
      {
       SPELL_METAL_SPLINTERS,
       SPELL_QUICKSILVER_BOLT,
       SPELL_NO_SPELL,
       SPELL_METAL_SPLINTERS,
       SPELL_QUICKSILVER_BOLT,
       SPELL_NO_SPELL
      }
    },

    {  MST_SERPENT_OF_HELL_TARTARUS,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_MIASMA_BREATH,
       SPELL_NO_SPELL,
       SPELL_BOLT_OF_DRAINING,
       SPELL_MIASMA_BREATH,
       SPELL_NO_SPELL
      }
    },

    {  MST_BOGGART,
      {
       SPELL_CONFUSE,
       SPELL_SLOW,
       SPELL_INVISIBILITY,
       SPELL_BLINK,
       SPELL_SHADOW_CREATURES,
       SPELL_SHADOW_CREATURES
      }
    },

    {  MST_EYE_OF_DEVASTATION,
      {
       SPELL_ENERGY_BOLT,
       SPELL_ENERGY_BOLT,
       SPELL_NO_SPELL,
       SPELL_ENERGY_BOLT,
       SPELL_ENERGY_BOLT,
       SPELL_NO_SPELL
      }
    },

    {  MST_QUICKSILVER_DRAGON,
      {
       SPELL_QUICKSILVER_BOLT,
       SPELL_QUICKSILVER_BOLT,
       SPELL_NO_SPELL,
       SPELL_QUICKSILVER_BOLT,
       SPELL_QUICKSILVER_BOLT,
       SPELL_NO_SPELL
      }
    },

    {  MST_IRON_DRAGON,
      {
       SPELL_METAL_SPLINTERS,
       SPELL_METAL_SPLINTERS,
       SPELL_NO_SPELL,
       SPELL_METAL_SPLINTERS,
       SPELL_METAL_SPLINTERS,
       SPELL_NO_SPELL
      }
    },

    {  MST_NORRIS,
      {
       SPELL_BRAIN_FEED,
       SPELL_SMITING,
       SPELL_INVISIBILITY,
       SPELL_CONFUSE,
       SPELL_PARALYSE,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_DEATH_DRAKE,
      {
       SPELL_MIASMA_BREATH,
       SPELL_MIASMA_BREATH,
       SPELL_NO_SPELL,
       SPELL_MIASMA_BREATH,
       SPELL_MIASMA_BREATH,
       SPELL_NO_SPELL
      }
    },

    {  MST_DRAC_SCORCHER,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_STICKY_FLAME_RANGE,
       SPELL_NO_SPELL,
       SPELL_FIREBALL,
       SPELL_HELLFIRE,
       SPELL_HELLFIRE_BURST
      }
    },

    {  MST_DRAC_CALLER,
      {
       SPELL_NO_SPELL,
       SPELL_SUMMON_DRAKES,
       SPELL_SUMMON_DRAKES,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_SUMMON_DRAKES
      }
    },

    {  MST_DRAC_SHIFTER,
      {
       SPELL_DIMENSION_ANCHOR,
       SPELL_BLINK_OTHER,
       SPELL_BLINK_RANGE,
       SPELL_BLINK_ALLIES_ENCIRCLE,
       SPELL_NO_SPELL,
       SPELL_BLINK_AWAY
      }
    },

    // Curse toe menu should be kept full, because otherwise the toe spends
    // too much time crawling around.
    {  MST_CURSE_TOE,
      {
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_SUMMON_MUSHROOMS, // fungal theme
       SPELL_SUMMON_MUSHROOMS,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_SUMMON_MUSHROOMS,
       SPELL_NO_SPELL
      }
    },

    {  MST_ICE_STATUE,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_THROW_ICICLE,
       SPELL_NO_SPELL,
       SPELL_FREEZING_CLOUD,
       SPELL_SUMMON_ICE_BEAST,
       SPELL_SUMMON_ICE_BEAST // in Zotdef forced off
      }
    },

    // fake spell for monsters berserking when fleeing
    {  MST_BERSERK_ESCAPE,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BERSERKER_RAGE
      }
    },

    {  MST_MERFOLK_AQUAMANCER,
      {
        SPELL_PRIMAL_WAVE,
        SPELL_STEAM_BALL,
        SPELL_NO_SPELL,
        SPELL_THROW_ICICLE,
        SPELL_NO_SPELL,
        SPELL_BLINK
      }
    },

    {  MST_ALLIGATOR,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_SWIFTNESS,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    // ---------------------
    // uniques' "spellbooks"
    // ---------------------

    // Jessica is now a bit more evil-themed than before.
    {  MST_JESSICA,
      {
       SPELL_PAIN, // Formerly Magic Dart.
       SPELL_SLOW,
       SPELL_HASTE,
       SPELL_PAIN, // Formerly Magic Dart.
       SPELL_BLINK,
       SPELL_BLINK
      }
    },

    {  MST_RUPERT,
      {
       SPELL_PARALYSE,
       SPELL_CONFUSE,
       SPELL_BERSERKER_RAGE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BERSERKER_RAGE
      }
    },

    // statue, earth spells
    // Blink Other for foes out of the other spells' range
    { MST_ROXANNE,
      {
       SPELL_LEHUDIBS_CRYSTAL_SPEAR,
       SPELL_IRON_SHOT,
       SPELL_BLINK_OTHER_CLOSE,
       SPELL_BOLT_OF_MAGMA,
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_NO_SPELL
      }
    },

    {  MST_SONJA,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BLINK,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_TELEPORT_SELF
      }
    },

    { MST_MENKAURE,
      {
       SPELL_PAIN,
       SPELL_NO_SPELL,
       SPELL_HASTE,
       SPELL_NO_SPELL,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_NO_SPELL
      }
    },

    { MST_EUSTACHIO,
      {
       SPELL_SUMMON_SMALL_MAMMAL,
       SPELL_SUMMON_MINOR_DEMON,
       SPELL_BLINK,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BLINK
      }
    },

    { MST_ILSUIW,
      {
       SPELL_THROW_ICICLE,
       SPELL_CALL_TIDE,
       SPELL_INVISIBILITY,
       SPELL_BLINK,
       SPELL_WATER_ELEMENTALS,
       SPELL_WATER_ELEMENTALS
      }
    },

    {  MST_PRINCE_RIBBIT,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_NESSOS,
      {
       SPELL_NO_SPELL,
       SPELL_BLINK_RANGE,
       SPELL_HASTE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_KIRKE,
      {
       SPELL_PORKALATOR,
       SPELL_SLOW,
       SPELL_SUMMON_UGLY_THING,
       SPELL_PORKALATOR,
       SPELL_CORONA,
       SPELL_INVISIBILITY
      }
    },

    {  MST_DOWAN,
      {
       SPELL_THROW_FROST,
       SPELL_CORONA,
       SPELL_BLINK,
       SPELL_THROW_FLAME,
       SPELL_HASTE_OTHER,
       SPELL_MINOR_HEALING,
      }
    },

    {  MST_FANNAR,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_OZOCUBUS_REFRIGERATION,
       SPELL_SUMMON_ICE_BEAST,
       SPELL_SUMMON_ICE_BEAST,
       SPELL_OZOCUBUS_REFRIGERATION,
       SPELL_BLINK,
      }
    },

    {  MST_GASTRONOK,
      {
       SPELL_AIRSTRIKE,
       SPELL_SLOW,
       SPELL_SWIFTNESS,
       SPELL_SUMMON_SMALL_MAMMAL,
       SPELL_CANTRIP,
       SPELL_AIRSTRIKE,
      }
    },

    {  MST_MAURICE,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_INVISIBILITY,
       SPELL_SWIFTNESS,
       SPELL_BLINK,
       SPELL_TELEPORT_SELF,
      }
    },

    // demon lords
    {  MST_GERYON,
      {
       SPELL_SUMMON_HELL_BEAST,
       SPELL_SUMMON_HELL_BEAST,
       SPELL_NO_SPELL,
       SPELL_SUMMON_HELL_BEAST,
       SPELL_NO_SPELL,
       SPELL_SUMMON_HELL_BEAST
      }
    },

    {  MST_DISPATER,
      {
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_LEHUDIBS_CRYSTAL_SPEAR,
       SPELL_NO_SPELL,
       SPELL_IRON_SHOT,
       SPELL_HELLFIRE,
       SPELL_SUMMON_GREATER_DEMON
      }
    },

    {  MST_ASMODEUS,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_HELLFIRE,
       SPELL_NO_SPELL,
       SPELL_FIRE_SUMMON,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_ERESHKIGAL,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_SILENCE,
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_PARALYSE,
       SPELL_MAJOR_HEALING
      }
    },

    {  MST_ANTAEUS,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_LIGHTNING_BOLT,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_MNOLEG,
      {
       SPELL_SUMMON_EYEBALLS,
       SPELL_SMITING,
       SPELL_MALIGN_GATEWAY,
       SPELL_SUMMON_HORRIBLE_THINGS,
       SPELL_SUMMON_HORRIBLE_THINGS,
       SPELL_NO_SPELL
      }
    },

    {  MST_LOM_LOBON,
      {
       SPELL_ICE_STORM,
       SPELL_CONJURE_BALL_LIGHTNING,
       SPELL_MAJOR_HEALING,
       SPELL_TORNADO,
       SPELL_BLINK_RANGE,
       SPELL_TORNADO
      }
    },

    {  MST_CEREBOV,
      {
       SPELL_FIRE_STORM,
       SPELL_IRON_SHOT,
       SPELL_HASTE,
       SPELL_HASTE,
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_HASTE
      }
    },

    {  MST_GLOORX_VLOQ,
      {
       SPELL_POISON_ARROW,
       SPELL_MIASMA_BREATH,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_BOLT_OF_DRAINING,
       SPELL_DISPEL_UNDEAD,
       SPELL_NO_SPELL
      }
    },

    {  MST_KHUFU,
      {
       SPELL_SUMMON_DEMON,
       SPELL_SMITING,
       SPELL_SMITING,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_SUMMON_UNDEAD,
       SPELL_TOMB_OF_DOROKLOHE
      }
    },

    {  MST_NIKOLA,
      {
       SPELL_SHOCK,
       SPELL_CHAIN_LIGHTNING,
       SPELL_BLINK,
       SPELL_LIGHTNING_BOLT,
       SPELL_CHAIN_LIGHTNING,
       SPELL_BLINK
      }
    },

    {  MST_DISSOLUTION,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_SUMMON_EYEBALLS,
       SPELL_NO_SPELL,
       SPELL_SUMMON_EYEBALLS
      }
    },

    {  MST_AIZUL,
      {
       SPELL_VENOM_BOLT,
       SPELL_SLEEP,
       SPELL_NO_SPELL,
       SPELL_POISON_ARROW,
       SPELL_VENOM_BOLT,
       SPELL_NO_SPELL
      }
    },

    {  MST_EXECUTIONER,
      {
       SPELL_NO_SPELL,
       SPELL_PAIN,
       SPELL_HASTE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_HASTE
      }
    },

    {  MST_IGNACIO,
      {
       SPELL_AGONY,
       SPELL_PAIN,
       SPELL_HASTE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_HASTE
      }
    },

    {  MST_HAROLD,
      {
       SPELL_NO_SPELL,
       SPELL_BOLT_OF_FIRE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BLINK
      }
    },

    {  MST_MARA,
      {
       SPELL_MISLEAD,
       SPELL_BOLT_OF_FIRE,
       SPELL_FAKE_MARA_SUMMON,
       SPELL_SUMMON_ILLUSION,
       SPELL_PAIN,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_MARA_FAKE,
      {
       SPELL_MISLEAD,
       SPELL_BOLT_OF_FIRE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_PAIN,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_BORIS,
      {
       SPELL_IRON_SHOT,
       SPELL_BOLT_OF_COLD,
       SPELL_INVISIBILITY,
       SPELL_ANIMATE_DEAD,
       SPELL_IOOD,
       SPELL_INVISIBILITY
      }
    },

    {  MST_FREDERICK,
      {
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_BOLT_OF_COLD,
       SPELL_INVISIBILITY,
       SPELL_NO_SPELL,
       SPELL_IRON_SHOT,
       SPELL_INVISIBILITY
      }
    },

    {  MST_BK_OKAWARU,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_MIGHT,
       SPELL_HASTE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_SPRIGGAN_DRUID,
      {
       SPELL_SUNRAY,
       SPELL_AWAKEN_FOREST,
       SPELL_HASTE_PLANTS,
       SPELL_DRUIDS_CALL,
       SPELL_HASTE_PLANTS,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_SPRIGGAN_AIR_MAGE,
      {
       SPELL_AIRSTRIKE,
       SPELL_LIGHTNING_BOLT,
       SPELL_CONTROL_WINDS,
       SPELL_SHOCK,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    { MST_THE_ENCHANTRESS,
      {
       SPELL_DIMENSION_ANCHOR,
       SPELL_SLOW,
       SPELL_HASTE,
       SPELL_MASS_CONFUSION,
       SPELL_STRIP_RESISTANCE,
       SPELL_INVISIBILITY
      }
    },

    {  MST_HELLEPHANT,
      {
       SPELL_FIRE_BREATH,
       SPELL_NO_SPELL,
       SPELL_BLINK,
       SPELL_FIRE_BREATH,
       SPELL_NO_SPELL,
       SPELL_BLINK
      }
    },

    {  MST_GRINDER,
      {
       SPELL_PAIN,
       SPELL_PARALYSE,
       SPELL_BLINK,
       SPELL_PAIN,
       SPELL_PARALYSE,
       SPELL_BLINK
      }
    },

    {  MST_IRON_ELEMENTAL,
      {
       SPELL_IRON_SHOT,
       SPELL_SLOW,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_METAL_SPLINTERS,
       SPELL_NO_SPELL
      }
    },

    {  MST_IRON_GIANT,
      {
       SPELL_IRON_SHOT,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_IRON_ELEMENTALS,
       SPELL_NO_SPELL
      }
    },

    {  MST_MENNAS,
      {
       SPELL_NO_SPELL,
       SPELL_CONFUSE,
       SPELL_SILENCE,
       SPELL_NO_SPELL,
       SPELL_MINOR_HEALING,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_NERGALLE,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_SUMMON_SPECTRAL_ORCS,
       SPELL_SUMMON_SPECTRAL_ORCS,
       SPELL_DISPEL_UNDEAD,
       SPELL_HASTE_OTHER,
       SPELL_DEATHS_DOOR
      }
    },

    {  MST_JUMPING_SPIDER,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BLINK_CLOSE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BLINK_AWAY
      }
    },

    {  MST_ORB_SPIDER,
      {
       SPELL_IOOD,
       SPELL_NO_SPELL,
       SPELL_IOOD,
       SPELL_IOOD,
       SPELL_NO_SPELL,
       SPELL_IOOD
      }
    },

    {  MST_PEARL_DRAGON,
      {
       SPELL_HOLY_BREATH,
       SPELL_HOLY_BREATH,
       SPELL_NO_SPELL,
       SPELL_HOLY_BREATH,
       SPELL_HOLY_BREATH,
       SPELL_NO_SPELL
      }
    },

    {  MST_OPHAN,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_HOLY_FLAMES,
       SPELL_NO_SPELL,
       SPELL_BOLT_OF_FIRE,
       SPELL_HOLY_FLAMES,
       SPELL_NO_SPELL
      }
    },

    {  MST_BOG_BODY,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_SLOW,
       SPELL_NO_SPELL,
       SPELL_BOLT_OF_COLD,
       SPELL_CORPSE_ROT,
       SPELL_NO_SPELL
      }
    },

    {  MST_JORY,
      {
       SPELL_LEHUDIBS_CRYSTAL_SPEAR,
       SPELL_MESMERISE,
       SPELL_BLINK_CLOSE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_VAMPIRIC_DRAINING,
      }
    },

    {  MST_SHEDU,
      {
       SPELL_HEAL_OTHER,
       SPELL_NO_SPELL,
       SPELL_HEAL_OTHER,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
      }
    },

    {  MST_EIDOLON,
      {
       SPELL_MELEE,
       SPELL_BOLT_OF_DRAINING,
       SPELL_NO_SPELL,
       SPELL_CAUSE_FEAR,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
      }
    },

    {  MST_BASILISK,
      {
       SPELL_NO_SPELL,
       SPELL_PETRIFY,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
      }
    },

    {  MST_CATOBLEPAS,
      {
       SPELL_PETRIFYING_CLOUD,
       SPELL_PETRIFYING_CLOUD,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_SHADOW_DEMON,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_SHADOW_CREATURES,
       SPELL_SUMMON_DEMON,
       SPELL_NO_SPELL
      }
    },

    {  MST_SILVER_STAR,
      {
       SPELL_HOLY_LIGHT,
       SPELL_HOLY_LIGHT,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_SILVER_BLAST,
       SPELL_SILVER_BLAST
      }
    },

    {  MST_GNOLL_SHAMAN,
      {
       SPELL_CORONA,
       SPELL_NO_SPELL,
       SPELL_HEAL_OTHER,
       SPELL_HASTE_OTHER,
       SPELL_HEAL_OTHER,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_ARACHNE,
      {
       SPELL_ENSNARE,
       SPELL_POISON_ARROW,
       SPELL_BLINK,
       SPELL_ENSNARE,
       SPELL_VENOM_BOLT,
       SPELL_ENSNARE
      }
    },

    {  MST_HELLBINDER,
      {
       SPELL_SUMMON_MINOR_DEMON,
       SPELL_SUMMON_DEMON,
       SPELL_HASTE,
       SPELL_HELLFIRE,
       SPELL_BLINK_AWAY,
       SPELL_NO_SPELL
      }
    },

    {  MST_CLOUD_MAGE,
      {
       SPELL_MEPHITIC_CLOUD,
       SPELL_FREEZING_CLOUD,
       SPELL_POISONOUS_CLOUD,
       SPELL_AIRSTRIKE,
       SPELL_BLINK_RANGE,
       SPELL_BLINK_RANGE
      }
    },

    {  MST_SOUL_EATER,
      {
       SPELL_DRAIN_LIFE,
       SPELL_MELEE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_WRETCHED_STAR,
      {
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_TENTACLED_STARSPAWN,
      {
       SPELL_CREATE_TENTACLES,
       SPELL_CREATE_TENTACLES,
       SPELL_CREATE_TENTACLES,
       SPELL_CREATE_TENTACLES,
       SPELL_CREATE_TENTACLES,
       SPELL_NO_SPELL
      }
    },

    {  MST_FRANCES,
      {
       SPELL_THROW_ICICLE,
       SPELL_SUMMON_DEMON,
       SPELL_HASTE,
       SPELL_IRON_SHOT,
       SPELL_SUMMON_DEMON,
       SPELL_HASTE
      }
    },

    {  MST_THRASHING_HORROR,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_FRENZY
      }
    },

    {  MST_JORGRUN,
      {
       SPELL_LRD,
       SPELL_PETRIFY,
       SPELL_NO_SPELL,
       SPELL_SHATTER,
       SPELL_DIG,
       SPELL_NO_SPELL
      }
    },

    {  MST_LAMIA,
      {
       SPELL_IOOD,
       SPELL_MESMERISE,
       SPELL_HASTE,
       SPELL_POISON_ARROW,
       SPELL_NO_SPELL,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_DEEP_TROLL_EARTH_MAGE,
      {
       SPELL_LRD,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_DIG,
       SPELL_NO_SPELL,
      }
    },

    {  MST_DEEP_TROLL_SHAMAN,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_HASTE_OTHER,
       SPELL_MIGHT_OTHER,
       SPELL_MINOR_HEALING,
      }
    },

    {  MST_VAULT_SENTINEL,
      {
       SPELL_SENTINEL_MARK,
       SPELL_SENTINEL_MARK,
       SPELL_NO_SPELL,
       SPELL_SENTINEL_MARK,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
      }
    },

    {  MST_IRONBRAND_CONVOKER,
      {
       SPELL_MIGHT_OTHER,
       SPELL_MIGHT_OTHER,
       SPELL_NO_SPELL,
       SPELL_WORD_OF_RECALL,
       SPELL_WORD_OF_RECALL,
       SPELL_NO_SPELL,
      }
    },

    {  MST_IRONHEART_PRESERVER,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_MINOR_HEALING,
       SPELL_INJURY_BOND,
       SPELL_NO_SPELL,
       SPELL_MINOR_HEALING,
      }
    },

    {  MST_TEST_SPAWNER,
      {
       SPELL_SHADOW_CREATURES,
       SPELL_SHADOW_CREATURES,
       SPELL_SHADOW_CREATURES,
       SPELL_SHADOW_CREATURES,
       SPELL_SHADOW_CREATURES,
       SPELL_SHADOW_CREATURES
      }
    },

    {  MST_ANCIENT_CHAMPION_I,
      {
       SPELL_IRON_SHOT,
       SPELL_NO_SPELL,
       SPELL_HASTE,
       SPELL_PAIN,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_ANCIENT_CHAMPION_II,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_NO_SPELL,
       SPELL_HASTE,
       SPELL_STONE_ARROW,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_ANCIENT_CHAMPION_III,
      {
       SPELL_VENOM_BOLT,
       SPELL_NO_SPELL,
       SPELL_ANIMATE_DEAD,
       SPELL_HAUNT,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_ANCIENT_CHAMPION_IV,
      {
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_SLOW,
       SPELL_HASTE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_REVENANT,
      {
       SPELL_GHOSTLY_FIREBALL,
       SPELL_GHOSTLY_FLAMES,
       SPELL_NO_SPELL,
       SPELL_DISPEL_UNDEAD,
       SPELL_NO_SPELL,
       SPELL_BLINK_AWAY
      }
    },

    {  MST_DRYAD,
      {
       SPELL_AWAKEN_VINES,
       SPELL_AWAKEN_VINES,
       SPELL_NO_SPELL,
       SPELL_AWAKEN_FOREST,
       SPELL_AWAKEN_FOREST,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_FAUN,
      {
       SPELL_CORONA,
       SPELL_NO_SPELL,
       SPELL_CORONA,
       SPELL_SLOW,
       SPELL_CONFUSE,
       SPELL_NO_SPELL
      }
    },

    {  MST_SATYR,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_CAUSE_FEAR,
       SPELL_CAUSE_FEAR,
       SPELL_SLEEP,
       SPELL_NO_SPELL
      }
    },

    {  MST_PAN,
      {
       SPELL_NO_SPELL,
       SPELL_MASS_CONFUSION,
       SPELL_NO_SPELL,
       SPELL_ENGLACIATION,
       SPELL_SLEEP,
       SPELL_BLINK
      }
    },

    {  MST_TENGU_CONJURER_I,
      {
       SPELL_FORCE_LANCE,
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_BATTLESPHERE,
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_BATTLESPHERE,
       SPELL_BLINK
      }
    },

    {  MST_TENGU_CONJURER_II,
      {
       SPELL_THROW_FLAME,
       SPELL_BOLT_OF_MAGMA,
       SPELL_BATTLESPHERE,
       SPELL_BOLT_OF_MAGMA,
       SPELL_BATTLESPHERE,
       SPELL_BLINK
      }
    },

    {  MST_TENGU_CONJURER_III,
      {
       SPELL_SHOCK,
       SPELL_LIGHTNING_BOLT,
       SPELL_BATTLESPHERE,
       SPELL_LIGHTNING_BOLT,
       SPELL_BATTLESPHERE,
       SPELL_BLINK
      }
    },

    {  MST_TENGU_CONJURER_IV,
      {
       SPELL_MEPHITIC_CLOUD,
       SPELL_VENOM_BOLT,
       SPELL_BATTLESPHERE,
       SPELL_VENOM_BOLT,
       SPELL_BATTLESPHERE,
       SPELL_BLINK
      }
    },

    {  MST_TENGU_REAVER_I,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_LIGHTNING_BOLT,
       SPELL_BATTLESPHERE,
       SPELL_FREEZING_CLOUD,
       SPELL_BATTLESPHERE,
       SPELL_NO_SPELL
      }
    },

    {  MST_TENGU_REAVER_II,
      {
       SPELL_BOLT_OF_MAGMA,
       SPELL_NO_SPELL,
       SPELL_BATTLESPHERE,
       SPELL_FIREBALL,
       SPELL_BATTLESPHERE,
       SPELL_NO_SPELL
      }
    },

    {  MST_TENGU_REAVER_III,
      {
       SPELL_VENOM_BOLT,
       SPELL_VENOM_BOLT,
       SPELL_BATTLESPHERE,
       SPELL_POISONOUS_CLOUD,
       SPELL_BATTLESPHERE,
       SPELL_NO_SPELL
      }
    },

    {  MST_SPRIGGAN_ENCHANTER,
      {
       SPELL_SLEEP,
       SPELL_SLEEP,
       SPELL_INVISIBILITY,
       SPELL_NO_SPELL,
       SPELL_STRIP_RESISTANCE,
       SPELL_NO_SPELL
      }
    },

    {  MST_SOJOBO,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_NO_SPELL,
       SPELL_WIND_BLAST,
       SPELL_AIRSTRIKE,
       SPELL_AIR_ELEMENTALS,
       SPELL_WIND_BLAST
      }
    },

    {  MST_WIND_DRAKE,
      {
       SPELL_AIRSTRIKE,
       SPELL_MELEE,
       SPELL_NO_SPELL,
       SPELL_AIRSTRIKE,
       SPELL_MELEE,
       SPELL_NO_SPELL
      }
    },

    {  MST_FORMICID,
      {
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_DIG,
       SPELL_SHAFT_SELF,
      }
    },

    {  MST_FORMICID_VENOM_MAGE,
      {
       SPELL_VENOM_BOLT,
       SPELL_POISON_ARROW,
       SPELL_CANTRIP,
       SPELL_STONE_ARROW,
       SPELL_DIG,
       SPELL_SHAFT_SELF,
      }
    },

    {  MST_THORN_HUNTER,
      {
       SPELL_THORN_VOLLEY,
       SPELL_THORN_VOLLEY,
       SPELL_NO_SPELL,
       SPELL_WALL_OF_BRAMBLES,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_WATER_NYMPH,
      {
       SPELL_WATERSTRIKE,
       SPELL_WATERSTRIKE,
       SPELL_NO_SPELL,
       SPELL_WATERSTRIKE,
       SPELL_WATERSTRIKE,
       SPELL_NO_SPELL
      }
    },

    {  MST_THORN_LOTUS,
      {
       SPELL_THORN_VOLLEY,
       SPELL_THORN_VOLLEY,
       SPELL_NO_SPELL,
       SPELL_THORN_VOLLEY,
       SPELL_THORN_VOLLEY,
       SPELL_NO_SPELL
      }
    },

    {  MST_VAPOUR,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_LIGHTNING_BOLT,
       SPELL_MELEE,
       SPELL_LIGHTNING_BOLT,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_ELEMENTAL_WELLSPRING,
      {
       SPELL_PRIMAL_WAVE,
       SPELL_PRIMAL_WAVE,
       SPELL_NO_SPELL,
       SPELL_PRIMAL_WAVE,
       SPELL_PRIMAL_WAVE,
       SPELL_NO_SPELL
      }
    },

    {  MST_DRACONIAN_ZEALOT,
      {
       SPELL_SUMMON_DEMON,
       SPELL_HELLFIRE_BURST,
       SPELL_MINOR_HEALING,
       SPELL_SMITING,
       SPELL_ANIMATE_DEAD,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_DEEP_ELF_MAGE_I,
      {
       SPELL_FREEZE,
       SPELL_THROW_ICICLE,
       SPELL_FREEZE,
       SPELL_SUMMON_ICE_BEAST,
       SPELL_THROW_ICICLE,
       SPELL_NO_SPELL
      }
    },

    {  MST_DEEP_ELF_MAGE_II,
      {
       SPELL_BOLT_OF_MAGMA,
       SPELL_STONE_ARROW,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_STONE_ARROW,
       SPELL_NO_SPELL
      }
    },

    {  MST_DEEP_ELF_MAGE_III,
      {
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_SLOW,
       SPELL_NO_SPELL,
       SPELL_VENOM_BOLT,
       SPELL_BLINK,
       SPELL_BLINK
      }
    },

    {  MST_DEEP_ELF_MAGE_IV,
      {
       SPELL_FLAME_TONGUE,
       SPELL_STICKY_FLAME_RANGE,
       SPELL_FIREBALL,
       SPELL_FIREBALL,
       SPELL_THROW_FLAME,
       SPELL_NO_SPELL
      }
    },

    {  MST_DEEP_ELF_MAGE_V,
      {
       SPELL_MAGIC_DART,
       SPELL_FORCE_LANCE,
       SPELL_BATTLESPHERE,
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_BATTLESPHERE,
       SPELL_NO_SPELL
      }
    },

    {  MST_IGNIS,
      {
       SPELL_CORONA,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },


#endif
