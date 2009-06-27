/*
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef MON_SPLL_H
#define MON_SPLL_H


/* *********************************************************************


    Template Format:

        {
             bolt spell,
             enchantment,
             self-enchantment,          // 50% tried after others fail
             misc(1) spell,
             misc(2) spell,             // SPELL_DIG must be here to work!
             emergency spell            // only when fleeing
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

    {  MST_GUARDIAN_NAGA,
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
       SPELL_BOLT_OF_IRON,
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
       SPELL_ANIMATE_DEAD,
       SPELL_SUMMON_UNDEAD,
       SPELL_THROW_FROST,
       SPELL_LEHUDIBS_CRYSTAL_SPEAR,
       SPELL_SUMMON_UNDEAD
      }
    },

    {  MST_LICH_IV,
      {
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_BOLT_OF_COLD,
       SPELL_INVISIBILITY,
       SPELL_ANIMATE_DEAD,
       SPELL_BOLT_OF_IRON,
       SPELL_INVISIBILITY
      }
    },

    {  MST_BURNING_DEVIL,
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
       SPELL_VAMPIRE_SUMMON,
       SPELL_CONFUSE,
       SPELL_INVISIBILITY,
       SPELL_NO_SPELL,
       SPELL_VAMPIRE_SUMMON,
       SPELL_VAMPIRE_SUMMON
      }
    },

    {  MST_VAMPIRE_KNIGHT,
      {
       SPELL_VAMPIRE_SUMMON,
       SPELL_PARALYSE,
       SPELL_HASTE,
       SPELL_INVISIBILITY,
       SPELL_VAMPIRE_SUMMON,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_VAMPIRE_MAGE,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_SUMMON_UNDEAD,
       SPELL_INVISIBILITY,
       SPELL_ANIMATE_DEAD,
       SPELL_ANIMATE_DEAD,
       SPELL_TELEPORT_SELF
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
       SPELL_POLYMORPH_OTHER,
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

    {  MST_ORC_SORCERER,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_BOLT_OF_DRAINING,
       SPELL_SUMMON_DEMON,
       SPELL_PARALYSE,
       SPELL_ANIMATE_DEAD,
       SPELL_TELEPORT_SELF
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
       SPELL_NO_SPELL,
       SPELL_PAIN,
       SPELL_HASTE,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_HASTE
      }
    },

    {  MST_HELL_KNIGHT_II,
      {
       SPELL_NO_SPELL,
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
       SPELL_BOLT_OF_DRAINING,
       SPELL_NO_SPELL,
       SPELL_ANIMATE_DEAD,
       SPELL_ANIMATE_DEAD,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_NECROMANCER_II,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_PAIN,
       SPELL_INVISIBILITY,
       SPELL_ANIMATE_DEAD,
       SPELL_ANIMATE_DEAD,
       SPELL_BLINK
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
       SPELL_PARALYSE,
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
       SPELL_NO_SPELL,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_ORC_HIGH_PRIEST,
      {
       SPELL_PAIN,
       SPELL_SUMMON_DEMON,
       SPELL_SUMMON_DEMON,
       SPELL_SMITING,
       SPELL_ANIMATE_DEAD,
       SPELL_MINOR_HEALING
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

    {  MST_ICE_FIEND,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_BOLT_OF_COLD,
       SPELL_NO_SPELL,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_NO_SPELL,
       SPELL_SUMMON_DEMON
      }
    },

    {  MST_SHADOW_FIEND,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_BOLT_OF_DRAINING,
       SPELL_NO_SPELL,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_DISPEL_UNDEAD,
       SPELL_SUMMON_DEMON
      }
    },

    {  MST_TORMENTOR,
      {
       SPELL_PAIN,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_PAIN,
       SPELL_NO_SPELL,
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
       SPELL_BOLT_OF_IRON,
       SPELL_SUMMON_UFETUBUS,
       SPELL_NO_SPELL,
       SPELL_SUMMON_UFETUBUS,
       SPELL_NO_SPELL
      }
    },

    {  MST_NEQOXEC,
      {
       SPELL_POLYMORPH_OTHER,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BRAIN_FEED,
       SPELL_CALL_IMP,
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
       SPELL_STICKY_FLAME,
       SPELL_STEAM_BALL,
       SPELL_NO_SPELL,
       SPELL_SMITING,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_CACODEMON,
      {
       SPELL_CALL_IMP,
       SPELL_CALL_IMP,
       SPELL_CALL_IMP,
       SPELL_POLYMORPH_OTHER,
       SPELL_DIG,
       SPELL_SUMMON_DEMON
      }
    },

    {  MST_GREEN_DEATH,
      {
       SPELL_POISON_ARROW,
       SPELL_POISONOUS_CLOUD,
       SPELL_NO_SPELL,
       SPELL_VENOM_BOLT,
       SPELL_CALL_IMP,
       SPELL_BLINK
      }
    },

    {  MST_BALRUG,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_FIREBALL,
       SPELL_NO_SPELL,
       SPELL_STICKY_FLAME,
       SPELL_SMITING,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_BLUE_DEATH,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_BOLT_OF_COLD,
       SPELL_NO_SPELL,
       SPELL_CALL_IMP,
       SPELL_SHADOW_CREATURES,
       SPELL_TELEPORT_OTHER
      }
    },

    {  MST_TITAN,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_NO_SPELL,
       SPELL_MINOR_HEALING,
       SPELL_NO_SPELL,
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
       SPELL_CALL_IMP,
       SPELL_SUMMON_UFETUBUS,
       SPELL_VAMPIRE_SUMMON,
       SPELL_SUMMON_DEMON,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_DEEP_ELF_CONJURER_I,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_BOLT_OF_COLD,
       SPELL_CANTRIP,
       SPELL_LIGHTNING_BOLT,
       SPELL_STICKY_FLAME,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_DEEP_ELF_CONJURER_II,
      {
       SPELL_STICKY_FLAME,
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_INVISIBILITY,
       SPELL_STONE_ARROW,
       SPELL_BOLT_OF_DRAINING,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_DEEP_ELF_PRIEST,
      {
       SPELL_PAIN,
       SPELL_CANTRIP,
       SPELL_MINOR_HEALING,
       SPELL_SMITING,
       SPELL_ANIMATE_DEAD,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_DEEP_ELF_HIGH_PRIEST,
      {
       SPELL_SUMMON_DEMON,
       SPELL_HELLFIRE_BURST,
       SPELL_MINOR_HEALING,
       SPELL_SMITING,
       SPELL_ANIMATE_DEAD,
       SPELL_MINOR_HEALING
      }
    },

    {  MST_DEEP_ELF_DEMONOLOGIST,
      {
       SPELL_SUMMON_DEMON,
       SPELL_BANISHMENT,
       SPELL_SUMMON_DEMON,
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_CALL_IMP,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_DEEP_ELF_ANNIHILATOR,
      {
       SPELL_LIGHTNING_BOLT,
       SPELL_LEHUDIBS_CRYSTAL_SPEAR,
       SPELL_BLINK,
       SPELL_BOLT_OF_IRON,
       SPELL_POISON_ARROW,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_DEEP_ELF_SORCERER,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_BANISHMENT,
       SPELL_HASTE,
       SPELL_SUMMON_DEMON,
       SPELL_HELLFIRE,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_DEEP_ELF_DEATH_MAGE,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_BOLT_OF_DRAINING,
       SPELL_MINOR_HEALING,
       SPELL_ANIMATE_DEAD,
       SPELL_ANIMATE_DEAD,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_KOBOLD_DEMONOLOGIST,
      {
       SPELL_CALL_IMP,
       SPELL_SUMMON_DEMON,
       SPELL_CANTRIP,
       SPELL_CALL_IMP,
       SPELL_SUMMON_DEMON,
       SPELL_CANTRIP           // this should be cute -- bw
      },
    },

    {  MST_NAGA,
      {
       SPELL_POISON_SPLASH,
       SPELL_POISON_SPLASH,
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
       SPELL_POLYMORPH_OTHER,
       SPELL_POLYMORPH_OTHER,
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
       SPELL_POLYMORPH_OTHER,
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
       SPELL_MEPHITIC_CLOUD,
       SPELL_MEPHITIC_CLOUD,
       SPELL_NO_SPELL,
       SPELL_MEPHITIC_CLOUD,
       SPELL_MEPHITIC_CLOUD,
       SPELL_NO_SPELL
      }
    },

    {  MST_SERPENT_OF_HELL,
      {
       SPELL_HELLFIRE,
       SPELL_HELLFIRE,
       SPELL_NO_SPELL,
       SPELL_HELLFIRE,
       SPELL_HELLFIRE,
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

    {  MST_SKELETAL_WARRIOR,
      {
       SPELL_ANIMATE_DEAD,
       SPELL_NO_SPELL,
       SPELL_ANIMATE_DEAD,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_MYSTIC,
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
       SPELL_MIASMA,
       SPELL_MIASMA,
       SPELL_NO_SPELL,
       SPELL_MIASMA,
       SPELL_MIASMA,
       SPELL_NO_SPELL
      }
    },

    {  MST_DRAC_SCORCHER,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_STICKY_FLAME,
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
       SPELL_BANISHMENT,
       SPELL_BLINK_OTHER,
       SPELL_BLINK,
       SPELL_NO_SPELL,
       SPELL_BLINK_OTHER,
       SPELL_CONTROLLED_BLINK
      }
    },

    // Curse toe menu should be kept full, because otherwise the toe spends
    // too much time crawling around.
    {  MST_CURSE_TOE,
      {
       SPELL_SUMMON_UNDEAD,
       SPELL_SUMMON_MUSHROOMS, // fungal theme
       SPELL_SUMMON_MUSHROOMS,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_SUMMON_UNDEAD,
       SPELL_SYMBOL_OF_TORMENT
      }
    },

    {  MST_ICE_STATUE,
      {
       SPELL_BOLT_OF_COLD,
       SPELL_ICE_BOLT,
       SPELL_NO_SPELL,
       SPELL_FREEZING_CLOUD,
       SPELL_SUMMON_ICE_BEAST,
       SPELL_SUMMON_ICE_BEAST
      }
    },

    // ---------------------
    // uniques' "spellbooks"
    // ---------------------

    // Crusader theme.
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
       SPELL_BOLT_OF_IRON,
       SPELL_BLINK_OTHER,
       SPELL_BOLT_OF_MAGMA,
       SPELL_ISKENDERUNS_MYSTIC_BLAST,
       SPELL_STONE_ARROW
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

    { MST_EUSTACHIO,
      {
       SPELL_SUMMON_SMALL_MAMMALS,
       SPELL_CALL_IMP,
       SPELL_BLINK,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL,
       SPELL_BLINK
      }
    },

    { MST_ILSUIW,
      {
       SPELL_THROW_FROST, // was: SPELL_CONFUSED (jpeg)
       SPELL_SLOW,
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
       SPELL_NO_SPELL,
       SPELL_HASTE,
       SPELL_ANIMATE_DEAD,
       SPELL_NO_SPELL,
       SPELL_NO_SPELL
      }
    },

    {  MST_KIRKE,
      {
       SPELL_PORKALATOR,
       SPELL_SLOW,
       SPELL_SUMMON_UGLY_THING,
       SPELL_SUMMON_ICE_BEAST,
       SPELL_BACKLIGHT,
       SPELL_INVISIBILITY
      }
    },

    {  MST_GERYON,
      {
       SPELL_SUMMON_BEAST,
       SPELL_SUMMON_BEAST,
       SPELL_NO_SPELL,
       SPELL_SUMMON_BEAST,
       SPELL_NO_SPELL,
       SPELL_SUMMON_BEAST
      }
    },

    {  MST_DISPATER,
      {
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_BOLT_OF_IRON,
       SPELL_SUMMON_DEMON,
       SPELL_LIGHTNING_BOLT,
       SPELL_HELLFIRE,
       SPELL_SUMMON_GREATER_DEMON
      }
    },

    {  MST_ASMODEUS,
      {
       SPELL_BOLT_OF_FIRE,
       SPELL_HELLFIRE,
       SPELL_SUMMON_DEMON,
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_BOLT_OF_DRAINING,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_ERESHKIGAL,
      {
       SPELL_BOLT_OF_DRAINING,
       SPELL_BOLT_OF_COLD,
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_PARALYSE,
       SPELL_MINOR_HEALING
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
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_SMITING,
       SPELL_INVISIBILITY,
       SPELL_SUMMON_HORRIBLE_THINGS,
       SPELL_SUMMON_HORRIBLE_THINGS,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_LOM_LOBON,
      {
       SPELL_ICE_STORM,
       SPELL_CONJURE_BALL_LIGHTNING,
       SPELL_MAJOR_HEALING,
       SPELL_BLINK,
       SPELL_BLINK,
       SPELL_TELEPORT_SELF
      }
    },

    {  MST_CEREBOV,
      {
       SPELL_FIRE_STORM,
       SPELL_BOLT_OF_IRON,
       SPELL_HASTE,
       SPELL_HASTE,
       SPELL_SUMMON_GREATER_DEMON,
       SPELL_HASTE
      }
    },

    {  MST_GLOORX_VLOQ,
      {
       SPELL_POISON_ARROW,
       SPELL_MIASMA,
       SPELL_SYMBOL_OF_TORMENT,
       SPELL_BOLT_OF_DRAINING,
       SPELL_DISPEL_UNDEAD,
       SPELL_INVISIBILITY
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
    }

#endif
