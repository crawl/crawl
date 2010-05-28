#ifndef ITEMPROP_ENUM_H
#define ITEMPROP_ENUM_H

#include "tags.h"

enum armour_type
{
    ARM_ROBE,                    //    0
    ARM_LEATHER_ARMOUR,
    ARM_RING_MAIL,
    ARM_SCALE_MAIL,
    ARM_CHAIN_MAIL,
    ARM_SPLINT_MAIL,             //    5
    ARM_BANDED_MAIL,
    ARM_PLATE_MAIL,

    ARM_CLOAK,

    ARM_CAP,
    ARM_WIZARD_HAT,              //   10
    ARM_HELMET,

    ARM_GLOVES,

    ARM_BOOTS,

    ARM_BUCKLER,
    ARM_SHIELD,                  //   15
    ARM_LARGE_SHIELD,
    ARM_MAX_RACIAL = ARM_LARGE_SHIELD,

    ARM_CRYSTAL_PLATE_MAIL,

    ARM_ANIMAL_SKIN,

    ARM_TROLL_HIDE,
    ARM_TROLL_LEATHER_ARMOUR,    //   20

    ARM_DRAGON_HIDE,
    ARM_DRAGON_ARMOUR,
    ARM_ICE_DRAGON_HIDE,
    ARM_ICE_DRAGON_ARMOUR,
    ARM_STEAM_DRAGON_HIDE,       //   25
    ARM_STEAM_DRAGON_ARMOUR,
    ARM_MOTTLED_DRAGON_HIDE,
    ARM_MOTTLED_DRAGON_ARMOUR,
    ARM_STORM_DRAGON_HIDE,
    ARM_STORM_DRAGON_ARMOUR,     //   30
    ARM_GOLD_DRAGON_HIDE,
    ARM_GOLD_DRAGON_ARMOUR,
    ARM_SWAMP_DRAGON_HIDE,
    ARM_SWAMP_DRAGON_ARMOUR,

    ARM_CENTAUR_BARDING,         //   35
    ARM_NAGA_BARDING,

    NUM_ARMOURS                  //   37
};

enum armour_property_type
{
    PARM_AC,                           //    0
    PARM_EVASION
};

enum boot_type          // used in pluses2
{
    TBOOT_BOOTS = 0,
    TBOOT_NAGA_BARDING,
    TBOOT_CENTAUR_BARDING,
    NUM_BOOT_TYPES
};

const int SP_FORBID_EGO   = -1;
const int SP_FORBID_BRAND = -1;
const int SP_UNKNOWN_BRAND = 31; // seen_weapon/armour is a 32-bit bitfield

enum brand_type // equivalent to (you.inv[].special or mitm[].special) % 30
{
    SPWPN_FORBID_BRAND = -1,
    SPWPN_NORMAL,
    SPWPN_FLAMING,
    SPWPN_FREEZING,
    SPWPN_HOLY_WRATH,
    SPWPN_ELECTROCUTION,
    SPWPN_ORC_SLAYING,
    SPWPN_DRAGON_SLAYING,
    SPWPN_VENOM,
    SPWPN_PROTECTION,
    SPWPN_DRAINING,
    SPWPN_SPEED,
    SPWPN_VORPAL,
    SPWPN_FLAME,   // ranged, only
    SPWPN_FROST,   // ranged, only
    SPWPN_VAMPIRICISM,
    SPWPN_PAIN,
    SPWPN_DISTORTION,
    SPWPN_REACHING,
    SPWPN_RETURNING,
    SPWPN_CHAOS,
    SPWPN_EVASION,

    MAX_PAN_LORD_BRANDS = SPWPN_EVASION,

    SPWPN_CONFUSE,
    SPWPN_PENETRATION,
    SPWPN_REAPING,

    NUM_REAL_SPECIAL_WEAPONS,

    SPWPN_ACID,    // acid bite only for the moment
    SPWPN_DEBUG_RANDART,
    NUM_SPECIAL_WEAPONS,
    SPWPN_DUMMY_CRUSHING         // ONLY TEMPORARY USAGE -- converts to VORPAL
};

enum corpse_type
{
    CORPSE_BODY,                       //    0
    CORPSE_SKELETON
};

enum hands_reqd_type
{
    HANDS_ONE,
    HANDS_HALF,
    HANDS_TWO,

    HANDS_DOUBLE        // not a level, marks double ended weapons (== half)
};

enum helmet_desc_type
{
    THELM_DESC_PLAIN    = 0,
    THELM_DESC_WINGED,
    THELM_DESC_HORNED,
    THELM_DESC_CRESTED,
    THELM_DESC_PLUMED,
    THELM_DESC_MAX_SOFT = THELM_DESC_PLUMED,
    THELM_DESC_SPIKED,
    THELM_DESC_VISORED,
    THELM_DESC_GOLDEN,
    THELM_NUM_DESCS
};

enum gloves_desc_type
{
    TGLOV_DESC_GLOVES = 0,
    TGLOV_DESC_GAUNTLETS,
    TGLOV_DESC_BRACERS
};

enum jewellery_type
{
    RING_FIRST_RING = 0,

    RING_REGENERATION = RING_FIRST_RING, //    0
    RING_PROTECTION,
    RING_PROTECTION_FROM_FIRE,
    RING_POISON_RESISTANCE,
    RING_PROTECTION_FROM_COLD,
    RING_STRENGTH,                     //    5
    RING_SLAYING,
    RING_SEE_INVISIBLE,
    RING_INVISIBILITY,
    RING_HUNGER,
    RING_TELEPORTATION,                //   10
    RING_EVASION,
    RING_SUSTAIN_ABILITIES,
    RING_SUSTENANCE,
    RING_DEXTERITY,
    RING_INTELLIGENCE,                 //   15
    RING_WIZARDRY,
    RING_MAGICAL_POWER,
    RING_LEVITATION,
    RING_LIFE_PROTECTION,
    RING_PROTECTION_FROM_MAGIC,        //   20
    RING_FIRE,
    RING_ICE,
    RING_TELEPORT_CONTROL,             //   23

    NUM_RINGS,                         //   24, keep as last ring; can overlap
                                       //   safely with first amulet.

    AMU_FIRST_AMULET = 35,
    AMU_RAGE = AMU_FIRST_AMULET,       //   35
    AMU_CLARITY,
    AMU_WARDING,
    AMU_RESIST_CORROSION,
    AMU_THE_GOURMAND,                  //   40
    AMU_CONSERVATION,
    AMU_CONTROLLED_FLIGHT,
    AMU_INACCURACY,
    AMU_RESIST_MUTATION,
    AMU_GUARDIAN_SPIRIT,
    AMU_FAITH,
    AMU_STASIS,

    NUM_JEWELLERY
};

enum launch_retval
{
    LRET_FUMBLED = 0,                  // must be left as 0
    LRET_LAUNCHED,
    LRET_THROWN
};

enum misc_item_type
{
    MISC_BOTTLED_EFREET,               //    0
    MISC_CRYSTAL_BALL_OF_SEEING,
    MISC_AIR_ELEMENTAL_FAN,
    MISC_LAMP_OF_FIRE,
    MISC_STONE_OF_EARTH_ELEMENTALS,
    MISC_LANTERN_OF_SHADOWS,
    MISC_HORN_OF_GERYON,
    MISC_BOX_OF_BEASTS,
    MISC_CRYSTAL_BALL_OF_ENERGY,
    MISC_EMPTY_EBONY_CASKET,
    MISC_CRYSTAL_BALL_OF_FIXATION,
    MISC_DISC_OF_STORMS,

    // pure decks
    MISC_DECK_OF_ESCAPE,
    MISC_DECK_OF_DESTRUCTION,
    MISC_DECK_OF_DUNGEONS,
    MISC_DECK_OF_SUMMONING,
    MISC_DECK_OF_WONDERS,
    MISC_DECK_OF_PUNISHMENT,

    // mixed decks
    MISC_DECK_OF_WAR,
    MISC_DECK_OF_CHANGES,
    MISC_DECK_OF_DEFENCE,

    MISC_RUNE_OF_ZOT,

    NUM_MISCELLANY // mv: used for random generation
};

enum missile_type
{
    MI_DART,                           //    0
    MI_NEEDLE,
    MI_ARROW,
    MI_BOLT,
    MI_JAVELIN,
    MI_MAX_RACIAL = MI_JAVELIN,

    MI_STONE,                          //    5
    MI_LARGE_ROCK,
    MI_SLING_BULLET,
    MI_THROWING_NET,

    NUM_MISSILES,                      //    9
    MI_NONE             // was MI_EGGPLANT... used for launch type detection
};

enum rune_type
{
    RUNE_SWAMP,
    RUNE_SNAKE_PIT,
    RUNE_SHOALS,
    RUNE_SLIME_PITS,
    RUNE_ELVEN_HALLS, // unused
    RUNE_VAULTS,
    RUNE_TOMB,

    RUNE_DIS,
    RUNE_GEHENNA,
    RUNE_COCYTUS,
    RUNE_TARTARUS,

    RUNE_ABYSSAL,

    RUNE_DEMONIC,
    RUNE_MNOLEG,
    RUNE_LOM_LOBON,
    RUNE_CEREBOV,
    RUNE_GLOORX_VLOQ,
    NUM_RUNE_TYPES             // should always be last
};

enum scroll_type
{
    SCR_IDENTIFY,                      //    0
    SCR_TELEPORTATION,
    SCR_FEAR,
    SCR_NOISE,
    SCR_REMOVE_CURSE,
    SCR_DETECT_CURSE,                  //   5
    SCR_SUMMONING,
    SCR_ENCHANT_WEAPON_I,
    SCR_ENCHANT_ARMOUR,
    SCR_TORMENT,
    SCR_RANDOM_USELESSNESS,            //   10
    SCR_CURSE_WEAPON,
    SCR_CURSE_ARMOUR,
    SCR_IMMOLATION,
    SCR_BLINKING,
    SCR_PAPER,                         //   15
    SCR_MAGIC_MAPPING,
    SCR_FOG,
    SCR_ACQUIREMENT,
    SCR_ENCHANT_WEAPON_II,
    SCR_VORPALISE_WEAPON,              //   20
    SCR_RECHARGING,
    SCR_ENCHANT_WEAPON_III,
    SCR_HOLY_WORD,
    SCR_VULNERABILITY,
    SCR_SILENCE,                       //   25
    NUM_SCROLLS
};

enum special_armour_type
{
    SPARM_FORBID_EGO = -1,             //   -1
    SPARM_NORMAL,                      //    0
    SPARM_RUNNING,
    SPARM_FIRE_RESISTANCE,
    SPARM_COLD_RESISTANCE,
    SPARM_POISON_RESISTANCE,
    SPARM_SEE_INVISIBLE,               //    5
    SPARM_DARKNESS,
    SPARM_STRENGTH,
    SPARM_DEXTERITY,
    SPARM_INTELLIGENCE,
    SPARM_PONDEROUSNESS,               //   10
    SPARM_LEVITATION,
    SPARM_MAGIC_RESISTANCE,
    SPARM_PROTECTION,
    SPARM_STEALTH,
    SPARM_RESISTANCE,                  //   15
    SPARM_POSITIVE_ENERGY,
    SPARM_ARCHMAGI,
    SPARM_PRESERVATION,
    SPARM_REFLECTION,
    SPARM_SPIRIT_SHIELD,               //   20
    SPARM_ARCHERY,
    NUM_SPECIAL_ARMOURS                //   22
};

enum special_missile_type // to separate from weapons in general {dlb}
{
    SPMSL_FORBID_BRAND = -1,           //   -1
    SPMSL_NORMAL,                      //    0
    SPMSL_FLAME,
    SPMSL_FROST,
    SPMSL_POISONED,
    SPMSL_CURARE,                      // Needle-only brand
    SPMSL_RETURNING,                   //    5
    SPMSL_CHAOS,
    SPMSL_PENETRATION,
    SPMSL_REAPING,
    SPMSL_DISPERSAL,
    SPMSL_EXPLODING,                   //   10
    SPMSL_STEEL,
    SPMSL_SILVER,
    SPMSL_PARALYSIS,                   // paralysis, needle only from here in
    SPMSL_SLOW,                        // makes slow
    SPMSL_SLEEP,                       // sleep
    SPMSL_CONFUSION,                   // confusing
    SPMSL_SICKNESS,                    // sickness/disease
    SPMSL_RAGE,                        // berserk rage
    NUM_SPECIAL_MISSILES               // 20
};

enum special_ring_type // jewellery mitm[].special values
{
    SPRING_RANDART = 200,
    SPRING_UNRANDART = 201
};

enum stave_type
{
    // staves
    STAFF_WIZARDRY = 0,
    STAFF_POWER,
    STAFF_FIRE,
    STAFF_COLD,
    STAFF_POISON,
    STAFF_ENERGY,
    STAFF_DEATH,
    STAFF_CONJURATION,
    STAFF_ENCHANTMENT,
    STAFF_SUMMONING,
    STAFF_AIR,
    STAFF_EARTH,
    STAFF_CHANNELING,           // 12
    // rods
    STAFF_FIRST_ROD,
    STAFF_SMITING = STAFF_FIRST_ROD,
    STAFF_SPELL_SUMMONING,
    STAFF_DESTRUCTION_I,
    STAFF_DESTRUCTION_II,
    STAFF_DESTRUCTION_III,
    STAFF_DESTRUCTION_IV,
    STAFF_WARDING,
    STAFF_DISCOVERY,
    STAFF_DEMONOLOGY,
    STAFF_STRIKING,
    STAFF_VENOM,                // 23
    NUM_STAVES                  // must remain last member {dlb}
};

enum weapon_type
{
    WPN_CLUB,
    WPN_WHIP,
    WPN_HAMMER,
    WPN_MACE,
    WPN_FLAIL,
    WPN_MORNINGSTAR,
    WPN_SPIKED_FLAIL,
    WPN_DIRE_FLAIL,
    WPN_EVENINGSTAR,
    WPN_GREAT_MACE,

    WPN_DAGGER,
    WPN_QUICK_BLADE,
    WPN_SHORT_SWORD,
    WPN_SABRE,

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

    WPN_BLOWGUN,
    WPN_CROSSBOW,
    // Was hand crossbows, they are gone.
    WPN_BOW,
    WPN_LONGBOW,
    WPN_MAX_RACIAL = WPN_LONGBOW,

    WPN_ANKUS,
    WPN_DEMON_WHIP,
    WPN_GIANT_CLUB,
    WPN_GIANT_SPIKED_CLUB,

    WPN_KNIFE,

    WPN_KATANA,
    WPN_DEMON_BLADE,
    WPN_DOUBLE_SWORD,                  //   40
    WPN_TRIPLE_SWORD,

    WPN_DEMON_TRIDENT,
    WPN_SCYTHE,

    WPN_QUARTERSTAFF,
    WPN_LAJATANG,                      //   45

    WPN_SLING,

    WPN_MAX_NONBLESSED = WPN_SLING,

    WPN_BLESSED_FALCHION,
    WPN_BLESSED_LONG_SWORD,
    WPN_BLESSED_SCIMITAR,
    WPN_BLESSED_GREAT_SWORD,           //   50
    WPN_BLESSED_KATANA,
    WPN_HOLY_BLADE,
    WPN_BLESSED_DOUBLE_SWORD,
    WPN_BLESSED_TRIPLE_SWORD,
    WPN_HOLY_SCOURGE,

    NUM_WEAPONS,

// special cases
    WPN_UNARMED = 500,                 //  500
    WPN_UNKNOWN = 1000,                // 1000
    WPN_RANDOM,
    WPN_VIABLE
};

enum weapon_description_type
{
    DWPN_PLAIN = 0,                    //    0 - added to round out enum {dlb}
    DWPN_RUNED = 1,                    //    1
    DWPN_GLOWING,
    DWPN_ORCISH,
    DWPN_ELVEN,
    DWPN_DWARVEN                       //    5
};

enum weapon_property_type
{
    PWPN_DAMAGE,                       //    0
    PWPN_HIT,
    PWPN_SPEED,
    PWPN_ACQ_WEIGHT
};

enum vorpal_damage_type
{
    // These are the types of damage a weapon can do.  You can set more
    // than one of these.
    DAM_BASH            = 0x0000,       // non-melee weapon blugeoning
    DAM_BLUDGEON        = 0x0001,       // crushing
    DAM_SLICE           = 0x0002,       // slicing/chopping
    DAM_PIERCE          = 0x0004,       // stabbing/piercing
    DAM_WHIP            = 0x0008,       // whip slashing (no butcher)
    DAM_MAX_TYPE        = DAM_WHIP,

    // These are used for vorpal weapon descriptions.  You shouldn't set
    // more than one of these.
    DVORP_NONE          = 0x0000,       // used for non-melee weapons
    DVORP_CRUSHING      = 0x1000,
    DVORP_SLICING       = 0x2000,
    DVORP_PIERCING      = 0x3000,
    DVORP_CHOPPING      = 0x4000,       // used for axes
    DVORP_SLASHING      = 0x5000,       // used for whips
    DVORP_STABBING      = 0x6000,       // used for knives/daggers

    DVORP_CLAWING       = 0x7000,       // claw damage
    DVORP_TENTACLE      = 0x8000,       // tentacle damage

    // These are shortcuts to tie vorpal/damage types for easy setting...
    // as above, setting more than one vorpal type is trouble.
    DAMV_NON_MELEE      = DVORP_NONE     | DAM_BASH,            // launchers
    DAMV_CRUSHING       = DVORP_CRUSHING | DAM_BLUDGEON,
    DAMV_SLICING        = DVORP_SLICING  | DAM_SLICE,
    DAMV_PIERCING       = DVORP_PIERCING | DAM_PIERCE,
    DAMV_CHOPPING       = DVORP_CHOPPING | DAM_SLICE,
    DAMV_SLASHING       = DVORP_SLASHING | DAM_WHIP,
    DAMV_STABBING       = DVORP_STABBING | DAM_PIERCE,

    DAM_MASK            = 0x0fff,       // strips vorpal specification
    DAMV_MASK           = 0xf000        // strips non-vorpal specification
};

enum wand_type
{
    WAND_FLAME,
    WAND_FROST,
    WAND_SLOWING,
    WAND_HASTING,
    WAND_MAGIC_DARTS,
    WAND_HEALING,
    WAND_PARALYSIS,
    WAND_FIRE,
    WAND_COLD,
    WAND_CONFUSION,
    WAND_INVISIBILITY,
    WAND_DIGGING,
    WAND_FIREBALL,
    WAND_TELEPORTATION,
    WAND_LIGHTNING,
    WAND_POLYMORPH_OTHER,
    WAND_ENSLAVEMENT,
    WAND_DRAINING,
    WAND_RANDOM_EFFECTS,
    WAND_DISINTEGRATION,
    NUM_WANDS
};

enum zap_count_type
{
    ZAPCOUNT_EMPTY       = -1,
    ZAPCOUNT_UNKNOWN     = -2,
    ZAPCOUNT_RECHARGED   = -3,
    ZAPCOUNT_MAX_CHARGED = -4
};

#endif
