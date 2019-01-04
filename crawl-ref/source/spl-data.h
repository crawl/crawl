/**
 * @file
 * @brief Spell definitions and descriptions. See spell_desc struct in
 *        spl-util.cc.
 * Flag descriptions are in spl-cast.h.
**/

/*
struct spell_desc
{
    enum, spell name,
    spell schools,
    flags,
    level,
    power_cap,
    min_range, max_range, (-1 if not applicable)
    noise, effect_noise
    tile
}
*/

static const struct spell_desc spelldata[] =
{

{
    SPELL_TELEPORT_SELF, "Teleport Self",
    SPTYP_TRANSLOCATION,
    spflag::escape | spflag::emergency | spflag::utility | spflag::monster,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_TELEPORT,
},

{
    SPELL_CAUSE_FEAR, "Cause Fear",
    SPTYP_HEXES,
    spflag::area | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_CAUSE_FEAR,
},

{
    SPELL_MAGIC_DART, "Magic Dart",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::needs_tracer,
    1,
    25,
    LOS_RADIUS, LOS_RADIUS,
    1, 0,
    TILEG_MAGIC_DART,
},

{
    SPELL_FIREBALL, "Fireball",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    5, 5,
    5, 0,
    TILEG_FIREBALL,
},

{
    SPELL_APPORTATION, "Apportation",
    SPTYP_TRANSLOCATION,
    spflag::target | spflag::obj | spflag::not_self,
    1,
    50,
    LOS_RADIUS, LOS_RADIUS,
    1, 0,
    TILEG_APPORTATION,
},
#if TAG_MAJOR_VERSION == 34
{
    SPELL_DELAYED_FIREBALL, "Delayed Fireball",
    SPTYP_FIRE | SPTYP_CONJURATION,
    spflag::utility,
    7,
    0,
    -1, -1,
    7, 0,
    TILEG_ERROR,
},
#endif
{
    SPELL_CONJURE_FLAME, "Conjure Flame",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::target | spflag::neutral | spflag::not_self,
    3,
    100,
    3, 3,
    3, 2,
    TILEG_CONJURE_FLAME,
},

{
    SPELL_DIG, "Dig",
    SPTYP_EARTH,
    spflag::dir_or_target | spflag::not_self | spflag::neutral
        | spflag::utility,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_DIG,
},

{
    SPELL_BOLT_OF_FIRE, "Bolt of Fire",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    6, 6,
    6, 0,
    TILEG_BOLT_OF_FIRE,
},

{
    SPELL_BOLT_OF_COLD, "Bolt of Cold",
    SPTYP_CONJURATION | SPTYP_ICE,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    5, 5,
    6, 0,
    TILEG_BOLT_OF_COLD,
},

{
    SPELL_LIGHTNING_BOLT, "Lightning Bolt",
    SPTYP_CONJURATION | SPTYP_AIR,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    4, 11, // capped at LOS, yet this 11 matters since range increases linearly
    5, 25,
    TILEG_LIGHTNING_BOLT,
},

{
    SPELL_BLINKBOLT, "Blinkbolt",
    SPTYP_AIR | SPTYP_TRANSLOCATION,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    200,
    4, 11,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BOLT_OF_MAGMA, "Bolt of Magma",
    SPTYP_CONJURATION | SPTYP_FIRE | SPTYP_EARTH,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    4, 4,
    5, 0,
    TILEG_BOLT_OF_MAGMA,
},

{
    SPELL_POLYMORPH, "Polymorph",
    SPTYP_TRANSMUTATION | SPTYP_HEXES,
    spflag::dir_or_target | spflag::chaotic | spflag::monster
        | spflag::needs_tracer | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_POLYMORPH,
},

{
    SPELL_SLOW, "Slow",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    2,
    200,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_SLOW,
},

{
    SPELL_HASTE, "Haste",
    SPTYP_CHARMS,
    spflag::helpful | spflag::hasty | spflag::selfench | spflag::utility,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_HASTE,
},

{
    SPELL_PETRIFY, "Petrify",
    SPTYP_TRANSMUTATION | SPTYP_EARTH,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_PETRIFY,
},

{
    SPELL_CONFUSE, "Confuse",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_CONFUSE,
},

{
    SPELL_INVISIBILITY, "Invisibility",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::helpful | spflag::selfench
        | spflag::emergency | spflag::needs_tracer,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_INVISIBILITY,
},

{
    SPELL_THROW_FLAME, "Throw Flame",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::dir_or_target | spflag::needs_tracer,
    2,
    50,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_THROW_FLAME,
},

{
    SPELL_THROW_FROST, "Throw Frost",
    SPTYP_CONJURATION | SPTYP_ICE,
    spflag::dir_or_target | spflag::needs_tracer,
    2,
    50,
    6, 6,
    2, 0,
    TILEG_THROW_FROST,
},

{
    SPELL_CONTROLLED_BLINK, "Controlled Blink",
    SPTYP_TRANSLOCATION,
    spflag::escape | spflag::emergency | spflag::utility,
    8,
    0,
    -1, -1,
    2, 0, // Not noisier than Blink, to keep this spell relevant
          // for stabbers. [rob]
    TILEG_CONTROLLED_BLINK,
},

{
    SPELL_DISJUNCTION, "Disjunction",
    SPTYP_TRANSLOCATION,
    spflag::escape | spflag::utility,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_DISJUNCTION,
},

{
    SPELL_FREEZING_CLOUD, "Freezing Cloud",
    SPTYP_CONJURATION | SPTYP_ICE | SPTYP_AIR,
    spflag::target | spflag::area | spflag::needs_tracer
        | spflag::cloud,
    6,
    200,
    5, 5,
    6, 2,
    TILEG_FREEZING_CLOUD,
},

{
    SPELL_MEPHITIC_CLOUD, "Mephitic Cloud",
    SPTYP_CONJURATION | SPTYP_POISON | SPTYP_AIR,
    spflag::dir_or_target | spflag::area
        | spflag::needs_tracer | spflag::cloud,
    3,
    100,
    4, 4,
    3, 0,
    TILEG_MEPHITIC_CLOUD,
},

{
    SPELL_RING_OF_FLAMES, "Ring of Flames",
    SPTYP_CHARMS | SPTYP_FIRE,
    spflag::area,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_RING_OF_FLAMES,
},

{
    SPELL_RING_OF_THUNDER, "Ring of Thunder",
    SPTYP_CHARMS | SPTYP_AIR,
    spflag::area,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_VENOM_BOLT, "Venom Bolt",
    SPTYP_CONJURATION | SPTYP_POISON,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    5, 5,
    5, 0,
    TILEG_VENOM_BOLT,
},

{
    SPELL_OLGREBS_TOXIC_RADIANCE, "Olgreb's Toxic Radiance",
    SPTYP_POISON,
    spflag::area,
    4,
    100,
    -1, -1,
    2, 0,
    TILEG_OLGREBS_TOXIC_RADIANCE,
},

{
    SPELL_TELEPORT_OTHER, "Teleport Other",
    SPTYP_TRANSLOCATION,
    spflag::dir_or_target | spflag::not_self | spflag::escape
        | spflag::emergency | spflag::needs_tracer | spflag::MR_check,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_TELEPORT_OTHER,
},

{
    SPELL_DEATHS_DOOR, "Death's Door",
    SPTYP_CHARMS | SPTYP_NECROMANCY,
    spflag::emergency | spflag::utility | spflag::no_ghost,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_DEATHS_DOOR,
},

{
    SPELL_MASS_CONFUSION, "Mass Confusion",
    SPTYP_HEXES,
    spflag::area | spflag::MR_check,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_MASS_CONFUSION,
},

{
    SPELL_SMITING, "Smiting",
    SPTYP_NONE,
    spflag::target | spflag::not_self,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_SMITING,
},

{
    SPELL_SUMMON_SMALL_MAMMAL, "Summon Small Mammal",
    SPTYP_SUMMONING,
    spflag::none,
    1,
    25,
    -1, -1,
    1, 0,
    TILEG_SUMMON_SMALL_MAMMAL,
},

// Used indirectly, by monsters abjuring via other summon spells.
{
    SPELL_ABJURATION, "Abjuration",
    SPTYP_SUMMONING,
    spflag::escape | spflag::needs_tracer | spflag::monster,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_MASS_ABJURATION,
},

{
    SPELL_AURA_OF_ABJURATION, "Aura of Abjuration",
    SPTYP_SUMMONING,
    spflag::area | spflag::neutral | spflag::escape,
    5,
    200,
    -1, -1,
    5, 0,
    TILEG_MASS_ABJURATION,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SUMMON_SCORPIONS, "Summon Scorpions",
    SPTYP_SUMMONING | SPTYP_POISON,
    spflag::none,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_BOLT_OF_DRAINING, "Bolt of Draining",
    SPTYP_CONJURATION | SPTYP_NECROMANCY,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    5, 5,
    2, 0, //the beam is silent
    TILEG_BOLT_OF_DRAINING,
},

{
    SPELL_LEHUDIBS_CRYSTAL_SPEAR, "Lehudib's Crystal Spear",
    SPTYP_CONJURATION | SPTYP_EARTH,
    spflag::dir_or_target | spflag::needs_tracer,
    8,
    200,
    3, 3,
    8, 0,
    TILEG_LEHUDIBS_CRYSTAL_SPEAR,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_BOLT_OF_INACCURACY, "Bolt of Inaccuracy",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::needs_tracer, // rod/tome of destruction
    3,
    1000,
    6, 6,
    3, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_TORNADO, "Tornado",
    SPTYP_AIR,
    spflag::area,
    9,
    200,
    TORNADO_RADIUS, TORNADO_RADIUS,
    5, 0,
    TILEG_TORNADO,
},

{
    SPELL_POISONOUS_CLOUD, "Poisonous Cloud",
    SPTYP_CONJURATION | SPTYP_POISON | SPTYP_AIR,
    spflag::target | spflag::area | spflag::needs_tracer | spflag::cloud,
    5,
    200,
    5, 5,
    6, 2,
    TILEG_POISONOUS_CLOUD,
},

{
    SPELL_FIRE_STORM, "Fire Storm",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::target | spflag::area | spflag::needs_tracer,
    9,
    200,
    5, 5,
    9, 0,
    TILEG_FIRE_STORM,
},

{
    SPELL_CALL_DOWN_DAMNATION, "Call Down Damnation",
    SPTYP_CONJURATION,
    spflag::target | spflag::area | spflag::unholy | spflag::needs_tracer,
    9,
    200,
    LOS_RADIUS, LOS_RADIUS,
    9, 0,
    TILEG_CALL_DOWN_DAMNATION,
},

{
    SPELL_BLINK, "Blink",
    SPTYP_TRANSLOCATION,
    spflag::escape | spflag::selfench | spflag::emergency | spflag::utility,
    2,
    0,
    -1, -1,
    2, 0,
    TILEG_BLINK,
},

{
    SPELL_BLINK_RANGE, "Blink Range", // XXX needs better name
    SPTYP_TRANSLOCATION,
    spflag::escape | spflag::monster | spflag::selfench | spflag::emergency,
    2,
    0,
    -1, -1,
    2, 0,
    TILEG_BLINK,
},

{
    SPELL_BLINK_AWAY, "Blink Away",
    SPTYP_TRANSLOCATION,
    spflag::escape | spflag::monster | spflag::emergency | spflag::selfench,
    2,
    0,
    -1, -1,
    2, 0,
    TILEG_BLINK,
},

{
    SPELL_BLINK_CLOSE, "Blink Close",
    SPTYP_TRANSLOCATION,
    spflag::monster,
    2,
    0,
    -1, -1,
    2, 0,
    TILEG_BLINK,
},

// The following name was found in the hack.exe file of an early version
// of PCHACK - credit goes to its creator (whoever that may be):
{
    SPELL_ISKENDERUNS_MYSTIC_BLAST, "Iskenderun's Mystic Blast",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    100,
    6, 6,
    4, 10,
    TILEG_ISKENDERUNS_MYSTIC_BLAST,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SUMMON_SWARM, "Summon Swarm",
    SPTYP_SUMMONING,
    spflag::none,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_SUMMON_HORRIBLE_THINGS, "Summon Horrible Things",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::chaotic | spflag::mons_abjure,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_SUMMON_HORRIBLE_THINGS,
},

{
    SPELL_MALIGN_GATEWAY, "Malign Gateway",
    SPTYP_SUMMONING | SPTYP_TRANSLOCATION,
    spflag::unholy | spflag::chaotic,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_MALIGN_GATEWAY,
},

{
    SPELL_ENSLAVEMENT, "Enslavement",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::monster | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_ENSLAVEMENT,
},

{
    SPELL_ANIMATE_DEAD, "Animate Dead",
    SPTYP_NECROMANCY,
    spflag::area | spflag::neutral | spflag::corpse_violating | spflag::utility,
    4,
    0,
    -1, -1,
    3, 0,
    TILEG_ANIMATE_DEAD,
},

{
    SPELL_PAIN, "Pain",
    SPTYP_NECROMANCY,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    1,
    25,
    5, 5,
    1, 0,
    TILEG_PAIN,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CONTROL_UNDEAD, "Control Undead",
    SPTYP_NECROMANCY,
    spflag::MR_check,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_ANIMATE_SKELETON, "Animate Skeleton",
    SPTYP_NECROMANCY,
    spflag::corpse_violating | spflag::utility,
    1,
    0,
    -1, -1,
    1, 0,
    TILEG_ANIMATE_SKELETON,
},

{
    SPELL_VAMPIRIC_DRAINING, "Vampiric Draining",
    SPTYP_NECROMANCY,
    spflag::dir_or_target | spflag::not_self | spflag::emergency
        | spflag::selfench,
    3,
    200,
    1, 1,
    3, 0,
    TILEG_VAMPIRIC_DRAINING,
},

{
    SPELL_HAUNT, "Haunt",
    SPTYP_SUMMONING | SPTYP_NECROMANCY,
    spflag::target | spflag::not_self | spflag::mons_abjure,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_HAUNT,
},

{
    SPELL_BORGNJORS_REVIVIFICATION, "Borgnjor's Revivification",
    SPTYP_NECROMANCY,
    spflag::utility,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_BORGNJORS_REVIVIFICATION,
},

{
    SPELL_FREEZE, "Freeze",
    SPTYP_ICE,
    spflag::dir_or_target | spflag::not_self,
    1,
    25,
    1, 1,
    1, 0,
    TILEG_FREEZE,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SUMMON_ELEMENTAL, "Summon Elemental",
    SPTYP_SUMMONING,
    spflag::none,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_OZOCUBUS_REFRIGERATION, "Ozocubu's Refrigeration",
    SPTYP_ICE,
    spflag::area,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_OZOCUBUS_REFRIGERATION,
},

{
    SPELL_STICKY_FLAME, "Sticky Flame",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    100,
    1, 1,
    4, 0,
    TILEG_STICKY_FLAME,
},

{
    SPELL_SUMMON_ICE_BEAST, "Summon Ice Beast",
    SPTYP_ICE | SPTYP_SUMMONING,
    spflag::none,
    4,
    100,
    -1, -1,
    3, 0,
    TILEG_SUMMON_ICE_BEAST,
},

{
    SPELL_OZOCUBUS_ARMOUR, "Ozocubu's Armour",
    SPTYP_CHARMS | SPTYP_ICE,
    spflag::no_ghost,
    3,
    100,
    -1, -1,
    3, 0,
    TILEG_OZOCUBUS_ARMOUR,
},

{
    SPELL_CALL_IMP, "Call Imp",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::selfench,
    2,
    100,
    -1, -1,
    2, 0,
    TILEG_CALL_IMP,
},

{
    SPELL_REPEL_MISSILES, "Repel Missiles",
    SPTYP_CHARMS | SPTYP_AIR,
    spflag::monster,
    2,
    50,
    -1, -1,
    1, 0,
    TILEG_REPEL_MISSILES,
},

{
    SPELL_BERSERKER_RAGE, "Berserker Rage",
    SPTYP_CHARMS,
    spflag::hasty | spflag::monster,
    3,
    0,
    -1, -1,
    3, 0,
    TILEG_BERSERKER_RAGE,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_FRENZY, "Frenzy",
    SPTYP_CHARMS,
    spflag::hasty | spflag::monster,
    3,
    0,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_DISPEL_UNDEAD, "Dispel Undead",
    SPTYP_NECROMANCY,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    100,
    4, 4,
    4, 0,
    TILEG_DISPEL_UNDEAD,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_FULSOME_DISTILLATION, "Fulsome Distillation",
    SPTYP_TRANSMUTATION | SPTYP_NECROMANCY,
    spflag::corpse_violating,
    1,
    0,
    -1, -1,
    1, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_POISON_ARROW, "Poison Arrow",
    SPTYP_CONJURATION | SPTYP_POISON,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    6, 6,
    6, 0,
    TILEG_POISON_ARROW,
},

{
    SPELL_TWISTED_RESURRECTION, "Twisted Resurrection",
    SPTYP_NECROMANCY,
    spflag::chaotic | spflag::corpse_violating | spflag::utility
        | spflag::monster,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_TWISTED_RESURRECTION,
},

{
    SPELL_REGENERATION, "Regeneration",
    SPTYP_CHARMS | SPTYP_NECROMANCY,
    spflag::selfench | spflag::utility,
    3,
    200,
    -1, -1,
    3, 0,
    TILEG_REGENERATION,
},

// Monster-only, players can use Lugonu's ability
{
    SPELL_BANISHMENT, "Banishment",
    SPTYP_TRANSLOCATION,
    spflag::dir_or_target | spflag::unholy | spflag::chaotic | spflag::monster
        | spflag::emergency | spflag::needs_tracer | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_BANISHMENT,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CIGOTUVIS_DEGENERATION, "Cigotuvi's Degeneration",
    SPTYP_TRANSMUTATION | SPTYP_NECROMANCY,
    spflag::dir_or_target | spflag::not_self | spflag::chaotic,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_STING, "Sting",
    SPTYP_CONJURATION | SPTYP_POISON,
    spflag::dir_or_target | spflag::needs_tracer,
    1,
    25,
    6, 6,
    1, 0,
    TILEG_STING,
},

{
    SPELL_SUBLIMATION_OF_BLOOD, "Sublimation of Blood",
    SPTYP_NECROMANCY,
    spflag::utility,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_SUBLIMATION_OF_BLOOD,
},

{
    SPELL_TUKIMAS_DANCE, "Tukima's Dance",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check
        | spflag::not_self,
    3,
    100,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_TUKIMAS_DANCE,
},

{
    SPELL_SUMMON_DEMON, "Summon Demon",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::selfench | spflag::mons_abjure,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_SUMMON_DEMON,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_DEMONIC_HORDE, "Demonic Horde",
    SPTYP_SUMMONING,
    spflag::unholy,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_SUMMON_GREATER_DEMON, "Summon Greater Demon",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::selfench | spflag::mons_abjure,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_SUMMON_GREATER_DEMON,
},

{
    SPELL_CORPSE_ROT, "Corpse Rot",
    SPTYP_NECROMANCY,
    spflag::area | spflag::neutral | spflag::unclean,
    2,
    0,
    -1, -1,
    2, 0,
    TILEG_CORPSE_ROT,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_FIRE_BRAND, "Fire Brand",
    SPTYP_CHARMS | SPTYP_FIRE,
    spflag::helpful,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_ERROR,
},

{
    SPELL_FREEZING_AURA, "Freezing Aura",
    SPTYP_CHARMS | SPTYP_ICE,
    spflag::helpful,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_ERROR,
},

{
    SPELL_LETHAL_INFUSION, "Lethal Infusion",
    SPTYP_CHARMS | SPTYP_NECROMANCY,
    spflag::helpful,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_ERROR,
},

#endif
{
    SPELL_IRON_SHOT, "Iron Shot",
    SPTYP_CONJURATION | SPTYP_EARTH,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    4, 4,
    6, 0,
    TILEG_IRON_SHOT,
},

{
    SPELL_STONE_ARROW, "Stone Arrow",
    SPTYP_CONJURATION | SPTYP_EARTH,
    spflag::dir_or_target | spflag::needs_tracer,
    3,
    50,
    4, 4,
    3, 0,
    TILEG_STONE_ARROW,
},

{
    SPELL_SHOCK, "Shock",
    SPTYP_CONJURATION | SPTYP_AIR,
    spflag::dir_or_target | spflag::needs_tracer,
    1,
    25,
    LOS_RADIUS, LOS_RADIUS,
    1, 0,
    TILEG_SHOCK,
},

{
    SPELL_SWIFTNESS, "Swiftness",
    SPTYP_CHARMS | SPTYP_AIR,
    spflag::hasty | spflag::selfench | spflag::utility,
    2,
    100,
    -1, -1,
    2, 0,
    TILEG_SWIFTNESS,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_FLY, "Flight",
    SPTYP_CHARMS | SPTYP_AIR,
    spflag::utility,
    3,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},

{
    SPELL_INSULATION, "Insulation",
    SPTYP_CHARMS | SPTYP_AIR,
    spflag::none,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},
#endif

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CURE_POISON, "Cure Poison",
    SPTYP_POISON,
    spflag::recovery | spflag::helpful | spflag::utility,
    2,
    200,
    -1, -1,
    1, 0,
    TILEG_ERROR,
},
#endif

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CONTROL_TELEPORT, "Control Teleport",
    SPTYP_CHARMS | SPTYP_TRANSLOCATION,
    spflag::helpful | spflag::utility,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},

{
    SPELL_POISON_WEAPON, "Poison Weapon",
    SPTYP_CHARMS | SPTYP_POISON,
    spflag::helpful,
    3,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},

#endif
{
    SPELL_DEBUGGING_RAY, "Debugging Ray",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::testing,
    7,
    100,
    LOS_RADIUS, LOS_RADIUS,
    7, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_RECALL, "Recall",
    SPTYP_SUMMONING | SPTYP_TRANSLOCATION,
    spflag::utility,
    3,
    0,
    -1, -1,
    3, 0,
    TILEG_RECALL,
},

{
    SPELL_AGONY, "Agony",
    SPTYP_NECROMANCY,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_AGONY,
},

{
    SPELL_SPIDER_FORM, "Spider Form",
    SPTYP_TRANSMUTATION | SPTYP_POISON,
    spflag::helpful | spflag::chaotic | spflag::utility,
    3,
    200,
    -1, -1,
    2, 0,
    TILEG_SPIDER_FORM,
},

{
    SPELL_DISINTEGRATE, "Disintegrate",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::MR_check,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BLADE_HANDS, "Blade Hands",
    SPTYP_TRANSMUTATION,
    spflag::helpful | spflag::chaotic | spflag::utility,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_BLADE_HANDS,
},

{
    SPELL_STATUE_FORM, "Statue Form",
    SPTYP_TRANSMUTATION | SPTYP_EARTH,
    spflag::helpful | spflag::chaotic | spflag::utility,
    6,
    150,
    -1, -1,
    5, 0,
    TILEG_STATUE_FORM,
},

{
    SPELL_ICE_FORM, "Ice Form",
    SPTYP_ICE | SPTYP_TRANSMUTATION,
    spflag::helpful | spflag::chaotic | spflag::utility,
    4,
    100,
    -1, -1,
    3, 0,
    TILEG_ICE_FORM,
},

{
    SPELL_DRAGON_FORM, "Dragon Form",
    SPTYP_TRANSMUTATION,
    spflag::helpful | spflag::chaotic | spflag::utility,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_DRAGON_FORM,
},

{
    SPELL_HYDRA_FORM, "Hydra Form",
    SPTYP_TRANSMUTATION,
    spflag::helpful | spflag::chaotic | spflag::utility,
    6,
    200,
    -1, -1,
    6, 0,
    TILEG_HYDRA_FORM,
},

{
    SPELL_NECROMUTATION, "Necromutation",
    SPTYP_TRANSMUTATION | SPTYP_NECROMANCY,
    spflag::helpful | spflag::corpse_violating | spflag::chaotic,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_NECROMUTATION,
},

{
    SPELL_DEATH_CHANNEL, "Death Channel",
    SPTYP_NECROMANCY,
    spflag::helpful | spflag::utility,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_DEATH_CHANNEL,
},

// Monster-only, players can use Kiku's ability
{
    SPELL_SYMBOL_OF_TORMENT, "Symbol of Torment",
    SPTYP_NECROMANCY,
    spflag::area | spflag::monster,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_SYMBOL_OF_TORMENT,
},

{
    SPELL_DEFLECT_MISSILES, "Deflect Missiles",
    SPTYP_CHARMS | SPTYP_AIR,
    spflag::helpful | spflag::utility,
    6,
    200,
    -1, -1,
    3, 0,
    TILEG_DEFLECT_MISSILES,
},

{
    SPELL_THROW_ICICLE, "Throw Icicle",
    SPTYP_CONJURATION | SPTYP_ICE,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    100,
    5, 5,
    4, 0,
    TILEG_THROW_ICICLE,
},

{
    SPELL_AIRSTRIKE, "Airstrike",
    SPTYP_AIR,
    spflag::target | spflag::not_self,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    2, 4,
    TILEG_AIRSTRIKE,
},

{
    SPELL_SHADOW_CREATURES, "Shadow Creatures",
    SPTYP_SUMMONING,
    spflag::mons_abjure,
    6,
    0,
    -1, -1,
    4, 0,
    TILEG_SUMMON_SHADOW_CREATURES,
},

{
    SPELL_CONFUSING_TOUCH, "Confusing Touch",
    SPTYP_HEXES,
    spflag::none,
    1,
    50,
    -1, -1,
    2, 0,
    TILEG_CONFUSING_TOUCH,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SURE_BLADE, "Sure Blade",
    SPTYP_HEXES | SPTYP_CHARMS,
    spflag::helpful | spflag::utility,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_ERROR,
},
#endif
{
    SPELL_FLAME_TONGUE, "Flame Tongue",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer,
    1,
    40, // cap for range; damage cap is at 25
    2, 5,
    1, 0,
    TILEG_FLAME_TONGUE,
},

{
    SPELL_PASSWALL, "Passwall",
    SPTYP_TRANSMUTATION | SPTYP_EARTH,
    spflag::target | spflag::escape | spflag::not_self | spflag::utility,
    2,
    120,
    1, 7,
    0, 0, // make silent to keep passwall a viable stabbing spell [rob]
    TILEG_PASSWALL,
},

{
    SPELL_IGNITE_POISON, "Ignite Poison",
    SPTYP_FIRE | SPTYP_TRANSMUTATION | SPTYP_POISON,
    spflag::area,
    3,
    100,
    -1, -1,
    4, 0,
    TILEG_IGNITE_POISON,
},

{
    SPELL_STICKS_TO_SNAKES, "Sticks to Snakes",
    SPTYP_TRANSMUTATION,
    spflag::no_ghost,
    2,
    100,
    -1, -1,
    2, 0,
    TILEG_STICKS_TO_SNAKES,
},

{
    SPELL_CALL_CANINE_FAMILIAR, "Call Canine Familiar",
    SPTYP_SUMMONING,
    spflag::none,
    3,
    100,
    -1, -1,
    3, 0,
    TILEG_CALL_CANINE_FAMILIAR,
},

{
    SPELL_SUMMON_DRAGON, "Summon Dragon",
    SPTYP_SUMMONING,
    spflag::mons_abjure,
    9,
    200,
    -1, -1,
    7, 0,
    TILEG_SUMMON_DRAGON,
},

{
    SPELL_HIBERNATION, "Ensorcelled Hibernation",
    SPTYP_HEXES | SPTYP_ICE,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::MR_check,
    2,
    56,
    LOS_RADIUS, LOS_RADIUS,
    0, 0, //putting a monster to sleep should be silent
    TILEG_ENSORCELLED_HIBERNATION,
},

{
    SPELL_ENGLACIATION, "Metabolic Englaciation",
    SPTYP_HEXES | SPTYP_ICE,
    spflag::area,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_METABOLIC_ENGLACIATION,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SEE_INVISIBLE, "See Invisible",
    SPTYP_CHARMS,
    spflag::helpful,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},

{
    SPELL_PHASE_SHIFT, "Phase Shift",
    SPTYP_TRANSLOCATION,
    spflag::helpful | spflag::utility,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_SUMMON_BUTTERFLIES, "Summon Butterflies",
    SPTYP_SUMMONING,
    spflag::none,
    1,
    100,
    -1, -1,
    1, 0,
    TILEG_SUMMON_BUTTERFLIES,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_WARP_BRAND, "Warp Weapon",
    SPTYP_CHARMS | SPTYP_TRANSLOCATION,
    spflag::helpful | spflag::utility,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_SILENCE, "Silence",
    SPTYP_HEXES | SPTYP_AIR,
    spflag::area,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_SILENCE,
},

{
    SPELL_SHATTER, "Shatter",
    SPTYP_EARTH,
    spflag::area,
    9,
    200,
    -1, -1,
    7, 30,
    TILEG_SHATTER,
},

{
    SPELL_DISPERSAL, "Dispersal",
    SPTYP_TRANSLOCATION,
    spflag::area | spflag::escape,
    6,
    200,
    1, 4,
    5, 0,
    TILEG_DISPERSAL,
},

{
    SPELL_DISCHARGE, "Static Discharge",
    SPTYP_CONJURATION | SPTYP_AIR,
    spflag::area,
    3,
    100,
    1, 1,
    3, 0,
    TILEG_STATIC_DISCHARGE,
},

{
    SPELL_CORONA, "Corona",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::MR_check,
    1,
    200,
    LOS_RADIUS, LOS_RADIUS,
    1, 0,
    TILEG_CORONA,
},

{
    SPELL_INTOXICATE, "Alistair's Intoxication",
    SPTYP_TRANSMUTATION | SPTYP_POISON,
    spflag::none,
    5,
    100,
    -1, -1,
    3, 0,
    TILEG_ALISTAIRS_INTOXICATION,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_EVAPORATE, "Evaporate",
    SPTYP_FIRE | SPTYP_TRANSMUTATION,
    spflag::dir_or_target | spflag::area,
    2,
    50,
    5, 5,
    2, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_LRD, "Lee's Rapid Deconstruction",
    SPTYP_EARTH,
    spflag::target,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_LEES_RAPID_DECONSTRUCTION,
},

{
    SPELL_SANDBLAST, "Sandblast",
    SPTYP_EARTH,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer,
    1,
    50,
    3, 3,
    1, 0,
    TILEG_SANDBLAST,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CONDENSATION_SHIELD, "Condensation Shield",
    SPTYP_ICE,
    spflag::helpful | spflag::utility,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},

{
    SPELL_STONESKIN, "Stoneskin",
    SPTYP_EARTH | SPTYP_TRANSMUTATION,
    spflag::helpful | spflag::utility | spflag::no_ghost | spflag::monster,
    2,
    100,
    -1, -1,
    2, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_SIMULACRUM, "Simulacrum",
    SPTYP_ICE | SPTYP_NECROMANCY,
    spflag::corpse_violating,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_SIMULACRUM,
},

{
    SPELL_CONJURE_BALL_LIGHTNING, "Conjure Ball Lightning",
    SPTYP_AIR | SPTYP_CONJURATION,
    spflag::selfench,
    6,
    200,
    -1, -1,
    6, 0,
    TILEG_CONJURE_BALL_LIGHTNING,
},

{
    SPELL_CHAIN_LIGHTNING, "Chain Lightning",
    SPTYP_AIR | SPTYP_CONJURATION,
    spflag::area,
    8,
    200,
    -1, -1,
    8, 25,
    TILEG_CHAIN_LIGHTNING,
},

{
    SPELL_EXCRUCIATING_WOUNDS, "Excruciating Wounds",
    SPTYP_CHARMS | SPTYP_NECROMANCY,
    spflag::helpful,
    5,
    200,
    -1, -1,
    4, 15,
    TILEG_EXCRUCIATING_WOUNDS,
},

{
    SPELL_PORTAL_PROJECTILE, "Portal Projectile",
    SPTYP_TRANSLOCATION | SPTYP_HEXES,
    spflag::none,
    3,
    50,
    -1, -1,
    3, 0,
    TILEG_PORTAL_PROJECTILE,
},

{
    SPELL_MONSTROUS_MENAGERIE, "Monstrous Menagerie",
    SPTYP_SUMMONING,
    spflag::mons_abjure,
    7,
    200,
    -1, -1,
    5, 0,
    TILEG_MONSTROUS_MENAGERIE,
},

{
    SPELL_GOLUBRIAS_PASSAGE, "Passage of Golubria",
    SPTYP_TRANSLOCATION,
    spflag::target | spflag::neutral | spflag::escape | spflag::selfench,
    4,
    0,
    LOS_RADIUS, LOS_RADIUS,
    3, 8, // when it closes
    TILEG_PASSAGE_OF_GOLUBRIA,
},

{
    SPELL_FULMINANT_PRISM, "Fulminant Prism",
    SPTYP_CONJURATION | SPTYP_HEXES,
    spflag::target | spflag::area | spflag::not_self,
    4,
    200,
    4, 4,
    4, 0,
    TILEG_FULMINANT_PRISM,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SINGULARITY, "Singularity",
    SPTYP_TRANSLOCATION,
    spflag::target | spflag::area | spflag::not_self | spflag::monster,
    9,
    200,
    LOS_RADIUS, LOS_RADIUS,
    20, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_PARALYSE, "Paralyse",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer
        | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_PARALYSE,
},

{
    SPELL_MINOR_HEALING, "Minor Healing",
    SPTYP_NECROMANCY,
    spflag::recovery | spflag::helpful | spflag::monster | spflag::selfench
        | spflag::emergency | spflag::utility | spflag::not_evil,
    2,
    0,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_MINOR_HEALING,
},

{
    SPELL_MAJOR_HEALING, "Major Healing",
    SPTYP_NECROMANCY,
    spflag::recovery | spflag::helpful | spflag::monster | spflag::selfench
        | spflag::emergency | spflag::utility | spflag::not_evil,
    6,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_MAJOR_HEALING,
},

{
    SPELL_HURL_DAMNATION, "Hurl Damnation",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::unholy | spflag::monster
        | spflag::needs_tracer,
    // plus DS ability, staff of Dispater & Sceptre of Asmodeus
    9,
    200,
    6, 6,
    9, 0,
    TILEG_HURL_DAMNATION,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_VAMPIRE_SUMMON, "Vampire Summon",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::monster,
    3,
    0,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_BRAIN_FEED, "Brain Feed",
    SPTYP_NECROMANCY,
    spflag::target | spflag::monster,
    3,
    0,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_BRAIN_FEED,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_FAKE_RAKSHASA_SUMMON, "Rakshasa Summon",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::monster,
    3,
    0,
    -1, -1,
    3, 0,
    TILEG_FAKE_RAKSHASA_SUMMON,
},
#endif

{
    SPELL_NOXIOUS_CLOUD, "Noxious Cloud",
    SPTYP_CONJURATION | SPTYP_POISON | SPTYP_AIR,
    spflag::target | spflag::area | spflag::monster | spflag::needs_tracer
        | spflag::cloud,
    5,
    200,
    5, 5,
    5, 0,
    TILEG_NOXIOUS_CLOUD,
},

{
    SPELL_STEAM_BALL, "Steam Ball",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    4,
    0,
    6, 6,
    4, 0,
    TILEG_STEAM_BALL,
},

{
    SPELL_SUMMON_UFETUBUS, "Summon Ufetubus",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::monster | spflag::selfench,
    4,
    0,
    -1, -1,
    3, 0,
    TILEG_SUMMON_UFETUBUS,
},

{
    SPELL_SUMMON_HELL_BEAST, "Summon Hell Beast",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::monster | spflag::selfench,
    4,
    0,
    -1, -1,
    3, 0,
    TILEG_SUMMON_HELL_BEAST,
},

{
    SPELL_ENERGY_BOLT, "Energy Bolt",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    4,
    0,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_ENERGY_BOLT,
},

{
    SPELL_SPIT_POISON, "Spit Poison",
    SPTYP_POISON,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    2,
    0,
    6, 6,
    3, 0,
    TILEG_SPIT_POISON,
},

{
    SPELL_SUMMON_UNDEAD, "Summon Undead",
    SPTYP_SUMMONING | SPTYP_NECROMANCY,
    spflag::monster | spflag::mons_abjure,
    7,
    0,
    -1, -1,
    6, 0,
    TILEG_SUMMON_UNDEAD,
},

{
    SPELL_CANTRIP, "Cantrip",
    SPTYP_NONE,
    spflag::monster,
    1,
    0,
    -1, -1,
    2, 0,
    TILEG_CANTRIP,
},

{
    SPELL_QUICKSILVER_BOLT, "Quicksilver Bolt",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_QUICKSILVER_BOLT,
},

{
    SPELL_METAL_SPLINTERS, "Metal Splinters",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    0,
    4, 4,
    5, 0,
    TILEG_METAL_SPLINTERS,
},

{
    SPELL_MIASMA_BREATH, "Miasma Breath",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::unclean | spflag::monster
        | spflag::needs_tracer | spflag::cloud,
    6,
    0,
    5, 5,
    6, 0,
    TILEG_MIASMA_BREATH,
},

{
    SPELL_SUMMON_DRAKES, "Summon Drakes",
    SPTYP_SUMMONING | SPTYP_NECROMANCY, // since it can summon shadow dragons
    spflag::unclean | spflag::monster | spflag::mons_abjure,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_SUMMON_DRAKES,
},

{
    SPELL_BLINK_OTHER, "Blink Other",
    SPTYP_TRANSLOCATION,
    spflag::dir_or_target | spflag::not_self | spflag::escape | spflag::monster
        | spflag::emergency | spflag::needs_tracer,
    2,
    0,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_BLINK_OTHER,
},

{
    SPELL_BLINK_OTHER_CLOSE, "Blink Other Close",
    SPTYP_TRANSLOCATION,
    spflag::target | spflag::not_self | spflag::monster | spflag::needs_tracer,
    2,
    0,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_BLINK_OTHER,
},

{
    SPELL_SUMMON_MUSHROOMS, "Summon Mushrooms",
    SPTYP_SUMMONING,
    spflag::monster | spflag::selfench | spflag::mons_abjure,
    4,
    0,
    -1, -1,
    3, 0,
    TILEG_SUMMON_MUSHROOMS,
},

{
    SPELL_SPIT_ACID, "Spit Acid",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_SPIT_ACID,
},

{ SPELL_ACID_SPLASH, "Acid Splash",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_SPIT_ACID,
},

// Monster version of the spell (with full range)
{
    SPELL_STICKY_FLAME_RANGE, "Sticky Flame Range",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    4,
    100,
    4, 4,
    4, 0,
    TILEG_STICKY_FLAME_RANGE,
},

{
    SPELL_FIRE_BREATH, "Fire Breath",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    5, 0,
    TILEG_FIRE_BREATH,
},

{
    SPELL_SEARING_BREATH, "Searing Breath",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    5, 0,
    TILEG_FIRE_BREATH,
},

{
    SPELL_CHAOS_BREATH, "Chaos Breath",
    SPTYP_CONJURATION | SPTYP_RANDOM,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer | spflag::cloud,
    5,
    0,
    5, 5,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_COLD_BREATH, "Cold Breath",
    SPTYP_CONJURATION | SPTYP_ICE,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    5, 0,
    TILEG_COLD_BREATH,
},

{
    SPELL_CHILLING_BREATH, "Chilling Breath",
    SPTYP_CONJURATION | SPTYP_ICE,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_DRACONIAN_BREATH, "Draconian Breath",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::noisy,
    8,
    0,
    LOS_RADIUS, LOS_RADIUS,
    8, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_WATER_ELEMENTALS, "Summon Water Elementals",
    SPTYP_SUMMONING,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_WATER_ELEMENTALS,
},

{
    SPELL_PORKALATOR, "Porkalator",
    SPTYP_HEXES | SPTYP_TRANSMUTATION,
    spflag::dir_or_target | spflag::chaotic | spflag::needs_tracer
        | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_PORKALATOR,
},

{
    SPELL_CREATE_TENTACLES, "Spawn Tentacles",
    SPTYP_NONE,
    spflag::monster | spflag::selfench,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_TOMB_OF_DOROKLOHE, "Tomb of Doroklohe",
    SPTYP_EARTH,
    spflag::monster | spflag::emergency,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SUMMON_EYEBALLS, "Summon Eyeballs",
    SPTYP_SUMMONING,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_SUMMON_EYEBALLS,
},

{
    SPELL_HASTE_OTHER, "Haste Other",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::not_self | spflag::helpful
        | spflag::hasty | spflag::needs_tracer | spflag::utility,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_HASTE_OTHER,
},

{
    SPELL_EARTH_ELEMENTALS, "Summon Earth Elementals",
    SPTYP_SUMMONING,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_EARTH_ELEMENTALS,
},

{
    SPELL_AIR_ELEMENTALS, "Summon Air Elementals",
    SPTYP_SUMMONING,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_AIR_ELEMENTALS,
},

{
    SPELL_FIRE_ELEMENTALS, "Summon Fire Elementals",
    SPTYP_SUMMONING,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_FIRE_ELEMENTALS,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_IRON_ELEMENTALS, "Summon Iron Elementals",
    SPTYP_SUMMONING,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_IRON_ELEMENTALS,
},
#endif

{
    SPELL_SLEEP, "Sleep",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_SLEEP,
},

{
    SPELL_FAKE_MARA_SUMMON, "Mara Summon",
    SPTYP_SUMMONING,
    spflag::monster | spflag::selfench,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_FAKE_MARA_SUMMON,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SUMMON_RAKSHASA, "Summon Rakshasa",
    SPTYP_SUMMONING,
    spflag::monster,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_ERROR,
},

{
    SPELL_MISLEAD, "Mislead",
    SPTYP_HEXES,
    spflag::target | spflag::not_self,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_SUMMON_ILLUSION, "Summon Illusion",
    SPTYP_SUMMONING,
    spflag::monster,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_PRIMAL_WAVE, "Primal Wave",
    SPTYP_CONJURATION | SPTYP_ICE,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    6, 6,
    6, 25,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CALL_TIDE, "Call Tide",
    SPTYP_TRANSLOCATION,
    spflag::monster,
    7,
    0,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_IOOD, "Orb of Destruction",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer,
    7,
    200,
    8, 8,
    7, 0,
    TILEG_IOOD,
},

{
    SPELL_INK_CLOUD, "Ink Cloud",
    SPTYP_CONJURATION | SPTYP_ICE, // it's a water spell
    spflag::monster | spflag::emergency | spflag::utility,
    7,
    0,
    -1, -1,
    7, 0,
    TILEG_INK_CLOUD,
},

{
    SPELL_MIGHT, "Might",
    SPTYP_CHARMS,
    spflag::helpful | spflag::selfench | spflag::emergency | spflag::utility,
    3,
    200,
    -1, -1,
    3, 0,
    TILEG_MIGHT,
},

{
    SPELL_MIGHT_OTHER, "Might Other",
    SPTYP_CHARMS,
    spflag::dir_or_target | spflag::not_self | spflag::helpful
        | spflag::needs_tracer | spflag::utility,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_MIGHT,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SUNRAY, "Sunray",
    SPTYP_CONJURATION,
    spflag::dir_or_target,
    6,
    200,
    8, 8,
    -9, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_AWAKEN_FOREST, "Awaken Forest",
    SPTYP_HEXES | SPTYP_SUMMONING,
    spflag::area,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DRUIDS_CALL, "Druid's Call",
    SPTYP_CHARMS,
    spflag::monster,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BROTHERS_IN_ARMS, "Brothers in Arms",
    SPTYP_SUMMONING,
    spflag::monster | spflag::emergency,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_BROTHERS_IN_ARMS,
},

{
    SPELL_TROGS_HAND, "Trog's Hand",
    SPTYP_NONE,
    spflag::monster | spflag::selfench,
    3,
    0,
    -1, -1,
    3, 0,
    TILEG_TROGS_HAND,
},

{
    SPELL_SUMMON_SPECTRAL_ORCS, "Summon Spectral Orcs",
    SPTYP_NECROMANCY | SPTYP_SUMMONING,
    spflag::monster | spflag::target,
    4,
    0,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_HOLY_LIGHT, "Holy Light",
    SPTYP_CONJURATION,
    spflag::dir_or_target,
    6,
    200,
    5, 5,
    6, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_SUMMON_HOLIES, "Summon Holies",
    SPTYP_SUMMONING,
    spflag::monster | spflag::mons_abjure | spflag::holy,
    5,
    0,
    -1, -1,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_HEAL_OTHER, "Heal Other",
    SPTYP_NECROMANCY,
    spflag::dir_or_target | spflag::not_self | spflag::helpful
        | spflag::needs_tracer | spflag::utility | spflag::not_evil,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_HEAL_OTHER,
},

{
    SPELL_HOLY_FLAMES, "Holy Flames",
    SPTYP_NONE,
    spflag::target | spflag::not_self | spflag::holy,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_HOLY_BREATH, "Holy Breath",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::area | spflag::needs_tracer | spflag::cloud
        | spflag::holy,
    5,
    200,
    5, 5,
    5, 2,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_INJURY_MIRROR, "Injury Mirror",
    SPTYP_NONE,
    spflag::dir_or_target | spflag::helpful | spflag::selfench
        | spflag::emergency | spflag::utility,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_INJURY_MIRROR,
},

{
    SPELL_DRAIN_LIFE, "Drain Life",
    SPTYP_NECROMANCY,
    // n.b. marked as spflag::monster for wizmode purposes, but this spell is
    // called by the yred ability.
    spflag::area | spflag::emergency | spflag::monster,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_DRAIN_LIFE,
},

{
    SPELL_LEDAS_LIQUEFACTION, "Leda's Liquefaction",
    SPTYP_EARTH | SPTYP_HEXES,
    spflag::area,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_LEDAS_LIQUEFACTION,
},

{
    SPELL_SUMMON_HYDRA, "Summon Hydra",
    SPTYP_SUMMONING,
    spflag::mons_abjure,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_SUMMON_HYDRA,
},

{
    SPELL_DARKNESS, "Darkness",
    SPTYP_HEXES,
    spflag::none,
    6,
    200,
    -1, -1,
    3, 0,
    TILEG_DARKNESS,
},

{
    SPELL_MESMERISE, "Mesmerise",
    SPTYP_HEXES,
    spflag::area | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_MELEE, "Melee",
    SPTYP_NONE,
    spflag::none,
    1,
    0,
    -1, -1,
    1, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_FIRE_SUMMON, "Fire Summon",
    SPTYP_SUMMONING | SPTYP_FIRE,
    spflag::monster | spflag::mons_abjure,
    8,
    0,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_PETRIFYING_CLOUD, "Petrifying Cloud",
    SPTYP_CONJURATION | SPTYP_EARTH | SPTYP_AIR,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_PETRIFYING_CLOUD,
},

{
    SPELL_SHROUD_OF_GOLUBRIA, "Shroud of Golubria",
    SPTYP_CHARMS | SPTYP_TRANSLOCATION,
    spflag::selfench,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_SHROUD_OF_GOLUBRIA,
},

{
    SPELL_INNER_FLAME, "Inner Flame",
    SPTYP_HEXES | SPTYP_FIRE,
    spflag::dir_or_target | spflag::not_self | spflag::neutral
        | spflag::MR_check,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_INNER_FLAME,
},

{
    SPELL_BEASTLY_APPENDAGE, "Beastly Appendage",
    SPTYP_TRANSMUTATION,
    spflag::helpful | spflag::chaotic,
    1,
    50,
    -1, -1,
    1, 0,
    TILEG_BEASTLY_APPENDAGE,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SILVER_BLAST, "Silver Blast",
    SPTYP_CONJURATION,
    spflag::dir_or_target,
    5,
    200,
    5, 5,
    0, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_ENSNARE, "Ensnare",
    SPTYP_CONJURATION | SPTYP_HEXES,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    5, 5,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_THUNDERBOLT, "Thunderbolt",
    SPTYP_CONJURATION | SPTYP_AIR,
    spflag::dir_or_target | spflag::not_self,
    2, // 2-5, sort of
    200,
    5, 5,
    15, 0,
    TILEG_THUNDERBOLT,
},

{
    SPELL_BATTLESPHERE, "Iskenderun's Battlesphere",
    SPTYP_CONJURATION | SPTYP_CHARMS,
    spflag::utility,
    5,
    100,
    -1, -1,
    5, 0,
    TILEG_BATTLESPHERE,
},

{
    SPELL_SUMMON_MINOR_DEMON, "Summon Minor Demon",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::selfench,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_SUMMON_MINOR_DEMON,
},

{
    SPELL_MALMUTATE, "Malmutate",
    SPTYP_TRANSMUTATION | SPTYP_HEXES,
    spflag::dir_or_target | spflag::not_self | spflag::chaotic
        | spflag::needs_tracer,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_MALMUTATE,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SUMMON_TWISTER, "Summon Twister",
    SPTYP_SUMMONING | SPTYP_AIR,
    spflag::unclean | spflag::monster,
    9,
    0,
    -1, -1,
    0, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_DAZZLING_SPRAY, "Dazzling Spray",
    SPTYP_CONJURATION | SPTYP_HEXES,
    spflag::dir_or_target | spflag::not_self,
    3,
    50,
    5, 5,
    3, 0,
    TILEG_DAZZLING_SPRAY,
},

{
    SPELL_FORCE_LANCE, "Force Lance",
    SPTYP_CONJURATION | SPTYP_TRANSLOCATION,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    100,
    3, 3,
    5, 0,
    TILEG_FORCE_LANCE,
},

{
    SPELL_SENTINEL_MARK, "Sentinel's Mark",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

// Ironbrand Convoker version (delayed activation, recalls only humanoids)
{
    SPELL_WORD_OF_RECALL, "Word of Recall",
    SPTYP_SUMMONING | SPTYP_TRANSLOCATION,
    spflag::utility,
    3,
    0,
    -1, -1,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_INJURY_BOND, "Injury Bond",
    SPTYP_CHARMS,
    spflag::area | spflag::helpful,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SPECTRAL_CLOUD, "Spectral Cloud",
    SPTYP_CONJURATION | SPTYP_NECROMANCY,
    spflag::dir_or_target | spflag::monster | spflag::cloud,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_SPECTRAL_CLOUD,
},

{
    SPELL_GHOSTLY_FIREBALL, "Ghostly Fireball",
    SPTYP_CONJURATION | SPTYP_NECROMANCY,
    spflag::dir_or_target | spflag::monster | spflag::unholy
        | spflag::needs_tracer,
    5,
    200,
    5, 5,
    5, 0,
    TILEG_GHOSTLY_FIREBALL,
},

{
    SPELL_CALL_LOST_SOUL, "Call Lost Soul",
    SPTYP_SUMMONING | SPTYP_NECROMANCY,
    spflag::unholy | spflag::monster,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DIMENSION_ANCHOR, "Dimension Anchor",
    SPTYP_TRANSLOCATION | SPTYP_HEXES,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BLINK_ALLIES_ENCIRCLE, "Blink Allies Encircling",
    SPTYP_TRANSLOCATION,
    spflag::area,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SHAFT_SELF, "Shaft Self",
    SPTYP_EARTH,
    spflag::escape,
    1,
    0,
    -1, -1,
    100, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_AWAKEN_VINES, "Awaken Vines",
    SPTYP_HEXES | SPTYP_SUMMONING,
    spflag::area | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CONTROL_WINDS, "Control Winds",
    SPTYP_CHARMS | SPTYP_AIR,
    spflag::area | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_THORN_VOLLEY, "Volley of Thorns",
    SPTYP_CONJURATION | SPTYP_EARTH,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    100,
    5, 5,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_WALL_OF_BRAMBLES, "Wall of Brambles",
    SPTYP_CONJURATION | SPTYP_EARTH,
    spflag::area | spflag::monster,
    5,
    100,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_WATERSTRIKE, "Waterstrike",
    SPTYP_ICE,
    spflag::target | spflag::not_self | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_HASTE_PLANTS, "Haste Plants",
    SPTYP_CHARMS,
    spflag::area | spflag::helpful,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_WIND_BLAST, "Wind Blast",
    SPTYP_AIR,
    spflag::area,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_STRIP_RESISTANCE, "Strip Resistance",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_INFUSION, "Infusion",
    SPTYP_CHARMS,
    spflag::utility,
    1,
    25,
    -1, -1,
    1, 0,
    TILEG_INFUSION,
},

{
    SPELL_SONG_OF_SLAYING, "Song of Slaying",
    SPTYP_CHARMS,
    spflag::utility,
    2,
    100,
    -1, -1,
    2, 8,
    TILEG_SONG_OF_SLAYING,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SONG_OF_SHIELDING, "Song of Shielding",
    SPTYP_CHARMS,
    spflag::none,
    4,
    100,
    -1, -1,
    0, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_SPECTRAL_WEAPON, "Spectral Weapon",
    SPTYP_HEXES | SPTYP_CHARMS,
    spflag::selfench | spflag::utility | spflag::no_ghost,
    3,
    100,
    -1, -1,
    3, 0,
    TILEG_SPECTRAL_WEAPON,
},

{
    SPELL_SUMMON_VERMIN, "Summon Vermin",
    SPTYP_SUMMONING,
    spflag::monster | spflag::unholy | spflag::selfench | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_SUMMON_VERMIN,
},

{
    SPELL_MALIGN_OFFERING, "Malign Offering",
    SPTYP_NECROMANCY,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 10,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SEARING_RAY, "Searing Ray",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::needs_tracer,
    2,
    50,
    4, 4,
    2, 0,
    TILEG_SEARING_RAY,
},

{
    SPELL_DISCORD, "Discord",
    SPTYP_HEXES,
    spflag::area | spflag::hasty,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_DISCORD,
},

{
    SPELL_INVISIBILITY_OTHER, "Invisibility Other",
    SPTYP_CHARMS | SPTYP_HEXES,
    spflag::dir_or_target | spflag::not_self | spflag::helpful,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_VIRULENCE, "Virulence",
    SPTYP_POISON | SPTYP_HEXES,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_IGNITE_POISON_SINGLE, "Localized Ignite Poison",
    SPTYP_FIRE | SPTYP_TRANSMUTATION,
    spflag::monster | spflag::dir_or_target | spflag::not_self
        | spflag::needs_tracer | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_ORB_OF_ELECTRICITY, "Orb of Electricity",
    SPTYP_CONJURATION | SPTYP_AIR,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    7, 0,
    TILEG_ORB_OF_ELECTRICITY,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_EXPLOSIVE_BOLT, "Explosive Bolt",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_FLASH_FREEZE, "Flash Freeze",
    SPTYP_CONJURATION | SPTYP_ICE,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    7, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_LEGENDARY_DESTRUCTION, "Legendary Destruction",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    8,
    200,
    5, 5,
    8, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_EPHEMERAL_INFUSION, "Ephemeral Infusion",
    SPTYP_CHARMS | SPTYP_NECROMANCY,
    spflag::monster | spflag::emergency,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_FORCEFUL_INVITATION, "Forceful Invitation",
    SPTYP_SUMMONING,
    spflag::monster,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_PLANEREND, "Plane Rend",
    SPTYP_SUMMONING,
    spflag::monster,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CHAIN_OF_CHAOS, "Chain of Chaos",
    SPTYP_CONJURATION,
    spflag::area | spflag::monster | spflag::chaotic,
    8,
    200,
    -1, -1,
    8, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CALL_OF_CHAOS, "Call of Chaos",
    SPTYP_CHARMS,
    spflag::area | spflag::chaotic | spflag::monster,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BLACK_MARK, "Black Mark",
    SPTYP_CHARMS | SPTYP_NECROMANCY,
    spflag::monster,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_GRAND_AVATAR, "Grand Avatar",
    SPTYP_CONJURATION | SPTYP_CHARMS | SPTYP_HEXES,
    spflag::monster | spflag::utility,
    4,
    100,
    -1, -1,
    4, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_SAP_MAGIC, "Sap Magic",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CORRUPT_BODY, "Corrupt Body",
    SPTYP_TRANSMUTATION | SPTYP_HEXES,
    spflag::dir_or_target | spflag::not_self | spflag::chaotic
        | spflag::needs_tracer,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_ERROR,
},

{
    SPELL_REARRANGE_PIECES, "Rearrange the Pieces",
    SPTYP_HEXES,
    spflag::area | spflag::monster | spflag::chaotic,
    8,
    200,
    -1, -1,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},
#endif

{
    SPELL_MAJOR_DESTRUCTION, "Major Destruction",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::chaotic | spflag::needs_tracer,
    7,
    200,
    6, 6,
    7, 0,
    TILEG_MAJOR_DESTRUCTION,
},

{
    SPELL_BLINK_ALLIES_AWAY, "Blink Allies Away",
    SPTYP_TRANSLOCATION,
    spflag::area,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SUMMON_FOREST, "Summon Forest",
    SPTYP_SUMMONING | SPTYP_TRANSLOCATION,
    spflag::none,
    5,
    200,
    -1, -1,
    4, 10,
    TILEG_SUMMON_FOREST,
},

{
    SPELL_SUMMON_LIGHTNING_SPIRE, "Summon Lightning Spire",
    SPTYP_SUMMONING | SPTYP_AIR,
    spflag::target | spflag::not_self | spflag::neutral,
    4,
    100,
    2, 2,
    2, 0,
    TILEG_SUMMON_LIGHTNING_SPIRE,
},

{
    SPELL_SUMMON_GUARDIAN_GOLEM, "Summon Guardian Golem",
    SPTYP_SUMMONING | SPTYP_HEXES,
    spflag::none,
    3,
    100,
    -1, -1,
    3, 0,
    TILEG_SUMMON_GUARDIAN_GOLEM,
},

{
    SPELL_SHADOW_SHARD, "Shadow Shard",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SHADOW_BOLT, "Shadow Bolt",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CRYSTAL_BOLT, "Crystal Bolt",
    SPTYP_CONJURATION | SPTYP_FIRE | SPTYP_ICE,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    6, 6,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_RANDOM_BOLT, "Random Bolt",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    200,
    5, 5,
    4, 0,
    TILEG_RANDOM_BOLT,
},

{
    SPELL_GLACIATE, "Glaciate",
    SPTYP_CONJURATION | SPTYP_ICE,
    spflag::dir_or_target | spflag::area | spflag::not_self,
    9,
    200,
    6, 6,
    9, 25,
    TILEG_ICE_STORM,
},

{
    SPELL_CLOUD_CONE, "Cloud Cone",
    SPTYP_CONJURATION | SPTYP_AIR,
    spflag::target | spflag::not_self,
    6,
    100,
    3, LOS_DEFAULT_RANGE,
    6, 0,
    TILEG_CLOUD_CONE,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_WEAVE_SHADOWS, "Weave Shadows",
    SPTYP_SUMMONING,
    spflag::none,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_DRAGON_CALL, "Dragon's Call",
    SPTYP_SUMMONING,
    spflag::none,
    9,
    200,
    -1, -1,
    7, 15,
    TILEG_SUMMON_DRAGON,
},

{
    SPELL_SPELLFORGED_SERVITOR, "Spellforged Servitor",
    SPTYP_CONJURATION | SPTYP_SUMMONING,
    spflag::none,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_SPELLFORGED_SERVITOR,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_FORCEFUL_DISMISSAL, "Forceful Dismissal",
    SPTYP_SUMMONING,
    spflag::area,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_SUMMON_MANA_VIPER, "Summon Mana Viper",
    SPTYP_SUMMONING | SPTYP_HEXES,
    spflag::mons_abjure,
    5,
    100,
    -1, -1,
    4, 0,
    TILEG_SUMMON_MANA_VIPER,
},

{
    SPELL_PHANTOM_MIRROR, "Phantom Mirror",
    SPTYP_CHARMS | SPTYP_HEXES,
    spflag::helpful | spflag::selfench,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_PHANTOM_MIRROR,
},

{
    SPELL_DRAIN_MAGIC, "Drain Magic",
    SPTYP_HEXES,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer
        | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CORROSIVE_BOLT, "Corrosive Bolt",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    5, 5,
    6, 0,
    TILEG_CORROSIVE_BOLT,
},

{
    SPELL_SERPENT_OF_HELL_GEH_BREATH, "gehenna serpent of hell breath",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SERPENT_OF_HELL_COC_BREATH, "cocytus serpent of hell breath",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SERPENT_OF_HELL_DIS_BREATH, "dis serpent of hell breath",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SERPENT_OF_HELL_TAR_BREATH, "tartarus serpent of hell breath",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SUMMON_EMPEROR_SCORPIONS, "Summon Emperor Scorpions",
    SPTYP_SUMMONING | SPTYP_POISON,
    spflag::mons_abjure,
    7,
    100,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_IRRADIATE, "Irradiate",
    SPTYP_CONJURATION | SPTYP_TRANSMUTATION,
    spflag::area | spflag::chaotic,
    5,
    200,
    1, 1,
    4, 0,
    TILEG_IRRADIATE,
},

{
    SPELL_SPIT_LAVA, "Spit Lava",
    SPTYP_CONJURATION | SPTYP_FIRE | SPTYP_EARTH,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_ELECTRICAL_BOLT, "Electrical Bolt",
    SPTYP_CONJURATION | SPTYP_AIR,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_LIGHTNING_BOLT,
},

{
    SPELL_FLAMING_CLOUD, "Flaming Cloud",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::target | spflag::area | spflag::monster | spflag::needs_tracer
        | spflag::cloud,
    5,
    0,
    5, 5,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_THROW_BARBS, "Throw Barbs",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    5, 5,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BATTLECRY, "Battlecry",
    SPTYP_CHARMS,
    spflag::area | spflag::monster | spflag::selfench,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_WARNING_CRY, "Warning Cry",
    SPTYP_HEXES,
    spflag::area | spflag::monster | spflag::selfench | spflag::noisy,
    6,
    0,
    -1, -1,
    25, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SEAL_DOORS, "Seal Doors",
    SPTYP_HEXES,
    spflag::area | spflag::monster | spflag::selfench,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_FLAY, "Flay",
    SPTYP_NECROMANCY,
    spflag::target | spflag::not_self | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BERSERK_OTHER, "Berserk Other",
    SPTYP_CHARMS,
    spflag::hasty | spflag::monster | spflag::not_self | spflag::helpful,
    3,
    0,
    3, 3,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_THROW, "Throw",
    SPTYP_TRANSLOCATION,
    spflag::monster | spflag::not_self,
    5,
    200,
    1, 1,
    0, 5,
    TILEG_ERROR,
},
#endif

{
    SPELL_CORRUPTING_PULSE, "Corrupting Pulse",
    SPTYP_HEXES | SPTYP_TRANSMUTATION,
    spflag::area | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SIREN_SONG, "Siren Song",
    SPTYP_HEXES,
    spflag::area | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_AVATAR_SONG, "Avatar Song",
    SPTYP_HEXES,
    spflag::area | spflag::MR_check,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_PARALYSIS_GAZE, "Paralysis Gaze",
    SPTYP_HEXES,
    spflag::target | spflag::not_self | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CONFUSION_GAZE, "Confusion Gaze",
    SPTYP_HEXES,
    spflag::target | spflag::not_self | spflag::monster | spflag::MR_check,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DRAINING_GAZE, "Draining Gaze",
    SPTYP_HEXES,
    spflag::target | spflag::not_self | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DEATH_RATTLE, "Death Rattle",
    SPTYP_CONJURATION | SPTYP_NECROMANCY | SPTYP_AIR,
    spflag::dir_or_target | spflag::monster,
    7,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SUMMON_SCARABS, "Summon Scarabs",
    SPTYP_SUMMONING | SPTYP_NECROMANCY,
    spflag::mons_abjure,
    7,
    100,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SCATTERSHOT, "Scattershot",
    SPTYP_CONJURATION | SPTYP_EARTH,
    spflag::dir_or_target | spflag::not_self,
    6,
    200,
    5, 5,
    6, 0,
    TILEG_SCATTERSHOT,
},

{
    SPELL_THROW_ALLY, "Throw Ally",
    SPTYP_TRANSLOCATION,
    spflag::target | spflag::monster | spflag::not_self,
    2,
    50,
    LOS_RADIUS, LOS_RADIUS,
    3, 5,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_HUNTING_CRY, "Hunting Cry",
    SPTYP_HEXES,
    spflag::area | spflag::monster | spflag::selfench | spflag::noisy,
    6,
    0,
    -1, -1,
    25, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_CLEANSING_FLAME, "Cleansing Flame",
    SPTYP_NONE,
    spflag::area | spflag::monster | spflag::holy,
    8,
    200,
    -1, -1,
    20, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CIGOTUVIS_EMBRACE, "Cigotuvi's Embrace",
    SPTYP_NECROMANCY,
    spflag::chaotic | spflag::corpse_violating | spflag::utility
        | spflag::no_ghost,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_GRAVITAS, "Gell's Gravitas",
    SPTYP_TRANSLOCATION,
    spflag::target | spflag::not_self | spflag::needs_tracer,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_GRAVITAS,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CHANT_FIRE_STORM, "Chant Fire Storm",
    SPTYP_CONJURATION | SPTYP_FIRE,
    spflag::utility,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_VIOLENT_UNRAVELLING, "Yara's Violent Unravelling",
    SPTYP_HEXES | SPTYP_TRANSMUTATION,
    spflag::dir_or_target | spflag::needs_tracer | spflag::no_ghost
        | spflag::chaotic,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_VIOLENT_UNRAVELLING,
},

{
    SPELL_ENTROPIC_WEAVE, "Entropic Weave",
    SPTYP_HEXES,
    spflag::utility,
    5,
    200,
    -1, -1,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SUMMON_EXECUTIONERS, "Summon Executioners",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::selfench | spflag::mons_abjure,
    9,
    200,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DOOM_HOWL, "Doom Howl",
    SPTYP_TRANSLOCATION | SPTYP_HEXES,
    spflag::dir_or_target,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    15, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_AWAKEN_EARTH, "Awaken Earth",
    SPTYP_SUMMONING | SPTYP_EARTH,
    spflag::monster | spflag::target,
    7,
    0,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_AURA_OF_BRILLIANCE, "Aura of Brilliance",
    SPTYP_CHARMS,
    spflag::area | spflag::monster,
    5,
    200,
    -1, -1,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_ICEBLAST, "Iceblast",
    SPTYP_CONJURATION | SPTYP_ICE,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    5, 5,
    5, 0,
    TILEG_ICEBLAST,
},

{
    SPELL_SLUG_DART, "Slug Dart",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    1,
    25,
    LOS_RADIUS, LOS_RADIUS,
    1, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SPRINT, "Sprint",
    SPTYP_CHARMS,
    spflag::hasty | spflag::selfench | spflag::utility,
    2,
    100,
    -1, -1,
    2, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_GREATER_SERVANT_MAKHLEB, "Greater Servant of Makhleb",
    SPTYP_SUMMONING,
    spflag::unholy | spflag::selfench | spflag::mons_abjure,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_ABILITY_MAKHLEB_GREATER_SERVANT,
},

{
    SPELL_BIND_SOULS, "Bind Souls",
    SPTYP_NECROMANCY | SPTYP_ICE,
    spflag::area | spflag::monster,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_DEATH_CHANNEL,
},

{
    SPELL_INFESTATION, "Infestation",
    SPTYP_NECROMANCY,
    spflag::target | spflag::unclean,
    8,
    200,
    LOS_RADIUS, LOS_RADIUS,
    8, 4,
    TILEG_INFESTATION,
},

{
    SPELL_STILL_WINDS, "Still Winds",
    SPTYP_HEXES | SPTYP_AIR,
    spflag::monster | spflag::selfench,
    6,
    200,
    -1, -1,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_RESONANCE_STRIKE, "Resonance Strike",
    SPTYP_EARTH,
    spflag::target | spflag::not_self | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_GHOSTLY_SACRIFICE, "Ghostly Sacrifice",
    SPTYP_NECROMANCY,
    spflag::target | spflag::monster,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DREAM_DUST, "Dream Dust",
    SPTYP_HEXES,
    spflag::target | spflag::not_self | spflag::monster,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BECKONING, "Lesser Beckoning",
    SPTYP_TRANSLOCATION,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer,
    3,
    200,
    2, LOS_DEFAULT_RANGE,
    2, 0,
    TILEG_BECKONING,
},

// Monster-only, players can use Qazlal's ability
{
    SPELL_UPHEAVAL, "Upheaval",
    SPTYP_CONJURATION,
    spflag::target | spflag::not_self | spflag::needs_tracer | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_ABILITY_QAZLAL_UPHEAVAL,
},

{
    SPELL_RANDOM_EFFECTS, "Random Effects",
    SPTYP_CONJURATION,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_ERROR,
},

{
    SPELL_POISONOUS_VAPOURS, "Poisonous Vapours",
    SPTYP_POISON | SPTYP_AIR,
    spflag::target | spflag::not_self,
    2,
    50,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_POISONOUS_VAPOURS,
},

{
    SPELL_IGNITION, "Ignition",
    SPTYP_FIRE,
    spflag::area,
    8,
    200,
    -1, -1,
    8, 0,
    TILEG_IGNITION,
},

{
    SPELL_VORTEX, "Vortex",
    SPTYP_AIR,
    spflag::area,
    5,
    200,
    VORTEX_RADIUS, VORTEX_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BORGNJORS_VILE_CLUTCH, "Borgnjor's Vile Clutch",
    SPTYP_NECROMANCY | SPTYP_EARTH,
    spflag::target,
    5,
    200,
    5, 5,
    5, 4,
    TILEG_BORGNJORS_VILE_CLUTCH,
},

{
    SPELL_HARPOON_SHOT, "Harpoon Shot",
    SPTYP_CONJURATION | SPTYP_EARTH,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    4,
    200,
    6, 6,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_GRASPING_ROOTS, "Grasping Roots",
    SPTYP_EARTH,
    spflag::target | spflag::not_self | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GRASPING_ROOTS,
},

{
    SPELL_THROW_PIE, "Throw Klown Pie",
    SPTYP_CONJURATION | SPTYP_HEXES,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_NO_SPELL, "nonexistent spell",
    SPTYP_NONE,
    spflag::testing,
    1,
    0,
    -1, -1,
    1, 0,
    TILEG_ERROR,
},

};
