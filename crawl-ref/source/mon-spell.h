#include "mon-book.h"

/* *************************************************************************

    This file determines which spells are contained in monster spellbooks.
    It is used by /util/gen-mst.pl to generate spellbook enums, which are
    listed in mon-mst.h.

    Template Format:

    {    mon_spellbook_type,
        {
            { spell type, frequency, flags },
            { spell type, frequency, flags },
            [...]
        }
    }

    Spellbooks are organized by shared generic books to non-uniques to
    uniques / vault monsters, base user glyph (alphabetical, lowercase
    to uppercase to numbers to misc), then loose thematic tiering.

************************************************************************* */

static const mon_spellbook mspell_list[] =
{

    /* Crimson imp, phantom, blink frog, and killer klown. */
    {  MST_BLINKER,
      {
       { SPELL_BLINK, 29, MON_SPELL_MAGICAL },
      }
    },

    /* Orc warlords and cherubs. */
    {  MST_BATTLECRY,
      {
       { SPELL_BATTLECRY, 100, MON_SPELL_NATURAL | MON_SPELL_NOISY
                               | MON_SPELL_NO_SILENT },
      }
    },

    /* Eyes of draining and ghost moths. */
    {  MST_DRAIN_GAZE,
      {
       { SPELL_DRAINING_GAZE, 100, MON_SPELL_MAGICAL | MON_SPELL_INSTANT },
      }
    },

    // Electric eels and lightning spires.
    {  MST_ZAPPER,
      {
       { SPELL_ELECTRICAL_BOLT, 100, MON_SPELL_NATURAL },
      }
    },

    // Oklob plants and acid blobs.
    {  MST_ACID_SPIT,
      {
       { SPELL_SPIT_ACID, 67, MON_SPELL_NATURAL },
      }
    },

    // War gargoyles and iron dragons.
    {  MST_SPLINTER_BREATH,
      {
       { SPELL_METAL_SPLINTERS, 68, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                     | MON_SPELL_NOISY },
      }
    },

    // Deep elf annihilators and draconian annihilators.
    {  MST_ANNIHILATOR,
      {
       { SPELL_LIGHTNING_BOLT, 11, MON_SPELL_WIZARD },
       { SPELL_POISON_ARROW, 11, MON_SPELL_WIZARD },
       { SPELL_IRON_SHOT, 11, MON_SPELL_WIZARD },
       { SPELL_LEHUDIBS_CRYSTAL_SPEAR, 11, MON_SPELL_WIZARD },
       { SPELL_BLINK, 22, MON_SPELL_WIZARD },
      }
    },

    // ('a') Ants and formicids.
    {  MST_ENTROPY_WEAVER,
      {
       { SPELL_ENTROPIC_WEAVE, 40, MON_SPELL_WIZARD },
      }
    },

    // ('b') Bats, birds, and butterflies.
    {  MST_SHARD_SHRIKE,
      {
        { SPELL_THROW_ICICLE, 24, MON_SPELL_NATURAL | MON_SPELL_BREATH },
      }
    },

    // ('c') Centaurs and such.
    {  MST_FAUN_I,
      {
       { SPELL_CORONA, 15, MON_SPELL_WIZARD },
       { SPELL_SLOW, 30, MON_SPELL_WIZARD },
      }
    },

    {  MST_FAUN_II,
      {
       { SPELL_CORONA, 15, MON_SPELL_WIZARD },
       { SPELL_CONFUSE, 10, MON_SPELL_WIZARD },
      }
    },

    {  MST_SATYR,
      {
       { SPELL_BATTLECRY, 25, MON_SPELL_NATURAL | MON_SPELL_NO_SILENT
                              | MON_SPELL_NOISY },
       { SPELL_CAUSE_FEAR, 32, MON_SPELL_WIZARD },
       { SPELL_SLEEP, 16, MON_SPELL_WIZARD },
      }
    },

    // ('e') Elves.
    {  MST_DEEP_ELF_MAGE_I,
      {
       { SPELL_STICKY_FLAME_RANGE, 15, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_FIRE, 15, MON_SPELL_WIZARD },
       { SPELL_FIRE_ELEMENTALS, 15, MON_SPELL_WIZARD },
       { SPELL_BLINK, 15, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_MAGE_II,
      {
       { SPELL_THROW_ICICLE, 15, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_COLD, 15, MON_SPELL_WIZARD },
       { SPELL_SUMMON_ICE_BEAST, 15, MON_SPELL_WIZARD },
       { SPELL_BLINK, 15, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_MAGE_III,
      {
       { SPELL_LIGHTNING_BOLT, 40, MON_SPELL_WIZARD },
       { SPELL_BLINK, 20, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_MAGE_IV,
      {
       { SPELL_BOLT_OF_DRAINING, 15, MON_SPELL_WIZARD },
       { SPELL_SUMMON_DEMON, 15, MON_SPELL_WIZARD },
       { SPELL_SUMMON_VERMIN, 15, MON_SPELL_WIZARD },
       { SPELL_BLINK, 15, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_MAGE_V,
      {
       { SPELL_STONE_ARROW, 15, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_MAGMA, 15, MON_SPELL_WIZARD },
       { SPELL_EARTH_ELEMENTALS, 15, MON_SPELL_WIZARD },
       { SPELL_BLINK, 15, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_MAGE_VI,
      {
       { SPELL_MAGIC_DART, 12, MON_SPELL_WIZARD },
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 12, MON_SPELL_WIZARD },
       { SPELL_FORCE_LANCE, 12, MON_SPELL_WIZARD },
       { SPELL_BATTLESPHERE, 24, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_KNIGHT_I,
      {
       { SPELL_THROW_ICICLE, 24, MON_SPELL_WIZARD },
       { SPELL_HASTE, 12, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_KNIGHT_II,
      {
       { SPELL_STONE_ARROW, 24, MON_SPELL_WIZARD },
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 12, MON_SPELL_WIZARD },
       { SPELL_HASTE, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_ARCHER,
      {
       { SPELL_SLOW, 12, MON_SPELL_WIZARD },
       { SPELL_CONFUSE, 12, MON_SPELL_WIZARD },
       { SPELL_REPEL_MISSILES, 12, MON_SPELL_WIZARD },
       { SPELL_BLINK_AWAY, 36, MON_SPELL_WIZARD | MON_SPELL_SHORT_RANGE },
      }
    },

    {  MST_DEEP_ELF_ELEMENTALIST,
      {
       { SPELL_FREEZING_CLOUD, 11, MON_SPELL_WIZARD },
       { SPELL_FIREBALL, 11, MON_SPELL_WIZARD },
       { SPELL_AWAKEN_EARTH, 44, MON_SPELL_WIZARD },
       { SPELL_REPEL_MISSILES, 11, MON_SPELL_WIZARD },
       { SPELL_HASTE, 11, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_SORCERER,
      {
       { SPELL_CORROSIVE_BOLT, 18, MON_SPELL_WIZARD },
       { SPELL_BANISHMENT, 11, MON_SPELL_WIZARD },
       { SPELL_HASTE, 22, MON_SPELL_WIZARD },
       { SPELL_HURL_DAMNATION, 11, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_HIGH_PRIEST,
      {
       { SPELL_AURA_OF_BRILLIANCE, 25, MON_SPELL_PRIEST },
       { SPELL_CALL_DOWN_DAMNATION, 12, MON_SPELL_PRIEST },
       { SPELL_MALIGN_OFFERING, 18, MON_SPELL_PRIEST },
       { SPELL_SMITING, 18, MON_SPELL_PRIEST },
       { SPELL_TWISTED_RESURRECTION, 12, MON_SPELL_PRIEST },
      }
    },

    {  MST_DEEP_ELF_DEMONOLOGIST,
      {
       { SPELL_SUMMON_DEMON, 24, MON_SPELL_WIZARD },
       { SPELL_SUMMON_GREATER_DEMON, 24, MON_SPELL_WIZARD },
       { SPELL_BANISHMENT, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_ELF_DEATH_MAGE,
      {
       { SPELL_BOLT_OF_DRAINING, 22, MON_SPELL_WIZARD },
       { SPELL_VAMPIRIC_DRAINING, 22, MON_SPELL_WIZARD },
       { SPELL_CALL_LOST_SOUL, 11, MON_SPELL_WIZARD },
       { SPELL_TWISTED_RESURRECTION, 11, MON_SPELL_WIZARD },
      }
    },

    // ('f') Mobile plants and fungi.
    {  MST_THORN_HUNTER,
      {
       { SPELL_THORN_VOLLEY, 43, MON_SPELL_NATURAL },
       { SPELL_WALL_OF_BRAMBLES, 22, MON_SPELL_MAGICAL },
      }
    },

    {  MST_DEATHCAP,
      {
       { SPELL_DRAIN_LIFE, 30, MON_SPELL_MAGICAL },
      }
    },

    // ('g') Small humanoids.
    {  MST_GNOLL_SHAMAN,
      {
       { SPELL_CORONA, 11, MON_SPELL_PRIEST },
       { SPELL_HEAL_OTHER, 21, MON_SPELL_PRIEST },
       { SPELL_HASTE_OTHER, 11, MON_SPELL_PRIEST },
       { SPELL_MINOR_HEALING, 11, MON_SPELL_PRIEST | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_BOGGART,
      {
       { SPELL_SHADOW_CREATURES, 33, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 11, MON_SPELL_WIZARD },
       { SPELL_BLINK, 11, MON_SPELL_WIZARD },
      }
    },

    // ('h') Carnivorous quadrupeds.
    { MST_HOWLER_MONKEY,
      {
        { SPELL_WARNING_CRY, 40, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                 | MON_SPELL_NO_SILENT },
      }
    },

    {  MST_BEAR,
      {
       { SPELL_BERSERKER_RAGE, 57, MON_SPELL_NATURAL | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_HELL_HOUND,
      {
       { SPELL_FIRE_BREATH, 20, MON_SPELL_NATURAL | MON_SPELL_BREATH },
      }
    },

    {  MST_RAIJU,
      {
       { SPELL_BLINKBOLT, 40, MON_SPELL_MAGICAL },
      }
    },

    {  MST_HELL_HOG,
      {
       { SPELL_FIREBALL, 60, MON_SPELL_NATURAL | MON_SPELL_BREATH },
      }
    },

    {  MST_DOOM_HOUND,
      {
       { SPELL_DOOM_HOWL, 30, MON_SPELL_NATURAL },
      }
    },

    // ('i') Spriggans.
    {  MST_SPRIGGAN_BERSERKER,
      {
       { SPELL_BROTHERS_IN_ARMS, 21, MON_SPELL_PRIEST },
       { SPELL_BERSERKER_RAGE, 21, MON_SPELL_PRIEST },
       { SPELL_TROGS_HAND, 21, MON_SPELL_PRIEST },
      }
    },

    {  MST_SPRIGGAN_DRUID,
      {
       { SPELL_STONE_ARROW, 15, MON_SPELL_WIZARD },
       { SPELL_DRUIDS_CALL, 15, MON_SPELL_WIZARD },
       { SPELL_AWAKEN_FOREST, 15, MON_SPELL_WIZARD },
       { SPELL_MINOR_HEALING, 15, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_SPRIGGAN_AIR_MAGE,
      {
       { SPELL_LIGHTNING_BOLT, 16, MON_SPELL_WIZARD },
       { SPELL_AIRSTRIKE, 32, MON_SPELL_WIZARD },
       { SPELL_DEFLECT_MISSILES, 16, MON_SPELL_WIZARD },
      }
    },

    // ('k') Drakes.
    { MST_RIME_DRAKE,
      {
        { SPELL_FLASH_FREEZE, 62, MON_SPELL_NATURAL | MON_SPELL_BREATH },
      }
    },

    {  MST_SWAMP_DRAKE,
      {
       { SPELL_NOXIOUS_CLOUD, 54, MON_SPELL_NATURAL | MON_SPELL_BREATH },
      }
    },

    {  MST_WIND_DRAKE,
      {
       { SPELL_WIND_BLAST, 50, MON_SPELL_NATURAL | MON_SPELL_BREATH },
       { SPELL_AIRSTRIKE, 29, MON_SPELL_NATURAL },
       { SPELL_DEFLECT_MISSILES, 29, MON_SPELL_NATURAL },
      }
    },

    {  MST_LINDWURM,
      {
       { SPELL_FIRE_BREATH, 62, MON_SPELL_NATURAL | MON_SPELL_BREATH },
      }
    },

    {  MST_DEATH_DRAKE,
      {
       { SPELL_MIASMA_BREATH, 59, MON_SPELL_NATURAL | MON_SPELL_BREATH },
      }
    },

    // ('l') Lizards.
    {  MST_BASILISK,
      {
       { SPELL_PETRIFY, 56, MON_SPELL_MAGICAL },
      }
    },

    // ('m') Merfolk and friends.
    {  MST_MERFOLK_SIREN,
      {
       { SPELL_SIREN_SONG, 160, MON_SPELL_WIZARD },
      }
    },

    {  MST_WATER_NYMPH,
      {
       { SPELL_WATERSTRIKE, 60, MON_SPELL_WIZARD },
      }
    },

    {  MST_MERFOLK_AVATAR,
      {
       { SPELL_AVATAR_SONG, 160, MON_SPELL_WIZARD },
      }
    },

    {  MST_MERFOLK_AQUAMANCER,
      {
        { SPELL_PRIMAL_WAVE, 16, MON_SPELL_WIZARD },
        { SPELL_STEAM_BALL, 16, MON_SPELL_WIZARD },
        { SPELL_THROW_ICICLE, 16, MON_SPELL_WIZARD },
        { SPELL_BLINK, 16, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    // ('n') Rotting hulks.
    {  MST_BOG_BODY,
      {
       { SPELL_BOLT_OF_COLD, 28, MON_SPELL_WIZARD },
       { SPELL_SLOW, 28, MON_SPELL_WIZARD },
      }
    },

    // ('o') Orcs.
    {  MST_ORC_PRIEST,
      {
       { SPELL_CANTRIP, 13, MON_SPELL_PRIEST },
       { SPELL_PAIN, 13, MON_SPELL_PRIEST },
       { SPELL_SMITING, 13, MON_SPELL_PRIEST },
       { SPELL_HEAL_OTHER, 13, MON_SPELL_PRIEST },
      }
    },

    {  MST_ORC_WIZARD_I,
      {
       { SPELL_MAGIC_DART, 18, MON_SPELL_WIZARD },
       { SPELL_SLOW, 9, MON_SPELL_WIZARD },
       { SPELL_HASTE, 9, MON_SPELL_WIZARD },
       { SPELL_BLINK, 18, MON_SPELL_WIZARD },
      }
    },

    // Shared with Sigmund.
    {  MST_ORC_WIZARD_II,
      {
       { SPELL_MAGIC_DART, 9, MON_SPELL_WIZARD },
       { SPELL_THROW_FLAME, 9, MON_SPELL_WIZARD },
       { SPELL_CONFUSE, 18, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 9, MON_SPELL_WIZARD },
      }
    },

    // Shared with Blork the orc.
    {  MST_ORC_WIZARD_III,
      {
       { SPELL_CANTRIP, 9, MON_SPELL_WIZARD },
       { SPELL_MAGIC_DART, 9, MON_SPELL_WIZARD },
       { SPELL_THROW_FROST, 9, MON_SPELL_WIZARD },
       { SPELL_THROW_FLAME, 9, MON_SPELL_WIZARD },
       { SPELL_HASTE, 9, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 9, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_ORC_KNIGHT,
      {
       { SPELL_BATTLECRY, 50, MON_SPELL_NATURAL | MON_SPELL_NOISY
                              | MON_SPELL_NO_SILENT },
      }
    },

    {  MST_ORC_HIGH_PRIEST,
      {
       { SPELL_PAIN, 10, MON_SPELL_PRIEST },
       { SPELL_SMITING, 10, MON_SPELL_PRIEST },
       { SPELL_SUMMON_DEMON, 20, MON_SPELL_PRIEST },
       { SPELL_HEAL_OTHER, 10, MON_SPELL_PRIEST },
       { SPELL_MINOR_HEALING, 10, MON_SPELL_PRIEST | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_ORC_SORCERER,
      {
       { SPELL_BOLT_OF_FIRE, 12, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_DRAINING, 12, MON_SPELL_WIZARD },
       { SPELL_SUMMON_DEMON, 12, MON_SPELL_WIZARD },
       { SPELL_PARALYSE, 12, MON_SPELL_WIZARD },
      }
    },

    // ('p') Humans.
    {  MST_NECROMANCER_I,
      {
       { SPELL_BOLT_OF_COLD, 15, MON_SPELL_WIZARD },
       { SPELL_AGONY, 15, MON_SPELL_WIZARD },
       { SPELL_BIND_SOULS, 30, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 10, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_NECROMANCER_II,
      {
       { SPELL_BOLT_OF_FIRE, 15, MON_SPELL_WIZARD },
       { SPELL_AGONY, 15, MON_SPELL_WIZARD },
       { SPELL_BIND_SOULS, 30, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 10, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    // Wizard sets are also used by Erolcha.
    {  MST_WIZARD_I,
      {
       { SPELL_STONE_ARROW, 14, MON_SPELL_WIZARD },
       { SPELL_LIGHTNING_BOLT, 12, MON_SPELL_WIZARD },
       { SPELL_BANISHMENT, 10, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 14, MON_SPELL_WIZARD },
      }
    },

    {  MST_WIZARD_II,
      {
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 14, MON_SPELL_WIZARD },
       { SPELL_FIREBALL, 12, MON_SPELL_WIZARD },
       { SPELL_PARALYSE, 12, MON_SPELL_WIZARD },
       { SPELL_BLINK, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_WIZARD_III,
      {
       { SPELL_BOLT_OF_COLD, 14, MON_SPELL_WIZARD },
       { SPELL_LEHUDIBS_CRYSTAL_SPEAR, 12, MON_SPELL_WIZARD },
       { SPELL_SLOW, 12, MON_SPELL_WIZARD },
       { SPELL_HASTE, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEATH_KNIGHT,
      {
       { SPELL_AGONY, 15, MON_SPELL_PRIEST },
       { SPELL_INJURY_MIRROR, 20, MON_SPELL_PRIEST },
      }
    },

    {  MST_HELL_KNIGHT_I,
      {
       { SPELL_PAIN, 13, MON_SPELL_WIZARD },
       { SPELL_THROW_FLAME, 13, MON_SPELL_WIZARD },
       { SPELL_HASTE, 26, MON_SPELL_WIZARD },
      }
    },

    {  MST_HELL_KNIGHT_II,
      {
       { SPELL_PAIN, 17, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_FIRE, 17, MON_SPELL_WIZARD },
       { SPELL_BLINK, 17, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_VAULT_SENTINEL,
      {
       { SPELL_WARNING_CRY, 50, MON_SPELL_NATURAL | MON_SPELL_NO_SILENT
                                | MON_SPELL_BREATH },
       { SPELL_SENTINEL_MARK, 58, MON_SPELL_WIZARD },
      }
    },

    {  MST_IRONHEART_PRESERVER,
      {
       { SPELL_INJURY_BOND, 21, MON_SPELL_WIZARD },
       { SPELL_MINOR_HEALING, 41, MON_SPELL_WIZARD },
      }
    },

    {  MST_IRONBRAND_CONVOKER,
      {
       { SPELL_WORD_OF_RECALL, 30, MON_SPELL_WIZARD },
       { SPELL_MIGHT_OTHER, 30, MON_SPELL_WIZARD },
      }
    },

    {  MST_VAULT_WARDEN,
      {
       { SPELL_SEAL_DOORS, 50, MON_SPELL_MAGICAL | MON_SPELL_NO_SILENT },
      }
    },

    { MST_IMPERIAL_MYRMIDON,
      {
       { SPELL_SLOW, 13, MON_SPELL_WIZARD },
       { SPELL_CONFUSE, 13, MON_SPELL_WIZARD },
       { SPELL_AGONY, 13, MON_SPELL_WIZARD },
      }
    },

    { MST_SERVANT_OF_WHISPERS,
      {
       { SPELL_STILL_WINDS, 20, MON_SPELL_PRIEST },
       { SPELL_LIGHTNING_BOLT, 40, MON_SPELL_PRIEST },
      }
    },

    { MST_RAGGED_HIEROPHANT,
      {
       { SPELL_INJURY_BOND, 20, MON_SPELL_WIZARD },
       { SPELL_RESONANCE_STRIKE, 40, MON_SPELL_WIZARD },
      }
    },

    { MST_HALAZID_WARLOCK,
      {
       { SPELL_GHOSTLY_SACRIFICE, 20, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_DRAINING, 20, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_COLD, 20, MON_SPELL_WIZARD },
       { SPELL_SHADOW_CREATURES, 20, MON_SPELL_WIZARD },
      }
    },

    // ('q') Classed draconians.

    {  MST_DRACONIAN_SCORCHER,
      {
       { SPELL_BOLT_OF_FIRE, 13, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_MAGMA, 13, MON_SPELL_WIZARD },
       { SPELL_FIREBALL, 13, MON_SPELL_WIZARD },
       { SPELL_HURL_DAMNATION, 13, MON_SPELL_WIZARD },
       { SPELL_CALL_DOWN_DAMNATION, 13, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_DRACONIAN_KNIGHT,
      {
       { SPELL_THROW_ICICLE, 13, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_COLD, 13, MON_SPELL_WIZARD },
       { SPELL_OZOCUBUS_ARMOUR, 13, MON_SPELL_WIZARD },
       { SPELL_HASTE, 13, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 13, MON_SPELL_WIZARD },
      }
    },

    {  MST_DRACONIAN_STORMCALLER,
      {
        { SPELL_SMITING, 20, MON_SPELL_PRIEST },
        { SPELL_SUMMON_DRAKES, 20, MON_SPELL_PRIEST },
        { SPELL_UPHEAVAL, 20, MON_SPELL_PRIEST },
      }
    },

    {  MST_DRACONIAN_SHIFTER,
      {
       { SPELL_DIMENSION_ANCHOR, 15, MON_SPELL_WIZARD },
       { SPELL_BLINK_OTHER, 15, MON_SPELL_WIZARD },
       { SPELL_BLINK_RANGE, 15, MON_SPELL_WIZARD },
       { SPELL_BLINK_ALLIES_ENCIRCLE, 15, MON_SPELL_WIZARD },
       { SPELL_BLINK_AWAY, 15, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    // ('s') Spiders and insects.
    {  MST_JUMPING_SPIDER,
      {
       { SPELL_BLINK_CLOSE, 29, MON_SPELL_NATURAL },
       { SPELL_BLINK_AWAY, 29, MON_SPELL_NATURAL },
      }
    },

    {  MST_ORB_SPIDER,
      {
       { SPELL_IOOD, 57, MON_SPELL_MAGICAL },
      }
    },

    // ('t') Testudines.
    {  MST_ALLIGATOR,
      {
       { SPELL_SPRINT, 31, MON_SPELL_NATURAL },
      }
    },

    {  MST_FIRE_CRAB,
      {
       { SPELL_FLAMING_CLOUD, 62, MON_SPELL_NATURAL },
      }
    },

    {  MST_GHOST_CRAB,
      {
       { SPELL_SPECTRAL_CLOUD, 62, MON_SPELL_NATURAL },
      }
    },

    {  MST_APOCALYPSE_CRAB,
      {
       { SPELL_CHAOS_BREATH, 62, MON_SPELL_NATURAL },
      }
    },

    // ('v') Vortices.
    {  MST_INSUBSTANTIAL_WISP,
      {
       { SPELL_BLINK, 18, MON_SPELL_MAGICAL },
       { SPELL_DISCHARGE, 32, MON_SPELL_MAGICAL },
      }
    },

    // ('w') Slugs and worms.
    { MST_DART_SLUG,
      {
        { SPELL_SLUG_DART, 40, MON_SPELL_NATURAL },
      }
    },

    // ('x') Lesser abominations and tentacled things.
    {  MST_WORLDBINDER,
      {
       { SPELL_FORCEFUL_INVITATION, 58, MON_SPELL_MAGICAL },
      }
    },

    // ('y') Flying insects.
    {  MST_MELIAI,
      {
       { SPELL_SMITING, 18, MON_SPELL_PRIEST },
       { SPELL_HEAL_OTHER, 18, MON_SPELL_PRIEST },
      }
    },

    {  MST_QUEEN_BEE,
      {
       { SPELL_BERSERK_OTHER, 50, MON_SPELL_NATURAL },
       { SPELL_BERSERK_OTHER, 75, MON_SPELL_NATURAL | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_SPARK_WASP,
      {
       { SPELL_BLINKBOLT, 40, MON_SPELL_MAGICAL | MON_SPELL_LONG_RANGE },
      }
    },

    {  MST_MOTH_OF_WRATH,
      {
       { SPELL_BERSERK_OTHER, 66, MON_SPELL_NATURAL },
      }
    },

    // ('z') Corporeal semi-humanoid undead.
    {  MST_ANCIENT_CHAMPION_I,
      {
       { SPELL_IRON_SHOT, 21, MON_SPELL_WIZARD },
       { SPELL_PAIN, 21, MON_SPELL_WIZARD },
       { SPELL_HASTE, 21, MON_SPELL_WIZARD },
      }
    },

    {  MST_ANCIENT_CHAMPION_II,
      {
       { SPELL_BOLT_OF_DRAINING, 21, MON_SPELL_WIZARD },
       { SPELL_STONE_ARROW, 21, MON_SPELL_WIZARD },
       { SPELL_HASTE, 21, MON_SPELL_WIZARD },
      }
    },

    {  MST_CURSE_SKULL,
      {
       { SPELL_SUMMON_UNDEAD, 48, MON_SPELL_MAGICAL | MON_SPELL_NOISY },
       { SPELL_SYMBOL_OF_TORMENT, 16, MON_SPELL_MAGICAL | MON_SPELL_NOISY },
      }
    },

    {  MST_CURSE_TOE,
      {
       { SPELL_SYMBOL_OF_TORMENT, 26, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_MUSHROOMS, 39, MON_SPELL_MAGICAL },
      }
    },

    // ('A') Angels.
    {  MST_OPHAN,
      {
       { SPELL_BOLT_OF_FIRE, 32, MON_SPELL_MAGICAL },
       { SPELL_HOLY_FLAMES, 32, MON_SPELL_MAGICAL },
      }
    },

    {  MST_ANGEL,
      {
       { SPELL_MINOR_HEALING, 62, MON_SPELL_MAGICAL },
      }
    },

    {  MST_DAEVA,
      {
       { SPELL_SMITING, 64, MON_SPELL_MAGICAL },
      }
    },

    {  MST_SERAPH,
      {
       { SPELL_WARNING_CRY, 50, MON_SPELL_NATURAL | MON_SPELL_NO_SILENT
                                | MON_SPELL_BREATH },
       { SPELL_SUMMON_HOLIES, 50, MON_SPELL_MAGICAL },
       { SPELL_INJURY_BOND, 50, MON_SPELL_MAGICAL },
       { SPELL_CLEANSING_FLAME, 25, MON_SPELL_MAGICAL },
      }
    },

    // ('C') Giants.
    {  MST_FIRE_GIANT,
      {
       { SPELL_BOLT_OF_FIRE, 29, MON_SPELL_WIZARD },
       { SPELL_FIREBALL, 29, MON_SPELL_WIZARD },
      }
    },

    {  MST_FROST_GIANT,
      {
       { SPELL_BOLT_OF_COLD, 66, MON_SPELL_WIZARD },
      }
    },

    {  MST_TITAN,
      {
       { SPELL_LIGHTNING_BOLT, 18, MON_SPELL_WIZARD },
       { SPELL_MINOR_HEALING, 36, MON_SPELL_WIZARD },
       { SPELL_AIRSTRIKE, 18, MON_SPELL_WIZARD },
      }
    },

    {  MST_IRON_GIANT,
      {
       { SPELL_IRON_SHOT, 40, MON_SPELL_WIZARD },
       { SPELL_THROW_ALLY, 80, MON_SPELL_NATURAL },
      }
    },

    // ('D') Dragons.
    {  MST_STEAM_DRAGON,
      {
       { SPELL_STEAM_BALL, 54, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                | MON_SPELL_NOISY },
      }
    },

    {  MST_ACID_DRAGON,
      {
       { SPELL_SPIT_ACID, 55, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                        | MON_SPELL_NOISY },
      }
    },

    {  MST_SWAMP_DRAGON,
      {
       { SPELL_POISONOUS_CLOUD, 59, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                     | MON_SPELL_NOISY },
      }
    },

    {  MST_FIRE_DRAGON_BREATH,
      {
       { SPELL_FIRE_BREATH, 62, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                 | MON_SPELL_NOISY },
      }
    },

    {  MST_ICE_DRAGON_BREATH,
      {
       { SPELL_COLD_BREATH, 62, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                 | MON_SPELL_NOISY },
      }
    },

    {  MST_STORM_DRAGON,
      {
       { SPELL_LIGHTNING_BOLT, 64, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                    | MON_SPELL_NOISY },
      }
    },

    {  MST_SHADOW_DRAGON,
      {
       { SPELL_BOLT_OF_DRAINING, 67, MON_SPELL_NATURAL | MON_SPELL_BREATH },
      }
    },

    {  MST_QUICKSILVER_DRAGON,
      {
       { SPELL_QUICKSILVER_BOLT, 66, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                      | MON_SPELL_NOISY },
      }
    },

    {  MST_GOLDEN_DRAGON,
      {
       { SPELL_BOLT_OF_FIRE, 23, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                  | MON_SPELL_NOISY },
       { SPELL_BOLT_OF_COLD, 23, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                  | MON_SPELL_NOISY },
       { SPELL_POISONOUS_CLOUD, 23, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                  | MON_SPELL_NOISY },
      }
    },

    {  MST_PEARL_DRAGON,
      {
       { SPELL_HOLY_BREATH, 68, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                 | MON_SPELL_NOISY },
      }
    },

    // ('E') Elementals.
    {  MST_ELEMENTAL_WELLSPRING,
      {
       { SPELL_PRIMAL_WAVE, 65, MON_SPELL_MAGICAL },
      }
    },

    {  MST_IRON_ELEMENTAL,
      {
       { SPELL_IRON_SHOT, 21, MON_SPELL_MAGICAL },
       { SPELL_METAL_SPLINTERS, 21, MON_SPELL_MAGICAL },
       { SPELL_SLOW, 21, MON_SPELL_MAGICAL },
      }
    },

    // ('G') Organs.
    {  MST_FLOATING_EYE,
      {
       { SPELL_PARALYSIS_GAZE, 80, MON_SPELL_MAGICAL | MON_SPELL_INSTANT },
      }
    },

    {  MST_GOLDEN_EYE,
      {
       { SPELL_CONFUSION_GAZE, 100, MON_SPELL_MAGICAL | MON_SPELL_INSTANT },
       { SPELL_BLINK, 29, MON_SPELL_MAGICAL },
      }
    },

    {  MST_EYE_OF_DEVASTATION,
      {
       { SPELL_ENERGY_BOLT, 60, MON_SPELL_MAGICAL },
      }
    },

    {  MST_SHINING_EYE,
      {
       { SPELL_MALMUTATE, 57, MON_SPELL_MAGICAL },
      }
    },

    {  MST_GREAT_ORB_OF_EYES,
      {
       { SPELL_PARALYSE, 12, MON_SPELL_MAGICAL },
       { SPELL_DISINTEGRATE, 12, MON_SPELL_MAGICAL },
       { SPELL_DRAIN_MAGIC, 12, MON_SPELL_MAGICAL },
       { SPELL_CONFUSE, 12, MON_SPELL_MAGICAL },
      }
    },

    {  MST_GLOWING_ORANGE_BRAIN,
      {
       { SPELL_BRAIN_FEED, 10, MON_SPELL_MAGICAL },
       { SPELL_CAUSE_FEAR, 10, MON_SPELL_MAGICAL },
       { SPELL_SHADOW_CREATURES, 10, MON_SPELL_MAGICAL },
       { SPELL_MASS_CONFUSION, 10, MON_SPELL_MAGICAL },
       { SPELL_BLINK, 10, MON_SPELL_MAGICAL },
      }
    },

    // ('H') Animal hybrids.
    {  MST_MANTICORE,
      {
       { SPELL_THROW_BARBS, 100, MON_SPELL_NATURAL },
      }
    },

    {  MST_SPHINX,
      {
       { SPELL_CONFUSE, 11, MON_SPELL_WIZARD },
       { SPELL_SLOW, 11, MON_SPELL_WIZARD },
       { SPELL_PARALYSE, 11, MON_SPELL_WIZARD },
       { SPELL_SMITING, 11, MON_SPELL_WIZARD },
       { SPELL_MINOR_HEALING, 22, MON_SPELL_WIZARD },
      }
    },

    // ('K') Kobolds.
    {  MST_KOBOLD_DEMONOLOGIST,
      {
       { SPELL_CANTRIP, 19, MON_SPELL_WIZARD },
       { SPELL_SUMMON_MINOR_DEMON, 19, MON_SPELL_WIZARD },
       { SPELL_SUMMON_DEMON, 19, MON_SPELL_WIZARD },
      }
    },

    // ('L') Liches and the like.
    {  MST_REVENANT,
      {
       { SPELL_SPECTRAL_CLOUD, 17, MON_SPELL_WIZARD },
       { SPELL_GHOSTLY_FIREBALL, 17, MON_SPELL_WIZARD },
       { SPELL_DISPEL_UNDEAD, 17, MON_SPELL_WIZARD },
       { SPELL_BLINK_AWAY, 17, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_LICH_I,
      {
       { SPELL_BOLT_OF_FIRE, 12, MON_SPELL_WIZARD },
       { SPELL_IRON_SHOT, 12, MON_SPELL_WIZARD },
       { SPELL_SUMMON_GREATER_DEMON, 12, MON_SPELL_WIZARD },
       { SPELL_CONFUSE, 12, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_LICH_II,
      {
       { SPELL_BOLT_OF_COLD, 12, MON_SPELL_WIZARD },
       { SPELL_FORCE_LANCE, 12, MON_SPELL_WIZARD },
       { SPELL_SUMMON_GREATER_DEMON, 12, MON_SPELL_WIZARD },
       { SPELL_SLOW, 12, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_LICH_III,
      {
       { SPELL_LIGHTNING_BOLT, 18, MON_SPELL_WIZARD },
       { SPELL_IOOD, 12, MON_SPELL_WIZARD },
       { SPELL_PARALYSE, 18, MON_SPELL_WIZARD },
       { SPELL_HASTE, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_LICH_IV,
      {
       { SPELL_CORROSIVE_BOLT, 18, MON_SPELL_WIZARD },
       { SPELL_LEHUDIBS_CRYSTAL_SPEAR, 12, MON_SPELL_WIZARD },
       { SPELL_PETRIFY, 18, MON_SPELL_WIZARD },
       { SPELL_HASTE, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_LICH_V,
      {
       { SPELL_BOLT_OF_DRAINING, 12, MON_SPELL_WIZARD },
       { SPELL_POISON_ARROW, 12, MON_SPELL_WIZARD },
       { SPELL_HAUNT, 12, MON_SPELL_WIZARD },
       { SPELL_BANISHMENT, 12, MON_SPELL_WIZARD },
       { SPELL_HASTE, 12, MON_SPELL_WIZARD },
      }
    },

    // ('M') Mummies.
    {  MST_MUMMY_PRIEST,
      {
       { SPELL_SUMMON_DEMON, 12, MON_SPELL_PRIEST },
       { SPELL_SUMMON_UNDEAD, 24, MON_SPELL_PRIEST },
       { SPELL_SMITING, 12, MON_SPELL_PRIEST },
       { SPELL_SYMBOL_OF_TORMENT, 12, MON_SPELL_PRIEST },
      }
    },

    {  MST_GREATER_MUMMY_I,
      {
       { SPELL_SMITING, 13, MON_SPELL_WIZARD },
       { SPELL_SYMBOL_OF_TORMENT, 13, MON_SPELL_WIZARD },
       { SPELL_SUMMON_DEMON, 13, MON_SPELL_WIZARD },
       { SPELL_SUMMON_UNDEAD, 26, MON_SPELL_WIZARD },
      }
    },

    {  MST_GREATER_MUMMY_II,
      {
       { SPELL_SMITING, 13, MON_SPELL_WIZARD },
       { SPELL_SYMBOL_OF_TORMENT, 13, MON_SPELL_WIZARD },
       { SPELL_SUMMON_DEMON, 13, MON_SPELL_WIZARD },
       { SPELL_SUMMON_EMPEROR_SCORPIONS, 26, MON_SPELL_WIZARD },
      }
    },

    {  MST_GREATER_MUMMY_III,
      {
       { SPELL_SMITING, 13, MON_SPELL_WIZARD },
       { SPELL_SYMBOL_OF_TORMENT, 13, MON_SPELL_WIZARD },
       { SPELL_SUMMON_MANA_VIPER, 13, MON_SPELL_WIZARD },
       { SPELL_SUMMON_UNDEAD, 26, MON_SPELL_WIZARD },
      }
    },

    {  MST_GREATER_MUMMY_IV,
      {
       { SPELL_SMITING, 13, MON_SPELL_WIZARD },
       { SPELL_SYMBOL_OF_TORMENT, 13, MON_SPELL_WIZARD },
       { SPELL_SUMMON_EMPEROR_SCORPIONS, 13, MON_SPELL_WIZARD },
       { SPELL_SUMMON_SCARABS, 26, MON_SPELL_WIZARD },
      }
    },

    // ('N') Naga and other snake people.
    {  MST_NAGA,
      {
       { SPELL_SPIT_POISON, 55, MON_SPELL_NATURAL | MON_SPELL_BREATH },
      }
    },

    // Shared with nagaraja.
    {  MST_NAGA_MAGE,
      {
       { SPELL_SPIT_POISON, 55, MON_SPELL_NATURAL | MON_SPELL_BREATH },
       { SPELL_VENOM_BOLT, 10, MON_SPELL_WIZARD },
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 10, MON_SPELL_WIZARD },
       { SPELL_POISON_ARROW, 10, MON_SPELL_WIZARD },
       { SPELL_TELEPORT_OTHER, 10, MON_SPELL_WIZARD },
       { SPELL_HASTE, 10, MON_SPELL_WIZARD },
      }
    },

    {  MST_NAGA_SHARPSHOOTER,
      {
       { SPELL_SPIT_POISON, 55, MON_SPELL_NATURAL | MON_SPELL_BREATH },
       { SPELL_PORTAL_PROJECTILE, 59, MON_SPELL_WIZARD },
      }
    },

    {  MST_NAGA_RITUALIST,
      {
       { SPELL_SPIT_POISON, 55, MON_SPELL_NATURAL | MON_SPELL_BREATH },
       { SPELL_FORCE_LANCE, 12, MON_SPELL_WIZARD },
       { SPELL_OLGREBS_TOXIC_RADIANCE, 23, MON_SPELL_WIZARD },
       { SPELL_VIRULENCE, 23, MON_SPELL_WIZARD },
      }
    },

    {  MST_SALAMANDER_MYSTIC,
      {
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 12, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_MAGMA, 12, MON_SPELL_WIZARD },
       { SPELL_HASTE_OTHER, 24, MON_SPELL_WIZARD },
      }
    },

    // ('O') Ogres.
    {  MST_OGRE_MAGE_I,
      {
       { SPELL_LIGHTNING_BOLT, 12, MON_SPELL_WIZARD },
       { SPELL_HASTE_OTHER, 10, MON_SPELL_WIZARD },
       { SPELL_SLOW, 10, MON_SPELL_WIZARD },
       { SPELL_PARALYSE, 10, MON_SPELL_WIZARD },
       { SPELL_HASTE, 10, MON_SPELL_WIZARD },
      }
    },

    {  MST_OGRE_MAGE_II,
      {
       { SPELL_VENOM_BOLT, 12, MON_SPELL_WIZARD },
       { SPELL_HASTE_OTHER, 10, MON_SPELL_WIZARD },
       { SPELL_CONFUSE, 10, MON_SPELL_WIZARD },
       { SPELL_SLOW, 10, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 10, MON_SPELL_WIZARD },
      }
    },

    {  MST_OGRE_MAGE_III,
      {
       { SPELL_BOLT_OF_FIRE, 10, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_COLD, 10, MON_SPELL_WIZARD },
       { SPELL_LEHUDIBS_CRYSTAL_SPEAR, 12, MON_SPELL_WIZARD },
       { SPELL_HASTE_OTHER, 10, MON_SPELL_WIZARD },
       { SPELL_BLINK, 10, MON_SPELL_WIZARD },
      }
    },

    // ('P') Immobile plants.
    {  MST_BURNING_BUSH,
      {
       { SPELL_THROW_FLAME, 67, MON_SPELL_NATURAL },
      }
    },

    {  MST_OKLOB_SAPLING,
      {
       { SPELL_SPIT_ACID, 50, MON_SPELL_NATURAL },
      }
    },

    // ('Q') Tengu.
    {  MST_TENGU_CONJURER_I,
      {
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 19, MON_SPELL_WIZARD },
       { SPELL_FORCE_LANCE, 10, MON_SPELL_WIZARD },
       { SPELL_BATTLESPHERE, 19, MON_SPELL_WIZARD },
       { SPELL_BLINK, 10, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_TENGU_CONJURER_II,
      {
       { SPELL_THROW_FLAME, 10, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_MAGMA, 19, MON_SPELL_WIZARD },
       { SPELL_BATTLESPHERE, 19, MON_SPELL_WIZARD },
       { SPELL_BLINK, 10, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_TENGU_CONJURER_III,
      {
       { SPELL_SHOCK, 10, MON_SPELL_WIZARD },
       { SPELL_LIGHTNING_BOLT, 19, MON_SPELL_WIZARD },
       { SPELL_BATTLESPHERE, 19, MON_SPELL_WIZARD },
       { SPELL_BLINK, 10, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_TENGU_CONJURER_IV,
      {
       { SPELL_MEPHITIC_CLOUD, 10, MON_SPELL_WIZARD },
       { SPELL_VENOM_BOLT, 19, MON_SPELL_WIZARD },
       { SPELL_BATTLESPHERE, 19, MON_SPELL_WIZARD },
       { SPELL_BLINK, 10, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_TENGU_REAVER_I,
      {
       { SPELL_FREEZING_CLOUD, 13, MON_SPELL_WIZARD },
       { SPELL_LIGHTNING_BOLT, 27, MON_SPELL_WIZARD },
       { SPELL_BATTLESPHERE, 27, MON_SPELL_WIZARD },
      }
    },

    {  MST_TENGU_REAVER_II,
      {
       { SPELL_BOLT_OF_MAGMA, 17, MON_SPELL_WIZARD },
       { SPELL_FIREBALL, 17, MON_SPELL_WIZARD },
       { SPELL_BATTLESPHERE, 34, MON_SPELL_WIZARD },
      }
    },

    {  MST_TENGU_REAVER_III,
      {
       { SPELL_VENOM_BOLT, 13, MON_SPELL_WIZARD },
       { SPELL_POISONOUS_CLOUD, 13, MON_SPELL_WIZARD },
       { SPELL_CORROSIVE_BOLT, 13, MON_SPELL_WIZARD },
       { SPELL_BATTLESPHERE, 27, MON_SPELL_WIZARD },
      }
    },

    // ('R') Spirituals.
    {  MST_RAKSHASA,
      {
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 20, MON_SPELL_MAGICAL },
       { SPELL_PHANTOM_MIRROR, 20, MON_SPELL_MAGICAL },
       { SPELL_BLINK, 10, MON_SPELL_MAGICAL },
      }
    },

    {  MST_EFREET,
      {
       { SPELL_BOLT_OF_FIRE, 29, MON_SPELL_MAGICAL },
       { SPELL_FIREBALL, 29, MON_SPELL_MAGICAL },
      }
    },

    {  MST_DRYAD,
      {
       { SPELL_AWAKEN_VINES, 23, MON_SPELL_WIZARD },
       { SPELL_AWAKEN_FOREST, 23, MON_SPELL_WIZARD },
       { SPELL_MINOR_HEALING, 11, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    // ('S') Snakes.
    {  MST_LAVA_SNAKE,
      {
       { SPELL_SPIT_LAVA, 100, MON_SPELL_NATURAL | MON_SPELL_BREATH },
      }
    },

    {  MST_GUARDIAN_SERPENT,
      {
       { SPELL_VENOM_BOLT, 19, MON_SPELL_WIZARD },
       { SPELL_SLOW, 19, MON_SPELL_WIZARD },
       { SPELL_BLINK_ALLIES_ENCIRCLE, 19, MON_SPELL_WIZARD },
      }
    },

    {  MST_SHOCK_SERPENT,
      {
       { SPELL_ELECTRICAL_BOLT, 33, MON_SPELL_NATURAL },
      }
    },

    // ('T') Trolls.
    {  MST_DEEP_TROLL_EARTH_MAGE,
      {
       { SPELL_LRD, 31, MON_SPELL_WIZARD },
       { SPELL_DIG, 31, MON_SPELL_WIZARD },
      }
    },

    {  MST_DEEP_TROLL_SHAMAN,
      {
       { SPELL_HASTE_OTHER, 21, MON_SPELL_PRIEST },
       { SPELL_MIGHT_OTHER, 21, MON_SPELL_PRIEST },
       { SPELL_MINOR_HEALING, 21, MON_SPELL_PRIEST | MON_SPELL_EMERGENCY },
      }
    },

    // ('V') Vampires.
    {  MST_VAMPIRE,
      {
       { SPELL_VAMPIRIC_DRAINING, 50, MON_SPELL_WIZARD },
       { SPELL_CONFUSE, 14, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 14, MON_SPELL_WIZARD },
      }
    },

    {  MST_VAMPIRE_MAGE,
      {
       { SPELL_BOLT_OF_DRAINING, 10, MON_SPELL_WIZARD },
       { SPELL_SUMMON_UNDEAD, 10, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 20, MON_SPELL_WIZARD },
       { SPELL_VAMPIRIC_DRAINING, 50, MON_SPELL_WIZARD },
      }
    },

    {  MST_VAMPIRE_KNIGHT,
      {
       { SPELL_BLINK_CLOSE, 12, MON_SPELL_WIZARD },
       { SPELL_PARALYSE, 12, MON_SPELL_WIZARD },
       { SPELL_HASTE, 12, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 12, MON_SPELL_WIZARD },
       { SPELL_VAMPIRIC_DRAINING, 50, MON_SPELL_WIZARD },
      }
    },

    // ('W') Incorporeal undead.
    {  MST_SHADOW,
      {
       { SPELL_INVISIBILITY, 33, MON_SPELL_NATURAL },
      }
    },

    {  MST_PHANTASMAL_WARRIOR,
      {
       { SPELL_BLINK_CLOSE, 67, MON_SPELL_MAGICAL },
      }
    },

    {  MST_EIDOLON,
      {
       { SPELL_BOLT_OF_DRAINING, 21, MON_SPELL_MAGICAL },
       { SPELL_CAUSE_FEAR, 21, MON_SPELL_MAGICAL },
      }
    },

    {  MST_FLAYED_GHOST,
      {
       { SPELL_FLAY, 40, MON_SPELL_MAGICAL },
      }
    },

    // ('X') Greater horrors and tentacled things.
    {  MST_KRAKEN,
      {
       { SPELL_CREATE_TENTACLES, 44, MON_SPELL_NATURAL },
       { SPELL_INK_CLOUD, 22, MON_SPELL_NATURAL | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_THRASHING_HORROR,
      {
       { SPELL_MIGHT, 59, MON_SPELL_NATURAL | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_TENTACLED_STARSPAWN,
      {
       { SPELL_CREATE_TENTACLES, 66, MON_SPELL_NATURAL },
      }
    },

    // ('Y') Bovids and elephants.
    {  MST_CATOBLEPAS,
      {
       { SPELL_PETRIFYING_CLOUD, 41, MON_SPELL_NATURAL },
      }
    },

    { MST_DREAM_SHEEP,
      {
       { SPELL_DREAM_DUST, 40, MON_SPELL_NATURAL },
      }
    },

    {  MST_HELLEPHANT,
      {
       { SPELL_FIRE_BREATH, 35, MON_SPELL_NATURAL | MON_SPELL_BREATH },
       { SPELL_BLINK, 35, MON_SPELL_MAGICAL },
      }
    },

    // ('5') Minor demons.
    {  MST_WHITE_IMP,
      {
       { SPELL_THROW_FROST, 52, MON_SPELL_MAGICAL },
      }
    },

    {  MST_SHADOW_IMP,
      {
       { SPELL_PAIN, 17, MON_SPELL_MAGICAL },
      }
    },

    // ('3') Common demons.
    {  MST_YNOXINUL,
      {
       { SPELL_IRON_SHOT, 19, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_UFETUBUS, 38, MON_SPELL_MAGICAL },
      }
    },

    {  MST_SMOKE_DEMON,
      {
       { SPELL_STICKY_FLAME_RANGE, 19, MON_SPELL_MAGICAL },
       { SPELL_STEAM_BALL, 19, MON_SPELL_MAGICAL },
       { SPELL_SMITING, 19, MON_SPELL_MAGICAL },
      }
    },

    {  MST_SOUL_EATER,
      {
       { SPELL_DRAIN_LIFE, 30, MON_SPELL_MAGICAL },
      }
    },

    {  MST_NEQOXEC,
      {
       { SPELL_MALMUTATE, 19, MON_SPELL_MAGICAL },
       { SPELL_BRAIN_FEED, 19, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_MINOR_DEMON, 19, MON_SPELL_MAGICAL },
      }
    },

    // ('2') Greater demons.
    {  MST_SHADOW_DEMON,
      {
       { SPELL_SHADOW_CREATURES, 30, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_DEMON, 30, MON_SPELL_MAGICAL },
      }
    },

    {  MST_GREEN_DEATH,
      {
       { SPELL_POISON_ARROW, 21, MON_SPELL_MAGICAL },
       { SPELL_POISONOUS_CLOUD, 21, MON_SPELL_MAGICAL },
       { SPELL_VENOM_BOLT, 21, MON_SPELL_MAGICAL },
      }
    },

    {  MST_BALRUG,
      {
       { SPELL_STICKY_FLAME_RANGE, 16, MON_SPELL_MAGICAL },
       { SPELL_BOLT_OF_FIRE, 16, MON_SPELL_MAGICAL },
       { SPELL_FIREBALL, 16, MON_SPELL_MAGICAL },
       { SPELL_SMITING, 16, MON_SPELL_MAGICAL },
      }
    },

    {  MST_BLIZZARD_DEMON,
      {
       { SPELL_BOLT_OF_COLD, 16, MON_SPELL_MAGICAL },
       { SPELL_LIGHTNING_BOLT, 16, MON_SPELL_MAGICAL },
       { SPELL_FREEZING_CLOUD, 16, MON_SPELL_MAGICAL },
       { SPELL_AIRSTRIKE, 16, MON_SPELL_MAGICAL },
      }
    },

    {  MST_CACODEMON,
      {
       { SPELL_ENERGY_BOLT, 13, MON_SPELL_MAGICAL },
       { SPELL_SLOW, 13, MON_SPELL_MAGICAL },
       { SPELL_CONFUSE, 13, MON_SPELL_MAGICAL },
       { SPELL_MALMUTATE, 13, MON_SPELL_MAGICAL },
       { SPELL_DIG, 13, MON_SPELL_MAGICAL },
      }
    },

    {  MST_HELLION,
      {
       { SPELL_CALL_DOWN_DAMNATION, 57, MON_SPELL_MAGICAL },
      }
    },

    {  MST_TORMENTOR,
      {
       { SPELL_PAIN, 28, MON_SPELL_MAGICAL },
       { SPELL_SYMBOL_OF_TORMENT, 29, MON_SPELL_MAGICAL },
      }
    },

    // ('1') Fiends and friends.
    {  MST_EXECUTIONER,
      {
       { SPELL_PAIN, 21, MON_SPELL_MAGICAL },
       { SPELL_HASTE, 41, MON_SPELL_MAGICAL },
      }
    },

    {  MST_BRIMSTONE_FIEND,
      {
       { SPELL_HURL_DAMNATION, 34, MON_SPELL_MAGICAL },
       { SPELL_SYMBOL_OF_TORMENT, 17, MON_SPELL_MAGICAL },
      }
    },

    {  MST_ICE_FIEND,
      {
       { SPELL_BOLT_OF_COLD, 46, MON_SPELL_MAGICAL },
       { SPELL_SYMBOL_OF_TORMENT, 23, MON_SPELL_MAGICAL },
      }
    },

    {  MST_TZITZIMITL,
      {
       { SPELL_BOLT_OF_COLD, 15, MON_SPELL_MAGICAL },
       { SPELL_BOLT_OF_DRAINING, 15, MON_SPELL_MAGICAL },
       { SPELL_SYMBOL_OF_TORMENT, 25, MON_SPELL_MAGICAL },
       { SPELL_DISPEL_UNDEAD, 25, MON_SPELL_MAGICAL },
      }
    },

    {  MST_HELL_SENTINEL,
      {
       { SPELL_HURL_DAMNATION, 28, MON_SPELL_MAGICAL },
       { SPELL_IRON_SHOT, 14, MON_SPELL_MAGICAL },
      }
    },

    // ('6') Demonspawn.
    {  MST_DEMONSPAWN_WARMONGER,
      {
       { SPELL_SAP_MAGIC, 18, MON_SPELL_PRIEST },
       { SPELL_HASTE_OTHER, 32, MON_SPELL_PRIEST },
      }
    },

    {  MST_DEMONSPAWN_BLOOD_SAINT,
      {
       { SPELL_LEGENDARY_DESTRUCTION, 40, MON_SPELL_PRIEST },
       { SPELL_CALL_OF_CHAOS, 20, MON_SPELL_PRIEST },
      }
    },

    {  MST_DEMONSPAWN_CORRUPTER,
      {
       { SPELL_PLANEREND, 40, MON_SPELL_PRIEST },
       { SPELL_CORRUPTING_PULSE, 20, MON_SPELL_PRIEST },
      }
    },

    {  MST_DEMONSPAWN_BLACK_SUN,
      {
       { SPELL_BOLT_OF_DRAINING, 18, MON_SPELL_PRIEST },
       { SPELL_DISPEL_UNDEAD, 18, MON_SPELL_PRIEST },
       { SPELL_BLACK_MARK, 28, MON_SPELL_PRIEST },
      }
    },

    // ('8') Animate / statues.
    {  MST_USHABTI,
      {
       { SPELL_DEATH_RATTLE, 36, MON_SPELL_MAGICAL },
       { SPELL_WARNING_CRY, 56, MON_SPELL_NATURAL | MON_SPELL_NO_SILENT
                                | MON_SPELL_BREATH },
       { SPELL_DISPEL_UNDEAD, 44, MON_SPELL_MAGICAL },
      }
    },

    {  MST_PEACEKEEPER,
      {
       { SPELL_THROW_BARBS, 33, MON_SPELL_NATURAL },
       { SPELL_BATTLECRY, 25, MON_SPELL_NATURAL | MON_SPELL_NO_SILENT
                              | MON_SPELL_NOISY },
      }
    },

    {  MST_CRYSTAL_GUARDIAN,
      {
       { SPELL_CRYSTAL_BOLT, 66, MON_SPELL_MAGICAL },
      }
    },

    {  MST_ELECTRIC_GOLEM,
      {
       { SPELL_LIGHTNING_BOLT, 44, MON_SPELL_MAGICAL },
       { SPELL_BLINK, 22, MON_SPELL_MAGICAL },
      }
    },

    {  MST_ICE_STATUE,
      {
       { SPELL_BOLT_OF_COLD, 20, MON_SPELL_MAGICAL },
       { SPELL_THROW_ICICLE, 20, MON_SPELL_MAGICAL },
       { SPELL_FREEZING_CLOUD, 20, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_ICE_BEAST, 40, MON_SPELL_MAGICAL },
      }
    },

    {  MST_OBSIDIAN_STATUE,
      {
       { SPELL_SUMMON_MINOR_DEMON, 33, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_DEMON, 33, MON_SPELL_MAGICAL },
       { SPELL_MESMERISE, 33, MON_SPELL_MAGICAL },
      }
    },

    {  MST_ORANGE_CRYSTAL_STATUE,
      {
       { SPELL_SHADOW_CREATURES, 40, MON_SPELL_MAGICAL },
       { SPELL_BRAIN_FEED, 20, MON_SPELL_MAGICAL },
       { SPELL_DRAINING_GAZE, 20, MON_SPELL_MAGICAL },
       { SPELL_MASS_CONFUSION, 20, MON_SPELL_MAGICAL },
      }
    },

    // ('9') Gargoyles.
    {  MST_GARGOYLE,
      {
       { SPELL_STONE_ARROW, 56, MON_SPELL_NATURAL },
      }
    },

    {  MST_MOLTEN_GARGOYLE,
      {
       { SPELL_BOLT_OF_MAGMA, 57, MON_SPELL_NATURAL },
      }
    },

    // ('*') Concentrated orbs.
    {  MST_WRETCHED_STAR,
      {
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 60, MON_SPELL_MAGICAL },
       { SPELL_CORRUPTING_PULSE, 40, MON_SPELL_MAGICAL },
      }
    },

    {  MST_ORB_OF_FIRE,
      {
       { SPELL_BOLT_OF_FIRE, 32, MON_SPELL_MAGICAL },
       { SPELL_FIREBALL, 32, MON_SPELL_MAGICAL },
       { SPELL_MALMUTATE, 16, MON_SPELL_MAGICAL },
      }
    },

    // ---------------------
    // Uniques' spellbooks
    // ---------------------

    // ('c') Centaurs and such.
    {  MST_NESSOS,
      {
       { SPELL_BLINK_RANGE, 30, MON_SPELL_WIZARD },
       { SPELL_HASTE, 30, MON_SPELL_WIZARD },
      }
    },

    // ('d') Draconians.
    { MST_BAI_SUZHEN,
      {
        { SPELL_SUMMON_HYDRA, 40, MON_SPELL_WIZARD },
      }
    },

    // ('e') Elves.
    {  MST_DOWAN,
      {
       { SPELL_THROW_FLAME, 9, MON_SPELL_WIZARD },
       { SPELL_THROW_FROST, 9, MON_SPELL_WIZARD },
       { SPELL_BLINK, 9, MON_SPELL_WIZARD },
       { SPELL_CORONA, 9, MON_SPELL_WIZARD },
       { SPELL_HASTE_OTHER, 9, MON_SPELL_WIZARD },
      }
    },

    {  MST_FANNAR,
      {
       { SPELL_BOLT_OF_COLD, 10, MON_SPELL_WIZARD },
       { SPELL_OZOCUBUS_REFRIGERATION, 20, MON_SPELL_WIZARD },
       { SPELL_SUMMON_ICE_BEAST, 20, MON_SPELL_WIZARD },
       { SPELL_OZOCUBUS_ARMOUR, 10, MON_SPELL_WIZARD },
       { SPELL_BLINK, 10, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    // ('g') Small humanoids.
    {  MST_ROBIN,
      {
        { SPELL_BATTLECRY, 50, MON_SPELL_NATURAL | MON_SPELL_NOISY
                               | MON_SPELL_NO_SILENT },
        { SPELL_THROW_ALLY, 80, MON_SPELL_NATURAL }
      }
    },

    {  MST_JORGRUN,
      {
       { SPELL_LRD, 16, MON_SPELL_WIZARD },
       { SPELL_SHATTER, 16, MON_SPELL_WIZARD },
       { SPELL_PETRIFY, 16, MON_SPELL_WIZARD },
       { SPELL_DIG, 16, MON_SPELL_WIZARD },
      }
    },

    // ('h') Carnivorous quadrupeds.
    {  MST_NATASHA,
      {
       { SPELL_MAGIC_DART, 13, MON_SPELL_WIZARD },
       { SPELL_CALL_IMP, 13, MON_SPELL_WIZARD },
       { SPELL_SLOW, 13, MON_SPELL_WIZARD },
      }
    },

    // ('i') Spriggans.
    { MST_THE_ENCHANTRESS,
      {
       { SPELL_SLOW, 11, MON_SPELL_WIZARD },
       { SPELL_DIMENSION_ANCHOR, 11, MON_SPELL_WIZARD },
       { SPELL_MASS_CONFUSION, 11, MON_SPELL_WIZARD },
       { SPELL_STRIP_RESISTANCE, 11, MON_SPELL_WIZARD },
       { SPELL_HASTE, 11, MON_SPELL_WIZARD },
       { SPELL_DEFLECT_MISSILES, 11, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 11, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    // ('m') Merfolk and friends.
    { MST_ILSUIW,
      {
       { SPELL_THROW_ICICLE, 11, MON_SPELL_WIZARD },
       { SPELL_WATER_ELEMENTALS, 22, MON_SPELL_WIZARD },
       { SPELL_CALL_TIDE, 11, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 11, MON_SPELL_WIZARD },
       { SPELL_BLINK, 11, MON_SPELL_WIZARD },
      }
    },

    // ('o') Orcs.
    {  MST_NERGALLE,
      {
       { SPELL_BOLT_OF_DRAINING, 12, MON_SPELL_WIZARD },
       { SPELL_SUMMON_SPECTRAL_ORCS, 24, MON_SPELL_WIZARD },
       { SPELL_HASTE_OTHER, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_SAINT_ROKA,
      {
       { SPELL_BATTLECRY, 100, MON_SPELL_NATURAL | MON_SPELL_NOISY
                               | MON_SPELL_NO_SILENT },
       { SPELL_SMITING, 64, MON_SPELL_PRIEST },
      }
    },

    // ('w') Slugs and worms.
    {  MST_GASTRONOK,
      {
       { SPELL_CANTRIP, 12, MON_SPELL_WIZARD },
       { SPELL_AIRSTRIKE, 23, MON_SPELL_WIZARD },
       { SPELL_SUMMON_SMALL_MAMMAL, 12, MON_SPELL_WIZARD },
       { SPELL_SLOW, 12, MON_SPELL_WIZARD },
       { SPELL_SPRINT, 12, MON_SPELL_WIZARD },
      }
    },

    // ('z') Corporeal undead.
    {  MST_MURRAY,
      {
       { SPELL_SYMBOL_OF_TORMENT, 11, MON_SPELL_MAGICAL | MON_SPELL_NOISY },
       { SPELL_SUMMON_UNDEAD, 33, MON_SPELL_MAGICAL | MON_SPELL_NOISY },
       { SPELL_BLACK_MARK, 22, MON_SPELL_MAGICAL | MON_SPELL_NOISY },
      }
    },

    // ('A') Angels.
    {  MST_MENNAS,
      {
       { SPELL_MASS_CONFUSION, 17, MON_SPELL_WIZARD },
       { SPELL_SILENCE, 17, MON_SPELL_WIZARD },
       { SPELL_MINOR_HEALING, 34, MON_SPELL_WIZARD },
      }
    },

    // ('C') Giants.
    {  MST_POLYPHEMUS,
      {
        { SPELL_THROW_ALLY, 50, MON_SPELL_NATURAL }
      }
    },

    // ('D') Dragons.
    {  MST_XTAHUA,
      {
       { SPELL_SEARING_BREATH, 62, MON_SPELL_NATURAL | MON_SPELL_BREATH
                                    | MON_SPELL_NOISY },
       { SPELL_PARALYSE, 18, MON_SPELL_MAGICAL | MON_SPELL_NO_SILENT },
      }
    },

    { MST_BAI_SUZHEN_DRAGON,
      {
        { SPELL_PRIMAL_WAVE, 60, MON_SPELL_NATURAL | MON_SPELL_BREATH },
        { SPELL_RING_OF_THUNDER, 100, MON_SPELL_NATURAL | MON_SPELL_INSTANT },
      }
    },

    {  MST_SERPENT_OF_HELL_GEH,
      {
       { SPELL_SERPENT_OF_HELL_GEH_BREATH, 35,
           MON_SPELL_NATURAL | MON_SPELL_BREATH | MON_SPELL_NOISY },
       { SPELL_SUMMON_DRAGON, 35, MON_SPELL_MAGICAL },
      }
    },

    {  MST_SERPENT_OF_HELL_COC,
      {
       { SPELL_SERPENT_OF_HELL_COC_BREATH, 35,
         MON_SPELL_NATURAL | MON_SPELL_BREATH | MON_SPELL_NOISY },
       { SPELL_SUMMON_DRAGON, 35, MON_SPELL_MAGICAL },
      }
    },

    {  MST_SERPENT_OF_HELL_DIS,
      {
       { SPELL_SERPENT_OF_HELL_DIS_BREATH, 35,
         MON_SPELL_NATURAL | MON_SPELL_BREATH | MON_SPELL_NOISY },
       { SPELL_SUMMON_DRAGON, 35, MON_SPELL_MAGICAL },
      }
    },

    {  MST_SERPENT_OF_HELL_TAR,
      {
       { SPELL_SERPENT_OF_HELL_TAR_BREATH, 35,
         MON_SPELL_NATURAL | MON_SPELL_BREATH | MON_SPELL_NOISY },
       { SPELL_SUMMON_DRAGON, 35, MON_SPELL_MAGICAL },
      }
    },

    // ('F') Frogs.
    {  MST_PRINCE_RIBBIT,
      {
       { SPELL_BLINK, 29, MON_SPELL_MAGICAL },
       { SPELL_TELEPORT_SELF, 66, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    // ('H') Hybrids.
    {  MST_ARACHNE,
      {
       { SPELL_VENOM_BOLT, 11, MON_SPELL_WIZARD },
       { SPELL_POISON_ARROW, 11, MON_SPELL_WIZARD },
       { SPELL_ENSNARE, 34, MON_SPELL_WIZARD },
       { SPELL_BLINK, 11, MON_SPELL_WIZARD },
      }
    },

    {  MST_ASTERION,
      {
       { SPELL_MAJOR_DESTRUCTION, 11, MON_SPELL_PRIEST },
       { SPELL_GREATER_SERVANT_MAKHLEB, 22, MON_SPELL_PRIEST },
       { SPELL_HASTE, 32, MON_SPELL_PRIEST },
      }
    },

    // ('J') Jellies.
    {  MST_DISSOLUTION,
      {
       { SPELL_SUMMON_EYEBALLS, 62, MON_SPELL_PRIEST },
      }
    },

    // ('K') Kobolds.
    {  MST_SONJA,
      {
       { SPELL_BLINK, 28, MON_SPELL_WIZARD },
       { SPELL_TELEPORT_SELF, 28, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    // ('L') Liches.
    {  MST_BORIS,
      {
       { SPELL_BOLT_OF_COLD, 12, MON_SPELL_WIZARD },
       { SPELL_IRON_SHOT, 12, MON_SPELL_WIZARD },
       { SPELL_IOOD, 12, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 24, MON_SPELL_WIZARD },
      }
    },

    // ('M') Mummies.
    {  MST_MENKAURE,
      {
       { SPELL_PAIN, 18, MON_SPELL_WIZARD },
       { SPELL_HASTE, 18, MON_SPELL_WIZARD },
       { SPELL_SYMBOL_OF_TORMENT, 18, MON_SPELL_WIZARD },
      }
    },

    {  MST_KHUFU,
      {
       { SPELL_SMITING, 23, MON_SPELL_WIZARD },
       { SPELL_SYMBOL_OF_TORMENT, 11, MON_SPELL_WIZARD },
       { SPELL_SUMMON_EMPEROR_SCORPIONS, 11, MON_SPELL_WIZARD },
       { SPELL_SUMMON_UNDEAD, 11, MON_SPELL_WIZARD },
       { SPELL_TOMB_OF_DOROKLOHE, 11, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    // ('N') Naga.
    {  MST_VASHNIA,
      {
       { SPELL_SPIT_POISON, 55, MON_SPELL_NATURAL | MON_SPELL_BREATH },
       { SPELL_PORTAL_PROJECTILE, 22, MON_SPELL_WIZARD },
       { SPELL_BLINK_OTHER, 11, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
       { SPELL_BLINK_ALLIES_AWAY, 22, MON_SPELL_WIZARD },
       { SPELL_BLINK_AWAY, 11, MON_SPELL_WIZARD },
      }
    },

    // ('Q') Tengu.
    {  MST_SOJOBO,
      {
       { SPELL_WIND_BLAST, 28, MON_SPELL_WIZARD },
       { SPELL_LIGHTNING_BOLT, 14, MON_SPELL_WIZARD },
       { SPELL_AIRSTRIKE, 14, MON_SPELL_WIZARD },
       { SPELL_AIR_ELEMENTALS, 14, MON_SPELL_WIZARD },
       { SPELL_DEFLECT_MISSILES, 14, MON_SPELL_WIZARD },
      }
    },

    // ('R') Spiritual beings.
    {  MST_AZRAEL,
      {
       { SPELL_BOLT_OF_FIRE, 13, MON_SPELL_MAGICAL },
       { SPELL_STICKY_FLAME_RANGE, 13, MON_SPELL_MAGICAL },
       { SPELL_FIREBALL, 13, MON_SPELL_MAGICAL },
       { SPELL_HURL_DAMNATION, 13, MON_SPELL_MAGICAL },
       { SPELL_CALL_DOWN_DAMNATION, 13, MON_SPELL_MAGICAL | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_MARA,
      {
       { SPELL_PAIN, 14, MON_SPELL_MAGICAL },
       { SPELL_BOLT_OF_FIRE, 14, MON_SPELL_MAGICAL },
       { SPELL_FAKE_MARA_SUMMON, 14, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_ILLUSION, 14, MON_SPELL_MAGICAL },
       { SPELL_BLINK, 29, MON_SPELL_MAGICAL },
      }
    },

    // ('S') Snakes.
    {  MST_AIZUL,
      {
       { SPELL_VENOM_BOLT, 32, MON_SPELL_WIZARD },
       { SPELL_POISON_ARROW, 16, MON_SPELL_WIZARD },
       { SPELL_SLEEP, 16, MON_SPELL_WIZARD },
      }
    },

    // ('T') Trolls.
    {  MST_SNORG,
      {
       { SPELL_BERSERKER_RAGE, 33, MON_SPELL_NATURAL },
      }
    },

    {  MST_MOON_TROLL,
      {
       { SPELL_CORROSIVE_BOLT, 50, MON_SPELL_WIZARD },
      }
    },

    // ('V') Vampires.
    {  MST_JORY,
      {
       { SPELL_VAMPIRIC_DRAINING, 50, MON_SPELL_WIZARD },
       { SPELL_LEHUDIBS_CRYSTAL_SPEAR, 17, MON_SPELL_WIZARD },
       { SPELL_MESMERISE, 17, MON_SPELL_WIZARD },
       { SPELL_BLINK_CLOSE, 17, MON_SPELL_WIZARD },
      }
    },

    // ('5') Lesser demons.
    {  MST_GRINDER,
      {
       { SPELL_PAIN, 19, MON_SPELL_MAGICAL },
       { SPELL_PARALYSE, 19, MON_SPELL_MAGICAL },
       { SPELL_BLINK, 19, MON_SPELL_MAGICAL },
      }
    },

    // ('1') Greater demons.
    {  MST_IGNACIO,
      {
       { SPELL_AGONY, 21, MON_SPELL_MAGICAL },
       { SPELL_HASTE, 41, MON_SPELL_MAGICAL },
      }
    },

    // ('8') Statues.
    { MST_ROXANNE,
      {
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 13, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_MAGMA, 13, MON_SPELL_WIZARD },
       { SPELL_IRON_SHOT, 13, MON_SPELL_WIZARD },
       { SPELL_LEHUDIBS_CRYSTAL_SPEAR, 13, MON_SPELL_WIZARD },
       { SPELL_BLINK_OTHER_CLOSE, 13, MON_SPELL_WIZARD },
      }
    },

    // ('@') Human (or close enough to) uniques.
    {  MST_JESSICA,
      {
       { SPELL_PAIN, 17, MON_SPELL_WIZARD },
       { SPELL_SLOW, 8, MON_SPELL_WIZARD },
       { SPELL_HASTE, 8, MON_SPELL_WIZARD },
       { SPELL_BLINK, 17, MON_SPELL_WIZARD },
      }
    },

    { MST_EUSTACHIO,
      {
       { SPELL_SUMMON_SMALL_MAMMAL, 14, MON_SPELL_WIZARD },
       { SPELL_SUMMON_MINOR_DEMON, 14, MON_SPELL_WIZARD },
       { SPELL_BLINK, 27, MON_SPELL_WIZARD },
      }
    },

    {  MST_ERICA,
      {
       { SPELL_VENOM_BOLT, 10, MON_SPELL_WIZARD },
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 10, MON_SPELL_WIZARD },
       { SPELL_CONFUSE, 10, MON_SPELL_WIZARD },
       { SPELL_SLOW, 10, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 10, MON_SPELL_WIZARD },
      }
    },

    {  MST_MAURICE,
      {
       { SPELL_SPRINT, 14, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 14, MON_SPELL_WIZARD },
       { SPELL_BLINK, 14, MON_SPELL_WIZARD },
       { SPELL_TELEPORT_SELF, 14, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_HAROLD,
      {
       { SPELL_BOLT_OF_FIRE, 30, MON_SPELL_WIZARD },
       { SPELL_BLINK, 29, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_JOSEPHINE,
      {
       { SPELL_VAMPIRIC_DRAINING, 12, MON_SPELL_WIZARD },
       { SPELL_GHOSTLY_FIREBALL, 24, MON_SPELL_WIZARD },
       { SPELL_DISPEL_UNDEAD, 12, MON_SPELL_WIZARD },
      }
    },

    {  MST_RUPERT,
      {
       { SPELL_PARALYSE, 16, MON_SPELL_MAGICAL | MON_SPELL_NO_SILENT },
       { SPELL_CONFUSE, 16, MON_SPELL_MAGICAL | MON_SPELL_NO_SILENT },
       { SPELL_BERSERKER_RAGE, 33, MON_SPELL_MAGICAL },
      }
    },

    {  MST_LOUISE,
      {
       { SPELL_STING, 10, MON_SPELL_WIZARD },
       { SPELL_STONE_ARROW, 10, MON_SPELL_WIZARD },
       { SPELL_LIGHTNING_BOLT, 10, MON_SPELL_WIZARD },
       { SPELL_BANISHMENT, 10, MON_SPELL_WIZARD },
       { SPELL_BLINK, 10, MON_SPELL_WIZARD },
      }
    },

    {  MST_FRANCES,
      {
       { SPELL_THROW_ICICLE, 12, MON_SPELL_WIZARD },
       { SPELL_IRON_SHOT, 12, MON_SPELL_WIZARD },
       { SPELL_SUMMON_DEMON, 20, MON_SPELL_WIZARD },
       { SPELL_HASTE, 20, MON_SPELL_WIZARD },
      }
    },

    {  MST_KIRKE,
      {
       { SPELL_PORKALATOR, 22, MON_SPELL_WIZARD },
       { SPELL_SLOW, 11, MON_SPELL_WIZARD },
       { SPELL_MONSTROUS_MENAGERIE, 11, MON_SPELL_WIZARD },
       { SPELL_INVISIBILITY, 11, MON_SPELL_WIZARD | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_DONALD,
      {
       { SPELL_MIGHT, 34, MON_SPELL_WIZARD },
       { SPELL_HASTE, 34, MON_SPELL_WIZARD },
      }
    },

    {  MST_HELLBINDER,
      {
       { SPELL_HURL_DAMNATION, 14, MON_SPELL_WIZARD },
       { SPELL_SUMMON_MINOR_DEMON, 14, MON_SPELL_WIZARD },
       { SPELL_SUMMON_DEMON, 14, MON_SPELL_WIZARD },
       { SPELL_HASTE, 14, MON_SPELL_WIZARD },
       { SPELL_BLINK_AWAY, 14, MON_SPELL_WIZARD },
      }
    },

    {  MST_CLOUD_MAGE,
      {
       { SPELL_AIRSTRIKE, 25, MON_SPELL_WIZARD },
       { SPELL_TORNADO, 40, MON_SPELL_WIZARD },
      }
    },

    {  MST_NIKOLA,
      {
       { SPELL_SHOCK, 11, MON_SPELL_WIZARD },
       { SPELL_LIGHTNING_BOLT, 11, MON_SPELL_WIZARD },
       { SPELL_CHAIN_LIGHTNING, 23, MON_SPELL_WIZARD },
       { SPELL_BLINK, 23, MON_SPELL_WIZARD },
      }
    },

    {  MST_MARGERY,
      {
       { SPELL_BOLT_OF_FIRE, 36, MON_SPELL_WIZARD },
       { SPELL_FIREBALL, 36, MON_SPELL_WIZARD },
      }
    },

    {  MST_FREDERICK,
      {
       { SPELL_ISKENDERUNS_MYSTIC_BLAST, 12, MON_SPELL_WIZARD },
       { SPELL_BOLT_OF_COLD, 12, MON_SPELL_WIZARD },
       { SPELL_IRON_SHOT, 12, MON_SPELL_WIZARD },
       { SPELL_SPELLFORGED_SERVITOR, 24, MON_SPELL_WIZARD },
      }
    },
    // ('&', mostly) Demon lords.
    {  MST_GERYON,
      {
       { SPELL_SUMMON_HELL_BEAST, 65, MON_SPELL_MAGICAL | MON_SPELL_NO_SILENT },
      }
    },

    {  MST_ASMODEUS,
      {
       { SPELL_BOLT_OF_FIRE, 22, MON_SPELL_MAGICAL },
       { SPELL_HURL_DAMNATION, 22, MON_SPELL_MAGICAL },
       { SPELL_FIRE_SUMMON, 22, MON_SPELL_MAGICAL },
      }
    },

    {  MST_ANTAEUS,
      {
       { SPELL_LIGHTNING_BOLT, 24, MON_SPELL_MAGICAL },
       { SPELL_FLASH_FREEZE, 48, MON_SPELL_MAGICAL },
      }
    },

    {  MST_ERESHKIGAL,
      {
       { SPELL_BOLT_OF_COLD, 11, MON_SPELL_MAGICAL },
       { SPELL_SYMBOL_OF_TORMENT, 11, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_GREATER_DEMON, 11, MON_SPELL_MAGICAL },
       { SPELL_PARALYSE, 11, MON_SPELL_MAGICAL },
       { SPELL_SILENCE, 11, MON_SPELL_MAGICAL },
       { SPELL_MAJOR_HEALING, 11, MON_SPELL_MAGICAL | MON_SPELL_EMERGENCY },
      }
    },

    {  MST_DISPATER,
      {
       { SPELL_IRON_SHOT, 13, MON_SPELL_MAGICAL },
       { SPELL_LEHUDIBS_CRYSTAL_SPEAR, 13, MON_SPELL_MAGICAL },
       { SPELL_HURL_DAMNATION, 13, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_GREATER_DEMON, 26, MON_SPELL_MAGICAL },
      }
    },

    {  MST_MNOLEG,
      {
       { SPELL_RANDOM_BOLT, 13, MON_SPELL_MAGICAL },
       { SPELL_MALIGN_GATEWAY, 27, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_EYEBALLS, 13, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_HORRIBLE_THINGS, 27, MON_SPELL_MAGICAL },
       { SPELL_CALL_OF_CHAOS, 27, MON_SPELL_MAGICAL },
       { SPELL_DIG, 13, MON_SPELL_MAGICAL },
      }
    },

    {  MST_LOM_LOBON,
      {
       { SPELL_CONJURE_BALL_LIGHTNING, 30, MON_SPELL_MAGICAL },
       { SPELL_GLACIATE, 30, MON_SPELL_MAGICAL },
       { SPELL_TORNADO, 60, MON_SPELL_MAGICAL },
       { SPELL_MAJOR_HEALING, 30, MON_SPELL_MAGICAL },
       { SPELL_BLINK_RANGE, 30, MON_SPELL_MAGICAL },
      }
    },

    {  MST_CEREBOV,
      {
       { SPELL_IRON_SHOT, 11, MON_SPELL_MAGICAL },
       { SPELL_FIRE_STORM, 11, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_GREATER_DEMON, 11, MON_SPELL_MAGICAL },
       { SPELL_HASTE, 36, MON_SPELL_MAGICAL },
      }
    },

    {  MST_GLOORX_VLOQ,
      {
       { SPELL_POISON_ARROW, 20, MON_SPELL_MAGICAL },
       { SPELL_MIASMA_BREATH, 20, MON_SPELL_MAGICAL },
       { SPELL_SYMBOL_OF_TORMENT, 20, MON_SPELL_MAGICAL },
       { SPELL_DISPEL_UNDEAD, 20, MON_SPELL_MAGICAL },
       { SPELL_SUMMON_EXECUTIONERS, 40, MON_SPELL_MAGICAL },
      }
    },

    // A monster that doesn't show up anywhere, for Arena testing.
    {  MST_TEST_SPAWNER,
      {
       { SPELL_SHADOW_CREATURES, 100, MON_SPELL_MAGICAL },
       { SPELL_PLANEREND, 67, MON_SPELL_MAGICAL },
       { SPELL_PHANTOM_MIRROR, 33, MON_SPELL_MAGICAL },
      }
    },

};
