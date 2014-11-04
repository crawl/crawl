/**
 * @file
 * @brief Global (ick) enums.
**/

#ifndef ENUM_H
#define ENUM_H

#include "tag-version.h"

enum lang_t
{
    LANG_EN = 0,
    LANG_CS,
    LANG_DA,
    LANG_DE,
    LANG_EL,
    LANG_ES,
    LANG_FI,
    LANG_FR,
    LANG_HU,
    LANG_IT,
    LANG_JA,
    LANG_KO,
    LANG_LT,
    LANG_LV,
    LANG_NL,
    LANG_PL,
    LANG_PT,
    LANG_RU,
    LANG_SV,
    LANG_ZH,
    // fake languages
    LANG_DWARVEN,
    LANG_JAGERKIN,
    LANG_KRAUT,
    LANG_FUTHARK,
    LANG_WIDE,
    LANG_GRUNT
};

enum ability_type
{
    ABIL_NON_ABILITY = -1,
    // Innate abilities and (Demonspawn) mutations.
    ABIL_SPIT_POISON = 1,
    ABIL_BREATHE_FIRE,
    ABIL_BREATHE_FROST,
    ABIL_BREATHE_POISON,
    ABIL_BREATHE_LIGHTNING,
    ABIL_BREATHE_POWER,
    ABIL_BREATHE_STICKY_FLAME,
    ABIL_BREATHE_STEAM,
    ABIL_BREATHE_MEPHITIC,
    ABIL_SPIT_ACID,
    ABIL_BLINK,
    // Others
    ABIL_DELAYED_FIREBALL,
    ABIL_END_TRANSFORMATION,
    ABIL_STOP_SINGING, // From song of slaying

    // Species-specific abilities.
    // Demonspawn-only
    ABIL_HELLFIRE,
    // Tengu, Draconians
    ABIL_FLY,
#if TAG_MAJOR_VERSION == 34
    ABIL_WISP_BLINK,
#endif
    ABIL_STOP_FLYING,
    // Mummies
    ABIL_MUMMY_RESTORATION,
    // Vampires
    ABIL_TRAN_BAT,
    ABIL_BOTTLE_BLOOD,
    // Deep Dwarves
    ABIL_RECHARGING,
    // Formicids
    ABIL_DIG,
    ABIL_SHAFT_SELF,
    ABIL_MAX_INTRINSIC = ABIL_SHAFT_SELF,

    // Evoking items.
    ABIL_EVOKE_BERSERK = 40,
    ABIL_MIN_EVOKE = ABIL_EVOKE_BERSERK,
    ABIL_EVOKE_TELEPORTATION,
    ABIL_EVOKE_BLINK,
    ABIL_EVOKE_TURN_INVISIBLE,
    ABIL_EVOKE_TURN_VISIBLE,
    ABIL_EVOKE_FLIGHT,
#if TAG_MAJOR_VERSION == 34
    ABIL_EVOKE_STOP_LEVITATING,
#endif
    ABIL_EVOKE_FOG,
    ABIL_EVOKE_TELEPORT_CONTROL,
    ABIL_MAX_EVOKE = ABIL_EVOKE_TELEPORT_CONTROL,

    // Divine abilities
    // Zin
    ABIL_ZIN_SUSTENANCE = 1000,
    ABIL_ZIN_RECITE,
    ABIL_ZIN_VITALISATION,
    ABIL_ZIN_IMPRISON,
    ABIL_ZIN_SANCTUARY,
    ABIL_ZIN_CURE_ALL_MUTATIONS,
    // TSO
    ABIL_TSO_DIVINE_SHIELD = 1010,
    ABIL_TSO_CLEANSING_FLAME,
    ABIL_TSO_SUMMON_DIVINE_WARRIOR,
    // Kiku
    ABIL_KIKU_RECEIVE_CORPSES = 1020,
    ABIL_KIKU_TORMENT,
    // Yredelemnul
    ABIL_YRED_INJURY_MIRROR = 1030,
    ABIL_YRED_ANIMATE_REMAINS,
    ABIL_YRED_RECALL_UNDEAD_SLAVES,
    ABIL_YRED_ANIMATE_DEAD,
    ABIL_YRED_DRAIN_LIFE,
    ABIL_YRED_ENSLAVE_SOUL,
    ABIL_YRED_ANIMATE_REMAINS_OR_DEAD,
    // Vehumet
    // = 1040
    // Okawaru
    ABIL_OKAWARU_HEROISM = 1050,
    ABIL_OKAWARU_FINESSE,
    // Makhleb
    ABIL_MAKHLEB_MINOR_DESTRUCTION = 1060,
    ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB,
    ABIL_MAKHLEB_MAJOR_DESTRUCTION,
    ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB,
    // Sif Muna
    ABIL_SIF_MUNA_CHANNEL_ENERGY = 1070,
    ABIL_SIF_MUNA_FORGET_SPELL,
    // Trog
    ABIL_TROG_BURN_SPELLBOOKS = 1080,
    ABIL_TROG_BERSERK,
    ABIL_TROG_REGEN_MR,
    ABIL_TROG_BROTHERS_IN_ARMS,
    // Elyvilon
    ABIL_ELYVILON_LIFESAVING = 1090,
    ABIL_ELYVILON_LESSER_HEALING_SELF,
    ABIL_ELYVILON_LESSER_HEALING_OTHERS,
    ABIL_ELYVILON_PURIFICATION,
    ABIL_ELYVILON_GREATER_HEALING_SELF,
    ABIL_ELYVILON_GREATER_HEALING_OTHERS,
    ABIL_ELYVILON_DIVINE_VIGOUR,
    // Lugonu
    ABIL_LUGONU_ABYSS_EXIT = 1100,
    ABIL_LUGONU_BEND_SPACE,
    ABIL_LUGONU_BANISH,
    ABIL_LUGONU_CORRUPT,
    ABIL_LUGONU_ABYSS_ENTER,
    // Nemelex
#if TAG_MAJOR_VERSION == 34
    ABIL_NEMELEX_DRAW_ONE = 1110,
    ABIL_NEMELEX_PEEK_TWO,
#endif
    ABIL_NEMELEX_TRIPLE_DRAW = 1112,
    ABIL_NEMELEX_DEAL_FOUR,
    ABIL_NEMELEX_STACK_FIVE,
    // Beogh
    ABIL_BEOGH_SMITING = 1120,
    ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS,
    ABIL_BEOGH_GIFT_ITEM,
    // Jiyva
    ABIL_JIYVA_CALL_JELLY = 1130,
    ABIL_JIYVA_JELLY_PARALYSE,
    ABIL_JIYVA_SLIMIFY,
    ABIL_JIYVA_CURE_BAD_MUTATION,
    // Fedhas
    ABIL_FEDHAS_SUNLIGHT = 1140,
    ABIL_FEDHAS_RAIN,
    ABIL_FEDHAS_PLANT_RING,
    ABIL_FEDHAS_SPAWN_SPORES,
    ABIL_FEDHAS_EVOLUTION,
    // Cheibriados
    ABIL_CHEIBRIADOS_TIME_STEP = 1151,
    ABIL_CHEIBRIADOS_TIME_BEND,
    ABIL_CHEIBRIADOS_SLOUCH,
    ABIL_CHEIBRIADOS_DISTORTION,
    // Ashenzari
    ABIL_ASHENZARI_SCRYING = 1160,
    ABIL_ASHENZARI_TRANSFER_KNOWLEDGE,
    ABIL_ASHENZARI_END_TRANSFER,
    // Dithmenos
    ABIL_DITHMENOS_SHADOW_STEP = 1170,
    ABIL_DITHMENOS_SHADOW_FORM,
    // Gozag
    ABIL_GOZAG_POTION_PETITION = 1180,
    ABIL_GOZAG_CALL_MERCHANT,
    ABIL_GOZAG_BRIBE_BRANCH,
    // Qazlal
    ABIL_QAZLAL_UPHEAVAL = 1190,
    ABIL_QAZLAL_ELEMENTAL_FORCE,
    ABIL_QAZLAL_DISASTER_AREA,
    // Ru
    ABIL_RU_DRAW_OUT_POWER = 1200,
    ABIL_RU_POWER_LEAP,
    ABIL_RU_APOCALYPSE,

    ABIL_RU_SACRIFICE_PURITY,
        ABIL_FIRST_SACRIFICE = ABIL_RU_SACRIFICE_PURITY,
    ABIL_RU_SACRIFICE_WORDS,
    ABIL_RU_SACRIFICE_DRINK,
    ABIL_RU_SACRIFICE_ESSENCE,
    ABIL_RU_SACRIFICE_HEALTH,
    ABIL_RU_SACRIFICE_STEALTH,
    ABIL_RU_SACRIFICE_ARTIFICE,
    ABIL_RU_SACRIFICE_LOVE,
    ABIL_RU_SACRIFICE_COURAGE,
    ABIL_RU_SACRIFICE_ARCANA,
    ABIL_RU_SACRIFICE_NIMBLENESS,
    ABIL_RU_SACRIFICE_DURABILITY,
    ABIL_RU_SACRIFICE_HAND,
        ABIL_FINAL_SACRIFICE = ABIL_RU_SACRIFICE_HAND,
    ABIL_RU_REJECT_SACRIFICES,

    // For both Yred and Beogh
    ABIL_STOP_RECALL = 1500,

    // General divine (pseudo) abilities.
    ABIL_RENOUNCE_RELIGION,
    ABIL_CONVERT_TO_BEOGH,

    // Zot Defence abilities
    ABIL_MAKE_FUNGUS = 2000,
    ABIL_MIN_ZOTDEF = ABIL_MAKE_FUNGUS,
    ABIL_MAKE_PLANT,
    ABIL_MAKE_OKLOB_SAPLING,
#if TAG_MAJOR_VERSION == 34
    ABIL_MAKE_DART_TRAP,
#endif
    ABIL_MAKE_ICE_STATUE,
    ABIL_MAKE_OCS,
    ABIL_MAKE_OBSIDIAN_STATUE,
    ABIL_MAKE_CURSE_SKULL,
    ABIL_MAKE_TELEPORT,
    ABIL_MAKE_ARROW_TRAP,
    ABIL_MAKE_BOLT_TRAP,
    ABIL_MAKE_SPEAR_TRAP,
    ABIL_MAKE_NEEDLE_TRAP,
    ABIL_MAKE_NET_TRAP,
#if TAG_MAJOR_VERSION == 34
        ABIL_UNUSED_Z1,
#endif
    ABIL_MAKE_ALARM_TRAP,
    ABIL_MAKE_BLADE_TRAP,
    ABIL_MAKE_OKLOB_CIRCLE,
    ABIL_MAKE_ACQUIRE_GOLD,
    ABIL_MAKE_ACQUIREMENT,
    ABIL_MAKE_WATER,
    ABIL_MAKE_LIGHTNING_SPIRE,
    ABIL_MAKE_BAZAAR,
    ABIL_MAKE_ALTAR,
    ABIL_MAKE_GRENADES,
#if TAG_MAJOR_VERSION == 34
    ABIL_MAKE_SAGE,
#endif
    ABIL_MAKE_OKLOB_PLANT,
    ABIL_MAKE_BURNING_BUSH,
    ABIL_REMOVE_CURSE,
    ABIL_MAX_ZOTDEF = ABIL_REMOVE_CURSE,
    NUM_ABILITIES
};

enum activity_interrupt_type
{
    AI_FORCE_INTERRUPT = 0,         // Forcibly kills any activity that can be
                                    // interrupted.
    AI_KEYPRESS,
    AI_FULL_HP,                     // Player is fully healed
    AI_FULL_MP,                     // Player has recovered all mp
    AI_STATUE,                      // Bad statue has come into view
    AI_HUNGRY,                      // Hunger increased
    AI_MESSAGE,                     // Message was displayed
    AI_HP_LOSS,
    AI_STAT_CHANGE,
    AI_SEE_MONSTER,
    AI_MONSTER_ATTACKS,
    AI_TELEPORT,
    AI_HIT_MONSTER,                 // Player hit monster (invis or
                                    // mimic) during travel/explore.
    AI_SENSE_MONSTER,
    AI_MIMIC,

    // Always the last.
    NUM_AINTERRUPTS
};

enum attribute_type
{
    ATTR_DIVINE_LIGHTNING_PROTECTION,
#if TAG_MAJOR_VERSION == 34
    ATTR_DIVINE_REGENERATION,
#endif
    ATTR_DIVINE_DEATH_CHANNEL,
    ATTR_CARD_COUNTDOWN,
    ATTR_BANISHMENT_IMMUNITY,   // banishment immunity until
    ATTR_DELAYED_FIREBALL,      // bwr: reserve fireballs
    ATTR_HELD,                  // caught in a net or web
    ATTR_ABYSS_ENTOURAGE,       // maximum number of hostile monsters in
                                // sight of the player while in the Abyss.
    ATTR_DIVINE_VIGOUR,         // strength of Ely's Divine Vigour
    ATTR_DIVINE_STAMINA,        // strength of Zin's Divine Stamina
    ATTR_DIVINE_SHIELD,         // strength of TSO's Divine Shield
#if TAG_MAJOR_VERSION == 34
    ATTR_WEAPON_SWAP_INTERRUPTED,
#endif
    ATTR_GOLD_FOUND,
    ATTR_PURCHASES,            // Gold amount spent at shops.
    ATTR_DONATIONS,            // Gold amount donated to Zin.
    ATTR_MISC_SPENDING,        // Spending for things like ziggurats.
#if TAG_MAJOR_VERSION == 34
    ATTR_UNUSED1,              // was ATTR_RND_LVL_BOOKS
    ATTR_NOISES,
#endif
    ATTR_SHADOWS,              // Lantern of shadows effect.
#if TAG_MAJOR_VERSION == 34
    ATTR_UNUSED2,              // was ATTR_FRUIT_FOUND
#endif
    ATTR_FLIGHT_UNCANCELLABLE, // Potion of flight is in effect.
    ATTR_INVIS_UNCANCELLABLE,  // Potion/spell/wand of invis is in effect.
    ATTR_PERM_FLIGHT,          // Tengu flight or boots of flying are on.
    ATTR_SEEN_INVIS_TURN,      // Last turn you saw something invisible.
    ATTR_SEEN_INVIS_SEED,      // Random seed for invis monster positions.
    ATTR_APPENDAGE,            // eq slot of Beastly Appendage
    ATTR_TITHE_BASE,           // Remainder of untithed gold.
    ATTR_EVOL_XP,              // XP gained since last evolved mutation
    ATTR_LIFE_GAINED,          // XL when a felid gained a life.
    ATTR_TEMP_MUTATIONS,       // Number of temporary mutations the player has.
    ATTR_TEMP_MUT_XP,          // Amount of XP remaining before some temp muts
                               // will be removed
    ATTR_NEXT_RECALL_TIME,     // aut remaining until next ally will be recalled
    ATTR_NEXT_RECALL_INDEX,    // index+1 into recall_list for next recall
#if TAG_MAJOR_VERSION == 34
    ATTR_EVOKER_XP,            // How much xp remaining until next evoker charge
#endif
    ATTR_SEEN_BEOGH,           // Did an orc priest already offer conversion?
    ATTR_XP_DRAIN,             // Severity of current skill drain
    ATTR_SEARING_RAY,          // Are we currently firing a searing ray?
    ATTR_RECITE_TYPE,          // Recitation type.
    ATTR_RECITE_SEED,          // Recite text seed.
    ATTR_RECITE_HP,            // HP on start of recitation.
    ATTR_SWIFTNESS,            // Duration of future antiswiftness.
#if TAG_MAJOR_VERSION == 34
    ATTR_BARBS_MSG,            // Have we already printed a message on move?
#endif
    ATTR_BARBS_POW,            // How badly we are currently skewered
    ATTR_REPEL_MISSILES,       // Repel missiles active
    ATTR_DEFLECT_MISSILES,     // Deflect missiles active
    ATTR_PORTAL_PROJECTILE,    // Accuracy bonus during portal projectile
    ATTR_GOD_WRATH_XP,         // How much XP before our next god wrath check?
    ATTR_GOD_WRATH_COUNT,      // Number of stored retributions
    ATTR_NEXT_DRAGON_TIME,     // aut remaining until Dragon's Call summons another
    ATTR_GOLD_GENERATED,       // Count gold generated this game.
    ATTR_GOZAG_POTIONS,        // Number of times you've bought potions from Gozag.
    ATTR_GOZAG_SHOPS,          // Number of shops you've funded from Gozag.
    ATTR_GOZAG_SHOPS_CURRENT,  // As above, but since most recent time worshipping.
#if TAG_MAJOR_VERSION == 34
    ATTR_DIVINE_FIRE_RES,      // Divine fire resistance (Qazlal).
    ATTR_DIVINE_COLD_RES,      // Divine cold resistance (Qazlal).
    ATTR_DIVINE_ELEC_RES,      // Divine electricity resistance (Qazlal).
    ATTR_DIVINE_AC,            // Divine AC bonus (Qazlal).
#endif
    ATTR_GOZAG_GOLD_USED,      // Gold spent for Gozag abilities.
    NUM_ATTRIBUTES
};

enum transformation_type
{
    TRAN_NONE,
    TRAN_SPIDER,
    TRAN_BLADE_HANDS,
    TRAN_STATUE,
    TRAN_ICE_BEAST,
    TRAN_DRAGON,
    TRAN_LICH,
    TRAN_BAT,
    TRAN_PIG,
    TRAN_APPENDAGE,
    TRAN_TREE,
    TRAN_PORCUPINE,
    TRAN_WISP,
#if TAG_MAJOR_VERSION == 34
    TRAN_JELLY,
#endif
    TRAN_FUNGUS,
    TRAN_SHADOW,
    TRAN_HYDRA,
    NUM_TRANSFORMS,
};

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
    BEAM_HELLFIRE,
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
    BEAM_GHOSTLY_FLAME,

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
    BEAM_ENSLAVE_SOUL,
    BEAM_PAIN,
    BEAM_DISPEL_UNDEAD,
    BEAM_DISINTEGRATION,
    BEAM_BLINK,
    BEAM_BLINK_CLOSE,
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
    BEAM_IGNITE_POISON,
    BEAM_AGILITY,
    BEAM_SAP_MAGIC,
    BEAM_CORRUPT_BODY,
    BEAM_CHAOTIC_REFLECTION,
    BEAM_DRAIN_MAGIC,
    BEAM_TUKIMAS_DANCE,
    BEAM_LAST_ENCHANTMENT = BEAM_TUKIMAS_DANCE,

    BEAM_MEPHITIC,
    BEAM_INK,
    BEAM_HOLY_FLAME,
    BEAM_AIR,
    BEAM_PETRIFYING_CLOUD,
    BEAM_ENSNARE,
    BEAM_CRYSTAL,
    BEAM_DEATH_RATTLE,
    BEAM_LAST_REAL = BEAM_DEATH_RATTLE,

    // For getting the visual effect of a beam.
    BEAM_VISUAL,
    BEAM_BOUNCY_TRACER,           // Used for random bolt tracer (bounces as
                                  // crystal bolt, but irresistable).

    BEAM_TORMENT_DAMAGE,          // Pseudo-beam for damage flavour.
    BEAM_FIRST_PSEUDO = BEAM_TORMENT_DAMAGE,

    NUM_BEAMS
};

enum book_type
{
    BOOK_MINOR_MAGIC,
    BOOK_CONJURATIONS,
    BOOK_FLAMES,
    BOOK_FROST,
    BOOK_SUMMONINGS,
    BOOK_FIRE,
    BOOK_ICE,
    BOOK_SPATIAL_TRANSLOCATIONS,
    BOOK_ENCHANTMENTS,
    BOOK_YOUNG_POISONERS,
    BOOK_TEMPESTS,
    BOOK_DEATH,
    BOOK_HINDERANCE,
    BOOK_CHANGES,
    BOOK_TRANSFIGURATIONS,
    BOOK_FEN,
#if TAG_MAJOR_VERSION == 34
    BOOK_WAR_CHANTS = BOOK_FEN,
#else
    BOOK_BATTLE,
#endif
    BOOK_CLOUDS,
    BOOK_NECROMANCY,
    BOOK_CALLINGS,
    BOOK_MALEDICT,
    BOOK_AIR,
    BOOK_SKY,
    BOOK_WARP,
    BOOK_ENVENOMATIONS,
    BOOK_UNLIFE,
    BOOK_CONTROL,
#if TAG_MAJOR_VERSION == 34
    BOOK_BATTLE, // was BOOK_MUTATIONS
#endif
    BOOK_GEOMANCY,
    BOOK_EARTH,
    BOOK_WIZARDRY,
    BOOK_POWER,
    BOOK_CANTRIPS,
    BOOK_PARTY_TRICKS,
#if TAG_MAJOR_VERSION == 34
    BOOK_STALKING,
#endif
    BOOK_DEBILITATION,
    BOOK_DRAGON,
    BOOK_BURGLARY,
    BOOK_DREAMS,
    BOOK_ALCHEMY,
    BOOK_BEASTS,
    MAX_NORMAL_BOOK = BOOK_BEASTS,

    MIN_RARE_BOOK,
    BOOK_ANNIHILATIONS = MIN_RARE_BOOK,
    BOOK_GRAND_GRIMOIRE,
    BOOK_NECRONOMICON,
    MAX_RARE_BOOK = BOOK_NECRONOMICON,

    MAX_FIXED_BOOK = MAX_RARE_BOOK,

    BOOK_RANDART_LEVEL,
    BOOK_RANDART_THEME,

    BOOK_MANUAL,
    BOOK_DESTRUCTION,
    NUM_BOOKS
};

#define NUM_NORMAL_BOOKS     (MAX_NORMAL_BOOK + 1)
#define NUM_FIXED_BOOKS      (MAX_FIXED_BOOK + 1)

enum branch_type                // you.where_are_you
{
    BRANCH_DUNGEON,
    BRANCH_TEMPLE,
    BRANCH_FIRST_NON_DUNGEON = BRANCH_TEMPLE,
    BRANCH_ORC,
    BRANCH_ELF,
#if TAG_MAJOR_VERSION == 34
    BRANCH_DWARF,
#endif
    BRANCH_LAIR,
    BRANCH_SWAMP,
    BRANCH_SHOALS,
    BRANCH_SNAKE,
    BRANCH_SPIDER,
    BRANCH_SLIME,
    BRANCH_VAULTS,
#if TAG_MAJOR_VERSION == 34
    BRANCH_BLADE,
#endif
    BRANCH_CRYPT,
    BRANCH_TOMB,
#if TAG_MAJOR_VERSION > 34
    BRANCH_DEPTHS,
#endif
    BRANCH_VESTIBULE,
    BRANCH_DIS,
    BRANCH_GEHENNA,
    BRANCH_COCYTUS,
    BRANCH_TARTARUS,
      BRANCH_FIRST_HELL = BRANCH_DIS,
      BRANCH_LAST_HELL = BRANCH_TARTARUS,
    BRANCH_ZOT,
#if TAG_MAJOR_VERSION == 34
    BRANCH_FOREST,
#endif
    BRANCH_ABYSS,
    BRANCH_PANDEMONIUM,
    BRANCH_ZIGGURAT,
    BRANCH_LABYRINTH,
    BRANCH_BAZAAR,
    BRANCH_TROVE,
    BRANCH_SEWER,
    BRANCH_OSSUARY,
    BRANCH_BAILEY,
    BRANCH_ICE_CAVE,
    BRANCH_VOLCANO,
    BRANCH_WIZLAB,
#if TAG_MAJOR_VERSION == 34
    BRANCH_DEPTHS,
#endif
    NUM_BRANCHES
};

enum caction_type    // Primary categorization of counted actions.
{                    // A subtype will also be given in each case:
    CACT_MELEE,      // weapon subtype or unrand index
    CACT_FIRE,       // weapon subtype or unrand index
    CACT_THROW,      // item basetype << 16 | subtype
    CACT_CAST,       // spell_type
    CACT_INVOKE,     // ability_type
    CACT_ABIL,       // ability_type
    CACT_EVOKE,      // evoc_type
                     //   or item.basetype << 16 | subtype
                     //   or unrand index
    CACT_USE,        // object_class_type
    CACT_STAB,       // stab_type
    CACT_EAT,        // food_type, or -1 for corpse
    NUM_CACTIONS,
};

enum canned_message_type
{
    MSG_SOMETHING_APPEARS,
    MSG_NOTHING_HAPPENS,
    MSG_YOU_UNAFFECTED,
    MSG_YOU_RESIST,
    MSG_YOU_PARTIALLY_RESIST,
    MSG_TOO_BERSERK,
    MSG_TOO_CONFUSED,
    MSG_PRESENT_FORM,
    MSG_NOTHING_CARRIED,
    MSG_CANNOT_DO_YET,
    MSG_OK,
    MSG_UNTHINKING_ACT,
    MSG_NOTHING_THERE,
    MSG_NOTHING_CLOSE_ENOUGH,
    MSG_NO_ENERGY,
    MSG_SPELL_FIZZLES,
    MSG_HUH,
    MSG_EMPTY_HANDED_ALREADY,
    MSG_EMPTY_HANDED_NOW,
    MSG_YOU_BLINK,
    MSG_STRANGE_STASIS,
    MSG_NO_SPELLS,
    MSG_MANA_INCREASE,
    MSG_MANA_DECREASE,
    MSG_DISORIENTED,
    MSG_TOO_HUNGRY,
    MSG_DETECT_NOTHING,
    MSG_CALL_DEAD,
    MSG_ANIMATE_REMAINS,
    MSG_DECK_EXHAUSTED,
    MSG_CANNOT_MOVE,
    MSG_YOU_DIE,
};

enum char_set_type
{
    CSET_DEFAULT,
    CSET_ASCII,         // flat 7-bit ASCII
    NUM_CSET
};

enum cleansing_flame_source_type
{
    CLEANSING_FLAME_GENERIC    = -1,
    CLEANSING_FLAME_SPELL      = -2, // SPELL_FLAME_OF_CLEANSING
    CLEANSING_FLAME_INVOCATION = -3, // ABIL_TSO_CLEANSING_FLAME
    CLEANSING_FLAME_TSO        = -4, // TSO effect
};

enum cloud_type
{
    CLOUD_NONE,
    CLOUD_FIRE,
    CLOUD_MEPHITIC,
    CLOUD_COLD,
    CLOUD_POISON,
    CLOUD_BLACK_SMOKE,
    CLOUD_GREY_SMOKE,
    CLOUD_BLUE_SMOKE,
    CLOUD_PURPLE_SMOKE,
    CLOUD_TLOC_ENERGY,
    CLOUD_FOREST_FIRE,
    CLOUD_STEAM,
#if TAG_MAJOR_VERSION == 34
    CLOUD_GLOOM,
#endif
    CLOUD_INK,
    CLOUD_PETRIFY,
    CLOUD_HOLY_FLAMES,
    CLOUD_MIASMA,
    CLOUD_MIST,
    CLOUD_CHAOS,
    CLOUD_RAIN,
    CLOUD_MUTAGENIC,
    CLOUD_MAGIC_TRAIL,
    CLOUD_TORNADO,
    CLOUD_DUST_TRAIL,
    CLOUD_GHOSTLY_FLAME,
    CLOUD_ACID,
    CLOUD_STORM,
    CLOUD_NEGATIVE_ENERGY,
    NUM_CLOUD_TYPES,

    CLOUD_OPAQUE_FIRST = CLOUD_BLACK_SMOKE,
    CLOUD_OPAQUE_LAST  = CLOUD_HOLY_FLAMES,

    // Random per-square.
    CLOUD_RANDOM_SMOKE = 97,
    CLOUD_RANDOM,
    CLOUD_DEBUGGING,
};

enum command_type
{
    CMD_NO_CMD = 2000,
    CMD_NO_CMD_DEFAULT, // hack to allow assignment of keys to CMD_NO_CMD
    CMD_MOVE_NOWHERE,
    CMD_MOVE_LEFT,
    CMD_MOVE_DOWN,
    CMD_MOVE_UP,
    CMD_MOVE_RIGHT,
    CMD_MOVE_UP_LEFT,
    CMD_MOVE_DOWN_LEFT,
    CMD_MOVE_UP_RIGHT,
    CMD_MOVE_DOWN_RIGHT,
    CMD_RUN_LEFT,
    CMD_RUN_DOWN,
    CMD_RUN_UP,
    CMD_RUN_RIGHT,
    CMD_RUN_UP_LEFT,
    CMD_RUN_DOWN_LEFT,
    CMD_RUN_UP_RIGHT,
    CMD_RUN_DOWN_RIGHT,
    CMD_OPEN_DOOR_LEFT,
    CMD_OPEN_DOOR_DOWN,
    CMD_OPEN_DOOR_UP,
    CMD_OPEN_DOOR_RIGHT,
    CMD_OPEN_DOOR_UP_LEFT,
    CMD_OPEN_DOOR_DOWN_LEFT,
    CMD_OPEN_DOOR_UP_RIGHT,
    CMD_OPEN_DOOR_DOWN_RIGHT,
    CMD_OPEN_DOOR,
    CMD_CLOSE_DOOR,
    CMD_REST,
    CMD_GO_UPSTAIRS,
    CMD_GO_DOWNSTAIRS,
    CMD_TOGGLE_AUTOPICKUP,
    CMD_TOGGLE_VIEWPORT_MONSTER_HP,
    CMD_TOGGLE_VIEWPORT_WEAPONS,
    CMD_TOGGLE_TRAVEL_SPEED,
    CMD_PICKUP,
    CMD_PICKUP_QUANTITY,
    CMD_DROP,
    CMD_DROP_LAST,
    CMD_BUTCHER,
    CMD_INSPECT_FLOOR,
    CMD_SHOW_TERRAIN,
    CMD_FULL_VIEW,
    CMD_EVOKE,
    CMD_EVOKE_WIELDED,
    CMD_FORCE_EVOKE_WIELDED,
    CMD_WIELD_WEAPON,
    CMD_WEAPON_SWAP,
    CMD_FIRE,
    CMD_QUIVER_ITEM,
    CMD_THROW_ITEM_NO_QUIVER,
    CMD_WEAR_ARMOUR,
    CMD_REMOVE_ARMOUR,
    CMD_WEAR_JEWELLERY,
    CMD_REMOVE_JEWELLERY,
    CMD_CYCLE_QUIVER_FORWARD,
    CMD_CYCLE_QUIVER_BACKWARD,
    CMD_LIST_WEAPONS,
    CMD_LIST_ARMOUR,
    CMD_LIST_JEWELLERY,
    CMD_LIST_EQUIPMENT,
    CMD_LIST_GOLD,
    CMD_ZAP_WAND,
    CMD_CAST_SPELL,
    CMD_FORCE_CAST_SPELL,
    CMD_MEMORISE_SPELL,
    CMD_USE_ABILITY,
    CMD_PRAY,
    CMD_EAT,
    CMD_QUAFF,
    CMD_READ,
    CMD_LOOK_AROUND,
    CMD_WAIT,
    CMD_SHOUT,
    CMD_CHARACTER_DUMP,
    CMD_DISPLAY_COMMANDS,
    CMD_DISPLAY_INVENTORY,
    CMD_DISPLAY_KNOWN_OBJECTS,
    CMD_DISPLAY_MUTATIONS,
    CMD_DISPLAY_SKILLS,
    CMD_DISPLAY_MAP,
    CMD_DISPLAY_OVERMAP,
    CMD_DISPLAY_RELIGION,
    CMD_DISPLAY_RUNES,
    CMD_DISPLAY_CHARACTER_STATUS,
    CMD_DISPLAY_SPELLS,
    CMD_EXPERIENCE_CHECK,
    CMD_ADJUST_INVENTORY,
    CMD_REPLAY_MESSAGES,
    CMD_REDRAW_SCREEN,
    CMD_MACRO_ADD,
    CMD_SAVE_GAME,
    CMD_SAVE_GAME_NOW,
    CMD_SUSPEND_GAME,
    CMD_QUIT,
    CMD_WIZARD,

    CMD_SEARCH_STASHES,
    CMD_EXPLORE,
    CMD_INTERLEVEL_TRAVEL,
    CMD_FIX_WAYPOINT,

    CMD_CLEAR_MAP,
    CMD_INSCRIBE_ITEM,
    CMD_MAKE_NOTE,
    CMD_RESISTS_SCREEN,

    CMD_READ_MESSAGES,

    CMD_MOUSE_MOVE,
    CMD_MOUSE_CLICK,

    CMD_ANNOTATE_LEVEL,

#ifdef CLUA_BINDINGS
    CMD_AUTOFIGHT,
    CMD_AUTOFIGHT_NOMOVE,
#endif

#ifdef USE_TILE
    CMD_EDIT_PLAYER_TILE,
    CMD_MIN_TILE = CMD_EDIT_PLAYER_TILE,
    CMD_MAX_TILE = CMD_EDIT_PLAYER_TILE,
#endif

#ifdef TOUCH_UI
    // zoom on dungeon
    CMD_ZOOM_IN,
    CMD_ZOOM_OUT,
#endif

    // Repeat previous command
    CMD_PREV_CMD_AGAIN,

    // Repeat next command a given number of times
    CMD_REPEAT_CMD,

    CMD_LUA_CONSOLE,

    CMD_MAX_NORMAL = CMD_LUA_CONSOLE,

    // overmap commands
    CMD_MAP_CLEAR_MAP,
    CMD_MIN_OVERMAP = CMD_MAP_CLEAR_MAP,
    CMD_MAP_ADD_WAYPOINT,
    CMD_MAP_EXCLUDE_AREA,
    CMD_MAP_CLEAR_EXCLUDES,
    CMD_MAP_EXCLUDE_RADIUS,

    CMD_MAP_MOVE_LEFT,
    CMD_MAP_MOVE_DOWN,
    CMD_MAP_MOVE_UP,
    CMD_MAP_MOVE_RIGHT,
    CMD_MAP_MOVE_UP_LEFT,
    CMD_MAP_MOVE_DOWN_LEFT,
    CMD_MAP_MOVE_UP_RIGHT,
    CMD_MAP_MOVE_DOWN_RIGHT,

    CMD_MAP_JUMP_LEFT,
    CMD_MAP_JUMP_DOWN,
    CMD_MAP_JUMP_UP,
    CMD_MAP_JUMP_RIGHT,
    CMD_MAP_JUMP_UP_LEFT,
    CMD_MAP_JUMP_DOWN_LEFT,
    CMD_MAP_JUMP_UP_RIGHT,
    CMD_MAP_JUMP_DOWN_RIGHT,

    CMD_MAP_NEXT_LEVEL,
    CMD_MAP_PREV_LEVEL,
    CMD_MAP_GOTO_LEVEL,

    CMD_MAP_SCROLL_DOWN,
    CMD_MAP_SCROLL_UP,

    CMD_MAP_FIND_UPSTAIR,
    CMD_MAP_FIND_DOWNSTAIR,
    CMD_MAP_FIND_YOU,
    CMD_MAP_FIND_PORTAL,
    CMD_MAP_FIND_TRAP,
    CMD_MAP_FIND_ALTAR,
    CMD_MAP_FIND_EXCLUDED,
    CMD_MAP_FIND_WAYPOINT,
    CMD_MAP_FIND_STASH,
    CMD_MAP_FIND_STASH_REVERSE,

    CMD_MAP_GOTO_TARGET,
    CMD_MAP_ANNOTATE_LEVEL,

    CMD_MAP_EXPLORE,

    CMD_MAP_WIZARD_TELEPORT,

    CMD_MAP_HELP,
    CMD_MAP_FORGET,
    CMD_MAP_UNFORGET,

    CMD_MAP_EXIT_MAP,

    CMD_MAX_OVERMAP = CMD_MAP_EXIT_MAP,

    // targeting commands
    CMD_TARGET_DOWN_LEFT,
    CMD_MIN_TARGET = CMD_TARGET_DOWN_LEFT,
    CMD_TARGET_DOWN,
    CMD_TARGET_DOWN_RIGHT,
    CMD_TARGET_LEFT,
    CMD_TARGET_RIGHT,
    CMD_TARGET_UP_LEFT,
    CMD_TARGET_UP,
    CMD_TARGET_UP_RIGHT,

    CMD_TARGET_DIR_DOWN_LEFT,
    CMD_TARGET_DIR_DOWN,
    CMD_TARGET_DIR_DOWN_RIGHT,
    CMD_TARGET_DIR_LEFT,
    CMD_TARGET_DIR_RIGHT,
    CMD_TARGET_DIR_UP_LEFT,
    CMD_TARGET_DIR_UP,
    CMD_TARGET_DIR_UP_RIGHT,

    CMD_TARGET_DESCRIBE,
    CMD_TARGET_PREV_TARGET,
    CMD_TARGET_MAYBE_PREV_TARGET,
    CMD_TARGET_SELECT,
    CMD_TARGET_SELECT_ENDPOINT,
    CMD_TARGET_SELECT_FORCE,
    CMD_TARGET_SELECT_FORCE_ENDPOINT,
    CMD_TARGET_GET,
    CMD_TARGET_OBJ_CYCLE_BACK,
    CMD_TARGET_OBJ_CYCLE_FORWARD,
    CMD_TARGET_CYCLE_FORWARD,
    CMD_TARGET_CYCLE_BACK,
    CMD_TARGET_CYCLE_BEAM,
    CMD_TARGET_CYCLE_MLIST = CMD_NO_CMD + 1000, // for indices a-z in the monster list
    CMD_TARGET_CYCLE_MLIST_END = CMD_NO_CMD + 1025,
    CMD_TARGET_TOGGLE_MLIST,
    CMD_TARGET_TOGGLE_BEAM,
    CMD_TARGET_CANCEL,
    CMD_TARGET_SHOW_PROMPT,
    CMD_TARGET_OLD_SPACE,
    CMD_TARGET_EXCLUDE,
    CMD_TARGET_FIND_TRAP,
    CMD_TARGET_FIND_PORTAL,
    CMD_TARGET_FIND_ALTAR,
    CMD_TARGET_FIND_UPSTAIR,
    CMD_TARGET_FIND_DOWNSTAIR,
    CMD_TARGET_FIND_YOU,
    CMD_TARGET_WIZARD_MAKE_FRIENDLY,
    CMD_TARGET_WIZARD_BLESS_MONSTER,
    CMD_TARGET_WIZARD_MAKE_SHOUT,
    CMD_TARGET_WIZARD_GIVE_ITEM,
    CMD_TARGET_WIZARD_MOVE,
    CMD_TARGET_WIZARD_PATHFIND,
    CMD_TARGET_WIZARD_GAIN_LEVEL,
    CMD_TARGET_WIZARD_MISCAST,
    CMD_TARGET_WIZARD_MAKE_SUMMONED,
    CMD_TARGET_WIZARD_POLYMORPH,
    CMD_TARGET_WIZARD_DEBUG_MONSTER,
    CMD_TARGET_WIZARD_HEAL_MONSTER,
    CMD_TARGET_WIZARD_HURT_MONSTER,
    CMD_TARGET_WIZARD_DEBUG_PORTAL,
    CMD_TARGET_WIZARD_KILL_MONSTER,
    CMD_TARGET_WIZARD_BANISH_MONSTER,
    CMD_TARGET_WIZARD_CREATE_MIMIC,
    CMD_TARGET_MOUSE_MOVE,
    CMD_TARGET_MOUSE_SELECT,
    CMD_TARGET_HELP,
    CMD_MAX_TARGET = CMD_TARGET_HELP,

#ifdef USE_TILE
    // Tile doll editing screen
    CMD_DOLL_RANDOMIZE,
    CMD_MIN_DOLL = CMD_DOLL_RANDOMIZE,
    CMD_DOLL_SELECT_NEXT_DOLL,
    CMD_DOLL_SELECT_PREV_DOLL,
    CMD_DOLL_SELECT_NEXT_PART,
    CMD_DOLL_SELECT_PREV_PART,
    CMD_DOLL_CHANGE_PART_NEXT,
    CMD_DOLL_CHANGE_PART_PREV,
    CMD_DOLL_CONFIRM_CHOICE,
    CMD_DOLL_COPY,
    CMD_DOLL_PASTE,
    CMD_DOLL_TAKE_OFF,
    CMD_DOLL_TAKE_OFF_ALL,
    CMD_DOLL_TOGGLE_EQUIP,
    CMD_DOLL_TOGGLE_EQUIP_ALL,
    CMD_DOLL_JOB_DEFAULT,
    CMD_DOLL_CHANGE_MODE,
    CMD_DOLL_SAVE,
    CMD_DOLL_QUIT,
    CMD_MAX_DOLL = CMD_DOLL_QUIT,
#endif

    // Disable/enable -more- prompts.
    CMD_DISABLE_MORE,
    CMD_MIN_SYNTHETIC = CMD_DISABLE_MORE,
    CMD_ENABLE_MORE,
    CMD_UNWIELD_WEAPON,

    // [ds] Silently ignored, requests another round of input.
    CMD_NEXT_CMD,

    // Must always be last
    CMD_MAX_CMD
};

enum conduct_type
{
    DID_NOTHING,
    DID_NECROMANCY,                       // vamp/drain/pain/reap, Zong/Curses
    DID_HOLY,                             // holy wrath, holy word scrolls
    DID_UNHOLY,                           // demon weapons, demon spells
    DID_ATTACK_HOLY,
    DID_ATTACK_NEUTRAL,
    DID_ATTACK_FRIEND,
    DID_FRIEND_DIED,
    DID_UNCHIVALRIC_ATTACK,
    DID_POISON,
    DID_KILL_LIVING,
    DID_KILL_UNDEAD,
    DID_KILL_DEMON,
    DID_KILL_NATURAL_UNHOLY,              // TSO
    DID_KILL_NATURAL_EVIL,                // TSO
    DID_KILL_UNCLEAN,                     // Zin
    DID_KILL_CHAOTIC,                     // Zin
    DID_KILL_WIZARD,                      // Trog
    DID_KILL_PRIEST,                      // Beogh
    DID_KILL_HOLY,
    DID_KILL_FAST,                        // Cheibriados
    DID_LIVING_KILLED_BY_UNDEAD_SLAVE,
    DID_LIVING_KILLED_BY_SERVANT,
    DID_UNDEAD_KILLED_BY_UNDEAD_SLAVE,
    DID_UNDEAD_KILLED_BY_SERVANT,
    DID_DEMON_KILLED_BY_UNDEAD_SLAVE,
    DID_DEMON_KILLED_BY_SERVANT,
    DID_NATURAL_UNHOLY_KILLED_BY_SERVANT, // TSO
    DID_NATURAL_EVIL_KILLED_BY_SERVANT,   // TSO
    DID_HOLY_KILLED_BY_UNDEAD_SLAVE,
    DID_HOLY_KILLED_BY_SERVANT,
    DID_BANISH,
    DID_SPELL_MEMORISE,
    DID_SPELL_CASTING,
    DID_SPELL_PRACTISE,
    DID_DRINK_BLOOD,
    DID_CANNIBALISM,
    DID_DESECRATE_SOULED_BEING,           // Zin
    DID_DELIBERATE_MUTATING,              // Zin
    DID_CAUSE_GLOWING,                    // Zin
    DID_UNCLEAN,                          // Zin (used unclean weapon/magic)
    DID_CHAOS,                            // Zin (used chaotic weapon/magic)
    DID_DESECRATE_ORCISH_REMAINS,         // Beogh
    DID_DESTROY_ORCISH_IDOL,              // Beogh
    DID_KILL_SLIME,                       // Jiyva
    DID_KILL_PLANT,                       // Fedhas
    DID_PLANT_KILLED_BY_SERVANT,          // Fedhas
    DID_HASTY,                            // Cheibriados
    DID_CORPSE_VIOLATION,                 // Fedhas (Necromancy involving
                                          // corpses/chunks).
    DID_SOULED_FRIEND_DIED,               // Zin
    DID_UNCLEAN_KILLED_BY_SERVANT,        // Zin
    DID_CHAOTIC_KILLED_BY_SERVANT,        // Zin
    DID_ATTACK_IN_SANCTUARY,              // Zin
    DID_KILL_ARTIFICIAL,                  // Yredelemnul
    DID_ARTIFICIAL_KILLED_BY_UNDEAD_SLAVE,// Yredelemnul
    DID_ARTIFICIAL_KILLED_BY_SERVANT,     // Yredelemnul
    DID_DESTROY_SPELLBOOK,                // Sif Muna
    DID_EXPLORATION,                      // Ashenzari, wrath timers
    DID_DESECRATE_HOLY_REMAINS,           // Zin/Ely/TSO/Yredelemnul
    DID_SEE_MONSTER,                      // TSO
    DID_FIRE,                             // Dithmenos
    DID_KILL_FIERY,                       // Dithmenos
    DID_SACRIFICE_LOVE,                   // Ru

    NUM_CONDUCTS
};

enum confirm_butcher_type
{
    CONFIRM_NEVER,
    CONFIRM_ALWAYS,
    CONFIRM_AUTO,
};

enum confirm_prompt_type
{
    CONFIRM_CANCEL,             // automatically answer 'no', i.e. disallow
    CONFIRM_PROMPT,             // prompt
    CONFIRM_NONE,               // automatically answer 'yes'
};

enum confirm_level_type
{
    CONFIRM_NONE_EASY,
    CONFIRM_SAFE_EASY,
    CONFIRM_ALL_EASY,
};

// When adding new delays, update their names in delay.cc
enum delay_type
{
    DELAY_NOT_DELAYED,
    DELAY_EAT,
    DELAY_FEED_VAMPIRE,
    DELAY_ARMOUR_ON,
    DELAY_ARMOUR_OFF,
    DELAY_JEWELLERY_ON,
    DELAY_MEMORISE,
    DELAY_BUTCHER,
    DELAY_BOTTLE_BLOOD,
#if TAG_MAJOR_VERSION == 34
    DELAY_WEAPON_SWAP,
#endif
    DELAY_PASSWALL,
    DELAY_DROP_ITEM,
    DELAY_MULTIDROP,
    DELAY_ASCENDING_STAIRS,
    DELAY_DESCENDING_STAIRS,
#if TAG_MAJOR_VERSION == 34
    DELAY_UNUSED, // was DELAY_RECITE
#endif

    // [dshaligram] Shift-running, resting, travel and macros are now
    // also handled as delays.
    DELAY_RUN,
    DELAY_REST,
    DELAY_TRAVEL,

    DELAY_MACRO,

    // In a macro delay, a stacked delay to tell Crawl to read and act on
    // one input command.
    DELAY_MACRO_PROCESS_KEY,

    DELAY_INTERRUPTIBLE,                // simple interruptible delay
    DELAY_UNINTERRUPTIBLE,              // simple uninterruptible delay

    DELAY_SHAFT_SELF, // Formicid ability

    NUM_DELAYS
};

enum description_level_type
{
    DESC_THE,
    DESC_A,
    DESC_YOUR,
    DESC_PLAIN,
    DESC_ITS,
    DESC_INVENTORY_EQUIP,
    DESC_INVENTORY,

    // Partial item names.
    DESC_BASENAME,                     // Base name of item subtype
    DESC_QUALNAME,                     // Name without articles, quantities,
                                       // enchantments.
    DESC_DBNAME,                       // Name with which to look up item
                                       // description in the db.

    DESC_NONE
};

enum evoc_type
{
    EVOC_WAND,
    EVOC_ROD,
    EVOC_DECK,
#if TAG_MAJOR_VERSION == 34
    EVOC_MISC,
#endif
    EVOC_TOME,
};

enum game_direction_type
{
    GDT_GAME_START = 0,
    GDT_DESCENDING,
    GDT_ASCENDING,
};

enum game_type
{
    GAME_TYPE_UNSPECIFIED,
    GAME_TYPE_NORMAL,
    GAME_TYPE_TUTORIAL,
    GAME_TYPE_ARENA,
    GAME_TYPE_SPRINT,
    GAME_TYPE_HINTS,
    GAME_TYPE_ZOTDEF,
    GAME_TYPE_INSTRUCTIONS,
    GAME_TYPE_HIGH_SCORES,
    NUM_GAME_TYPE
};

enum level_flag_type
{
    LFLAG_NONE = 0,

    LFLAG_NO_TELE_CONTROL = (1 << 0), // Teleport control not allowed.
    LFLAG_NO_MAP          = (1 << 2), // Level can't be persistently mapped.
};

// Volatile state and cache.
enum level_state_type
{
    LSTATE_NONE = 0,

    LSTATE_GOLUBRIA       = (1 << 0), // A Golubria trap exists.
    LSTATE_GLOW_MOLD      = (1 << 1), // Any glowing mold exists.
    LSTATE_DELETED        = (1 << 2), // The level won't be saved.
    LSTATE_BEOGH          = (1 << 3), // Possibly an orcish priest around.
    LSTATE_SLIMY_WALL     = (1 << 4), // Any slime walls exist.
};

// NOTE: The order of these is very important to their usage!
// [dshaligram] If adding/removing from this list, also update viewchar.cc!
enum dungeon_char_type
{
    DCHAR_WALL,
    DCHAR_PERMAWALL,
    DCHAR_WALL_MAGIC,
    DCHAR_FLOOR,
    DCHAR_FLOOR_MAGIC,
    DCHAR_DOOR_OPEN,
    DCHAR_DOOR_CLOSED,
    DCHAR_TRAP,
    DCHAR_STAIRS_DOWN,
    DCHAR_STAIRS_UP,
    DCHAR_GRATE,
    DCHAR_ALTAR,
    DCHAR_ARCH,
    DCHAR_FOUNTAIN,
    DCHAR_WAVY,
    DCHAR_STATUE,
    DCHAR_INVIS_EXPOSED,
    DCHAR_ITEM_DETECTED,
    DCHAR_ITEM_ORB,
    DCHAR_ITEM_RUNE,
    DCHAR_ITEM_WEAPON,
    DCHAR_ITEM_ARMOUR,
    DCHAR_ITEM_WAND,
    DCHAR_ITEM_FOOD,
    DCHAR_ITEM_SCROLL,
    DCHAR_ITEM_RING,
    DCHAR_ITEM_POTION,
    DCHAR_ITEM_MISSILE,
    DCHAR_ITEM_BOOK,
    DCHAR_ITEM_STAFF,
    DCHAR_ITEM_ROD,
    DCHAR_ITEM_MISCELLANY,
    DCHAR_ITEM_CORPSE,
    DCHAR_ITEM_SKELETON,
    DCHAR_ITEM_GOLD,
    DCHAR_ITEM_AMULET,
    DCHAR_CLOUD,
    DCHAR_TREE,
    DCHAR_TELEPORTER,

    DCHAR_SPACE,
    DCHAR_FIRED_FLASK,
    DCHAR_FIRED_BOLT,
    DCHAR_FIRED_CHUNK,
    DCHAR_FIRED_BOOK,
    DCHAR_FIRED_WEAPON,
    DCHAR_FIRED_ZAP,
    DCHAR_FIRED_BURST,
    DCHAR_FIRED_STICK,
    DCHAR_FIRED_TRINKET,
    DCHAR_FIRED_SCROLL,
    DCHAR_FIRED_DEBUG,
    DCHAR_FIRED_ARMOUR,
    DCHAR_FIRED_MISSILE,
    DCHAR_EXPLOSION,

    DCHAR_FRAME_HORIZ,
    DCHAR_FRAME_VERT,
    DCHAR_FRAME_TL,
    DCHAR_FRAME_TR,
    DCHAR_FRAME_BL,
    DCHAR_FRAME_BR,

    NUM_DCHAR_TYPES
};

// When adding:

// * Add an entry in feature-data.h for the feature.

// * edit dat/descript/features.txt and add a
//      long description if appropriate.

// * check the feat_* functions in terrain.cc and make sure
//      they return sane values for your new feature.

// * edit dungeon.cc and add a symbol to _glyph_to_feat() for the feature,
//      if you want vault maps to be able to use it. If you do, also update
//      docs/develop/levels/syntax.txt with the new symbol.
enum dungeon_feature_type
{
    DNGN_UNSEEN,
    DNGN_CLOSED_DOOR,
    DNGN_RUNED_DOOR,
    DNGN_SEALED_DOOR,
    DNGN_TREE,

    // Walls
    DNGN_METAL_WALL,
    DNGN_GREEN_CRYSTAL_WALL,
    DNGN_ROCK_WALL,
    DNGN_SLIMY_WALL,
    DNGN_STONE_WALL,
    DNGN_PERMAROCK_WALL,               // for undiggable walls
    DNGN_CLEAR_ROCK_WALL,              // transparent walls
    DNGN_CLEAR_STONE_WALL,
    DNGN_CLEAR_PERMAROCK_WALL,

    DNGN_GRATE,

    // Misc solid features
    DNGN_OPEN_SEA,                     // Shoals equivalent for permarock
    DNGN_LAVA_SEA,                     // Gehenna equivalent for permarock
    DNGN_ORCISH_IDOL,
    DNGN_GRANITE_STATUE,
    DNGN_MALIGN_GATEWAY,

#if TAG_MAJOR_VERSION == 34
    DNGN_LAVA            = 30,
#else
    DNGN_LAVA,
#endif
    DNGN_DEEP_WATER,

    DNGN_SHALLOW_WATER,

    DNGN_FLOOR,
    DNGN_OPEN_DOOR,

    DNGN_TRAP_MECHANICAL,
    DNGN_TRAP_TELEPORT,
    DNGN_TRAP_SHAFT,
    DNGN_TRAP_WEB,
    DNGN_UNDISCOVERED_TRAP,

    DNGN_ENTER_SHOP,
    DNGN_ABANDONED_SHOP,

    DNGN_STONE_STAIRS_DOWN_I,
    DNGN_STONE_STAIRS_DOWN_II,
    DNGN_STONE_STAIRS_DOWN_III,
    DNGN_ESCAPE_HATCH_DOWN,

    // corresponding up stairs (same order as above)
    DNGN_STONE_STAIRS_UP_I,
    DNGN_STONE_STAIRS_UP_II,
    DNGN_STONE_STAIRS_UP_III,
    DNGN_ESCAPE_HATCH_UP,

    // Various gates
    DNGN_ENTER_DIS,
    DNGN_ENTER_GEHENNA,
    DNGN_ENTER_COCYTUS,
    DNGN_ENTER_TARTARUS,
    DNGN_ENTER_ABYSS,
    DNGN_EXIT_ABYSS,
    DNGN_STONE_ARCH,
    DNGN_ENTER_PANDEMONIUM,
    DNGN_EXIT_PANDEMONIUM,
    DNGN_TRANSIT_PANDEMONIUM,
    DNGN_EXIT_DUNGEON,
    DNGN_EXIT_THROUGH_ABYSS,
    DNGN_EXIT_HELL,
    DNGN_ENTER_HELL,
    DNGN_ENTER_LABYRINTH,
    DNGN_TELEPORTER,
#if TAG_MAJOR_VERSION == 34
    DNGN_ENTER_PORTAL_VAULT,
    DNGN_EXIT_PORTAL_VAULT,
#endif
    DNGN_EXPIRED_PORTAL,

    // Entrances to various branches
#if TAG_MAJOR_VERSION == 34
    DNGN_ENTER_DWARF,
#endif
    DNGN_ENTER_ORC,
    DNGN_ENTER_LAIR,
    DNGN_ENTER_SLIME,
    DNGN_ENTER_VAULTS,
    DNGN_ENTER_CRYPT,
#if TAG_MAJOR_VERSION == 34
    DNGN_ENTER_BLADE,
#endif
    DNGN_ENTER_ZOT,
    DNGN_ENTER_TEMPLE,
    DNGN_ENTER_SNAKE,
    DNGN_ENTER_ELF,
    DNGN_ENTER_TOMB,
    DNGN_ENTER_SWAMP,
    DNGN_ENTER_SHOALS,
    DNGN_ENTER_SPIDER,
#if TAG_MAJOR_VERSION == 34
    DNGN_ENTER_FOREST,
#endif
    DNGN_ENTER_DEPTHS,

    // Exits from various branches
    // Order must be the same as above
#if TAG_MAJOR_VERSION == 34
    DNGN_RETURN_FROM_DWARF,
#endif
    DNGN_RETURN_FROM_ORC,
    DNGN_RETURN_FROM_LAIR,
    DNGN_RETURN_FROM_SLIME,
    DNGN_RETURN_FROM_VAULTS,
    DNGN_RETURN_FROM_CRYPT,
#if TAG_MAJOR_VERSION == 34
    DNGN_RETURN_FROM_BLADE,
#endif
    DNGN_RETURN_FROM_ZOT,
    DNGN_RETURN_FROM_TEMPLE,
    DNGN_RETURN_FROM_SNAKE,
    DNGN_RETURN_FROM_ELF,
    DNGN_RETURN_FROM_TOMB,
    DNGN_RETURN_FROM_SWAMP,
    DNGN_RETURN_FROM_SHOALS,
    DNGN_RETURN_FROM_SPIDER,
#if TAG_MAJOR_VERSION == 34
    DNGN_RETURN_FROM_FOREST,
#endif
    DNGN_RETURN_FROM_DEPTHS,

    DNGN_ALTAR_ZIN,
    DNGN_ALTAR_SHINING_ONE,
    DNGN_ALTAR_KIKUBAAQUDGHA,
    DNGN_ALTAR_YREDELEMNUL,
    DNGN_ALTAR_XOM,
    DNGN_ALTAR_VEHUMET,
    DNGN_ALTAR_OKAWARU,
    DNGN_ALTAR_MAKHLEB,
    DNGN_ALTAR_SIF_MUNA,
    DNGN_ALTAR_TROG,
    DNGN_ALTAR_NEMELEX_XOBEH,
    DNGN_ALTAR_ELYVILON,
    DNGN_ALTAR_LUGONU,
    DNGN_ALTAR_BEOGH,
    DNGN_ALTAR_JIYVA,
    DNGN_ALTAR_FEDHAS,
    DNGN_ALTAR_CHEIBRIADOS,
    DNGN_ALTAR_ASHENZARI,
    DNGN_ALTAR_DITHMENOS,
#if TAG_MAJOR_VERSION > 34
    DNGN_ALTAR_GOZAG,
    DNGN_ALTAR_QAZLAL,
    DNGN_ALTAR_RU,
#endif

    DNGN_FOUNTAIN_BLUE,
    DNGN_FOUNTAIN_SPARKLING,           // aka 'Magic Fountain' {dlb}
    DNGN_FOUNTAIN_BLOOD,
#if TAG_MAJOR_VERSION == 34
    DNGN_DRY_FOUNTAIN_BLUE,
    DNGN_DRY_FOUNTAIN_SPARKLING,
    DNGN_DRY_FOUNTAIN_BLOOD,
#endif
    DNGN_DRY_FOUNTAIN,

    // Not meant to ever appear in grd().
    DNGN_EXPLORE_HORIZON, // dummy for redefinition

    DNGN_UNKNOWN_ALTAR,
    DNGN_UNKNOWN_PORTAL,

    DNGN_ABYSSAL_STAIR,
#if TAG_MAJOR_VERSION == 34
    DNGN_BADLY_SEALED_DOOR,
#endif

    DNGN_SEALED_STAIRS_UP,
    DNGN_SEALED_STAIRS_DOWN,
    DNGN_TRAP_ALARM,
    DNGN_TRAP_ZOT,
    DNGN_PASSAGE_OF_GOLUBRIA,

    DNGN_ENTER_ZIGGURAT,
    DNGN_ENTER_BAZAAR,
    DNGN_ENTER_TROVE,
    DNGN_ENTER_SEWER,
    DNGN_ENTER_OSSUARY,
    DNGN_ENTER_BAILEY,
    DNGN_ENTER_ICE_CAVE,
    DNGN_ENTER_VOLCANO,
    DNGN_ENTER_WIZLAB,
#if TAG_MAJOR_VERSION == 34
    DNGN_UNUSED_ENTER_PORTAL_1,
#endif

    DNGN_EXIT_ZIGGURAT,
    DNGN_EXIT_BAZAAR,
    DNGN_EXIT_TROVE,
    DNGN_EXIT_SEWER,
    DNGN_EXIT_OSSUARY,
    DNGN_EXIT_BAILEY,
    DNGN_EXIT_ICE_CAVE,
    DNGN_EXIT_VOLCANO,
    DNGN_EXIT_WIZLAB,
    DNGN_EXIT_LABYRINTH,
#if TAG_MAJOR_VERSION == 34
    DNGN_UNUSED_EXIT_PORTAL_1,

    DNGN_ALTAR_GOZAG,
    DNGN_ALTAR_QAZLAL,
    DNGN_ALTAR_RU,
#endif

    NUM_FEATURES
};

enum duration_type
{
    DUR_INVIS,
    DUR_CONF,
    DUR_PARALYSIS,
    DUR_SLOW,
    DUR_MESMERISED,
    DUR_HASTE,
    DUR_MIGHT,
    DUR_BRILLIANCE,
    DUR_AGILITY,
    DUR_FLIGHT,
    DUR_BERSERK,
    DUR_POISONING,

    DUR_CONFUSING_TOUCH,
    DUR_SURE_BLADE,
    DUR_CORONA,
    DUR_DEATHS_DOOR,
    DUR_FIRE_SHIELD,

#if TAG_MAJOR_VERSION == 34
    DUR_BUILDING_RAGE,
#endif
    DUR_EXHAUSTED,              // fatigue counter for berserk

    DUR_LIQUID_FLAMES,
    DUR_ICY_ARMOUR,
#if TAG_MAJOR_VERSION == 34
    DUR_REPEL_MISSILES,
    DUR_JELLY_PRAYER,
#endif
    DUR_PIETY_POOL,             // distribute piety over time
    DUR_DIVINE_VIGOUR,          // duration of Ely's Divine Vigour
    DUR_DIVINE_STAMINA,         // duration of Zin's Divine Stamina
    DUR_DIVINE_SHIELD,          // duration of TSO's Divine Shield
    DUR_REGENERATION,
    DUR_SWIFTNESS,
#if TAG_MAJOR_VERSION == 34
    DUR_CONTROLLED_FLIGHT,
#endif
    DUR_TELEPORT,
    DUR_CONTROL_TELEPORT,
    DUR_BREATH_WEAPON,
    DUR_TRANSFORMATION,
    DUR_DEATH_CHANNEL,
#if TAG_MAJOR_VERSION == 34
    DUR_DEFLECT_MISSILES,
#endif
    DUR_PHASE_SHIFT,
#if TAG_MAJOR_VERSION == 34
    DUR_SEE_INVISIBLE,
#endif
    DUR_WEAPON_BRAND,           // general "branding" spell counter
    DUR_DEMONIC_GUARDIAN,       // demonic guardian timeout
    DUR_POWERED_BY_DEATH,
    DUR_SILENCE,
    DUR_CONDENSATION_SHIELD,
    DUR_STONESKIN,
    DUR_GOURMAND,
#if TAG_MAJOR_VERSION == 34
    DUR_BARGAIN,
    DUR_INSULATION,
#endif
    DUR_RESISTANCE,
#if TAG_MAJOR_VERSION == 34
    DUR_SLAYING,
#endif
    DUR_STEALTH,
    DUR_MAGIC_SHIELD,
    DUR_SLEEP,
    DUR_TELEPATHY,
    DUR_PETRIFIED,
    DUR_LOWERED_MR,
    DUR_REPEL_STAIRS_MOVE,
    DUR_REPEL_STAIRS_CLIMB,
    DUR_COLOUR_SMOKE_TRAIL,
    DUR_SLIMIFY,
    DUR_TIME_STEP,
    DUR_ICEMAIL_DEPLETED,       // Wait this many turns for Icemail to return
#if TAG_MAJOR_VERSION == 34
    DUR_MISLED,
#endif
    DUR_QUAD_DAMAGE,
    DUR_AFRAID,
    DUR_MIRROR_DAMAGE,
    DUR_SCRYING,
    DUR_TORNADO,
    DUR_LIQUEFYING,
    DUR_HEROISM,
    DUR_FINESSE,
    DUR_LIFESAVING,
    DUR_PARALYSIS_IMMUNITY,
    DUR_DARKNESS,
    DUR_PETRIFYING,
    DUR_SHROUD_OF_GOLUBRIA,
    DUR_TORNADO_COOLDOWN,
#if TAG_MAJOR_VERSION == 34
    DUR_NAUSEA,
    DUR_AMBROSIA,
    DUR_TEMP_MUTATIONS,
#endif
    DUR_DISJUNCTION,
    DUR_VEHUMET_GIFT,
#if TAG_MAJOR_VERSION == 34
    DUR_BATTLESPHERE,
#endif
    DUR_SENTINEL_MARK,
    DUR_SICKENING,
    DUR_WATER_HOLD,
    DUR_WATER_HOLD_IMMUNITY,
    DUR_FLAYED,
#if TAG_MAJOR_VERSION == 34
    DUR_RETCHING,
#endif
    DUR_WEAK,
    DUR_DIMENSION_ANCHOR,
    DUR_ANTIMAGIC,
#if TAG_MAJOR_VERSION == 34
    DUR_SPIRIT_HOWL,
#endif
    DUR_INFUSION,
    DUR_SONG_OF_SLAYING,
#if TAG_MAJOR_VERSION == 34
    DUR_SONG_OF_SHIELDING,
#endif
    DUR_TOXIC_RADIANCE,
    DUR_RECITE,
    DUR_GRASPING_ROOTS,
    DUR_SLEEP_IMMUNITY,
    DUR_FIRE_VULN,
    DUR_ELIXIR_HEALTH,
    DUR_ELIXIR_MAGIC,
#if TAG_MAJOR_VERSION == 34
    DUR_ANTENNAE_EXTEND,
#endif
    DUR_TROGS_HAND,
    DUR_BARBS,
    DUR_POISON_VULN,
    DUR_FROZEN,
    DUR_SAP_MAGIC,
    DUR_MAGIC_SAPPED,
    DUR_PORTAL_PROJECTILE,
    DUR_FORESTED,
    DUR_DRAGON_CALL,
    DUR_DRAGON_CALL_COOLDOWN,
    DUR_ABJURATION_AURA,
    DUR_MESMERISE_IMMUNE,
    DUR_NO_POTIONS,
    DUR_QAZLAL_FIRE_RES,
    DUR_QAZLAL_COLD_RES,
    DUR_QAZLAL_ELEC_RES,
    DUR_QAZLAL_AC,
    DUR_CORROSION,
    DUR_FORTITUDE,
    DUR_HORROR,
    DUR_NO_SCROLLS,
    DUR_NEGATIVE_VULN,
    NUM_DURATIONS
};

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
    ENCH_ROT,
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
    ENCH_BATTLE_FRENZY,  //   Monster is in a battle frenzy.
#if TAG_MAJOR_VERSION == 34
    ENCH_TEMP_PACIF,
#endif
    ENCH_PETRIFYING,
    ENCH_PETRIFIED,
    ENCH_LOWERED_MR,
    ENCH_SOUL_RIPE,
    ENCH_SLOWLY_DYING,
    ENCH_EAT_ITEMS,
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
    ENCH_BLEED,
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
    ENCH_STONESKIN,
    ENCH_FEAR_INSPIRING,
    ENCH_PORTAL_PACIFIED,
    ENCH_WITHDRAWN,
#if TAG_MAJOR_VERSION == 34
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
    ENCH_ROUSED,         // Monster has been roused to greatness
    ENCH_BREATH_WEAPON,  // just a simple timer for dragon breathweapon spam
    ENCH_DEATHS_DOOR,
    ENCH_ROLLING,        // Boulder Beetle in ball form
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
    ENCH_CONTROL_WINDS,
#if TAG_MAJOR_VERSION == 34
    ENCH_WIND_AIDED,
#endif
    ENCH_SUMMON_CAPPED,  // Abjuring quickly because a summon cap was hit
    ENCH_TOXIC_RADIANCE,
    ENCH_GRASPING_ROOTS_SOURCE, // Not actually entangled, but entangling others
    ENCH_GRASPING_ROOTS,
    ENCH_IOOD_CHARGED,
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
    ENCH_EPHEMERAL_INFUSION,
    ENCH_BLACK_MARK,
    ENCH_GRAND_AVATAR,
    ENCH_SAP_MAGIC,
    ENCH_SHROUD,
    ENCH_PHANTOM_MIRROR,
    ENCH_BRIBED,
    ENCH_PERMA_BRIBED,
    ENCH_CORROSION,
    ENCH_GOLD_LUST,
    ENCH_DRAINED,
    ENCH_REPEL_MISSILES,
    ENCH_DEFLECT_MISSILES,
    ENCH_NEGATIVE_VULN,
    ENCH_CONDENSATION_SHIELD,
    // Update enchantment names in mon-ench.cc when adding or removing
    // enchantments.
    NUM_ENCHANTMENTS
};

enum energy_use_type
{
    EUT_MOVE,
    EUT_SWIM,
    EUT_ATTACK,
    EUT_MISSILE,
    EUT_SPELL,
    EUT_SPECIAL,
    EUT_ITEM,
    EUT_PICKUP,
};

enum equipment_type
{
    EQ_NONE = -1,

    EQ_WEAPON,
    EQ_CLOAK,
    EQ_HELMET,
    EQ_GLOVES,
    EQ_BOOTS,
    EQ_SHIELD,
    EQ_BODY_ARMOUR,
    //Everything beyond here is jewellery
    EQ_LEFT_RING,
    EQ_RIGHT_RING,
    EQ_AMULET,
    //Octopodes don't have left and right rings. They have eight rings, instead.
    EQ_RING_ONE,
    EQ_RING_TWO,
    EQ_RING_THREE,
    EQ_RING_FOUR,
    EQ_RING_FIVE,
    EQ_RING_SIX,
    EQ_RING_SEVEN,
    EQ_RING_EIGHT,
    // Finger amulet provides an extra ring slot
    EQ_RING_AMULET,
    NUM_EQUIP,

    EQ_MIN_ARMOUR = EQ_CLOAK,
    EQ_MAX_ARMOUR = EQ_BODY_ARMOUR,
    EQ_MAX_WORN   = EQ_RING_AMULET,
    // these aren't actual equipment slots, they're categories for functions
    EQ_STAFF            = 100,         // weapon with base_type OBJ_STAVES
    EQ_RINGS,                          // check both rings
    EQ_RINGS_PLUS,                     // check both rings and sum plus
#if TAG_MAJOR_VERSION == 34
    EQ_RINGS_PLUS2,                    // check both rings and sum plus2
#endif
    EQ_ALL_ARMOUR,                     // check all armour types
};

enum eq_type
{
    ET_WEAPON,
    ET_SHIELD,
    ET_ARMOUR,
    ET_JEWELS,
    NUM_ET
};

enum eq_type_flags
{
    ETF_WEAPON = 0x1,
    ETF_SHIELD = 0x2,
    ETF_ARMOUR = 0x4,
    ETF_JEWELS = 0x8,
    ETF_ALL    = 0xF
};

enum feature_flag_type
{
    FFT_NONE          = 0,
    FFT_NOTABLE       = 1<< 0,           // should be noted for dungeon overview
    FFT_EXAMINE_HINT  = 1<< 1,           // could get an "examine-this" hint.
    FFT_OPAQUE        = 1<< 2,           // Does this feature block LOS?
    FFT_WALL          = 1<< 3,           // Is this a "wall"?
    FFT_SOLID         = 1<< 4,           // Does this feature block beams / normal movement?
};

enum flush_reason_type
{
    FLUSH_ON_FAILURE,                  // spell/ability failed to cast
    FLUSH_BEFORE_COMMAND,              // flush before getting a command
    FLUSH_ON_MESSAGE,                  // flush when printing a message
    FLUSH_ON_WARNING_MESSAGE,          // flush on MSGCH_WARN messages
    FLUSH_ON_DANGER_MESSAGE,           // flush on MSGCH_DANGER messages
    FLUSH_ON_PROMPT,                   // flush on MSGCH_PROMPT messages
    FLUSH_ON_UNSAFE_YES_OR_NO_PROMPT,  // flush when !safe set to yesno()
    FLUSH_LUA,                         // flush when Lua wants to flush
    FLUSH_KEY_REPLAY_CANCEL,           // flush when key replay is cancelled
    FLUSH_ABORT_MACRO,                 // something wrong with macro being
                                       // processed, so stop it
    FLUSH_REPLAY_SETUP_FAILURE,        // setup for key replay failed
    FLUSH_REPEAT_SETUP_DONE,           // command repeat done manipulating
                                       // the macro buffer
    NUM_FLUSH_REASONS
};

enum god_type
{
    GOD_NO_GOD,
    GOD_ZIN,
    GOD_SHINING_ONE,
    GOD_KIKUBAAQUDGHA,
    GOD_YREDELEMNUL,
    GOD_XOM,
    GOD_VEHUMET,
    GOD_OKAWARU,
    GOD_MAKHLEB,
    GOD_SIF_MUNA,
    GOD_TROG,
    GOD_NEMELEX_XOBEH,
    GOD_ELYVILON,
    GOD_LUGONU,
    GOD_BEOGH,
    GOD_JIYVA,
    GOD_FEDHAS,
    GOD_CHEIBRIADOS,
    GOD_ASHENZARI,
    GOD_DITHMENOS,
    GOD_GOZAG,
    GOD_QAZLAL,
    GOD_RU,
    NUM_GODS,                          // always after last god

    GOD_RANDOM = 100,
    GOD_NAMELESS,                      // for monsters with non-player gods
    GOD_VIABLE,
};

enum held_type
{
    HELD_NONE = 0,
    HELD_NET,         // currently unused
    HELD_WEB,         // currently unused
    HELD_MONSTER,     // but no damage
    HELD_CONSTRICTED, // damaging
};

enum holy_word_source_type
{
    HOLY_WORD_SCROLL,
    HOLY_WORD_ZIN,     // sanctuary
    HOLY_WORD_TSO,     // weapon blessing
};

enum hunger_state_t                    // you.hunger_state
{
    HS_STARVING,
    HS_NEAR_STARVING,
    HS_VERY_HUNGRY,
    HS_HUNGRY,
    HS_SATIATED,                       // "not hungry" state
    HS_FULL,
    HS_VERY_FULL,
    HS_ENGORGED,
};

enum item_status_flag_type  // per item flags: ie. ident status, cursed status
{
    ISFLAG_KNOW_CURSE        = 0x00000001,  // curse status
    ISFLAG_KNOW_TYPE         = 0x00000002,  // artefact name, sub/special types
    ISFLAG_KNOW_PLUSES       = 0x00000004,  // to hit/to dam/to AC/charges
    ISFLAG_KNOW_PROPERTIES   = 0x00000008,  // know special artefact properties
    ISFLAG_IDENT_MASK        = 0x0000000F,  // mask of all id related flags

    ISFLAG_CURSED            = 0x00000100,  // cursed
                             //0x00000200,  // was: ISFLAG_BLESSED
    ISFLAG_SEEN_CURSED       = 0x00000400,  // was seen being cursed
    ISFLAG_TRIED             = 0x00000800,  // tried but not (usually) ided

    ISFLAG_RANDART           = 0x00001000,  // special value is seed
    ISFLAG_UNRANDART         = 0x00002000,  // is an unrandart
    ISFLAG_ARTEFACT_MASK     = 0x00003000,  // randart or unrandart
    ISFLAG_DROPPED           = 0x00004000,  // dropped item (no autopickup)
    ISFLAG_THROWN            = 0x00008000,  // thrown missile weapon

    // these don't have to remain as flags
    ISFLAG_NO_DESC           = 0x00000000,  // used for clearing these flags
    ISFLAG_GLOWING           = 0x00010000,  // weapons or armour
    ISFLAG_RUNED             = 0x00020000,  // weapons or armour
    ISFLAG_EMBROIDERED_SHINY = 0x00040000,  // armour: depends on sub-type
    ISFLAG_COSMETIC_MASK     = 0x00070000,  // mask of cosmetic descriptions

    ISFLAG_UNOBTAINABLE      = 0x00080000,  // vault on display

    ISFLAG_MIMIC             = 0x00100000,  // mimic
    ISFLAG_NO_MIMIC          = 0x00200000,  // Can't be turned into a mimic

    ISFLAG_NO_PICKUP         = 0x00400000,  // Monsters won't pick this up

#if TAG_MAJOR_VERSION == 34
    ISFLAG_UNUSED1           = 0x01000000,  // was ISFLAG_ORCISH
    ISFLAG_UNUSED2           = 0x02000000,  // was ISFLAG_DWARVEN
    ISFLAG_UNUSED3           = 0x04000000,  // was ISFLAG_ELVEN
    ISFLAG_RACIAL_MASK       = 0x07000000,  // mask of racial equipment types
#endif
    ISFLAG_NOTED_ID          = 0x08000000,
    ISFLAG_NOTED_GET         = 0x10000000,

    ISFLAG_SEEN              = 0x20000000,  // has it been seen
    ISFLAG_SUMMONED          = 0x40000000,  // Item generated on a summon
#if TAG_MAJOR_VERSION == 34
    ISFLAG_UNUSED4           = 0x80000000,  // was ISFLAG_DROPPED_BY_ALLY
#endif
};

enum item_type_id_state_type
{
    ID_UNKNOWN_TYPE = 0,
    ID_MON_TRIED_TYPE,
    ID_TRIED_TYPE,
    ID_TRIED_ITEM_TYPE,
    ID_KNOWN_TYPE,
    NUM_ID_STATE_TYPES
};

enum job_type
{
    JOB_FIGHTER,
    JOB_WIZARD,
#if TAG_MAJOR_VERSION == 34
    JOB_PRIEST,
#endif
    JOB_GLADIATOR,
    JOB_NECROMANCER,
    JOB_ASSASSIN,
    JOB_BERSERKER,
    JOB_HUNTER,
    JOB_CONJURER,
    JOB_ENCHANTER,
    JOB_FIRE_ELEMENTALIST,
    JOB_ICE_ELEMENTALIST,
    JOB_SUMMONER,
    JOB_AIR_ELEMENTALIST,
    JOB_EARTH_ELEMENTALIST,
    JOB_SKALD,
    JOB_VENOM_MAGE,
    JOB_CHAOS_KNIGHT,
    JOB_TRANSMUTER,
    JOB_HEALER,
#if TAG_MAJOR_VERSION == 34
    JOB_STALKER,
#endif
    JOB_MONK,
    JOB_WARPER,
    JOB_WANDERER,
    JOB_ARTIFICER,                     //   Greenberg/Bane
    JOB_ARCANE_MARKSMAN,
    JOB_DEATH_KNIGHT,
    JOB_ABYSSAL_KNIGHT,
#if TAG_MAJOR_VERSION == 34
    JOB_JESTER,
#endif
    NUM_JOBS,                          // always after the last job

    JOB_UNKNOWN = 100,
    JOB_RANDOM,
    JOB_VIABLE,
};

enum KeymapContext
{
    KMC_DEFAULT,         // For no-arg getchm(), must be zero.
    KMC_LEVELMAP,        // When in the 'X' level map
    KMC_TARGETING,       // Only during 'x' and other targeting modes
    KMC_CONFIRM,         // When being asked y/n/q questions
    KMC_MENU,            // For menus
#ifdef USE_TILE
    KMC_DOLL,            // For the tiles doll menu editing screen
#endif

    KMC_CONTEXT_COUNT,   // Must always be the last real context

    KMC_NONE
};

// This order is *critical*. Don't mess with it (see mon_enchant)
enum kill_category
{
    KC_YOU,
    KC_FRIENDLY,
    KC_OTHER,
    KC_NCATEGORIES
};

enum killer_type                       // monster_die(), thing_thrown
{
    KILL_NONE,                         // no killer
    KILL_YOU,                          // you are the killer
    KILL_MON,                          // no, it was a monster!
    KILL_YOU_MISSILE,                  // in the library, with a dart
    KILL_MON_MISSILE,                  // in the dungeon, with a club
    KILL_YOU_CONF,                     // died while confused as caused by you
    KILL_MISCAST,                      // as a result of a spell miscast
    KILL_MISC,                         // any miscellaneous killing
    KILL_RESET,                        // excised from existence
    KILL_DISMISSED,                    // like KILL_RESET, but drops inventory
    KILL_BANISHED,                     // monsters what got banished
    KILL_UNSUMMONED,                   // summoned monsters whose timers ran out
    KILL_TIMEOUT,                      // non-summoned monsters whose times ran out
    KILL_PACIFIED,                     // only used by milestones and notes
    KILL_ENSLAVED,                     // only used by milestones and notes
};

enum flight_type
{
    FL_NONE = 0,
    FL_WINGED,                         // wings, etc... paralysis == fall
    FL_LEVITATE,                       // doesn't require physical effort
};

// Can't change this order without breaking saves.
enum map_marker_type
{
    MAT_FEATURE,              // Stock marker.
    MAT_LUA_MARKER,
    MAT_CORRUPTION_NEXUS,
    MAT_WIZ_PROPS,
    MAT_TOMB,
    MAT_MALIGN,
#if TAG_MAJOR_VERSION == 34
    MAT_PHOENIX,
#endif
    MAT_POSITION,
#if TAG_MAJOR_VERSION == 34
    MAT_DOOR_SEAL,
#endif
    MAT_TERRAIN_CHANGE,
    MAT_CLOUD_SPREADER,
    NUM_MAP_MARKER_TYPES,
    MAT_ANY,
};

enum terrain_change_type
{
    TERRAIN_CHANGE_GENERIC,
    TERRAIN_CHANGE_FLOOD,
    TERRAIN_CHANGE_TOMB,
    TERRAIN_CHANGE_IMPRISON,
    TERRAIN_CHANGE_DOOR_SEAL,
    TERRAIN_CHANGE_FORESTED,
    NUM_TERRAIN_CHANGE_TYPES
};

enum map_feature
{
    MF_UNSEEN,
    MF_FLOOR,
    MF_WALL,
    MF_MAP_FLOOR,
    MF_MAP_WALL,
    MF_DOOR,
    MF_ITEM,
    MF_MONS_FRIENDLY,
    MF_MONS_PEACEFUL,
    MF_MONS_NEUTRAL,
    MF_MONS_HOSTILE,
    MF_MONS_NO_EXP,
    MF_STAIR_UP,
    MF_STAIR_DOWN,
    MF_STAIR_BRANCH,
    MF_FEATURE,
    MF_WATER,
    MF_LAVA,
    MF_TRAP,
    MF_EXCL_ROOT,
    MF_EXCL,
    MF_PLAYER,
    MF_DEEP_WATER,
    MF_PORTAL,
    MF_MAX,

    MF_SKIP,
};

enum menu_type
{
    MT_ANY = -1,

    MT_INVLIST,                        // List inventory
    MT_DROP,
    MT_PICKUP,
    MT_KNOW,
    MT_RUNES,
    MT_SELONE,                         // Select one
};

enum mon_holy_type
{
    MH_HOLY,
    MH_NATURAL,
    MH_UNDEAD,
    MH_DEMONIC,
    MH_NONLIVING, // golems and other constructs
    MH_PLANT,
};

enum targ_mode_type
{
    TARG_ANY,
    TARG_ENEMY,  // hostile + neutral
    TARG_FRIEND,
    TARG_INJURED_FRIEND, // for healing
    TARG_HOSTILE,
    TARG_HOSTILE_SUBMERGED, // Target hostiles including submerged ones
    TARG_EVOLVABLE_PLANTS,  // Targeting mode for Fedhas' evolution
    TARG_HOSTILE_UNDEAD,    // For dispel undead
    TARG_BEOGH_GIFTABLE,    // For Beogh followers who can be given gifts
    TARG_NUM_MODES
};

// NOTE: Changing this order will break saves! Appending does not.
enum monster_type                      // menv[].type
{
    MONS_PROGRAM_BUG,
        MONS_0 = MONS_PROGRAM_BUG,

#if TAG_MAJOR_VERSION > 34
    MONS_GIANT_LIZARD,          // genus
#endif
    MONS_GIANT_NEWT,
    MONS_GIANT_GECKO,
    MONS_IGUANA,
    MONS_KOMODO_DRAGON,
    MONS_BASILISK,
    MONS_BAT,
    MONS_FIRE_BAT,
#if TAG_MAJOR_VERSION > 34
    MONS_SNAKE,                // genus
#endif
    MONS_BALL_PYTHON,
    MONS_ADDER,
    MONS_WATER_MOCCASIN,
    MONS_BLACK_MAMBA,
    MONS_ANACONDA,
    MONS_SEA_SNAKE,
#if TAG_MAJOR_VERSION > 34
    MONS_SHOCK_SERPENT,
    MONS_MANA_VIPER,
#endif
    MONS_RAT,
#if TAG_MAJOR_VERSION == 34
    MONS_GREY_RAT,
#endif
    MONS_GREEN_RAT,
    MONS_ORANGE_RAT,
#if TAG_MAJOR_VERSION == 34
    MONS_LABORATORY_RAT,
#endif
    MONS_QUOKKA,         // Quokka are a type of wallaby, returned -- bwr 382
    MONS_PORCUPINE,
    MONS_JACKAL,
    MONS_HOUND,
#if TAG_MAJOR_VERSION == 34
    MONS_WAR_DOG,
#endif
    MONS_WOLF,
    MONS_WARG,
    MONS_HELL_HOUND,
#if TAG_MAJOR_VERSION > 34
    MONS_RAIJU,
#endif
    MONS_HOG,
    MONS_HELL_HOG,
    MONS_HOLY_SWINE,            // porkalator
#if TAG_MAJOR_VERSION == 34
    MONS_GIANT_SLUG,
    MONS_AGATE_SNAIL,
#else
    MONS_TORPOR_SNAIL,
#endif
    MONS_ELEPHANT_SLUG,
    MONS_GIANT_LEECH,
    MONS_BABY_ALLIGATOR,
    MONS_ALLIGATOR,
    MONS_CROCODILE,
    MONS_HYDRA,
    MONS_SHEEP,
    MONS_YAK,
    MONS_DEATH_YAK,
    MONS_CATOBLEPAS,
    MONS_ELEPHANT,
    MONS_DIRE_ELEPHANT,
    MONS_HELLEPHANT,
    MONS_MANTICORE,
    MONS_HIPPOGRIFF,
    MONS_GRIFFON,
#if TAG_MAJOR_VERSION > 34
    MONS_CHIMERA,
#endif
    MONS_GIANT_FROG,
    MONS_SPINY_FROG,
    MONS_BLINK_FROG,
#if TAG_MAJOR_VERSION > 34
    MONS_BEAR,                  // genus
#endif
    MONS_GRIZZLY_BEAR,
    MONS_POLAR_BEAR,
    MONS_BLACK_BEAR,
    MONS_WORM,
    MONS_BRAIN_WORM,
#if TAG_MAJOR_VERSION == 34
    MONS_ROCK_WORM,
    MONS_SPINY_WORM,
#endif
    MONS_WYVERN,
#if TAG_MAJOR_VERSION > 34
    MONS_DRAKE,                 // genus
#endif
    MONS_LINDWURM,
    MONS_FIRE_DRAKE,
    MONS_SWAMP_DRAKE,
    MONS_DEATH_DRAKE,
#if TAG_MAJOR_VERSION > 34
    MONS_WIND_DRAKE,
    MONS_DRAGON,                // genus
#endif
    MONS_STEAM_DRAGON,
    MONS_MOTTLED_DRAGON,
    MONS_SWAMP_DRAGON,
    MONS_FIRE_DRAGON,
    MONS_ICE_DRAGON,
    MONS_SHADOW_DRAGON,
    MONS_STORM_DRAGON,
    MONS_BONE_DRAGON,
    MONS_QUICKSILVER_DRAGON,
    MONS_IRON_DRAGON,
    MONS_GOLDEN_DRAGON,
    MONS_PEARL_DRAGON,

    MONS_OOZE,
    MONS_JELLY,
#if TAG_MAJOR_VERSION == 34
    MONS_BROWN_OOZE,
    MONS_GIANT_AMOEBA,
#endif
    MONS_AZURE_JELLY,
    MONS_DEATH_OOZE,
    MONS_ACID_BLOB,
    MONS_SLIME_CREATURE,
#if TAG_MAJOR_VERSION == 34
    MONS_PULSATING_LUMP,
#endif
    MONS_GIANT_EYEBALL,
    MONS_EYE_OF_DRAINING,
    MONS_SHINING_EYE,
    MONS_EYE_OF_DEVASTATION,
    MONS_GREAT_ORB_OF_EYES,
    MONS_GIANT_ORANGE_BRAIN,

    MONS_DANCING_WEAPON,
#if TAG_MAJOR_VERSION > 34
    MONS_SPECTRAL_WEAPON,
    MONS_GRAND_AVATAR,
#endif
    MONS_HARPY,
    MONS_RAVEN,
#if TAG_MAJOR_VERSION > 34
    MONS_BENNU,
    MONS_CAUSTIC_SHRIKE,

    MONS_ANUBIS_GUARD,
#endif
    MONS_FIRE_CRAB,
#if TAG_MAJOR_VERSION == 34
    MONS_HOMUNCULUS,
    MONS_SOUPLING,
#else
    MONS_GHOST_CRAB,
    MONS_CRAB,
#endif

    MONS_BUTTERFLY,
#if TAG_MAJOR_VERSION == 34
    MONS_ANT_LARVA,
#endif
    MONS_WORKER_ANT,
    MONS_SOLDIER_ANT,
    MONS_QUEEN_ANT,
#if TAG_MAJOR_VERSION > 34
    MONS_FORMICID,
#endif
    MONS_KILLER_BEE,
    MONS_QUEEN_BEE,
    MONS_VAMPIRE_MOSQUITO,
#if TAG_MAJOR_VERSION == 34
    MONS_BUMBLEBEE,
#endif
    MONS_YELLOW_WASP,
    MONS_RED_WASP,
    MONS_GOLIATH_BEETLE,
    MONS_BORING_BEETLE,
    MONS_BOULDER_BEETLE,
#if TAG_MAJOR_VERSION > 34
    MONS_DEATH_SCARAB,
#endif
    MONS_GIANT_COCKROACH,
#if TAG_MAJOR_VERSION == 34
    MONS_GIANT_CENTIPEDE,
#endif
    MONS_GIANT_MITE,
    MONS_SPIDER,
    MONS_WOLF_SPIDER,
    MONS_TRAPDOOR_SPIDER,
    MONS_JUMPING_SPIDER,
    MONS_ORB_SPIDER,
    MONS_TARANTELLA,
    MONS_REDBACK,
    MONS_SCORPION,
    MONS_EMPEROR_SCORPION,
    MONS_MOTH,                  // genus
#if TAG_MAJOR_VERSION == 34
    MONS_MOTH_OF_SUPPRESSION,
#endif
    MONS_GHOST_MOTH,
    MONS_MOTH_OF_WRATH,
    MONS_DEMONIC_CRAWLER,
    MONS_SNAPPING_TURTLE,
    MONS_ALLIGATOR_SNAPPING_TURTLE,
#if TAG_MAJOR_VERSION == 34
    MONS_GNOME,
#endif
    MONS_HALFLING,              // recolouring + single vault.
    MONS_FELID,                 // recolouring + single vault.  Miaow!
    MONS_VAMPIRE_BAT,           // recolouring + vaults
    MONS_DEMIGOD,               // recolouring + single vault
    MONS_DEMONSPAWN,
#if TAG_MAJOR_VERSION > 34
    MONS_FIRST_DEMONSPAWN = MONS_DEMONSPAWN,
    MONS_MONSTROUS_DEMONSPAWN,
    MONS_FIRST_BASE_DEMONSPAWN = MONS_MONSTROUS_DEMONSPAWN,
    MONS_GELID_DEMONSPAWN,
    MONS_INFERNAL_DEMONSPAWN,
    MONS_PUTRID_DEMONSPAWN,
    MONS_TORTUROUS_DEMONSPAWN,
    MONS_LAST_BASE_DEMONSPAWN = MONS_TORTUROUS_DEMONSPAWN,
    MONS_BLOOD_SAINT,
    MONS_FIRST_NONBASE_DEMONSPAWN = MONS_BLOOD_SAINT,
    MONS_CHAOS_CHAMPION,
    MONS_WARMONGER,
    MONS_CORRUPTER,
    MONS_BLACK_SUN,
    MONS_LAST_NONBASE_DEMONSPAWN = MONS_BLACK_SUN,
    MONS_LAST_DEMONSPAWN = MONS_BLACK_SUN,
#endif
    MONS_GARGOYLE,
    MONS_WAR_GARGOYLE,
    MONS_MOLTEN_GARGOYLE,
    MONS_UGLY_THING,
    MONS_VERY_UGLY_THING,
    MONS_ICE_BEAST,
    MONS_SKY_BEAST,
    MONS_SPHINX,
    MONS_ORB_GUARDIAN,

    MONS_GOLEM,                 // genus
#if TAG_MAJOR_VERSION == 34
    MONS_CLAY_GOLEM,
    MONS_WOOD_GOLEM,
    MONS_STONE_GOLEM,
#endif
    MONS_IRON_GOLEM,
    MONS_CRYSTAL_GUARDIAN,
    MONS_TOENAIL_GOLEM,
    MONS_ELECTRIC_GOLEM, // replacing the guardian robot -- bwr
#if TAG_MAJOR_VERSION > 34
    MONS_GUARDIAN_GOLEM,
    MONS_SPELLFORGED_SERVITOR,
    MONS_USHABTI,
#endif
    MONS_ORB_OF_FIRE,    // Swords renamed to fit -- bwr
#if TAG_MAJOR_VERSION > 34
    MONS_ELEMENTAL,             // genus
#endif
    MONS_EARTH_ELEMENTAL,
    MONS_FIRE_ELEMENTAL,
    MONS_AIR_ELEMENTAL,
#if TAG_MAJOR_VERSION > 34
    MONS_IRON_ELEMENTAL,
#endif
    MONS_TWISTER,        // air miscasts
    MONS_GOLDEN_EYE,
    MONS_FIRE_VORTEX,
    MONS_SPATIAL_VORTEX,
    MONS_INSUBSTANTIAL_WISP,
#if TAG_MAJOR_VERSION == 34
    MONS_VAPOUR,

    // Mimics:
    MONS_INEPT_ITEM_MIMIC,
    MONS_ITEM_MIMIC,
    MONS_RAVENOUS_ITEM_MIMIC,
    MONS_MONSTROUS_ITEM_MIMIC,
    MONS_INEPT_FEATURE_MIMIC,
    MONS_FEATURE_MIMIC,
    MONS_RAVENOUS_FEATURE_MIMIC,
    MONS_MONSTROUS_FEATURE_MIMIC, // unused
#endif

    // Plants:
    MONS_TOADSTOOL,
    MONS_FUNGUS,
    MONS_WANDERING_MUSHROOM,
#if TAG_MAJOR_VERSION > 34
    MONS_DEATHCAP,
#endif
    MONS_PLANT,
    MONS_OKLOB_SAPLING,
    MONS_OKLOB_PLANT,
    MONS_BUSH,
    MONS_BURNING_BUSH,
#if TAG_MAJOR_VERSION > 34
    MONS_THORN_HUNTER,
    MONS_BRIAR_PATCH,
    MONS_SHAMBLING_MANGROVE,
    MONS_VINE_STALKER,
    MONS_ANIMATED_TREE,
#endif
    MONS_GIANT_SPORE,
    MONS_BALLISTOMYCETE,
    MONS_HYPERACTIVE_BALLISTOMYCETE,

    MONS_GOBLIN,
    MONS_HOBGOBLIN,
    MONS_GNOLL,
    MONS_GNOLL_SHAMAN,
    MONS_GNOLL_SERGEANT,
    MONS_BOGGART,
    MONS_KOBOLD,
    MONS_BIG_KOBOLD,
    MONS_KOBOLD_DEMONOLOGIST,
    MONS_ORC,
    MONS_ORC_WARRIOR,
    MONS_ORC_PRIEST,
    MONS_ORC_HIGH_PRIEST,
    MONS_ORC_WIZARD,
    MONS_ORC_KNIGHT,
    MONS_ORC_SORCERER,
    MONS_ORC_WARLORD,
    MONS_DWARF,
    MONS_DEEP_DWARF,
#if TAG_MAJOR_VERSION == 34
    MONS_DEEP_DWARF_SCION,
    MONS_DEEP_DWARF_ARTIFICER,
    MONS_DEEP_DWARF_NECROMANCER,
    MONS_DEEP_DWARF_BERSERKER,
    MONS_DEATH_KNIGHT,
    MONS_UNBORN,
#endif
    MONS_ELF,
#if TAG_MAJOR_VERSION == 34
    MONS_DEEP_ELF_SOLDIER,
#endif
    MONS_DEEP_ELF_FIGHTER,
    MONS_DEEP_ELF_KNIGHT,
    MONS_DEEP_ELF_MAGE,
    MONS_DEEP_ELF_SUMMONER,
    MONS_DEEP_ELF_CONJURER,
    MONS_DEEP_ELF_PRIEST,
    MONS_DEEP_ELF_HIGH_PRIEST,
    MONS_DEEP_ELF_DEMONOLOGIST,
    MONS_DEEP_ELF_ANNIHILATOR,
    MONS_DEEP_ELF_SORCERER,
    MONS_DEEP_ELF_DEATH_MAGE,
    MONS_DEEP_ELF_BLADEMASTER,
    MONS_DEEP_ELF_MASTER_ARCHER,
    MONS_SPRIGGAN,
    MONS_SPRIGGAN_DRUID,
#if TAG_MAJOR_VERSION == 34
    MONS_SPRIGGAN_ASSASSIN,
#endif
    MONS_SPRIGGAN_RIDER,
    MONS_SPRIGGAN_BERSERKER,
    MONS_SPRIGGAN_DEFENDER,
    MONS_SPRIGGAN_AIR_MAGE,
#if TAG_MAJOR_VERSION == 34
    MONS_FIREFLY,
#endif
    MONS_TENGU,
#if TAG_MAJOR_VERSION > 34
    MONS_TENGU_WARRIOR,
    MONS_TENGU_CONJURER,
    MONS_TENGU_REAVER,
#endif
    MONS_MINOTAUR,
    MONS_NAGA,
    MONS_NAGA_WARRIOR,
    MONS_NAGA_MAGE,
#if TAG_MAJOR_VERSION > 34
    MONS_NAGA_RITUALIST,
    MONS_NAGA_SHARPSHOOTER,
#endif
    MONS_GREATER_NAGA,
    MONS_GUARDIAN_SERPENT,
    MONS_OCTOPODE,
#if TAG_MAJOR_VERSION > 34
    MONS_OCTOPODE_CRUSHER,
#endif
    MONS_MERFOLK,
    MONS_SIREN,
    MONS_MERFOLK_AVATAR,
#if TAG_MAJOR_VERSION > 34
    MONS_DROWNED_SOUL,
#endif
    MONS_MERFOLK_IMPALER,
    MONS_MERFOLK_AQUAMANCER,
    MONS_MERFOLK_JAVELINEER,
#if TAG_MAJOR_VERSION > 34
    MONS_WATER_NYMPH,
#endif
    MONS_CENTAUR,
    MONS_CENTAUR_WARRIOR,
    MONS_YAKTAUR,
    MONS_YAKTAUR_CAPTAIN,
#if TAG_MAJOR_VERSION > 34
    MONS_FAUN,
    MONS_SATYR,
#endif
    MONS_OGRE,
    MONS_TWO_HEADED_OGRE,
    MONS_OGRE_MAGE,
    MONS_TROLL,
#if TAG_MAJOR_VERSION == 34
    MONS_ROCK_TROLL,
#endif
    MONS_IRON_TROLL,
    MONS_DEEP_TROLL,
#if TAG_MAJOR_VERSION > 34
    MONS_DEEP_TROLL_EARTH_MAGE,
    MONS_DEEP_TROLL_SHAMAN,
#endif
    MONS_GIANT,                 // genus
    MONS_HILL_GIANT,
    MONS_CYCLOPS,
    MONS_ETTIN,
    MONS_STONE_GIANT,
    MONS_FIRE_GIANT,
    MONS_FROST_GIANT,
    MONS_TITAN,
    MONS_HUMAN,
    MONS_SLAVE,
    MONS_HELL_KNIGHT,
#if TAG_MAJOR_VERSION > 34
    MONS_DEATH_KNIGHT,
#endif
    MONS_NECROMANCER,
    MONS_WIZARD,
    MONS_VAULT_GUARD,
#if TAG_MAJOR_VERSION > 34
    MONS_VAULT_SENTINEL,
    MONS_VAULT_WARDEN,
    MONS_IRONBRAND_CONVOKER,
    MONS_IRONHEART_PRESERVER,
#endif
    MONS_KILLER_KLOWN,
    MONS_SHAPESHIFTER,
    MONS_GLOWING_SHAPESHIFTER,

    // Draconians:
    MONS_DRACONIAN,
    MONS_FIRST_DRACONIAN = MONS_DRACONIAN,

    // If adding more drac colours, sync up colour names in
    // mon-util.cc.
    MONS_BLACK_DRACONIAN,
    MONS_FIRST_BASE_DRACONIAN = MONS_BLACK_DRACONIAN,
    MONS_MOTTLED_DRACONIAN,
    MONS_YELLOW_DRACONIAN,
    MONS_GREEN_DRACONIAN,
    MONS_PURPLE_DRACONIAN,
    MONS_RED_DRACONIAN,
    MONS_WHITE_DRACONIAN,
    MONS_GREY_DRACONIAN,
    MONS_PALE_DRACONIAN,
    MONS_LAST_BASE_DRACONIAN = MONS_PALE_DRACONIAN,

    // Sync up with mon-place.cc's draconian selection if adding more.
    MONS_DRACONIAN_CALLER,
    MONS_FIRST_NONBASE_DRACONIAN = MONS_DRACONIAN_CALLER,
    MONS_DRACONIAN_MONK,
    MONS_DRACONIAN_ZEALOT,
    MONS_DRACONIAN_SHIFTER,
    MONS_DRACONIAN_ANNIHILATOR,
    MONS_DRACONIAN_KNIGHT,
    MONS_DRACONIAN_SCORCHER,

    MONS_LAST_DRACONIAN = MONS_DRACONIAN_SCORCHER,
    MONS_LAST_NONBASE_DRACONIAN = MONS_DRACONIAN_SCORCHER,

    // Lava monsters:
#if TAG_MAJOR_VERSION == 34
    MONS_LAVA_WORM,
    MONS_LAVA_FISH,
#endif
    MONS_LAVA_SNAKE,
    MONS_SALAMANDER,
#if TAG_MAJOR_VERSION > 34
    MONS_SALAMANDER_FIREBRAND,
    MONS_SALAMANDER_MYSTIC,
#endif

    // Water monsters:
#if TAG_MAJOR_VERSION == 34
    MONS_BIG_FISH,
    MONS_GIANT_GOLDFISH,
#endif
    MONS_ELECTRIC_EEL,
#if TAG_MAJOR_VERSION == 34
    MONS_JELLYFISH,
#endif
    MONS_WATER_ELEMENTAL,
    MONS_SWAMP_WORM,
#if TAG_MAJOR_VERSION == 34
    MONS_SHARK,
#endif
    MONS_KRAKEN,
    MONS_KRAKEN_TENTACLE,
    MONS_KRAKEN_TENTACLE_SEGMENT,

    // Statuary
    MONS_ORANGE_STATUE,
    MONS_OBSIDIAN_STATUE,
    MONS_ICE_STATUE,
    MONS_STATUE,
    MONS_TRAINING_DUMMY,
    MONS_LIGHTNING_SPIRE,
#if TAG_MAJOR_VERSION > 34
    MONS_DIAMOND_OBELISK,
#endif

    // Demons:
    MONS_CRIMSON_IMP,
    MONS_QUASIT,
    MONS_WHITE_IMP,
#if TAG_MAJOR_VERSION == 34
    MONS_LEMURE,
#endif
    MONS_UFETUBUS,
    MONS_IRON_IMP,
    MONS_SHADOW_IMP,
    MONS_RED_DEVIL,
#if TAG_MAJOR_VERSION == 34
    MONS_ROTTING_DEVIL,
#endif
    MONS_HELLWING,
    MONS_SIXFIRHY,
    MONS_NEQOXEC,
    MONS_ORANGE_DEMON,
    MONS_SMOKE_DEMON,
    MONS_YNOXINUL,
    MONS_CHAOS_SPAWN,
    MONS_HELLION,
    MONS_LOROCYPROCA,
    MONS_TORMENTOR,
    MONS_REAPER,
    MONS_SOUL_EATER,
    MONS_ICE_DEVIL,
    MONS_BLUE_DEVIL,
    MONS_HELL_BEAST,
    MONS_RUST_DEVIL,
    MONS_EXECUTIONER,
    MONS_GREEN_DEATH,
    MONS_BLIZZARD_DEMON,
    MONS_BALRUG,
    MONS_CACODEMON,
    MONS_SUN_DEMON,
    MONS_SHADOW_DEMON,
    MONS_HELL_SENTINEL,
    MONS_BRIMSTONE_FIEND,
    MONS_ICE_FIEND,
    MONS_SHADOW_FIEND,
    MONS_PANDEMONIUM_LORD,

    // Spiritual beings ('R')
    MONS_EFREET,
    MONS_RAKSHASA,
#if TAG_MAJOR_VERSION == 34
    MONS_RAKSHASA_FAKE,
#endif
#if TAG_MAJOR_VERSION > 34
    MONS_DRYAD,
    MONS_SNAPLASHER_VINE,
    MONS_SNAPLASHER_VINE_SEGMENT,
#endif

    // Abyssals
    MONS_UNSEEN_HORROR,
    MONS_TENTACLED_STARSPAWN,
    MONS_LURKING_HORROR,
    MONS_THRASHING_HORROR,
    MONS_STARCURSED_MASS,
    MONS_ANCIENT_ZYME,
    MONS_WRETCHED_STAR,
#if TAG_MAJOR_VERSION > 34
    MONS_APOCALYPSE_CRAB,
    MONS_STARSPAWN_TENTACLE,
    MONS_STARSPAWN_TENTACLE_SEGMENT,
    MONS_SPATIAL_MAELSTROM,
    MONS_WORLDBINDER,
#endif
    MONS_ELDRITCH_TENTACLE,
    MONS_ELDRITCH_TENTACLE_SEGMENT,
    MONS_TENTACLED_MONSTROSITY,
    MONS_ABOMINATION_SMALL,
    MONS_ABOMINATION_LARGE,
    MONS_CRAWLING_CORPSE,
    MONS_MACABRE_MASS,

    // Undead:
#if TAG_MAJOR_VERSION > 34
    MONS_ZOMBIE,
    MONS_SKELETON,
    MONS_SIMULACRUM,
#endif
#if TAG_MAJOR_VERSION == 34
    MONS_PLAGUE_SHAMBLER,
#endif
    MONS_NECROPHAGE,
    MONS_GHOUL,
#if TAG_MAJOR_VERSION == 34
    MONS_FLAMING_CORPSE,
#endif
    MONS_MUMMY,
    MONS_BOG_BODY,
    MONS_GUARDIAN_MUMMY,
    MONS_GREATER_MUMMY,
    MONS_MUMMY_PRIEST,
    MONS_VAMPIRE,
    MONS_VAMPIRE_KNIGHT,
    MONS_VAMPIRE_MAGE,
    MONS_GHOST,                 // common genus for monster and player ghosts
    MONS_PHANTOM,
    MONS_SHADOW,
    MONS_HUNGRY_GHOST,
    MONS_FLAYED_GHOST,
    MONS_WIGHT,
    MONS_WRAITH,
    MONS_FREEZING_WRAITH,
    MONS_SHADOW_WRAITH,
    MONS_SILENT_SPECTRE,
    MONS_EIDOLON,
    MONS_FLYING_SKULL,
    MONS_SKELETAL_WARRIOR,
    MONS_PHANTASMAL_WARRIOR,
    MONS_LICH,
    MONS_ANCIENT_LICH,
    MONS_DEATH_COB,
    MONS_CURSE_TOE,
    MONS_CURSE_SKULL,
    MONS_PROFANE_SERVITOR,
#if TAG_MAJOR_VERSION > 34
    MONS_ANCIENT_CHAMPION,
    MONS_REVENANT,
    MONS_UNBORN,
    MONS_LOST_SOUL,
    MONS_JIANGSHI,
#endif
    MONS_SKELETON_SMALL,   // recolouring only
    MONS_SKELETON_LARGE,   // recolouring only
    MONS_ZOMBIE_SMALL,     // recolouring only
    MONS_ZOMBIE_LARGE,     // recolouring only
    MONS_SPECTRAL_THING,
    MONS_SIMULACRUM_SMALL, // recolouring only
    MONS_SIMULACRUM_LARGE, // recolouring only

    // Holies:
    MONS_ANGEL,
    MONS_DAEVA,
    MONS_CHERUB,
    MONS_SERAPH,
#if TAG_MAJOR_VERSION == 34
    MONS_PHOENIX,
    MONS_SILVER_STAR,
    MONS_BLESSED_TOE,
    MONS_SHEDU,
#endif
    MONS_OPHAN,
#if TAG_MAJOR_VERSION == 34
    MONS_SPIRIT,
    MONS_PALADIN,
#endif
    MONS_APIS,

    // Fixed uniques:
    MONS_GERYON,
    MONS_DISPATER,
    MONS_ASMODEUS,
    MONS_ANTAEUS,
    MONS_ERESHKIGAL,
    MONS_ROYAL_JELLY,
    MONS_THE_ENCHANTRESS,
    // the four Pan lords, order must match runes
    MONS_MNOLEG,
    MONS_LOM_LOBON,
    MONS_CEREBOV,
    MONS_GLOORX_VLOQ,
    MONS_SERPENT_OF_HELL,
#if TAG_MAJOR_VERSION > 34
    MONS_SERPENT_OF_HELL_COCYTUS,
    MONS_SERPENT_OF_HELL_DIS,
    MONS_SERPENT_OF_HELL_TARTARUS,
#endif
    // Random uniques:
    MONS_IJYB,
    MONS_JESSICA,
    MONS_SIGMUND,
    MONS_TERENCE,
    MONS_BLORK_THE_ORC,
    MONS_EDMUND,
    MONS_PSYCHE,
    MONS_EROLCHA,
    MONS_DONALD,
    MONS_URUG,
    MONS_JOSEPH,
    MONS_SNORG, // was Anita - 16jan2000 {dlb}
    MONS_ERICA,
    MONS_JOSEPHINE,
    MONS_HAROLD,
    MONS_AGNES,
    MONS_MAUD,
    MONS_LOUISE,
    MONS_FRANCES,
    MONS_RUPERT,
    MONS_WIGLAF,
    MONS_XTAHUA,
    MONS_NORRIS,
    MONS_FREDERICK,
    MONS_MARGERY,
    MONS_BORIS,
    MONS_POLYPHEMUS,
    MONS_MURRAY,
    MONS_TIAMAT,
    MONS_ROXANNE,
    MONS_SONJA,
    MONS_EUSTACHIO,
    MONS_AZRAEL,
    MONS_ILSUIW,
    MONS_PRINCE_RIBBIT,
    MONS_NERGALLE,
    MONS_SAINT_ROKA,
    MONS_NESSOS,
    MONS_LERNAEAN_HYDRA,
    MONS_DISSOLUTION,
    MONS_KIRKE,
    MONS_GRUM,
    MONS_PURGY,
    MONS_MENKAURE,
    MONS_DUVESSA,
    MONS_DOWAN,
    MONS_GASTRONOK,
    MONS_MAURICE,
    MONS_KHUFU,
    MONS_NIKOLA,
    MONS_AIZUL,
    MONS_PIKEL,
    MONS_CRAZY_YIUF,
    MONS_MENNAS,
    MONS_MARA,
#if TAG_MAJOR_VERSION == 34
    MONS_MARA_FAKE,
#endif
    MONS_GRINDER,
    MONS_JORY,
    MONS_IGNACIO,
    MONS_ARACHNE,
#if TAG_MAJOR_VERSION > 34
    MONS_HELLBINDER,
    MONS_CLOUD_MAGE,
    MONS_FANNAR,
    MONS_JORGRUN,
    MONS_SOJOBO,
    MONS_ASTERION,
    MONS_NATASHA,
    MONS_VASHNIA,
#endif
    // Sprint uniques:
    MONS_CHUCK,
    MONS_IRON_GIANT,
    MONS_NELLIE,
#if TAG_MAJOR_VERSION == 34
    MONS_IRON_ELEMENTAL,
#endif

    // Specials:
    MONS_PLAYER_ILLUSION,
    MONS_PLAYER_GHOST,
    MONS_BALL_LIGHTNING,
    MONS_ORB_OF_DESTRUCTION,    // a projectile, not a real mon
#if TAG_MAJOR_VERSION > 34
    MONS_FULMINANT_PRISM,
    MONS_BATTLESPHERE,
#endif
    MONS_PILLAR_OF_SALT,
#if TAG_MAJOR_VERSION > 34
    MONS_BLOCK_OF_ICE,
#endif
    MONS_HELL_LORD,             // genus
    MONS_MERGED_SLIME_CREATURE, // used only for recolouring
    MONS_SENSED,                // dummy monster for unspecified sensed mons
    MONS_SENSED_TRIVIAL,
    MONS_SENSED_EASY,
    MONS_SENSED_TOUGH,
    MONS_SENSED_NASTY,
    MONS_SENSED_FRIENDLY,
    MONS_PLAYER,                // a certain ugly creature
#if TAG_MAJOR_VERSION > 34
    MONS_PLAYER_SHADOW,         // Dithmenos
#endif
    MONS_TEST_SPAWNER,

    // Add new monsters here:
#if TAG_MAJOR_VERSION == 34
    MONS_SERPENT_OF_HELL_COCYTUS,
    MONS_SERPENT_OF_HELL_DIS,
    MONS_SERPENT_OF_HELL_TARTARUS,

    MONS_HELLBINDER,
    MONS_CLOUD_MAGE,
    MONS_ANIMATED_TREE,

    MONS_BEAR,                  // genus
    MONS_ELEMENTAL,             // genus

    MONS_FANNAR,
    MONS_APOCALYPSE_CRAB,
    MONS_STARSPAWN_TENTACLE,
    MONS_STARSPAWN_TENTACLE_SEGMENT,

    MONS_SPATIAL_MAELSTROM,
    MONS_CHAOS_BUTTERFLY,

    MONS_JORGRUN,
    MONS_LAMIA,

    MONS_FULMINANT_PRISM,
    MONS_BATTLESPHERE,

    MONS_GIANT_LIZARD,          // genus
    MONS_DRAKE,                 // genus
    MONS_PLAYER_SHADOW,         // Dithmenos

    MONS_DEEP_TROLL_EARTH_MAGE,
    MONS_DEEP_TROLL_SHAMAN,
    MONS_DIAMOND_OBELISK,

    MONS_VAULT_SENTINEL,
    MONS_VAULT_WARDEN,
    MONS_IRONBRAND_CONVOKER,
    MONS_IRONHEART_PRESERVER,

    MONS_ZOMBIE,
    MONS_SKELETON,
    MONS_SIMULACRUM,

    MONS_ANCIENT_CHAMPION,
    MONS_REVENANT,
    MONS_LOST_SOUL,
    MONS_JIANGSHI,

    MONS_DJINNI,
    MONS_LAVA_ORC,

    MONS_DRYAD,
    MONS_WIND_DRAKE,
    MONS_FAUN,
    MONS_SATYR,

    MONS_PAN,

    MONS_TENGU_WARRIOR,
    MONS_TENGU_CONJURER,
    MONS_TENGU_REAVER,

    MONS_SPRIGGAN_ENCHANTER,

    MONS_SOJOBO,

    MONS_CHIMERA,

    MONS_SNAPLASHER_VINE,
    MONS_SNAPLASHER_VINE_SEGMENT,
    MONS_THORN_HUNTER,
    MONS_BRIAR_PATCH,
    MONS_SPIRIT_WOLF,
    MONS_ANCIENT_BEAR,
    MONS_WATER_NYMPH,
    MONS_SHAMBLING_MANGROVE,
    MONS_THORN_LOTUS,
    MONS_SPECTRAL_WEAPON,
    MONS_ELEMENTAL_WELLSPRING,
    MONS_POLYMOTH,

    MONS_DEATHCAP,
    MONS_IGNIS,

    MONS_FORMICID,
    MONS_FORMICID_DRONE,
    MONS_FORMICID_VENOM_MAGE,

    MONS_RAIJU,

    MONS_DRAGON,                // genus
    MONS_SNAKE,                 // genus

    MONS_MONSTROUS_DEMONSPAWN,
    MONS_FIRST_DEMONSPAWN = MONS_MONSTROUS_DEMONSPAWN,
    MONS_FIRST_BASE_DEMONSPAWN = MONS_MONSTROUS_DEMONSPAWN,
    MONS_GELID_DEMONSPAWN,
    MONS_INFERNAL_DEMONSPAWN,
    MONS_PUTRID_DEMONSPAWN,
    MONS_TORTUROUS_DEMONSPAWN,
    MONS_LAST_BASE_DEMONSPAWN = MONS_TORTUROUS_DEMONSPAWN,
    MONS_BLOOD_SAINT,
    MONS_FIRST_NONBASE_DEMONSPAWN = MONS_BLOOD_SAINT,
    MONS_CHAOS_CHAMPION,
    MONS_WARMONGER,
    MONS_CORRUPTER,
    MONS_BLACK_SUN,
    MONS_LAST_NONBASE_DEMONSPAWN = MONS_BLACK_SUN,
    MONS_LAST_DEMONSPAWN = MONS_BLACK_SUN,

    MONS_WORLDBINDER,
    MONS_GRAND_AVATAR,
    MONS_VINE_STALKER,

    MONS_DROWNED_SOUL,

    MONS_SHOCK_SERPENT,
    MONS_MANA_VIPER,
    MONS_NAGA_RITUALIST,
    MONS_NAGA_SHARPSHOOTER,

    MONS_SALAMANDER_FIREBRAND,
    MONS_SALAMANDER_MYSTIC,

    MONS_ASTERION,
    MONS_NATASHA,
    MONS_VASHNIA,

    MONS_BLOCK_OF_ICE,
    MONS_GUARDIAN_GOLEM,
    MONS_SPELLFORGED_SERVITOR,
    MONS_OCTOPODE_CRUSHER,
    MONS_CRAB,
    MONS_GHOST_CRAB,
    MONS_TORPOR_SNAIL,
    MONS_MNOLEG_TENTACLE,
    MONS_MNOLEG_TENTACLE_SEGMENT,
    MONS_BENNU,
    MONS_USHABTI,
    MONS_DEATH_SCARAB,
    MONS_ANUBIS_GUARD,
    MONS_CAUSTIC_SHRIKE,
#endif

    NUM_MONSTERS,               // used for polymorph

    // MONS_NO_MONSTER can get put in savefiles, so it shouldn't change
    // when NUM_MONSTERS increases.
    MONS_NO_MONSTER = 1000,

    RANDOM_MONSTER = 2000, // used to distinguish between a random monster and using program bugs for error trapping {dlb}
    RANDOM_TOUGHER_MONSTER, // used for poly upgrading monsters.
    RANDOM_MOBILE_MONSTER, // used for monster generation (shadow creatures)
    RANDOM_COMPATIBLE_MONSTER, // used for player shadow creatures (prevents repulsing summons)
    RANDOM_BANDLESS_MONSTER,

    // A random draconian, either base coloured drac or specialised.
    RANDOM_DRACONIAN,
    // Any random base draconian colour.
    RANDOM_BASE_DRACONIAN,
    // Any random specialised draconian, such as a draconian knight.
    RANDOM_NONBASE_DRACONIAN,

    RANDOM_DEMON_LESSER,               //    0: Class V
    RANDOM_DEMON_COMMON,               //    1: Class II-IV
    RANDOM_DEMON_GREATER,              //    2: Class I
    RANDOM_DEMON,                      //    any of the above

    RANDOM_MODERATE_OOD, // +5 depth, AKA '9' glyph on maps
    RANDOM_SUPER_OOD, // *2 + 4 depth, AKA '8'

    RANDOM_DEMONSPAWN,
    RANDOM_BASE_DEMONSPAWN,
    RANDOM_NONBASE_DEMONSPAWN,

    WANDERING_MONSTER = 3500, // only used in monster placement routines - forced limit checks {dlb}
};

enum beh_type
{
    BEH_SLEEP,
    BEH_WANDER,
    BEH_SEEK,
    BEH_FLEE,
    BEH_CORNERED,                      //  wanting to flee, but blocked by an
                                       //  obstacle or monster
#if TAG_MAJOR_VERSION == 34
    BEH_PANIC,                         //  like flee but without running away
#endif
    BEH_LURK,                          //  stay still until discovered or
                                       //  enemy close by
    BEH_RETREAT,                       //  like flee but when cannot attack
    BEH_WITHDRAW,                      //  an ally given a command to withdraw
                                       //  (will not respond to attacks)
    NUM_BEHAVIOURS,                    //  max # of legal states
    BEH_CHARMED,                       //  hostile-but-charmed; creation only
    BEH_FRIENDLY,                      //  used during creation only
    BEH_GOOD_NEUTRAL,                  //  creation only
    BEH_STRICT_NEUTRAL,
    BEH_NEUTRAL,                       //  creation only
    BEH_HOSTILE,                       //  creation only
    BEH_GUARD,                         //  creation only - monster is guard
    BEH_COPY,                          //  creation only - copy from summoner
};

enum mon_attitude_type
{
    ATT_HOSTILE,                       // 0, default in most cases
    ATT_NEUTRAL,                       // neutral
    ATT_STRICT_NEUTRAL,                // neutral, won't attack player. Used by Jiyva.
    ATT_GOOD_NEUTRAL,                  // neutral, but won't attack friendlies
    ATT_FRIENDLY,                      // created friendly (or tamed?)
};

// Adding slots breaks saves. YHBW.
enum mon_inv_type           // menv[].inv[]
{
    MSLOT_WEAPON,           // Primary weapon (melee)
    MSLOT_ALT_WEAPON,       // Alternate weapon, ranged or second melee weapon
                            // for monsters that can use two weapons.
    MSLOT_MISSILE,
    MSLOT_ALT_MISSILE,
    MSLOT_ARMOUR,
    MSLOT_SHIELD,
    MSLOT_WAND,
    MSLOT_JEWELLERY,
    MSLOT_MISCELLANY,

    // [ds] Last monster gear slot that the player can observe by examining
    // the monster; i.e. the last slot that goes into monster_info.
    MSLOT_LAST_VISIBLE_SLOT = MSLOT_MISCELLANY,

    MSLOT_POTION,
    MSLOT_SCROLL,
    MSLOT_GOLD,
    NUM_MONSTER_SLOTS
};

enum mutation_type
{
    // body slot facets
    MUT_ANTENNAE,       // head
    MUT_BIG_WINGS,
    MUT_BEAK,           // head
    MUT_CLAWS,          // hands
    MUT_FANGS,
    MUT_HOOVES,         // feet
    MUT_HORNS,          // head
    MUT_STINGER,
    MUT_TALONS,         // feet
    MUT_TENTACLE_SPIKE, // Octopode only.

    // scales
    MUT_DISTORTION_FIELD,
    MUT_ICY_BLUE_SCALES,
    MUT_IRIDESCENT_SCALES,
    MUT_LARGE_BONE_PLATES,
    MUT_MOLTEN_SCALES,
    MUT_ROUGH_BLACK_SCALES,
    MUT_RUGGED_BROWN_SCALES,
    MUT_SLIMY_GREEN_SCALES,
    MUT_THIN_METALLIC_SCALES,
    MUT_THIN_SKELETAL_STRUCTURE,
    MUT_YELLOW_SCALES,
    MUT_CAMOUFLAGE,

    MUT_ACUTE_VISION,
    MUT_AGILE,
    MUT_BERSERK,
    MUT_BLINK,
    MUT_BLURRY_VISION,
    MUT_BREATHE_FLAMES,
    MUT_BREATHE_POISON,
    MUT_CARNIVOROUS,
    MUT_CLARITY,
    MUT_CLEVER,
    MUT_CLUMSY,
#if TAG_MAJOR_VERSION > 34
    MUT_COLD_BLOODED,
#endif
    MUT_COLD_RESISTANCE,
#if TAG_MAJOR_VERSION > 34
    MUT_COLD_VULNERABILITY,
#endif
#if TAG_MAJOR_VERSION == 34
    MUT_CONSERVE_POTIONS,
    MUT_CONSERVE_SCROLLS,
#endif
    MUT_DEFORMED,
    MUT_DEMONIC_GUARDIAN,
    MUT_DETERIORATION,
    MUT_DOPEY,
    MUT_HEAT_RESISTANCE,
#if TAG_MAJOR_VERSION > 34
    MUT_HEAT_VULNERABILITY,
    MUT_FLAME_CLOUD_IMMUNITY,
#endif
    MUT_HERBIVOROUS,
    MUT_HURL_HELLFIRE,

    MUT_FAST,
    MUT_FAST_METABOLISM,
#if TAG_MAJOR_VERSION == 34
    MUT_FLEXIBLE_WEAK,
#endif
    MUT_FRAIL,
    MUT_FOUL_STENCH,
    MUT_GOURMAND,
    MUT_HIGH_MAGIC,
#if TAG_MAJOR_VERSION > 34
    MUT_FREEZING_CLOUD_IMMUNITY,
#endif
    MUT_ICEMAIL,
    MUT_IGNITE_BLOOD,
    MUT_LOW_MAGIC,
    MUT_MAGIC_RESISTANCE,
    MUT_MUTATION_RESISTANCE,
    MUT_NEGATIVE_ENERGY_RESISTANCE,
    MUT_NIGHTSTALKER,
    MUT_PASSIVE_FREEZE,
    MUT_PASSIVE_MAPPING,
    MUT_POISON_RESISTANCE,
    MUT_POWERED_BY_DEATH,
    MUT_POWERED_BY_PAIN,
    MUT_REGENERATION,
    MUT_ROBUST,
#if TAG_MAJOR_VERSION == 34
    MUT_SAPROVOROUS,
#endif
    MUT_SCREAM,
    MUT_SHAGGY_FUR,
    MUT_SHOCK_RESISTANCE,
#if TAG_MAJOR_VERSION > 34
    MUT_SHOCK_VULNERABILITY,
#endif
    MUT_SLOW,
    MUT_SLOW_HEALING,
    MUT_SLOW_METABOLISM,
    MUT_SPINY,
    MUT_SPIT_POISON,
    MUT_STOCHASTIC_TORMENT_RESISTANCE,
    MUT_STRONG,
#if TAG_MAJOR_VERSION == 34
    MUT_STRONG_STIFF,
#endif
    MUT_TELEPORT,
    MUT_TELEPORT_CONTROL,
    MUT_TORMENT_RESISTANCE,
    MUT_TOUGH_SKIN,
    MUT_WEAK,
    MUT_WILD_MAGIC,
    MUT_UNBREATHING,
    MUT_ACIDIC_BITE,
    MUT_EYEBALLS,
#if TAG_MAJOR_VERSION == 34
    MUT_FOOD_JELLY,
#endif
    MUT_GELATINOUS_BODY,
    MUT_PSEUDOPODS,
    MUT_TRANSLUCENT_SKIN,
    MUT_EVOLUTION,
    MUT_AUGMENTATION,
    MUT_TENDRILS,
    MUT_JELLY_GROWTH,
    MUT_JELLY_MISSILE,
    MUT_MANA_SHIELD,
    MUT_MANA_REGENERATION,
    MUT_MANA_LINK,
    MUT_PETRIFICATION_RESISTANCE,
    MUT_TRAMPLE_RESISTANCE,
#if TAG_MAJOR_VERSION == 34
    MUT_CLING,
    MUT_FUMES,
    MUT_JUMP,
    MUT_EXOSKELETON,
#endif
    MUT_ANTIMAGIC_BITE,
    MUT_NO_DEVICE_HEAL,
#if TAG_MAJOR_VERSION == 34
    MUT_COLD_VULNERABILITY,
    MUT_HEAT_VULNERABILITY,
#endif
    MUT_BLACK_MARK,
#if TAG_MAJOR_VERSION == 34
    MUT_SHOCK_VULNERABILITY,
    MUT_COLD_BLOODED,
#endif
    MUT_ROT_IMMUNITY,
#if TAG_MAJOR_VERSION == 34
    MUT_FREEZING_CLOUD_IMMUNITY,
    MUT_FLAME_CLOUD_IMMUNITY,
#else
    MUT_SUSTAIN_ABILITIES,
#endif
    MUT_FORLORN,
    MUT_PLACID_MAGIC,
    MUT_NO_DRINK,
    MUT_NO_READ,
    MUT_MISSING_HAND,
    MUT_NO_STEALTH,
    MUT_NO_ARTIFICE,
    MUT_NO_LOVE,
    MUT_COWARDICE,
    MUT_NO_DODGING,
    MUT_NO_ARMOUR,
    MUT_NO_AIR_MAGIC,
    MUT_NO_CHARM_MAGIC,
    MUT_NO_CONJURATION_MAGIC,
    MUT_NO_EARTH_MAGIC,
    MUT_NO_FIRE_MAGIC,
    MUT_NO_HEXES_MAGIC,
    MUT_NO_ICE_MAGIC,
    MUT_NO_NECROMANCY_MAGIC,
    MUT_NO_POISON_MAGIC,
    MUT_NO_SUMMONING_MAGIC,
    MUT_NO_TRANSLOCATION_MAGIC,
    MUT_NO_TRANSMUTATION_MAGIC,
    MUT_PHYSICAL_VULNERABILITY,
    MUT_SLOW_REFLEXES,
    MUT_MAGICAL_VULNERABILITY,
    MUT_ANTI_WIZARDRY,

#if TAG_MAJOR_VERSION == 34
    MUT_SUSTAIN_ABILITIES,
#endif
    MUT_MP_WANDS,
    NUM_MUTATIONS,

    RANDOM_MUTATION,
    RANDOM_XOM_MUTATION,
    RANDOM_GOOD_MUTATION,
    RANDOM_BAD_MUTATION,
    RANDOM_SLIME_MUTATION,
    RANDOM_NON_SLIME_MUTATION,
    RANDOM_CORRUPT_MUTATION,
    RANDOM_QAZLAL_MUTATION,
    MUT_NON_MUTATION,
};

enum object_class_type                 // mitm[].base_type
{
    OBJ_WEAPONS,
    OBJ_MISSILES,
    OBJ_ARMOUR,
    OBJ_WANDS,
    OBJ_FOOD,
    OBJ_SCROLLS,
    OBJ_JEWELLERY,
    OBJ_POTIONS,
    OBJ_BOOKS,
    OBJ_STAVES,
    OBJ_ORBS,
    OBJ_MISCELLANY,
    OBJ_CORPSES,
    OBJ_GOLD,
    OBJ_RODS,
    NUM_OBJECT_CLASSES,
    OBJ_UNASSIGNED = 100,
    OBJ_RANDOM,      // used for blanket random sub_type .. see dungeon::items()
    OBJ_DETECTED,    // unknown item; item_info only
};

enum operation_types
{
    OPER_WIELD    = 'w',
    OPER_QUAFF    = 'q',
    OPER_DROP     = 'd',
    OPER_EAT      = 'e',
    OPER_TAKEOFF  = 'T',
    OPER_WEAR     = 'W',
    OPER_PUTON    = 'P',
    OPER_REMOVE   = 'R',
    OPER_READ     = 'r',
    OPER_MEMORISE = 'M',
    OPER_ZAP      = 'Z',
    OPER_FIRE     = 'f',
    OPER_PRAY     = 'p',
    OPER_EVOKE    = 'v',
    OPER_DESTROY  = 'D',
    OPER_QUIVER   = 'Q',
    OPER_ATTACK   = 'a',
    OPER_ANY      = 0,
};

enum orb_type
{
    ORB_ZOT,
};

enum recite_type
{
    RECITE_HERETIC,
    RECITE_CHAOTIC,
    RECITE_IMPURE,
    RECITE_UNHOLY,
    NUM_RECITE_TYPES
};

enum size_part_type
{
    PSIZE_BODY,         // entire body size -- used for EV/size of target
    PSIZE_TORSO,        // torso only (hybrids -- size of parts that use equip)
};

enum potion_type
{
    POT_CURING,
    POT_HEAL_WOUNDS,
    POT_HASTE,
    POT_MIGHT,
    POT_BRILLIANCE,
    POT_AGILITY,
#if TAG_MAJOR_VERSION == 34
    POT_GAIN_STRENGTH,
    POT_GAIN_DEXTERITY,
    POT_GAIN_INTELLIGENCE,
#endif
    POT_FLIGHT,
    POT_POISON,
    POT_SLOWING,
    POT_CANCELLATION,
    POT_CONFUSION,
    POT_INVISIBILITY,
    POT_PORRIDGE,
    POT_DEGENERATION,
    POT_DECAY,
#if TAG_MAJOR_VERSION == 34
    POT_WATER,
#endif
    POT_EXPERIENCE,
    POT_MAGIC,
    POT_RESTORE_ABILITIES,
#if TAG_MAJOR_VERSION == 34
    POT_STRONG_POISON,
#endif
    POT_BERSERK_RAGE,
    POT_CURE_MUTATION,
    POT_MUTATION,
    POT_RESISTANCE,
    POT_BLOOD,
#if TAG_MAJOR_VERSION == 34
    POT_BLOOD_COAGULATED,
#endif
    POT_LIGNIFY,
    POT_BENEFICIAL_MUTATION,
    NUM_POTIONS
};

enum pronoun_type
{
    PRONOUN_SUBJECTIVE,
    PRONOUN_POSSESSIVE,
    PRONOUN_REFLEXIVE,
    PRONOUN_OBJECTIVE,
    NUM_PRONOUN_CASES
};

enum gender_type
{
    GENDER_NEUTER,
    GENDER_MALE,
    GENDER_FEMALE,
    GENDER_YOU, // A person, not a gender, but close enough.
    NUM_GENDERS
};

// Be sure to update _prop_name[] in wiz-item.cc to match.  Also
// _randart_propnames(), but order doesn't matter there.
enum artefact_prop_type
{
    ARTP_BRAND,
    ARTP_AC,
    ARTP_EVASION,
    ARTP_STRENGTH,
    ARTP_INTELLIGENCE,
    ARTP_DEXTERITY,
    ARTP_FIRE,
    ARTP_COLD,
    ARTP_ELECTRICITY,
    ARTP_POISON,
    ARTP_NEGATIVE_ENERGY,
    ARTP_MAGIC,
    ARTP_EYESIGHT,
    ARTP_INVISIBLE,
    ARTP_FLY,
#if TAG_MAJOR_VERSION > 34
    ARTP_FOG,
#endif
    ARTP_BLINK,
    ARTP_BERSERK,
    ARTP_NOISES,
    ARTP_PREVENT_SPELLCASTING,
    ARTP_CAUSE_TELEPORTATION,
    ARTP_PREVENT_TELEPORTATION,
    ARTP_ANGRY,
#if TAG_MAJOR_VERSION == 34
    ARTP_METABOLISM,
#endif
    ARTP_MUTAGENIC,
#if TAG_MAJOR_VERSION == 34
    ARTP_ACCURACY,
#endif
    ARTP_SLAYING,
    ARTP_CURSED,
    ARTP_STEALTH,
    ARTP_MAGICAL_POWER,
    ARTP_BASE_DELAY,
    ARTP_HP,
    ARTP_CLARITY,
    ARTP_BASE_ACC,
    ARTP_BASE_DAM,
    ARTP_RMSL,
#if TAG_MAJOR_VERSION == 34
    ARTP_FOG,
#endif
    ARTP_REGENERATION,
    ARTP_SUSTAB,
    ARTP_NO_UPGRADE,
    ARTP_RCORR,
    ARTP_RMUT,
    ARTP_NUM_PROPERTIES
};

enum score_format_type
{
    SCORE_TERSE,                // one line
    SCORE_REGULAR,              // two lines (name, cause, blank)
    SCORE_VERBOSE,              // everything (dates, times, god, etc.)
};

enum sense_type
{
    SENSE_SMELL_BLOOD,
    SENSE_WEB_VIBRATION,
};

enum shop_type
{
    SHOP_WEAPON,
    SHOP_ARMOUR,
    SHOP_WEAPON_ANTIQUE,
    SHOP_ARMOUR_ANTIQUE,
    SHOP_GENERAL_ANTIQUE,
    SHOP_JEWELLERY,
    SHOP_EVOKABLES, // wands, rods, and misc items
    SHOP_BOOK,
    SHOP_FOOD,
    SHOP_DISTILLERY,
    SHOP_SCROLL,
    SHOP_GENERAL,
    NUM_SHOPS, // must remain last 'regular' member {dlb}
    SHOP_UNASSIGNED = 100,
    SHOP_RANDOM,
};

// These are often addressed relative to each other (esp. delta SIZE_MEDIUM).
enum size_type
{
    SIZE_TINY,              // rats/bats
    SIZE_LITTLE,            // spriggans
    SIZE_SMALL,             // halflings/kobolds
    SIZE_MEDIUM,            // humans/elves/dwarves
    SIZE_LARGE,             // trolls/ogres/centaurs/nagas
    SIZE_BIG,               // large quadrupeds
    SIZE_GIANT,             // giants
    NUM_SIZE_LEVELS,
    SIZE_CHARACTER,         // transformations that don't change size
};

// [dshaligram] If you add a new skill, update skills.cc, specifically
// the skills[] array and skill_display_order[]. New skills must go at the
// end of the list or in the unused skill numbers. NEVER rearrange this enum or
// move existing skills to new numbers; save file compatibility depends on this
// order.
enum skill_type
{
    SK_FIGHTING,
    SK_FIRST_SKILL = SK_FIGHTING,
    SK_SHORT_BLADES,
    SK_LONG_BLADES,
    SK_AXES,
    SK_MACES_FLAILS,
    SK_POLEARMS,
    SK_STAVES,
    SK_SLINGS,
    SK_BOWS,
    SK_CROSSBOWS,
    SK_THROWING,
    SK_ARMOUR,
    SK_DODGING,
    SK_STEALTH,
#if TAG_MAJOR_VERSION == 34
    SK_STABBING,
#endif
    SK_SHIELDS,
#if TAG_MAJOR_VERSION == 34
    SK_TRAPS,
#endif
    SK_UNARMED_COMBAT,
    SK_LAST_MUNDANE = SK_UNARMED_COMBAT,
    SK_SPELLCASTING,
    SK_CONJURATIONS,
    SK_FIRST_MAGIC_SCHOOL = SK_CONJURATIONS, // not SK_FIRST_MAGIC as no Spc
    SK_HEXES,
    SK_CHARMS,
    SK_SUMMONINGS,
    SK_NECROMANCY,
    SK_TRANSLOCATIONS,
    SK_TRANSMUTATIONS,
    SK_FIRE_MAGIC,
    SK_ICE_MAGIC,
    SK_AIR_MAGIC,
    SK_EARTH_MAGIC,
    SK_POISON_MAGIC,
    SK_LAST_MAGIC = SK_POISON_MAGIC,
    SK_INVOCATIONS,
    SK_EVOCATIONS,
    SK_LAST_SKILL = SK_EVOCATIONS,
    NUM_SKILLS,                        // must remain last regular member

    SK_BLANK_LINE,                     // used for skill output
    SK_COLUMN_BREAK,                   // used for skill output
    SK_TITLE,                          // used for skill output
    SK_NONE,
};

enum skill_menu_state
{
    SKM_NONE,
    SKM_DO_FOCUS,
    SKM_DO_PRACTISE,
    SKM_LEVEL_ENHANCED,
    SKM_LEVEL_NORMAL,
    SKM_MODE_AUTO,
    SKM_MODE_MANUAL,
    SKM_SHOW_DEFAULT,
    SKM_SHOW_KNOWN,
    SKM_SHOW_ALL,
    SKM_VIEW_NEW_LEVEL,
    SKM_VIEW_POINTS,
    SKM_VIEW_PROGRESS,
    SKM_VIEW_TRAINING,
    SKM_VIEW_TRANSFER,
};

enum skill_focus_mode
{
    SKM_FOCUS_OFF,
    SKM_FOCUS_ON,
    SKM_FOCUS_TOGGLE,
};

enum species_type
{
    SP_HUMAN,
    SP_HIGH_ELF,
    SP_DEEP_ELF,
#if TAG_MAJOR_VERSION == 34
    SP_SLUDGE_ELF,
#endif
    SP_HALFLING,
    SP_HILL_ORC,
    SP_KOBOLD,
    SP_MUMMY,
    SP_NAGA,
    SP_OGRE,
    SP_TROLL,
    SP_RED_DRACONIAN,
    SP_WHITE_DRACONIAN,
    SP_GREEN_DRACONIAN,
    SP_YELLOW_DRACONIAN,
    SP_GREY_DRACONIAN,
    SP_BLACK_DRACONIAN,
    SP_PURPLE_DRACONIAN,
    SP_MOTTLED_DRACONIAN,
    SP_PALE_DRACONIAN,
    SP_BASE_DRACONIAN,
    SP_CENTAUR,
    SP_DEMIGOD,
    SP_SPRIGGAN,
    SP_MINOTAUR,
    SP_DEMONSPAWN,
    SP_GHOUL,
    SP_TENGU,
    SP_MERFOLK,
    SP_VAMPIRE,
    SP_DEEP_DWARF,
    SP_FELID,
    SP_OCTOPODE,
#if TAG_MAJOR_VERSION == 34
    SP_DJINNI,
    SP_LAVA_ORC,
#endif
    SP_GARGOYLE,
    SP_FORMICID,
    SP_VINE_STALKER,
    LAST_VALID_SPECIES = SP_VINE_STALKER,
// The high scores viewer still needs enums for removed species.
    SP_ELF,                            // (placeholder)
    SP_HILL_DWARF,                     // (placeholder)
    SP_OGRE_MAGE,                      // (placeholder)
    SP_GREY_ELF,                       // (placeholder)
    SP_GNOME,                          // (placeholder)
    SP_MOUNTAIN_DWARF,                 // (placeholder)
#if TAG_MAJOR_VERSION > 34
    SP_SLUDGE_ELF,                     // (placeholder)
    SP_DJINNI,                         // (placeholder)
    SP_LAVA_ORC,                       // (placeholder)
#endif

    NUM_SPECIES,                       // always after the last species

    SP_UNKNOWN  = 100,
    SP_RANDOM   = 101,
    SP_VIABLE   = 102,
};

enum spell_type
{
    SPELL_NO_SPELL,
    SPELL_TELEPORT_SELF,
    SPELL_CAUSE_FEAR,
    SPELL_MAGIC_DART,
    SPELL_FIREBALL,
    SPELL_APPORTATION,
    SPELL_DELAYED_FIREBALL,
#if TAG_MAJOR_VERSION == 34
    SPELL_STRIKING,
#endif
    SPELL_CONJURE_FLAME,
    SPELL_DIG,
    SPELL_BOLT_OF_FIRE,
    SPELL_BOLT_OF_COLD,
    SPELL_LIGHTNING_BOLT,
    SPELL_BOLT_OF_MAGMA,
    SPELL_POLYMORPH,
    SPELL_SLOW,
    SPELL_HASTE,
    SPELL_PARALYSE,
    SPELL_CONFUSE,
    SPELL_INVISIBILITY,
    SPELL_THROW_FLAME,
    SPELL_THROW_FROST,
    SPELL_CONTROLLED_BLINK,
    SPELL_FREEZING_CLOUD,
    SPELL_MEPHITIC_CLOUD,
    SPELL_RING_OF_FLAMES,
    SPELL_VENOM_BOLT,
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_TELEPORT_OTHER,
    SPELL_MINOR_HEALING,
    SPELL_MAJOR_HEALING,
    SPELL_DEATHS_DOOR,
    SPELL_MASS_CONFUSION,
    SPELL_SMITING,
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_ABJURATION,
#if TAG_MAJOR_VERSION == 34
    SPELL_SUMMON_SCORPIONS,
#endif
    SPELL_BOLT_OF_DRAINING,
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_BOLT_OF_INACCURACY,
    SPELL_POISONOUS_CLOUD,
    SPELL_FIRE_STORM,
    SPELL_BLINK,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_SUMMON_SWARM,
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_ENSLAVEMENT,
    SPELL_ANIMATE_DEAD,
    SPELL_PAIN,
    SPELL_CONTROL_UNDEAD,
    SPELL_ANIMATE_SKELETON,
    SPELL_VAMPIRIC_DRAINING,
    SPELL_HAUNT,
    SPELL_BORGNJORS_REVIVIFICATION,
    SPELL_FREEZE,
#if TAG_MAJOR_VERSION == 34
    SPELL_SUMMON_ELEMENTAL,
#endif
    SPELL_OZOCUBUS_REFRIGERATION,
    SPELL_STICKY_FLAME,
    SPELL_SUMMON_ICE_BEAST,
    SPELL_OZOCUBUS_ARMOUR,
    SPELL_CALL_IMP,
    SPELL_REPEL_MISSILES,
    SPELL_BERSERKER_RAGE,
    SPELL_DISPEL_UNDEAD,
#if TAG_MAJOR_VERSION == 34
    SPELL_FULSOME_DISTILLATION,
#endif
    SPELL_POISON_ARROW,
    SPELL_TWISTED_RESURRECTION,
    SPELL_REGENERATION,
    SPELL_BANISHMENT,
#if TAG_MAJOR_VERSION == 34
    SPELL_CIGOTUVIS_DEGENERATION,
#endif
    SPELL_STING,
    SPELL_SUBLIMATION_OF_BLOOD,
    SPELL_TUKIMAS_DANCE,
    SPELL_HELLFIRE,
    SPELL_SUMMON_DEMON,
#if TAG_MAJOR_VERSION == 34
    SPELL_DEMONIC_HORDE,
#endif
    SPELL_SUMMON_GREATER_DEMON,
    SPELL_CORPSE_ROT,
#if TAG_MAJOR_VERSION == 34
    SPELL_FIRE_BRAND,
    SPELL_FREEZING_AURA,
    SPELL_LETHAL_INFUSION,
#endif
    SPELL_IRON_SHOT,
    SPELL_STONE_ARROW,
    SPELL_SHOCK,
    SPELL_SWIFTNESS,
    SPELL_FLY,
#if TAG_MAJOR_VERSION == 34
    SPELL_INSULATION,
#endif
    SPELL_CURE_POISON,
    SPELL_CONTROL_TELEPORT,
#if TAG_MAJOR_VERSION == 34
    SPELL_POISON_WEAPON,
#endif
    SPELL_DEBUGGING_RAY,
    SPELL_RECALL,
    SPELL_AGONY,
    SPELL_SPIDER_FORM,
    SPELL_DISINTEGRATE,
    SPELL_BLADE_HANDS,
    SPELL_STATUE_FORM,
    SPELL_ICE_FORM,
    SPELL_DRAGON_FORM,
#if TAG_MAJOR_VERSION > 34
    SPELL_HYDRA_FORM,
    SPELL_IRRADIATE,
#endif
    SPELL_NECROMUTATION,
    SPELL_DEATH_CHANNEL,
    SPELL_SYMBOL_OF_TORMENT,
    SPELL_DEFLECT_MISSILES,
    SPELL_THROW_ICICLE,
    SPELL_GLACIATE,
    SPELL_AIRSTRIKE,
    SPELL_SHADOW_CREATURES,
    SPELL_CONFUSING_TOUCH,
    SPELL_SURE_BLADE,
    SPELL_FLAME_TONGUE,
    SPELL_PASSWALL,
    SPELL_IGNITE_POISON,
    SPELL_STICKS_TO_SNAKES,
    SPELL_CALL_CANINE_FAMILIAR,
    SPELL_SUMMON_DRAGON,
    SPELL_HIBERNATION,
    SPELL_ENGLACIATION,
#if TAG_MAJOR_VERSION == 34
    SPELL_SEE_INVISIBLE,
#endif
    SPELL_PHASE_SHIFT,
    SPELL_SUMMON_BUTTERFLIES,
    SPELL_WARP_BRAND,
    SPELL_SILENCE,
    SPELL_SHATTER,
    SPELL_DISPERSAL,
    SPELL_DISCHARGE,
    SPELL_CORONA,
    SPELL_INTOXICATE,
#if TAG_MAJOR_VERSION == 34
    SPELL_EVAPORATE,
#endif
    SPELL_LRD,
    SPELL_SANDBLAST,
    SPELL_CONDENSATION_SHIELD,
    SPELL_STONESKIN,
    SPELL_SIMULACRUM,
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_CHAIN_LIGHTNING,
    SPELL_EXCRUCIATING_WOUNDS,
    SPELL_PORTAL_PROJECTILE,
    SPELL_MONSTROUS_MENAGERIE,
    SPELL_PETRIFY,
    SPELL_GOLUBRIAS_PASSAGE,

    // Mostly monster-only spells after this point:
    SPELL_HELLFIRE_BURST,
#if TAG_MAJOR_VERSION == 34
    SPELL_VAMPIRE_SUMMON,
#endif
    SPELL_BRAIN_FEED,
#if TAG_MAJOR_VERSION == 34
    SPELL_FAKE_RAKSHASA_SUMMON,
#endif
    SPELL_STEAM_BALL,
    SPELL_SUMMON_UFETUBUS,
    SPELL_SUMMON_HELL_BEAST,
    SPELL_ENERGY_BOLT,
    SPELL_SPIT_POISON,
    SPELL_SUMMON_UNDEAD,
    SPELL_CANTRIP,
    SPELL_QUICKSILVER_BOLT,
    SPELL_METAL_SPLINTERS,
    SPELL_MIASMA_BREATH,
    SPELL_SUMMON_DRAKES,
    SPELL_BLINK_OTHER,
    SPELL_SUMMON_MUSHROOMS,
    SPELL_SPIT_ACID,
    SPELL_STICKY_FLAME_SPLASH,
    SPELL_FIRE_BREATH,
    SPELL_COLD_BREATH,
#if TAG_MAJOR_VERSION == 34
    SPELL_DRACONIAN_BREATH,
#endif
    SPELL_WATER_ELEMENTALS,
    SPELL_PORKALATOR,
    SPELL_CREATE_TENTACLES,
    SPELL_TOMB_OF_DOROKLOHE,
    SPELL_SUMMON_EYEBALLS,
    SPELL_HASTE_OTHER,
    SPELL_FIRE_ELEMENTALS,
    SPELL_EARTH_ELEMENTALS,
    SPELL_AIR_ELEMENTALS,
    SPELL_SLEEP,
    SPELL_BLINK_OTHER_CLOSE,
    SPELL_BLINK_CLOSE,
    SPELL_BLINK_RANGE,
    SPELL_BLINK_AWAY,
#if TAG_MAJOR_VERSION == 34
    SPELL_MISLEAD,
#endif
    SPELL_FAKE_MARA_SUMMON,
#if TAG_MAJOR_VERSION == 34
    SPELL_SUMMON_RAKSHASA,
#endif
    SPELL_SUMMON_ILLUSION,
    SPELL_PRIMAL_WAVE,
    SPELL_CALL_TIDE,
    SPELL_IOOD,
    SPELL_INK_CLOUD,
    SPELL_MIGHT,
#if TAG_MAJOR_VERSION == 34
    SPELL_SUNRAY,
#endif
    SPELL_AWAKEN_FOREST,
    SPELL_DRUIDS_CALL,
    SPELL_IRON_ELEMENTALS,
    SPELL_SUMMON_SPECTRAL_ORCS,
#if TAG_MAJOR_VERSION == 34
    SPELL_RESURRECT,
    SPELL_HOLY_LIGHT,
    SPELL_HOLY_WORD,
    SPELL_SUMMON_HOLIES,
#endif
    SPELL_HEAL_OTHER,
#if TAG_MAJOR_VERSION == 34
    SPELL_SACRIFICE,
#endif
    SPELL_HOLY_FLAMES,
    SPELL_HOLY_BREATH,
    SPELL_TROGS_HAND,
    SPELL_BROTHERS_IN_ARMS,
    SPELL_INJURY_MIRROR,
    SPELL_DRAIN_LIFE,
#if TAG_MAJOR_VERSION == 34
    SPELL_MIASMA_CLOUD,
    SPELL_POISON_CLOUD,
    SPELL_FIRE_CLOUD,
    SPELL_STEAM_CLOUD,
#endif
    SPELL_MALIGN_GATEWAY,
    SPELL_NOXIOUS_CLOUD,
    SPELL_TORNADO,
    SPELL_STICKY_FLAME_RANGE,
    SPELL_LEDAS_LIQUEFACTION,
#if TAG_MAJOR_VERSION == 34
    SPELL_HOMUNCULUS,
#endif
    SPELL_SUMMON_HYDRA,
    SPELL_DARKNESS,
    SPELL_MESMERISE,
    SPELL_MELEE, // like SPELL_NO_SPELL, but doesn't cause a re-roll
    SPELL_FIRE_SUMMON,
    SPELL_SHROUD_OF_GOLUBRIA,
    SPELL_INNER_FLAME,
    SPELL_PETRIFYING_CLOUD,
    SPELL_AURA_OF_ABJURATION,
    SPELL_BEASTLY_APPENDAGE,
#if TAG_MAJOR_VERSION == 34
    SPELL_SILVER_BLAST,
#endif
    SPELL_ENSNARE,
    SPELL_THUNDERBOLT,
    SPELL_SUMMON_MINOR_DEMON,
    SPELL_DISJUNCTION,
    SPELL_CHAOS_BREATH,
    SPELL_FRENZY,
#if TAG_MAJOR_VERSION == 34
    SPELL_SUMMON_TWISTER,
#endif
    SPELL_BATTLESPHERE,
    SPELL_FULMINANT_PRISM,
    SPELL_DAZZLING_SPRAY,
    SPELL_FORCE_LANCE,
    SPELL_MALMUTATE,
    SPELL_MIGHT_OTHER,
    SPELL_SENTINEL_MARK,
    SPELL_WORD_OF_RECALL,
    SPELL_INJURY_BOND,
    SPELL_GHOSTLY_FLAMES,
    SPELL_GHOSTLY_FIREBALL,
    SPELL_CALL_LOST_SOUL,
    SPELL_DIMENSION_ANCHOR,
    SPELL_BLINK_ALLIES_ENCIRCLE,
    SPELL_AWAKEN_VINES,
    SPELL_CONTROL_WINDS,
    SPELL_THORN_VOLLEY,
    SPELL_WALL_OF_BRAMBLES,
    SPELL_WATERSTRIKE,
    SPELL_HASTE_PLANTS,
    SPELL_WIND_BLAST,
    SPELL_STRIP_RESISTANCE,
    SPELL_INFUSION,
    SPELL_SONG_OF_SLAYING,
    SPELL_SPECTRAL_WEAPON,
#if TAG_MAJOR_VERSION == 34
    SPELL_SONG_OF_SHIELDING,
#endif
    SPELL_SUMMON_VERMIN,
    SPELL_MALIGN_OFFERING,
    SPELL_SEARING_RAY,
    SPELL_DISCORD,
#if TAG_MAJOR_VERSION == 34
    SPELL_SHAFT_SELF,
#endif
    SPELL_BLINKBOLT,
    SPELL_INVISIBILITY_OTHER,
    SPELL_VIRULENCE,
    SPELL_IGNITE_POISON_SINGLE,
    SPELL_ORB_OF_ELECTRICITY,
    SPELL_EXPLOSIVE_BOLT,
    SPELL_FLASH_FREEZE,
    SPELL_LEGENDARY_DESTRUCTION,
    SPELL_EPHEMERAL_INFUSION,
    SPELL_FORCEFUL_INVITATION,
    SPELL_PLANEREND,
    SPELL_CHAIN_OF_CHAOS,
    SPELL_CHAOTIC_MIRROR,
    SPELL_BLACK_MARK,
    SPELL_GRAND_AVATAR,
    SPELL_SAP_MAGIC,
    SPELL_CORRUPT_BODY,
#if TAG_MAJOR_VERSION == 34
    SPELL_REARRANGE_PIECES,
#endif
    SPELL_MAJOR_DESTRUCTION,
    SPELL_BLINK_ALLIES_AWAY,
    SPELL_SHADOW_SHARD,
    SPELL_SHADOW_BOLT,
    SPELL_CRYSTAL_BOLT,
    SPELL_SUMMON_FOREST,
    SPELL_SUMMON_LIGHTNING_SPIRE,
    SPELL_SUMMON_GUARDIAN_GOLEM,
    SPELL_RANDOM_BOLT,
    SPELL_CLOUD_CONE,
    SPELL_WEAVE_SHADOWS,
    SPELL_DRAGON_CALL,
    SPELL_SPELLFORGED_SERVITOR,
    SPELL_FORCEFUL_DISMISSAL,
    SPELL_SUMMON_MANA_VIPER,
    SPELL_PHANTOM_MIRROR,
    SPELL_DRAIN_MAGIC,
    SPELL_CORROSIVE_BOLT,
    SPELL_SERPENT_OF_HELL_BREATH,
    SPELL_SUMMON_EMPEROR_SCORPIONS,
#if TAG_MAJOR_VERSION == 34
    SPELL_HYDRA_FORM,
    SPELL_IRRADIATE,
#endif
    SPELL_SPIT_LAVA,
    SPELL_ELECTRICAL_BOLT,
    SPELL_FLAMING_CLOUD,
    SPELL_THROW_BARBS,
    SPELL_BATTLECRY,
    SPELL_SIGNAL_HORN,
    SPELL_SEAL_DOORS,
    SPELL_FLAY,
    SPELL_BERSERK_OTHER,
    SPELL_TENTACLE_THROW,
    SPELL_CORRUPTING_PULSE,
    SPELL_SIREN_SONG,
    SPELL_AVATAR_SONG,
    SPELL_PARALYSIS_GAZE,
    SPELL_CONFUSION_GAZE,
    SPELL_DRAINING_GAZE,
    SPELL_DEATH_RATTLE,
    SPELL_SUMMON_SCARABS,
    SPELL_HUNTING_CRY,
    NUM_SPELLS
};

enum slot_select_mode
{
    SS_FORWARD      = 0,
    SS_BACKWARD     = 1,
};

enum stat_type
{
    STAT_STR,
    STAT_INT,
    STAT_DEX,
    NUM_STATS,
    STAT_ALL, // must remain after NUM_STATS
    STAT_RANDOM,
};

enum targeting_type
{
    DIR_NONE,
    DIR_TARGET,         // smite targeting
    DIR_DIR,            // needs a clear line to target
    DIR_TARGET_OBJECT,  // targets items
    DIR_MOVABLE_OBJECT, // skips corpses
    DIR_SHADOW_STEP,    // a shadow step target
    DIR_LEAP,           // power leap -- short-range cblink
};

enum torment_source_type
{
    TORMENT_GENERIC       = -1,
    TORMENT_CARDS         = -2,   // Symbol of torment
    TORMENT_SPWLD         = -3,   // Special wield torment
    TORMENT_SCROLL        = -4,
    TORMENT_SPELL         = -5,   // SPELL_SYMBOL_OF_TORMENT
    TORMENT_XOM           = -6,   // Xom effect
    TORMENT_KIKUBAAQUDGHA = -7,   // Kikubaaqudgha effect
};

enum trap_type
{
#if TAG_MAJOR_VERSION == 34
    TRAP_DART,
#endif
    TRAP_ARROW,
    TRAP_SPEAR,
#if TAG_MAJOR_VERSION > 34
    TRAP_TELEPORT,
#endif
    TRAP_TELEPORT_PERMANENT,
    TRAP_ALARM,
    TRAP_BLADE,
    TRAP_BOLT,
    TRAP_NET,
    TRAP_ZOT,
    TRAP_NEEDLE,
    TRAP_SHAFT,
    TRAP_GOLUBRIA,
    TRAP_PLATE,
    TRAP_WEB,
#if TAG_MAJOR_VERSION == 34
    TRAP_GAS,
    TRAP_TELEPORT,
#endif
    NUM_TRAPS,
    TRAP_MAX_REGULAR = TRAP_SHAFT,
    TRAP_UNASSIGNED = 100,
#if TAG_MAJOR_VERSION == 34
    TRAP_UNUSED1,                      // was TRAP_INDEPTH
    TRAP_UNUSED2,                      // was TRAP_NOTELEPORT
#endif
    TRAP_RANDOM,
};

enum undead_state_type                // you.is_undead
{
    US_ALIVE = 0,
    US_HUNGRY_DEAD,     // Ghouls
    US_UNDEAD,          // Mummies
    US_SEMI_UNDEAD,     // Vampires
};

enum unique_item_status_type
{
    UNIQ_NOT_EXISTS = 0,
    UNIQ_EXISTS = 1,
    UNIQ_LOST_IN_ABYSS = 2,
};

enum zap_type
{
    ZAP_THROW_FLAME,
    ZAP_THROW_FROST,
    ZAP_SLOW,
    ZAP_HASTE,
    ZAP_MAGIC_DART,
    ZAP_MAJOR_HEALING,
    ZAP_PARALYSE,
    ZAP_BOLT_OF_FIRE,
    ZAP_BOLT_OF_COLD,
    ZAP_CONFUSE,
    ZAP_INVISIBILITY,
    ZAP_DIG,
    ZAP_FIREBALL,
    ZAP_TELEPORT_OTHER,
    ZAP_LIGHTNING_BOLT,
    ZAP_POLYMORPH,
    ZAP_VENOM_BOLT,
    ZAP_BOLT_OF_DRAINING,
    ZAP_LEHUDIBS_CRYSTAL_SPEAR,
    ZAP_BOLT_OF_INACCURACY,
    ZAP_ISKENDERUNS_MYSTIC_BLAST,
    ZAP_ENSLAVEMENT,
    ZAP_PAIN,
    ZAP_STICKY_FLAME,
    ZAP_STICKY_FLAME_RANGE,
    ZAP_DISPEL_UNDEAD,
    ZAP_BANISHMENT,
    ZAP_STING,
    ZAP_HELLFIRE,
    ZAP_IRON_SHOT,
    ZAP_STONE_ARROW,
    ZAP_SHOCK,
    ZAP_ORB_OF_ELECTRICITY,
    ZAP_SPIT_POISON,
    ZAP_DEBUGGING_RAY,
    ZAP_BREATHE_FIRE,
    ZAP_BREATHE_FROST,
    ZAP_BREATHE_ACID,
    ZAP_BREATHE_POISON,
    ZAP_BREATHE_POWER,
    ZAP_AGONY,
    ZAP_DISINTEGRATE,
    ZAP_BREATHE_STEAM,
    ZAP_THROW_ICICLE,
    ZAP_CORONA,
    ZAP_HIBERNATION,
    ZAP_FLAME_TONGUE,
    ZAP_LARGE_SANDBLAST,
    ZAP_SANDBLAST,
    ZAP_SMALL_SANDBLAST,
    ZAP_BOLT_OF_MAGMA,
    ZAP_POISON_ARROW,
    ZAP_BREATHE_STICKY_FLAME,
    ZAP_PETRIFY,
    ZAP_ENSLAVE_SOUL,
    ZAP_PORKALATOR,
    ZAP_SLEEP,
    ZAP_PRIMAL_WAVE,
    ZAP_IOOD,
    ZAP_BREATHE_MEPHITIC,
    ZAP_INNER_FLAME,
    ZAP_DAZZLING_SPRAY,
    ZAP_FORCE_LANCE,
    ZAP_SEARING_RAY_I,
    ZAP_SEARING_RAY_II,
    ZAP_SEARING_RAY_III,
    ZAP_EXPLOSIVE_BOLT,
    ZAP_CRYSTAL_BOLT,
    ZAP_TUKIMAS_DANCE,
    ZAP_QUICKSILVER_BOLT,
    ZAP_CORROSIVE_BOLT,
    ZAP_RANDOM_BOLT_TRACER,

    NUM_ZAPS
};

enum montravel_target_type
{
    MTRAV_NONE = 0,
    MTRAV_FOE,         // Travelling to reach its foe.
    MTRAV_PATROL,      // Travelling to reach the patrol point.
    MTRAV_MERFOLK_AVATAR, // Merfolk avatars travelling towards deep water.
    MTRAV_UNREACHABLE, // Not travelling because target is unreachable.
    MTRAV_KNOWN_UNREACHABLE, // As above, and the player knows this.
};

enum maybe_bool
{
    MB_FALSE,
    MB_MAYBE,
    MB_TRUE,
};

enum reach_type
{
    REACH_NONE   = 2,
    REACH_KNIGHT = 5,
    REACH_TWO    = 8,
};

enum daction_type
{
    DACT_ALLY_HOLY,
    DACT_ALLY_UNHOLY_EVIL,
    DACT_ALLY_UNCLEAN_CHAOTIC,
    DACT_ALLY_SPELLCASTER,
    DACT_ALLY_YRED_SLAVE,
    DACT_ALLY_BEOGH, // both orcs and demons summoned by high priests
    DACT_ALLY_SLIME,
    DACT_ALLY_PLANT,

    NUM_DACTION_COUNTERS,

    // Leave space for new counters, as they need to be at the start.
    DACT_OLD_ENSLAVED_SOULS_POOF = 16,
#if TAG_MAJOR_VERSION == 34
    DACT_HOLY_NEW_ATTEMPT,
#endif
#if TAG_MAJOR_VERSION > 34
    DACT_SLIME_NEW_ATTEMPT,
#endif
    DACT_HOLY_PETS_GO_NEUTRAL,
    DACT_ALLY_TROG,

    DACT_SHUFFLE_DECKS,
    DACT_REAUTOMAP,
    DACT_REMOVE_JIYVA_ALTARS,
    DACT_PIKEL_SLAVES,
    DACT_ROT_CORPSES,
    DACT_TOMB_CTELE,
#if TAG_MAJOR_VERSION == 34
    DACT_SLIME_NEW_ATTEMPT,
#endif
    DACT_KIRKE_HOGS,
#if TAG_MAJOR_VERSION == 34
    DACT_END_SPIRIT_HOWL,
#endif
    DACT_GOLD_ON_TOP,
    DACT_BRIBE_TIMEOUT,
    DACT_REMOVE_GOZAG_SHOPS,
    DACT_SET_BRIBES,
    DACT_ALLY_MAKHLEB,
#if TAG_MAJOR_VERSION == 34
    DACT_ALLY_SACRIFICE_LOVE,
#endif
    NUM_DACTIONS,
    // If you want to add a new daction, you need to
    // add a corresponding entry to *daction_names[]
    // of dactions.cc to avoid breaking the debug build
};

enum disable_type
{
    DIS_SPAWNS,
    DIS_MON_ACT,
    DIS_MON_REGEN,
    DIS_PLAYER_REGEN,
    DIS_HUNGER,
    DIS_DEATH,
    DIS_DELAY,
    DIS_CONFIRMATIONS,
    DIS_AFFLICTIONS,
    DIS_MON_SIGHT,
    DIS_SAVE_CHECKPOINTS,
    NUM_DISABLEMENTS
};

enum seen_context_type
{
    SC_NONE,
    SC_JUST_SEEN,       // has already been announced this turn
    SC_NEWLY_SEEN,      // regular walking into view
    SC_ALREADY_SEEN,    // wasn't a threat before, is now
    SC_TELEPORT_IN,
    SC_SURFACES,                      // land-capable
    SC_SURFACES_BRIEFLY,              // land-capable, submerged back
    SC_FISH_SURFACES_SHOUT,           // water/lava-only, shouting
    SC_FISH_SURFACES,                 // water/lava-only
    SC_NONSWIMMER_SURFACES_FROM_DEEP, // impossible?!?
    SC_UNCHARM,
    SC_DOOR,            // they opened a door
    SC_GATE,            // ... or a big door
    SC_LEAP_IN,         // leaps into view
    SC_UPSTAIRS,        // comes up the stairs
    SC_DOWNSTAIRS,      // comes down the stairs
    SC_ARCH,            // through the gate
    SC_ABYSS,           // abyss creation
    SC_THROWN_IN,       // thrown into view from the monster throw ability
};

enum los_type
{
    LOS_NONE         = 0,
    LOS_ARENA        = LOS_NONE,
    LOS_DEFAULT      = (1 << 0),
    LOS_NO_TRANS     = (1 << 1),
    LOS_SOLID        = (1 << 2),
    LOS_SOLID_SEE    = (1 << 3),
};

enum ac_type
{
    AC_NONE,
    // These types block small amounts of damage, hardly affecting big hits.
    AC_NORMAL,
    AC_HALF,
    AC_TRIPLE,
    // This one stays fair over arbitrary splits.
    AC_PROPORTIONAL,
};

enum uncancellable_type
{
    UNC_ACQUIREMENT,           // arg is AQ_SCROLL
    UNC_DRAW_THREE,            // arg is inv slot of the deck
    UNC_STACK_FIVE,            // arg is inv slot of the deck
    UNC_MERCENARY,             // arg is mid of the monster
    UNC_POTION_PETITION,       // arg is ignored
    UNC_CALL_MERCHANT,         // arg is ignored
};

// game-wide random seeds
enum seed_type
{
    SEED_PASSIVE_MAP,          // determinist magic mapping
    NUM_SEEDS
};

// Tiles stuff.

enum screen_mode
{
    SCREENMODE_WINDOW = 0,
    SCREENMODE_FULL   = 1,
    SCREENMODE_AUTO   = 2,
};

enum cursor_type
{
    CURSOR_MOUSE,
    CURSOR_TUTORIAL,
    CURSOR_MAP,
    CURSOR_MAX,
};

// Ordering of tags is important: higher values cover up lower ones.
enum text_tag_type
{
    TAG_NAMED_MONSTER = 0,
    TAG_TUTORIAL      = 1,
    TAG_CELL_DESC     = 2,
    TAG_MAX,
};

enum tag_pref
{
    TAGPREF_NONE,     // never display text tags
    TAGPREF_TUTORIAL, // display text tags on "new" monsters
    TAGPREF_NAMED,    // display text tags on named monsters (incl. friendlies)
    TAGPREF_ENEMY,    // display text tags on enemy named monsters
    TAGPREF_MAX,
};
enum tile_flags ENUM_INT64
{
    //// Foreground flags

    // 3 mutually exclusive flags for attitude.
    TILE_FLAG_ATT_MASK   = 0x00030000ULL,
    TILE_FLAG_PET        = 0x00010000ULL,
    TILE_FLAG_GD_NEUTRAL = 0x00020000ULL,
    TILE_FLAG_NEUTRAL    = 0x00030000ULL,

    TILE_FLAG_S_UNDER    = 0x00040000ULL,
    TILE_FLAG_FLYING     = 0x00080000ULL,

    // 3 mutually exclusive flags for behaviour.
    TILE_FLAG_BEH_MASK   = 0x00300000ULL,
    TILE_FLAG_STAB       = 0x00100000ULL,
    TILE_FLAG_MAY_STAB   = 0x00200000ULL,
    TILE_FLAG_FLEEING    = 0x00300000ULL,

    TILE_FLAG_NET        = 0x00400000ULL,
    TILE_FLAG_POISON     = 0x00800000ULL,
    TILE_FLAG_WEB        = 0x01000000ULL,
    TILE_FLAG_GLOWING    = 0x02000000ULL,
    TILE_FLAG_STICKY_FLAME = 0x04000000ULL,
    TILE_FLAG_BERSERK    = 0x08000000ULL,
    TILE_FLAG_INNER_FLAME= 0x10000000ULL,
    TILE_FLAG_CONSTRICTED= 0x20000000ULL,
    TILE_FLAG_SLOWED     = 0x8000000000ULL,
    TILE_FLAG_PAIN_MIRROR = 0x10000000000ULL,
    TILE_FLAG_HASTED     = 0x20000000000ULL,
    TILE_FLAG_MIGHT      = 0x40000000000ULL,
    TILE_FLAG_PETRIFYING = 0x80000000000ULL,
    TILE_FLAG_PETRIFIED  = 0x100000000000ULL,
    TILE_FLAG_BLIND      = 0x200000000000ULL,
    TILE_FLAG_ANIM_WEP   = 0x400000000000ULL,
    TILE_FLAG_SUMMONED   = 0x800000000000ULL,
    TILE_FLAG_PERM_SUMMON= 0x1000000000000ULL,
    TILE_FLAG_DEATHS_DOOR = 0x2000000000000ULL,
    TILE_FLAG_RECALL =     0x4000000000000ULL,
    TILE_FLAG_DRAIN =      0x8000000000000ULL,

    // MDAM has 5 possibilities, so uses 3 bits.
    TILE_FLAG_MDAM_MASK  = 0x1C0000000ULL,
    TILE_FLAG_MDAM_LIGHT = 0x040000000ULL,
    TILE_FLAG_MDAM_MOD   = 0x080000000ULL,
    TILE_FLAG_MDAM_HEAVY = 0x0C0000000ULL,
    TILE_FLAG_MDAM_SEV   = 0x100000000ULL,
    TILE_FLAG_MDAM_ADEAD = 0x1C0000000ULL,

    // Demon difficulty has 5 possibilities, so uses 3 bits.
    TILE_FLAG_DEMON      = 0xE00000000ULL,
    TILE_FLAG_DEMON_5    = 0x200000000ULL,
    TILE_FLAG_DEMON_4    = 0x400000000ULL,
    TILE_FLAG_DEMON_3    = 0x600000000ULL,
    TILE_FLAG_DEMON_2    = 0x800000000ULL,
    TILE_FLAG_DEMON_1    = 0xE00000000ULL,

    // 3 mutually exclusive flags for mimics.
    TILE_FLAG_MIMIC_INEPT = 0x2000000000ULL,
    TILE_FLAG_MIMIC       = 0x4000000000ULL,
    TILE_FLAG_MIMIC_RAVEN = 0x6000000000ULL,
    TILE_FLAG_MIMIC_MASK  = 0x6000000000ULL,

    //// Background flags

    TILE_FLAG_RAY        = 0x00010000ULL,
    TILE_FLAG_MM_UNSEEN  = 0x00020000ULL,
    TILE_FLAG_UNSEEN     = 0x00040000ULL,

    // 3 mutually exclusive flags for cursors.
    TILE_FLAG_CURSOR1    = 0x00180000ULL,
    TILE_FLAG_CURSOR2    = 0x00080000ULL,
    TILE_FLAG_CURSOR3    = 0x00100000ULL,
    TILE_FLAG_CURSOR     = 0x00180000ULL,

    TILE_FLAG_TUT_CURSOR = 0x00200000ULL,
    TILE_FLAG_TRAV_EXCL  = 0x00400000ULL,
    TILE_FLAG_EXCL_CTR   = 0x00800000ULL,
    TILE_FLAG_RAY_OOR    = 0x01000000ULL,
    TILE_FLAG_OOR        = 0x02000000ULL,
    TILE_FLAG_WATER      = 0x04000000ULL,
    TILE_FLAG_NEW_STAIR  = 0x08000000ULL,

    // Tentacle overlay flags: direction and type.
    TILE_FLAG_TENTACLE_NW  = 0x020000000ULL,
    TILE_FLAG_TENTACLE_NE  = 0x040000000ULL,
    TILE_FLAG_TENTACLE_SE  = 0x080000000ULL,
    TILE_FLAG_TENTACLE_SW  = 0x100000000ULL,
    TILE_FLAG_TENTACLE_KRAKEN = 0x0200000000ULL,
    TILE_FLAG_TENTACLE_ELDRITCH = 0x0400000000ULL,
    TILE_FLAG_TENTACLE_STARSPAWN = 0x0800000000ULL,
    TILE_FLAG_TENTACLE_VINE = 0x1000000000ULL,

#if TAG_MAJOR_VERSION == 34
    // Starspawn tentacle overlays. Obviated by the above.
    TILE_FLAG_STARSPAWN_NW = 0x02000000000ULL,
    TILE_FLAG_STARSPAWN_NE = 0x04000000000ULL,
    TILE_FLAG_STARSPAWN_SE = 0x08000000000ULL,
    TILE_FLAG_STARSPAWN_SW = 0x10000000000ULL,
#endif

    //// General

    // Should go up with RAY/RAY_OOR, but they need to be exclusive for those
    // flags and there's no room.
    TILE_FLAG_LANDING     = 0x20000000000ULL,

    // Mask for the tile index itself.
    TILE_FLAG_MASK       = 0x0000FFFFULL,
};

enum tile_inventory_flags
{
    TILEI_FLAG_SELECT  = 0x0100,
    TILEI_FLAG_TRIED   = 0x0200,
    TILEI_FLAG_EQUIP   = 0x0400,
    TILEI_FLAG_FLOOR   = 0x0800,
    TILEI_FLAG_CURSE   = 0x1000,
    TILEI_FLAG_CURSOR  = 0x2000,
    TILEI_FLAG_MELDED  = 0x4000,
    TILEI_FLAG_INVALID = 0x8000,
};

enum tile_player_flags
{
    TILEP_SHOW_EQUIP    = 0x1000,
};

enum tile_player_flag_cut
{
    TILEP_FLAG_HIDE,
    TILEP_FLAG_NORMAL,
    TILEP_FLAG_CUT_CENTAUR,
    TILEP_FLAG_CUT_NAGA,
};

// normal tile size in px
enum
{
    TILE_X = 32,
    TILE_Y = 32,
};

// Don't change this without also modifying the data save/load routines.
enum
{
    NUM_MAX_DOLLS = 10,
};

#ifdef WIZARD

enum wizard_option_type
{
    WIZ_NEVER,                         // protect player from accidental wiz
    WIZ_NO,                            // don't start character in wiz mode
    WIZ_YES,                           // start character in wiz mode
};

#endif

enum timed_effect_type
{
    TIMER_CORPSES,
    TIMER_HELL_EFFECTS,
    TIMER_STAT_RECOVERY,
    TIMER_CONTAM,
    TIMER_DETERIORATION,
    TIMER_GOD_EFFECTS,
#if TAG_MAJOR_VERSION == 34
    TIMER_SCREAM,
#endif
    TIMER_FOOD_ROT,
    TIMER_PRACTICE,
    TIMER_LABYRINTH,
    TIMER_ABYSS_SPEED,
    TIMER_JIYVA,
    TIMER_EVOLUTION,
#if TAG_MAJOR_VERSION == 34
    TIMER_BRIBE_TIMEOUT,
#endif
    NUM_TIMERS,
};

enum deck_rarity_type
{
    DECK_RARITY_RANDOM,
    DECK_RARITY_COMMON,
    DECK_RARITY_RARE,
    DECK_RARITY_LEGENDARY,
};

#endif // ENUM_H
