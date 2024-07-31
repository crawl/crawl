#pragma once

#include <vector>

#include "tag-version.h"

using std::vector;

/* Don't change the order of any enums in this file unless you are breaking
 * save compatibility. See ../docs/develop/save_compatibility.txt for
 * more details, including how to schedule both the current and future
 * enum orders.
 *
 * If you do break compatibility and change the order, be sure to change
 * rltiles/dc-item.txt to match.
 */

enum armour_type
{
    ARM_ROBE, // order of mundane armour matters to _upgrade_body_armour
    ARM_FIRST_MUNDANE_BODY = ARM_ROBE,
    ARM_LEATHER_ARMOUR,
    ARM_RING_MAIL,
    ARM_SCALE_MAIL,
    ARM_CHAIN_MAIL,
    ARM_PLATE_ARMOUR,
    ARM_LAST_MUNDANE_BODY = ARM_PLATE_ARMOUR,
#if TAG_MAJOR_VERSION > 34
    ARM_CRYSTAL_PLATE_ARMOUR,
#endif

    ARM_CLOAK,
#if TAG_MAJOR_VERSION > 34
    ARM_SCARF,
#endif

#if TAG_MAJOR_VERSION == 34
    ARM_CAP,
#endif
    ARM_HAT,
    ARM_HELMET,

    ARM_GLOVES,

    ARM_BOOTS,

    ARM_BUCKLER, // order of shields matters
    ARM_FIRST_SHIELD = ARM_BUCKLER,
    ARM_KITE_SHIELD,
    ARM_TOWER_SHIELD,
    ARM_LAST_SHIELD = ARM_TOWER_SHIELD,
#if TAG_MAJOR_VERSION > 34
    ARM_ORB,
#endif

#if TAG_MAJOR_VERSION == 34
    ARM_CRYSTAL_PLATE_ARMOUR,
#endif

    ARM_ANIMAL_SKIN,

#if TAG_MAJOR_VERSION == 34
    ARM_TROLL_HIDE,
#endif
    ARM_TROLL_LEATHER_ARMOUR,

#if TAG_MAJOR_VERSION == 34
    ARM_FIRE_DRAGON_HIDE,
#endif
    ARM_FIRE_DRAGON_ARMOUR,
#if TAG_MAJOR_VERSION == 34
    ARM_ICE_DRAGON_HIDE,
#endif
    ARM_ICE_DRAGON_ARMOUR,
#if TAG_MAJOR_VERSION == 34
    ARM_STEAM_DRAGON_HIDE,
#endif
    ARM_STEAM_DRAGON_ARMOUR,
#if TAG_MAJOR_VERSION == 34
    ARM_ACID_DRAGON_HIDE,
#endif
    ARM_ACID_DRAGON_ARMOUR,
#if TAG_MAJOR_VERSION == 34
    ARM_STORM_DRAGON_HIDE,
#endif
    ARM_STORM_DRAGON_ARMOUR,
#if TAG_MAJOR_VERSION == 34
    ARM_GOLDEN_DRAGON_HIDE,
#endif
    ARM_GOLDEN_DRAGON_ARMOUR,
#if TAG_MAJOR_VERSION == 34
    ARM_SWAMP_DRAGON_HIDE,
#endif
    ARM_SWAMP_DRAGON_ARMOUR,
#if TAG_MAJOR_VERSION == 34
    ARM_PEARL_DRAGON_HIDE,
#endif
    ARM_PEARL_DRAGON_ARMOUR,
#if TAG_MAJOR_VERSION > 34
    ARM_SHADOW_DRAGON_ARMOUR,
    ARM_QUICKSILVER_DRAGON_ARMOUR,
#endif

#if TAG_MAJOR_VERSION == 34
    ARM_CENTAUR_BARDING,
#endif
    ARM_BARDING,

#if TAG_MAJOR_VERSION == 34
    ARM_SHADOW_DRAGON_HIDE,
    ARM_SHADOW_DRAGON_ARMOUR,
    ARM_QUICKSILVER_DRAGON_HIDE,
    ARM_QUICKSILVER_DRAGON_ARMOUR,
    ARM_SCARF,
    ARM_ORB,
#endif

    NUM_ARMOURS
};

enum armour_property_type
{
    PARM_AC,
    PARM_EVASION,
};

const int SP_FORBID_EGO   = -1;
const int SP_FORBID_BRAND = -1;

// Be sure to update _debug_acquirement_stats and _str_to_ego to match.
enum brand_type // item_def.special
{
    SPWPN_FORBID_BRAND = -1,
    SPWPN_NORMAL,
    SPWPN_FLAMING,
    SPWPN_FREEZING,
    SPWPN_HOLY_WRATH,
    SPWPN_ELECTROCUTION,
#if TAG_MAJOR_VERSION == 34
    SPWPN_ORC_SLAYING,
    SPWPN_DRAGON_SLAYING,
#endif
    SPWPN_VENOM,
    SPWPN_PROTECTION,
    SPWPN_DRAINING,
    SPWPN_SPEED,
    SPWPN_HEAVY,
#if TAG_MAJOR_VERSION == 34
    SPWPN_FLAME,   // ranged, only
    SPWPN_FROST,   // ranged, only
#endif
    SPWPN_VAMPIRISM,
    SPWPN_PAIN,
    SPWPN_ANTIMAGIC,
    SPWPN_DISTORTION,
#if TAG_MAJOR_VERSION == 34
    SPWPN_REACHING,
    SPWPN_RETURNING,
#endif
    SPWPN_CHAOS,
#if TAG_MAJOR_VERSION == 34
    SPWPN_EVASION,
    MAX_GHOST_BRAND = SPWPN_EVASION,
#else
    MAX_GHOST_BRAND = SPWPN_CHAOS,
#endif

#if TAG_MAJOR_VERSION == 34
    SPWPN_CONFUSE, // XXX not a real weapon brand, only for Confusing Touch
#endif
    SPWPN_PENETRATION,
    SPWPN_REAPING,
    SPWPN_SPECTRAL,

// From this point on save compat is irrelevant.
    NUM_REAL_SPECIAL_WEAPONS,

    SPWPN_ACID,    // acid bite and Punk
#if TAG_MAJOR_VERSION > 34
    SPWPN_CONFUSE, // Confusing Touch only for the moment
#endif
    SPWPN_WEAKNESS,
    SPWPN_VULNERABILITY,
    SPWPN_FOUL_FLAME,
    SPWPN_DEBUG_RANDART,
    NUM_SPECIAL_WEAPONS,
};

enum corpse_type
{
    CORPSE_BODY,
    CORPSE_SKELETON,
};

enum hands_reqd_type
{
    HANDS_ONE,
    HANDS_TWO,
};

enum jewellery_type
{
#if TAG_MAJOR_VERSION == 34
    RING_REGENERATION,
#endif
    RING_PROTECTION,
    RING_FIRST_RING = RING_PROTECTION,
    RING_PROTECTION_FROM_FIRE,
    RING_POISON_RESISTANCE,
    RING_PROTECTION_FROM_COLD,
    RING_STRENGTH,
    RING_SLAYING,
    RING_SEE_INVISIBLE,
    RING_RESIST_CORROSION,
#if TAG_MAJOR_VERSION == 34
    RING_ATTENTION,
    RING_TELEPORTATION,
#endif
    RING_EVASION,
#if TAG_MAJOR_VERSION == 34
    RING_SUSTAIN_ATTRIBUTES,
#endif
    RING_STEALTH,
    RING_DEXTERITY,
    RING_INTELLIGENCE,
    RING_WIZARDRY,
    RING_MAGICAL_POWER,
    RING_FLIGHT,
    RING_POSITIVE_ENERGY,
    RING_WILLPOWER,
    RING_FIRE,
    RING_ICE,
#if TAG_MAJOR_VERSION == 34
    RING_TELEPORT_CONTROL,
#endif
    NUM_RINGS,                         //   keep as last ring; should not overlap
                                       //   with amulets!
    // RINGS after num_rings are for unique types for artefacts
    //   (no non-artefact version).
    // Currently none.
    // XXX: trying to add one doesn't actually work

#if TAG_MAJOR_VERSION == 34
    AMU_RAGE = 35,
    AMU_FIRST_AMULET = AMU_RAGE,
    AMU_HARM,
#endif
#if TAG_MAJOR_VERSION > 34
    AMU_ACROBAT = 35,
    AMU_FIRST_AMULET = AMU_ACROBAT,
#elif TAG_MAJOR_VERSION == 34
    AMU_ACROBAT,
#endif
    AMU_MANA_REGENERATION,
#if TAG_MAJOR_VERSION == 34
    AMU_THE_GOURMAND,
    AMU_CONSERVATION,
    AMU_CONTROLLED_FLIGHT,
    AMU_INACCURACY,
#endif
    AMU_NOTHING,
    AMU_GUARDIAN_SPIRIT,
    AMU_FAITH,
    AMU_REFLECTION,
    AMU_REGENERATION,

    NUM_JEWELLERY
};

enum class launch_retval
{
    BUGGY = -1, // could be 0 maybe? TODO: test
    FUMBLED,
    LAUNCHED,
    THROWN,
};

enum misc_item_type
{
#if TAG_MAJOR_VERSION == 34
    MISC_BOTTLED_EFREET,
    MISC_FAN_OF_GALES,
    MISC_LAMP_OF_FIRE,
    MISC_STONE_OF_TREMORS,
    MISC_BUGGY_LANTERN_OF_SHADOWS,
#endif
    MISC_HORN_OF_GERYON,
    MISC_BOX_OF_BEASTS,
#if TAG_MAJOR_VERSION == 34
    MISC_CRYSTAL_BALL_OF_ENERGY,
    MISC_BUGGY_EBONY_CASKET,
#endif
    MISC_LIGHTNING_ROD,

#if TAG_MAJOR_VERSION == 34
    MISC_DECK_OF_ESCAPE,
    MISC_FIRST_DECK = MISC_DECK_OF_ESCAPE,
    MISC_DECK_OF_DESTRUCTION,
    MISC_DECK_OF_DUNGEONS,
    MISC_DECK_OF_SUMMONING,
    MISC_DECK_OF_WONDERS,
    MISC_DECK_OF_PUNISHMENT,
    MISC_DECK_OF_WAR,
    MISC_DECK_OF_CHANGES,
    MISC_DECK_OF_DEFENCE,
    MISC_LAST_DECK = MISC_DECK_OF_DEFENCE,

    MISC_RUNE_OF_ZOT,
#endif

    MISC_QUAD_DAMAGE, // Sprint only

    MISC_PHIAL_OF_FLOODS,
    MISC_SACK_OF_SPIDERS,
    MISC_ZIGGURAT,

    MISC_PHANTOM_MIRROR,
#if TAG_MAJOR_VERSION == 34
    MISC_DECK_OF_ODDITIES,
    MISC_XOMS_CHESSBOARD,
#endif
    MISC_TIN_OF_TREMORSTONES,
    MISC_CONDENSER_VANE,
    MISC_GRAVITAMBOURINE,

    NUM_MISCELLANY,
    MISC_DECK_UNKNOWN = NUM_MISCELLANY,
};

// in no particular order (but we need *a* fixed order for dbg-scan)
const vector<misc_item_type> misc_types =
{
#if TAG_MAJOR_VERSION == 34
    MISC_FAN_OF_GALES,
    MISC_LAMP_OF_FIRE,
    MISC_STONE_OF_TREMORS,
    MISC_BUGGY_LANTERN_OF_SHADOWS,
#endif
    MISC_HORN_OF_GERYON, MISC_BOX_OF_BEASTS,
#if TAG_MAJOR_VERSION == 34
    MISC_CRYSTAL_BALL_OF_ENERGY,
#endif
    MISC_LIGHTNING_ROD, MISC_PHIAL_OF_FLOODS,
    MISC_QUAD_DAMAGE,
    MISC_SACK_OF_SPIDERS,
    MISC_PHANTOM_MIRROR,
#if TAG_MAJOR_VERSION == 34
    MISC_XOMS_CHESSBOARD,
#endif
    MISC_ZIGGURAT,
#if TAG_MAJOR_VERSION == 34
    MISC_BOTTLED_EFREET, MISC_BUGGY_EBONY_CASKET,
#endif
    MISC_TIN_OF_TREMORSTONES,
    MISC_CONDENSER_VANE,
    MISC_GRAVITAMBOURINE,
};

enum missile_type
{
    MI_DART,
#if TAG_MAJOR_VERSION == 34
    MI_NEEDLE,
#endif
    MI_ARROW,
    MI_BOLT,
    MI_JAVELIN,

    MI_STONE,
    MI_LARGE_ROCK,
    MI_SLING_BULLET,
    MI_THROWING_NET,
    MI_BOOMERANG,

    MI_SLUG,

    NUM_MISSILES,
    MI_NONE             // was MI_EGGPLANT... used for launch type detection
};

enum rune_type
{
    RUNE_SWAMP,
    RUNE_SNAKE,
    RUNE_SHOALS,
    RUNE_SLIME,
    RUNE_ELF, // only used in sprints
    RUNE_VAULTS,
    RUNE_TOMB,

    RUNE_DIS,
    RUNE_GEHENNA,
    RUNE_COCYTUS,
    RUNE_TARTARUS,

    RUNE_ABYSSAL,

    RUNE_DEMONIC,

    // order must match monsters
    RUNE_MNOLEG,
    RUNE_LOM_LOBON,
    RUNE_CEREBOV,
    RUNE_GLOORX_VLOQ,

    RUNE_SPIDER,
    RUNE_FOREST, // only used in sprints
    NUM_RUNE_TYPES
};

// Order roughly matches branch_type.
enum gem_type
{
    GEM_DUNGEON,
#if TAG_MAJOR_VERSION == 34
    GEM_ORC,
#endif
    GEM_ELF,
    GEM_LAIR,
    GEM_SWAMP,
    GEM_SHOALS,
    GEM_SNAKE,
    GEM_SPIDER,
    GEM_SLIME,
    GEM_VAULTS,
    GEM_CRYPT,
    GEM_TOMB,
    GEM_DEPTHS,
    GEM_ZOT,
    NUM_GEM_TYPES
};

enum scroll_type
{
    SCR_IDENTIFY,
    SCR_TELEPORTATION,
    SCR_FEAR,
    SCR_NOISE,
#if TAG_MAJOR_VERSION == 34
    SCR_REMOVE_CURSE,
#endif
    SCR_SUMMONING,
    SCR_ENCHANT_WEAPON,
    SCR_ENCHANT_ARMOUR,
    SCR_TORMENT,
#if TAG_MAJOR_VERSION == 34
    SCR_RANDOM_USELESSNESS,
    SCR_CURSE_WEAPON,
    SCR_CURSE_ARMOUR,
#endif
    SCR_IMMOLATION,
    SCR_BLINKING,
    SCR_REVELATION,
    SCR_FOG,
    SCR_ACQUIREMENT,
#if TAG_MAJOR_VERSION == 34
    SCR_ENCHANT_WEAPON_II,
#endif
    SCR_BRAND_WEAPON,
#if TAG_MAJOR_VERSION == 34
    SCR_RECHARGING,
    SCR_ENCHANT_WEAPON_III,
    SCR_HOLY_WORD,
#endif
    SCR_VULNERABILITY,
    SCR_SILENCE,
    SCR_AMNESIA,
#if TAG_MAJOR_VERSION == 34
    SCR_CURSE_JEWELLERY,
#endif
    SCR_POISON,
    SCR_BUTTERFLIES,
    NUM_SCROLLS
};

// Be sure to update _debug_acquirement_stats and str_to_ego to match.
enum special_armour_type
{
    SPARM_FORBID_EGO = -1,
    SPARM_NORMAL,
#if TAG_MAJOR_VERSION == 34
    SPARM_RUNNING,
#endif
    SPARM_FIRE_RESISTANCE,
    SPARM_COLD_RESISTANCE,
    SPARM_POISON_RESISTANCE,
    SPARM_SEE_INVISIBLE,
    SPARM_INVISIBILITY,
    SPARM_STRENGTH,
    SPARM_DEXTERITY,
    SPARM_INTELLIGENCE,
    SPARM_PONDEROUSNESS,
    SPARM_FLYING,
    SPARM_WILLPOWER,
    SPARM_PROTECTION,
    SPARM_STEALTH,
    SPARM_RESISTANCE,
    SPARM_POSITIVE_ENERGY,
    SPARM_ARCHMAGI,
    SPARM_PRESERVATION,
    SPARM_REFLECTION,
    SPARM_SPIRIT_SHIELD,
    SPARM_HURLING,
#if TAG_MAJOR_VERSION == 34
    SPARM_JUMPING,
#endif
    SPARM_REPULSION,
#if TAG_MAJOR_VERSION == 34
    SPARM_CLOUD_IMMUNE,
#endif
    SPARM_HARM,
    SPARM_SHADOWS,
    SPARM_RAMPAGING,
    SPARM_INFUSION,
    SPARM_LIGHT,
    SPARM_RAGE,
    SPARM_MAYHEM,
    SPARM_GUILE,
    SPARM_ENERGY,
    NUM_REAL_SPECIAL_ARMOURS,
    NUM_SPECIAL_ARMOURS,
};

// Be sure to update _str_to_ego to match.
enum special_missile_type // to separate from weapons in general {dlb}
{
    SPMSL_FORBID_BRAND = -1,
    SPMSL_NORMAL,
    SPMSL_FLAME,
    SPMSL_FROST,
    SPMSL_POISONED,
    SPMSL_CURARE,                      // Needle-only brand
#if TAG_MAJOR_VERSION == 34
    SPMSL_RETURNING,
#endif
    SPMSL_CHAOS,
#if TAG_MAJOR_VERSION == 34
    SPMSL_PENETRATION,
#endif
    SPMSL_DISPERSAL,
    SPMSL_EXPLODING,                   // Only used by Damnation crossbow
#if TAG_MAJOR_VERSION == 34
    SPMSL_STEEL,
#endif
    SPMSL_SILVER,
#if TAG_MAJOR_VERSION == 34
    SPMSL_PARALYSIS,                   // dart only from here on
    SPMSL_SLOW,
    SPMSL_SLEEP,
    SPMSL_CONFUSION,
    SPMSL_SICKNESS,
#endif
    SPMSL_FRENZY,                      // Datura
    SPMSL_BLINDING,                    // Atropa
    NUM_REAL_SPECIAL_MISSILES,
    NUM_SPECIAL_MISSILES = NUM_REAL_SPECIAL_MISSILES,
};

enum special_ring_type // jewellery env.item[].special values
{
    SPRING_RANDART = 200,
    SPRING_UNRANDART = 201,
};

enum stave_type
{
#if TAG_MAJOR_VERSION == 34
    STAFF_WIZARDRY,
    STAFF_POWER,
#endif
    STAFF_FIRE,
    STAFF_COLD,
    STAFF_ALCHEMY,
#if TAG_MAJOR_VERSION == 34
    STAFF_ENERGY,
#endif
    STAFF_DEATH,
    STAFF_CONJURATION,
#if TAG_MAJOR_VERSION == 34
    STAFF_ENCHANTMENT,
    STAFF_SUMMONING,
#endif
    STAFF_AIR,
    STAFF_EARTH,
#if TAG_MAJOR_VERSION == 34
    STAFF_CHANNELLING,
#endif
    NUM_STAVES,
};

#if TAG_MAJOR_VERSION == 34
enum rod_type
{
    ROD_LIGHTNING,
    ROD_SWARM,
    ROD_IGNITION,
    ROD_CLOUDS,
    ROD_DESTRUCTION,
    ROD_INACCURACY,
    ROD_WARDING,
    ROD_SHADOWS,
    ROD_IRON,
    ROD_VENOM,
    NUM_RODS,
};
#endif

enum weapon_type
{
    WPN_CLUB,
    WPN_WHIP,
#if TAG_MAJOR_VERSION == 34
    WPN_HAMMER,
#endif
    WPN_MACE,
    WPN_FLAIL,
    WPN_MORNINGSTAR,
#if TAG_MAJOR_VERSION == 34
    WPN_SPIKED_FLAIL,
#endif
    WPN_DIRE_FLAIL,
    WPN_EVENINGSTAR,
    WPN_GREAT_MACE,

    WPN_DAGGER,
    WPN_QUICK_BLADE,
    WPN_SHORT_SWORD,
    WPN_RAPIER,

    WPN_FALCHION,
    WPN_LONG_SWORD,
    WPN_SCIMITAR,
    WPN_GREAT_SWORD,

    WPN_HAND_AXE,
    WPN_WAR_AXE,
    WPN_BROAD_AXE,
    WPN_BATTLEAXE,
    WPN_EXECUTIONERS_AXE,

    WPN_SPEAR,
    WPN_TRIDENT,
    WPN_HALBERD,
    WPN_GLAIVE,
    WPN_BARDICHE,

#if TAG_MAJOR_VERSION == 34
    WPN_BLOWGUN,
#endif

#if TAG_MAJOR_VERSION > 34
    WPN_HAND_CANNON,
#endif
    WPN_ARBALEST,
#if TAG_MAJOR_VERSION > 34
    WPN_TRIPLE_CROSSBOW,
#endif

    WPN_SHORTBOW,
#if TAG_MAJOR_VERSION > 34
    WPN_ORCBOW,
#endif
    WPN_LONGBOW,

#if TAG_MAJOR_VERSION > 34
    WPN_SLING,
#endif

    WPN_DEMON_WHIP,
    WPN_GIANT_CLUB,
    WPN_GIANT_SPIKED_CLUB,

    WPN_DEMON_BLADE,
    WPN_DOUBLE_SWORD,
    WPN_TRIPLE_SWORD,

    WPN_DEMON_TRIDENT,
#if TAG_MAJOR_VERSION == 34
    WPN_SCYTHE,
#endif

    WPN_STAFF,          // Just used for the weapon stats for magical staves.
    WPN_QUARTERSTAFF,
    WPN_LAJATANG,

#if TAG_MAJOR_VERSION == 34
    WPN_SLING,

    WPN_BLESSED_FALCHION,
    WPN_BLESSED_LONG_SWORD,
    WPN_BLESSED_SCIMITAR,
    WPN_BLESSED_GREAT_SWORD,
#endif
    WPN_EUDEMON_BLADE,
#if TAG_MAJOR_VERSION == 34
    WPN_BLESSED_DOUBLE_SWORD,
    WPN_BLESSED_TRIPLE_SWORD,
#endif
    WPN_SACRED_SCOURGE,
    WPN_TRISHULA,

#if TAG_MAJOR_VERSION == 34
    WPN_FUSTIBALUS,
    WPN_HAND_CANNON,
    WPN_TRIPLE_CROSSBOW,

    WPN_CUTLASS,
    WPN_ORCBOW,
#endif

    NUM_WEAPONS,

// special cases
    WPN_UNARMED,
    WPN_UNKNOWN,
    WPN_RANDOM,
    WPN_VIABLE,
};

enum weapon_property_type
{
    PWPN_DAMAGE,
    PWPN_HIT,
    PWPN_SPEED,
    PWPN_ACQ_WEIGHT,
};

enum vorpal_damage_type
{
    // These are the types of damage a weapon can do. You can set more
    // than one of these.
    DAM_BASH            = 0x0000,       // non-melee weapon blugeoning
    DAM_BLUDGEON        = 0x0001,       // crushing
    DAM_SLICE           = 0x0002,       // slicing/chopping
    DAM_PIERCE          = 0x0004,       // stabbing/piercing
    DAM_WHIP            = 0x0008,       // whip slashing
    DAM_MAX_TYPE        = DAM_WHIP,

    // These were used for vorpal weapon descriptions, many years ago.
    // You shouldn't set more than one of these.
    DVORP_NONE          = 0x0000,       // used for non-melee weapons
    DVORP_CRUSHING      = 0x1000,
    DVORP_SLICING       = 0x2000,
    DVORP_PIERCING      = 0x3000,
    DVORP_CHOPPING      = 0x4000,       // used for axes
    DVORP_SLASHING      = 0x5000,       // used for whips

    DVORP_CLAWING       = 0x6000,       // claw damage
    DVORP_TENTACLE      = 0x7000,       // tentacle damage

    // These are shortcuts to tie vorpal/damage types for easy setting...
    // as above, setting more than one vorpal type is trouble.
    DAMV_NON_MELEE      = DVORP_NONE     | DAM_BASH,            // launchers
    DAMV_CRUSHING       = DVORP_CRUSHING | DAM_BLUDGEON,
    DAMV_SLICING        = DVORP_SLICING  | DAM_SLICE,
    DAMV_PIERCING       = DVORP_PIERCING | DAM_PIERCE,
    DAMV_CHOPPING       = DVORP_CHOPPING | DAM_SLICE,
    DAMV_SLASHING       = DVORP_SLASHING | DAM_WHIP,

    DAM_MASK            = 0x0fff,       // strips vorpal specification
    DAMV_MASK           = 0xf000,       // strips non-vorpal specification
};

enum wand_type
{
    WAND_FLAME,
#if TAG_MAJOR_VERSION == 34
    WAND_FROST_REMOVED,
    WAND_SLOWING_REMOVED,
    WAND_HASTING_REMOVED,
    WAND_MAGIC_DARTS_REMOVED,
    WAND_HEAL_WOUNDS_REMOVED,
#endif
    WAND_PARALYSIS,
#if TAG_MAJOR_VERSION == 34
    WAND_FIRE_REMOVED,
    WAND_COLD_REMOVED,
    WAND_CONFUSION_REMOVED,
    WAND_INVISIBILITY_REMOVED,
#endif
    WAND_DIGGING,
    WAND_ICEBLAST,
#if TAG_MAJOR_VERSION == 34
    WAND_TELEPORTATION_REMOVED,
    WAND_LIGHTNING_REMOVED,
#endif
    WAND_POLYMORPH,
    WAND_CHARMING,
    WAND_ACID,
#if TAG_MAJOR_VERSION == 34
    WAND_RANDOM_EFFECTS_REMOVED,
#endif
    WAND_MINDBURST,
#if TAG_MAJOR_VERSION == 34
    WAND_CLOUDS_REMOVED,
    WAND_SCATTERSHOT_REMOVED,
#endif
    WAND_LIGHT,
    WAND_QUICKSILVER,
    WAND_ROOTS,
    WAND_WARPING,
    NUM_WANDS
};

#if TAG_MAJOR_VERSION == 34
enum food_type
{
    FOOD_RATION,
    FOOD_BREAD_RATION,
    FOOD_PEAR,
    FOOD_APPLE,
    FOOD_CHOKO,
    FOOD_ROYAL_JELLY,   // was: royal jelly
    FOOD_UNUSED, // was: royal jelly and/or pizza
    FOOD_FRUIT,  // was: snozzcumber
    FOOD_PIZZA,
    FOOD_APRICOT,
    FOOD_ORANGE,
    FOOD_BANANA,
    FOOD_STRAWBERRY,
    FOOD_RAMBUTAN,
    FOOD_LEMON,
    FOOD_GRAPE,
    FOOD_SULTANA,
    FOOD_LYCHEE,
    FOOD_BEEF_JERKY,
    FOOD_CHEESE,
    FOOD_SAUSAGE,
    FOOD_CHUNK,
    FOOD_AMBROSIA,
    NUM_FOODS
};
#endif

enum item_set_type
{
    ITEM_SET_HEX_WANDS,
    ITEM_SET_BEAM_WANDS,
    ITEM_SET_BLAST_WANDS,
    ITEM_SET_ALLY_SCROLLS,
    ITEM_SET_AREA_MISCELLANY,
    ITEM_SET_ALLY_MISCELLANY,
    ITEM_SET_CONTROL_MISCELLANY,
    NUM_ITEM_SET_TYPES
};

enum talisman_type
{
    TALISMAN_BEAST,
    TALISMAN_MAW,
    TALISMAN_SERPENT,
    TALISMAN_BLADE,
    TALISMAN_STATUE,
    TALISMAN_DRAGON,
    TALISMAN_DEATH,
    TALISMAN_STORM,
    TALISMAN_FLUX,
    NUM_TALISMANS,
};

enum special_gizmo_type
{
    SPGIZMO_NORMAL,
    SPGIZMO_MANAREV,
    SPGIZMO_GADGETEER,
    SPGIZMO_PARRYREV,
    SPGIZMO_AUTODAZZLE,
};
