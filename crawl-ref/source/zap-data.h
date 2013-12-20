/**
 * @file
 * @brief Zap definitions. See zap_info struct in beam.cc.
**/

/*
struct zap_info
{
    zap_type ztype;
    const char* name;           // NULL means handled specially
    int power_cap;
    dam_deducer* damage;
    tohit_deducer* tohit;       // Enchantments have power modifier here
    int colour;
    bool is_enchantment;
    beam_type flavour;
    dungeon_char_type glyph;
    bool always_obvious;
    bool can_beam;
    bool is_explosion;
    int hit_loudness;
}
*/

{
    ZAP_THROW_FLAME,
    "puff of flame",
    50,
    new dicedef_calculator<2, 4, 1, 10>,
    new tohit_calculator<8, 1, 10>,
    RED,
    false,
    BEAM_FIRE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    2
},

{
    ZAP_THROW_FROST,
    "puff of frost",
    50,
    new dicedef_calculator<2, 4, 1, 10>,
    new tohit_calculator<8, 1, 10>,
    WHITE,
    false,
    BEAM_COLD,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    2
},

{
    ZAP_SLOW,
    "",
    100,
    NULL,
    new tohit_calculator<0, 3, 2>,
    BLACK,
    true,
    BEAM_SLOW,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_HASTE,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_HASTE,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_MAGIC_DART,
    "magic dart",
    25,
    new dicedef_calculator<1, 3, 1, 5>,
    new tohit_calculator<AUTOMATIC_HIT>,
    LIGHTMAGENTA,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    1
},

{
    ZAP_MAJOR_HEALING,
    "",
    100,
    new dicedef_calculator<1, 7, 1, 3>,
    NULL,
    BLACK,
    true,
    BEAM_HEALING,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_PARALYSE,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_PARALYSIS,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_BOLT_OF_FIRE,
    "bolt of fire",
    200,
    new calcdice_calculator<6, 18, 2, 3>,
    new tohit_calculator<10, 1, 25>,
    RED,
    false,
    BEAM_FIRE,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    6
},

{
    ZAP_BOLT_OF_COLD,
    "bolt of cold",
    200,
    new calcdice_calculator<6, 18, 2, 3>,
    new tohit_calculator<10, 1, 25>,
    WHITE,
    false,
    BEAM_COLD,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    6
},

{
    ZAP_PRIMAL_WAVE,
    "great wave of water",
    200,
    new calcdice_calculator<4, 14, 3, 5>,
    new tohit_calculator<10, 1, 25>,
    LIGHTBLUE,
    false,
    BEAM_WATER,
    DCHAR_WAVY,
    true,
    false,
    false,
    6
},

{
    ZAP_CONFUSE,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_CONFUSION,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_INVISIBILITY,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_INVISIBILITY,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_DIG,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_DIGGING,
    NUM_DCHAR_TYPES,
    false,
    true,
    false,
    4
},

{
    ZAP_FIREBALL,
    "fireball",
    200,
    new calcdice_calculator<3, 10, 1, 2>,
    new tohit_calculator<40>,
    RED,
    false,
    BEAM_FIRE,
    DCHAR_FIRED_ZAP,
    false,
    false,
    true,
    0 // Noise comes from explosion
},

{
    ZAP_TELEPORT_OTHER,
    "",
    100,
    NULL,
    new tohit_calculator<0, 3, 2>,
    BLACK,
    true,
    BEAM_TELEPORT,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_LIGHTNING_BOLT,
    "bolt of lightning",
    200,
    new calcdice_calculator<1, 11, 3, 5>,
    new tohit_calculator<7, 1, 40>,
    LIGHTCYAN,
    false,
    BEAM_ELECTRICITY,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    5 // XXX: Maybe louder?
},

{
    ZAP_POLYMORPH,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_POLYMORPH,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_VENOM_BOLT,
    "bolt of poison",
    200,
    new calcdice_calculator<4, 16, 3, 5>,
    new tohit_calculator<8, 1, 20>,
    LIGHTGREEN,
    false,
    BEAM_POISON,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    5 // XXX: Quieter because it's poison?
},

{
    ZAP_BOLT_OF_DRAINING,
    "bolt of negative energy",
    200,
    new calcdice_calculator<4, 15, 3, 5>,
    new tohit_calculator<8, 1, 20>,
    DARKGREY,
    false,
    BEAM_NEG,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    0 // Draining is soundless
},

{
    ZAP_LEHUDIBS_CRYSTAL_SPEAR,
    "crystal spear",
    200,
    new calcdice_calculator<10, 23, 1, 1>,
    new tohit_calculator<10, 1, 15>,
    WHITE,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_MISSILE,
    true,
    false,
    false,
    8
},

{
    ZAP_BOLT_OF_INACCURACY,
    "narrow beam of energy",
    1000,
    new calcdice_calculator<10, 40, 1, 1>,
    new tohit_calculator<1>,
    YELLOW,
    false,
    BEAM_ENERGY,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    3
},

{
    ZAP_ISKENDERUNS_MYSTIC_BLAST,
    "orb of energy",
    100,
    new calcdice_calculator<2, 12, 1, 3>,
    new tohit_calculator<10, 1, 7>,
    LIGHTMAGENTA,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    4
},

{
    ZAP_ENSLAVEMENT,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_ENSLAVE,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_PAIN,
    "",
    100,
    new dicedef_calculator<1, 4, 1,5>,
    new tohit_calculator<0, 7, 2>,
    BLACK,
    true,
    BEAM_PAIN,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    1 // XXX: Should this be soundless?
},

{
    ZAP_STICKY_FLAME,
    "sticky flame",
    100,
    new dicedef_calculator<2, 3, 1, 12>,
    new tohit_calculator<AUTOMATIC_HIT>,
    RED,
    false,
    BEAM_FIRE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    1
},

{
    ZAP_STICKY_FLAME_RANGE,
    "sticky flame",
    100,
    new dicedef_calculator<2, 3, 1, 12>,
    new tohit_calculator<AUTOMATIC_HIT>,
    RED,
    false,
    BEAM_FIRE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    1
},

{
    ZAP_DISPEL_UNDEAD,
    "",
    100,
    new calcdice_calculator<3, 20, 3, 4>,
    new tohit_calculator<0, 3, 2>,
    BLACK,
    true,
    BEAM_DISPEL_UNDEAD,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_BANISHMENT,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_BANISH,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_STING,
    "sting",
    25,
    new dicedef_calculator<1, 3, 1, 5>,
    new tohit_calculator<8, 1, 5>,
    GREEN,
    false,
    BEAM_POISON,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    1 // XXX: Maybe silent because it's poison?
},

{
    ZAP_HELLFIRE,
    "hellfire",
    200,
    new calcdice_calculator<3, 8, 3, 5>,
    new tohit_calculator<20, 1, 10>,
    LIGHTRED,
    false,
    BEAM_HELLFIRE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    true,
    9 // XXX: Even louder because it's hellish?
},

{
    ZAP_IRON_SHOT,
    "iron shot",
    200,
    new calcdice_calculator<9, 15, 3, 4>,
    new tohit_calculator<7, 1, 15>,
    LIGHTCYAN,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_MISSILE,
    true,
    false,
    false,
    6
},

{
    ZAP_STONE_ARROW,
    "stone arrow",
    50,
    new dicedef_calculator<3, 5, 1, 8>,
    new tohit_calculator<8, 1, 10>,
    LIGHTGREY,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_MISSILE,
    true,
    false,
    false,
    3
},

{
    ZAP_SHOCK,
    "zap",
    25,
    new dicedef_calculator<1, 3, 1, 4>,
    new tohit_calculator<8, 1, 7>,
    LIGHTCYAN,
    false,
    BEAM_ELECTRICITY,             // beams & reflects
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    1 // XXX: maybe electricity should be louder?
},

{
    ZAP_ORB_OF_ELECTRICITY,
    "orb of electricity",
    200,
    new calcdice_calculator<0, 13, 4, 5>,
    new tohit_calculator<40>,
    LIGHTBLUE,
    false,
    BEAM_ELECTRICITY,
    DCHAR_FIRED_ZAP,
    true,
    false,
    true,
    6 // XXX: maybe electricity should be louder?
},

{
    ZAP_SPIT_POISON,
    "splash of poison",
    50,
    new dicedef_calculator<1, 4, 1, 2>,
    new tohit_calculator<5, 1, 6>,
    GREEN,
    false,
    BEAM_POISON,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    1
},

{
    ZAP_DEBUGGING_RAY,
    "debugging ray",
    10000,
    new dicedef_calculator<AUTOMATIC_HIT, 1, 0, 1>,
    new tohit_calculator<AUTOMATIC_HIT>,
    WHITE,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_DEBUG,
    false,
    false,
    false,
    0
},

// XXX: How loud should breath be?
{
    ZAP_BREATHE_FIRE,
    "fiery breath",
    50,
    new dicedef_calculator<3, 4, 1, 3>,
    new tohit_calculator<8, 1, 6>,
    RED,
    false,
    BEAM_FIRE,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    7
},

{
    ZAP_BREATHE_FROST,
    "freezing breath",
    50,
    new dicedef_calculator<3, 4, 1, 3>,
    new tohit_calculator<8, 1, 6>,
    WHITE,
    false,
    BEAM_COLD,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    6
},

{
    ZAP_BREATHE_ACID,
    "acid",
    50,
    new dicedef_calculator<3, 3, 1, 3>,
    new tohit_calculator<5, 1, 6>,
    YELLOW,
    false,
    BEAM_ACID,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    6
},

{
    ZAP_BREATHE_POISON,
    "poison gas",
    50,
    new dicedef_calculator<3, 2, 1, 6>,
    new tohit_calculator<6, 1, 6>,
    GREEN,
    false,
    BEAM_POISON,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    0 // Explosion does the noise.
},

{
    ZAP_BREATHE_POWER,
    "bolt of dispelling energy",
    50,
    new dicedef_calculator<3, 3, 1, 3>,
    new tohit_calculator<5, 1, 6>,
    BLUE,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    6
},

{
    ZAP_ENSLAVE_SOUL,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_ENSLAVE_SOUL,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_AGONY,
    "agony",
    100,
    NULL,
    new tohit_calculator<0, 5, 1>,
    BLACK,
    true,
    BEAM_PAIN,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_DISINTEGRATE,
    "",
    100,
    new calcdice_calculator<3, 15, 3, 4>,
    new tohit_calculator<0, 5, 2>,
    BLACK,
    true,
    BEAM_DISINTEGRATION,
    NUM_DCHAR_TYPES,
    false,
    true,
    false,
    6
},

{
    ZAP_BREATHE_STEAM,
    "ball of steam",
    50,
    new dicedef_calculator<3, 4, 1, 5>,
    new tohit_calculator<10, 1, 10>,
    LIGHTGREY,
    false,
    BEAM_STEAM,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    0 // Explosion does the noise.
},

{
    ZAP_THROW_ICICLE,
    "shard of ice",
    100,
    new calcdice_calculator<3, 10, 1, 2>,
    new tohit_calculator<9, 1, 12>,
    WHITE,
    false,
    BEAM_ICE,
    DCHAR_FIRED_ZAP,
    false,
    false,
    false,
    4
},

{                           // ench_power controls radius
    ZAP_ICE_STORM,
    "great blast of cold",
    200,
    new calcdice_calculator<7, 22, 1, 1>,
    new tohit_calculator<20, 1, 10>,
    BLUE,
    false,
    BEAM_ICE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    true,
    9 // XXX: Should a storm be louder?
},

{
    ZAP_CORONA,
    "",
    100,
    NULL,
    new tohit_calculator<0, 3, 2>,
    BLUE,
    true,
    BEAM_CORONA,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_HIBERNATION,
    "",
    100, // power is capped to 50 in spl-zap.cc:spell_zap_power.
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_HIBERNATION,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_FLAME_TONGUE,
    "flame",
    25,
    new dicedef_calculator<1, 8, 1, 4>,
    new tohit_calculator<11, 1, 6>,
    RED,
    false,
    BEAM_FIRE,
    DCHAR_FIRED_BOLT,
    true,
    false,
    false,
    1
},

{
    ZAP_LARGE_SANDBLAST,
    "large rocky blast",
    50,
    new dicedef_calculator<3, 4, 1, 3>,
    new tohit_calculator<13, 1, 10>,
    BROWN,
    false,
    BEAM_FRAG,
    DCHAR_FIRED_BOLT,
    true,
    false,
    false,
    3
},

{
    ZAP_SANDBLAST,
    "rocky blast",
    50,
    new dicedef_calculator<2, 4, 1, 3>,
    new tohit_calculator<13, 1, 10>,
    BROWN,
    false,
    BEAM_FRAG,
    DCHAR_FIRED_BOLT,
    true,
    false,
    false,
    2 // XXX: Sound 2 for level one spell?
},

{
    ZAP_SMALL_SANDBLAST,
    "blast of sand",
    25,
    new dicedef_calculator<1, 8, 1, 4>,
    new tohit_calculator<8, 1, 5>,
    BROWN,
    false,
    BEAM_FRAG,
    DCHAR_FIRED_BOLT,
    true,
    false,
    false,
    1
},

{
    ZAP_BOLT_OF_MAGMA,
    "bolt of magma",
    200,
    new calcdice_calculator<4, 16, 2, 3>,
    new tohit_calculator<8, 1, 25>,
    RED,
    false,
    BEAM_LAVA,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    5
},

{
    ZAP_POISON_ARROW,
    "poison arrow",
    200,
    new calcdice_calculator<4, 15, 1, 1>,
    new tohit_calculator<5, 1, 10>,
    LIGHTGREEN,
    false,
    BEAM_POISON_ARROW,             // extra damage
    DCHAR_FIRED_MISSILE,
    true,
    false,
    false,
    6 // XXX: Less noise because it's poison?
},

{
    ZAP_PETRIFY,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_PETRIFY,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_PORKALATOR,
    "porkalator",
    100,
    NULL,
    NULL,
    RED,
    true,
    BEAM_PORKALATOR,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_SLEEP,
    "",
    100,
    NULL,
    NULL,
    BLACK,
    true,
    BEAM_SLEEP,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_IOOD,
    "",
    200,
    NULL,
    new tohit_calculator<AUTOMATIC_HIT>,
    WHITE,
    false,
    BEAM_NUKE,
    NUM_DCHAR_TYPES, // no dchar, to get bolt.glyph == 0,
                     // hence invisible
    true,
    true,
    false,
    0
},

{
    ZAP_HOLY_LIGHT,
    "beam of light",
    200,
    new calcdice_calculator<3, 11, 1, 3>,
    new tohit_calculator<10, 1, 25>,
    ETC_HOLY,
    false,
    BEAM_HOLY_LIGHT,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    6
},

{
    ZAP_BREATHE_STICKY_FLAME,
    "splash of liquid fire",
    100,
    new dicedef_calculator<2, 3, 1, 12>,
    new tohit_calculator<11, 1, 10>,
    RED,
    false,
    BEAM_FIRE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    1
},

{
    ZAP_BREATHE_MEPHITIC,
    "blast of choking fumes",
    50,
    new dicedef_calculator<3, 2, 1, 6>,
    new tohit_calculator<6, 1, 6>,
    GREEN,
    false,
    BEAM_MEPHITIC,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    0
},

{
    ZAP_INNER_FLAME,
    "",
    100,
    NULL,
    new tohit_calculator<0, 3, 1>,
    BLACK,
    true,
    BEAM_INNER_FLAME,
    NUM_DCHAR_TYPES,
    false,
    false,
    false,
    0
},

{
    ZAP_DAZZLING_SPRAY,
    "spray of energy",
    50,
    new calcdice_calculator<2, 8, 1, 4>,
    new tohit_calculator<9, 1, 7>,
    LIGHTMAGENTA,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    3
},

{
    ZAP_FORCE_LANCE,
    "lance of force",
    100,
    new dicedef_calculator<2, 6, 1, 6>,
    new tohit_calculator<10, 1, 9>,
    CYAN,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_MISSILE,
    true,
    false,
    false,
    5
},

{
    ZAP_SEARING_RAY_I,
    "searing ray",
    50,
    new dicedef_calculator<2, 3, 1, 13>,
    new tohit_calculator<10, 1, 9>,
    MAGENTA,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    2
},

{
    ZAP_SEARING_RAY_II,
    "searing ray",
    50,
    new dicedef_calculator<3, 3, 1, 12>,
    new tohit_calculator<11, 1, 8>,
    LIGHTMAGENTA,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_ZAP,
    true,
    false,
    false,
    2
},

{
    ZAP_SEARING_RAY_III,
    "searing ray",
    50,
    new dicedef_calculator<4, 3, 1, 12>,
    new tohit_calculator<12, 1, 7>,
    WHITE,
    false,
    BEAM_MMISSILE,
    DCHAR_FIRED_ZAP,
    true,
    true,
    false,
    2
},
