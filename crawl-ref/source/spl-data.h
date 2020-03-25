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
    spschool::translocation,
    spflag::escape | spflag::emergency | spflag::utility | spflag::monster,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_TELEPORT,
},

{
    SPELL_CAUSE_FEAR, "Cause Fear",
    spschool::hexes,
    spflag::area | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_CAUSE_FEAR,
},

{
    SPELL_MAGIC_DART, "Magic Dart",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    1,
    25,
    LOS_RADIUS, LOS_RADIUS,
    1, 0,
    TILEG_MAGIC_DART,
},

{
    SPELL_FIREBALL, "Fireball",
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    5, 5,
    5, 0,
    TILEG_FIREBALL,
},

{
    SPELL_APPORTATION, "Apportation",
    spschool::translocation,
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
    spschool::fire | spschool::conjuration,
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
    spschool::conjuration | spschool::fire,
    spflag::neutral | spflag::no_ghost,
    3,
    100,
    -1, -1,
    3, 2,
    TILEG_CONJURE_FLAME,
},

{
    SPELL_DIG, "Dig",
    spschool::earth,
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
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    6, 6,
    6, 0,
    TILEG_BOLT_OF_FIRE,
},

{
    SPELL_BOLT_OF_COLD, "Bolt of Cold",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    5, 5,
    6, 0,
    TILEG_BOLT_OF_COLD,
},

{
    SPELL_LIGHTNING_BOLT, "Lightning Bolt",
    spschool::conjuration | spschool::air,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    4, 11, // capped at LOS, yet this 11 matters since range increases linearly
    5, 25,
    TILEG_LIGHTNING_BOLT,
},

{
    SPELL_BLINKBOLT, "Blinkbolt",
    spschool::air | spschool::translocation,
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
    spschool::conjuration | spschool::fire | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    4, 4,
    5, 0,
    TILEG_BOLT_OF_MAGMA,
},

{
    SPELL_POLYMORPH, "Polymorph",
    spschool::transmutation | spschool::hexes,
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
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    2,
    200,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_SLOW,
},

{
    SPELL_HASTE, "Haste",
    spschool::charms,
    spflag::helpful | spflag::hasty | spflag::selfench | spflag::utility,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_HASTE,
},

{
    SPELL_PETRIFY, "Petrify",
    spschool::transmutation | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_PETRIFY,
},

{
    SPELL_CONFUSE, "Confuse",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check
        | spflag::monster,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_CONFUSE,
},

{
    SPELL_INVISIBILITY, "Invisibility",
    spschool::hexes,
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
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    2,
    50,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_THROW_FLAME,
},

{
    SPELL_THROW_FROST, "Throw Frost",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    2,
    50,
    6, 6,
    2, 0,
    TILEG_THROW_FROST,
},

{
    SPELL_CONTROLLED_BLINK, "Controlled Blink",
    spschool::translocation,
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
    spschool::translocation,
    spflag::escape | spflag::utility,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_DISJUNCTION,
},

{
    SPELL_FREEZING_CLOUD, "Freezing Cloud",
    spschool::conjuration | spschool::ice | spschool::air,
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
    spschool::conjuration | spschool::poison | spschool::air,
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
    spschool::charms | spschool::fire,
    spflag::area,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_RING_OF_FLAMES,
},

{
    SPELL_RING_OF_THUNDER, "Ring of Thunder",
    spschool::charms | spschool::air,
    spflag::area,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_VENOM_BOLT, "Venom Bolt",
    spschool::conjuration | spschool::poison,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    5, 5,
    5, 0,
    TILEG_VENOM_BOLT,
},

{
    SPELL_OLGREBS_TOXIC_RADIANCE, "Olgreb's Toxic Radiance",
    spschool::poison,
    spflag::area,
    4,
    100,
    -1, -1,
    2, 0,
    TILEG_OLGREBS_TOXIC_RADIANCE,
},

{
    SPELL_TELEPORT_OTHER, "Teleport Other",
    spschool::translocation,
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
    spschool::charms | spschool::necromancy,
    spflag::emergency | spflag::utility | spflag::no_ghost,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_DEATHS_DOOR,
},

{
    SPELL_MASS_CONFUSION, "Mass Confusion",
    spschool::hexes,
    spflag::area | spflag::MR_check,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_MASS_CONFUSION,
},

{
    SPELL_SMITING, "Smiting",
    spschool::none,
    spflag::target | spflag::not_self,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_SMITING,
},

{
    SPELL_SUMMON_SMALL_MAMMAL, "Summon Small Mammal",
    spschool::summoning,
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
    spschool::summoning,
    spflag::escape | spflag::needs_tracer | spflag::monster,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_MASS_ABJURATION,
},

{
    SPELL_AURA_OF_ABJURATION, "Aura of Abjuration",
    spschool::summoning,
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
    spschool::summoning | spschool::poison,
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
    spschool::conjuration | spschool::necromancy,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    5, 5,
    2, 0, //the beam is silent
    TILEG_BOLT_OF_DRAINING,
},

{
    SPELL_LEHUDIBS_CRYSTAL_SPEAR, "Lehudib's Crystal Spear",
    spschool::conjuration | spschool::earth,
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
    spschool::conjuration,
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
    spschool::air,
    spflag::area,
    9,
    200,
    TORNADO_RADIUS, TORNADO_RADIUS,
    5, 0,
    TILEG_TORNADO,
},

{
    SPELL_POISONOUS_CLOUD, "Poisonous Cloud",
    spschool::conjuration | spschool::poison | spschool::air,
    spflag::target | spflag::area | spflag::needs_tracer | spflag::cloud,
    5,
    200,
    5, 5,
    6, 2,
    TILEG_POISONOUS_CLOUD,
},

{
    SPELL_FIRE_STORM, "Fire Storm",
    spschool::conjuration | spschool::fire,
    spflag::target | spflag::area | spflag::needs_tracer,
    9,
    200,
    5, 5,
    9, 0,
    TILEG_FIRE_STORM,
},

{
    SPELL_CALL_DOWN_DAMNATION, "Call Down Damnation",
    spschool::conjuration,
    spflag::target | spflag::area | spflag::unholy | spflag::needs_tracer,
    9,
    200,
    LOS_RADIUS, LOS_RADIUS,
    9, 0,
    TILEG_CALL_DOWN_DAMNATION,
},

{
    SPELL_BLINK, "Blink",
    spschool::translocation,
    spflag::escape | spflag::selfench | spflag::emergency | spflag::utility,
    2,
    0,
    -1, -1,
    2, 0,
    TILEG_BLINK,
},

{
    SPELL_BLINK_RANGE, "Blink Range", // XXX needs better name
    spschool::translocation,
    spflag::escape | spflag::monster | spflag::selfench | spflag::emergency,
    2,
    0,
    -1, -1,
    2, 0,
    TILEG_BLINK,
},

{
    SPELL_BLINK_AWAY, "Blink Away",
    spschool::translocation,
    spflag::escape | spflag::monster | spflag::emergency | spflag::selfench,
    2,
    0,
    -1, -1,
    2, 0,
    TILEG_BLINK,
},

{
    SPELL_BLINK_CLOSE, "Blink Close",
    spschool::translocation,
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
    spschool::conjuration | spschool::translocation,
    spflag::area,
    4,
    100,
    2, 2,
    4, 10,
    TILEG_ISKENDERUNS_MYSTIC_BLAST,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SUMMON_SWARM, "Summon Swarm",
    spschool::summoning,
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
    spschool::summoning,
    spflag::unholy | spflag::chaotic | spflag::mons_abjure,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_SUMMON_HORRIBLE_THINGS,
},

{
    SPELL_MALIGN_GATEWAY, "Malign Gateway",
    spschool::summoning | spschool::translocation,
    spflag::unholy | spflag::chaotic,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_MALIGN_GATEWAY,
},

{
    SPELL_ENSLAVEMENT, "Enslavement",
    spschool::hexes,
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
    spschool::necromancy,
    spflag::area | spflag::neutral | spflag::utility,
    4,
    0,
    -1, -1,
    3, 0,
    TILEG_ANIMATE_DEAD,
},

{
    SPELL_PAIN, "Pain",
    spschool::necromancy,
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
    spschool::necromancy,
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
    spschool::necromancy,
    spflag::utility,
    1,
    0,
    -1, -1,
    1, 0,
    TILEG_ANIMATE_SKELETON,
},

{
    SPELL_VAMPIRIC_DRAINING, "Vampiric Draining",
    spschool::necromancy,
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
    spschool::summoning | spschool::necromancy,
    spflag::target | spflag::not_self | spflag::mons_abjure,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_HAUNT,
},

{
    SPELL_BORGNJORS_REVIVIFICATION, "Borgnjor's Revivification",
    spschool::necromancy,
    spflag::utility,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_BORGNJORS_REVIVIFICATION,
},

{
    SPELL_FREEZE, "Freeze",
    spschool::ice,
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
    spschool::summoning,
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
    spschool::ice,
    spflag::area,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_OZOCUBUS_REFRIGERATION,
},

{
    SPELL_STICKY_FLAME, "Sticky Flame",
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    100,
    1, 1,
    4, 0,
    TILEG_STICKY_FLAME,
},

{
    SPELL_SUMMON_ICE_BEAST, "Summon Ice Beast",
    spschool::ice | spschool::summoning,
    spflag::none,
    4,
    100,
    -1, -1,
    3, 0,
    TILEG_SUMMON_ICE_BEAST,
},

{
    SPELL_OZOCUBUS_ARMOUR, "Ozocubu's Armour",
    spschool::charms | spschool::ice,
    spflag::no_ghost,
    3,
    100,
    -1, -1,
    3, 0,
    TILEG_OZOCUBUS_ARMOUR,
},

{
    SPELL_CALL_IMP, "Call Imp",
    spschool::summoning,
    spflag::unholy | spflag::selfench,
    2,
    100,
    -1, -1,
    2, 0,
    TILEG_CALL_IMP,
},

{
    SPELL_REPEL_MISSILES, "Repel Missiles",
    spschool::charms | spschool::air,
    spflag::monster,
    2,
    50,
    -1, -1,
    1, 0,
    TILEG_REPEL_MISSILES,
},

{
    SPELL_BERSERKER_RAGE, "Berserker Rage",
    spschool::charms,
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
    spschool::charms,
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
    spschool::necromancy,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    100,
    1, 1,
    4, 0,
    TILEG_DISPEL_UNDEAD,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_FULSOME_DISTILLATION, "Fulsome Distillation",
    spschool::transmutation | spschool::necromancy,
    spflag::none,
    1,
    0,
    -1, -1,
    1, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_POISON_ARROW, "Poison Arrow",
    spschool::conjuration | spschool::poison,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    6,
    200,
    6, 6,
    6, 0,
    TILEG_POISON_ARROW,
},

{
    SPELL_TWISTED_RESURRECTION, "Twisted Resurrection",
    spschool::necromancy,
    spflag::chaotic | spflag::utility | spflag::monster,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_TWISTED_RESURRECTION,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_REGENERATION, "Regeneration",
    spschool::charms | spschool::necromancy,
    spflag::selfench | spflag::utility,
    3,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},
#endif

// Monster-only, players can use Lugonu's ability
{
    SPELL_BANISHMENT, "Banishment",
    spschool::translocation,
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
    spschool::transmutation | spschool::necromancy,
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
    spschool::transmutation | spschool::poison,
    spflag::dir_or_target | spflag::needs_tracer,
    1,
    25,
    3, 3,
    1, 0,
    TILEG_STING,
},

{
    SPELL_SUBLIMATION_OF_BLOOD, "Sublimation of Blood",
    spschool::necromancy,
    spflag::utility,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_SUBLIMATION_OF_BLOOD,
},

{
    SPELL_TUKIMAS_DANCE, "Tukima's Dance",
    spschool::hexes,
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
    spschool::summoning,
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
    spschool::summoning,
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
    spschool::summoning,
    spflag::unholy | spflag::selfench | spflag::mons_abjure,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_SUMMON_GREATER_DEMON,
},

{
    SPELL_CORPSE_ROT, "Corpse Rot",
    spschool::necromancy | spschool::air,
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
    spschool::charms | spschool::fire,
    spflag::helpful,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_ERROR,
},

{
    SPELL_FREEZING_AURA, "Freezing Aura",
    spschool::charms | spschool::ice,
    spflag::helpful,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_ERROR,
},

{
    SPELL_LETHAL_INFUSION, "Lethal Infusion",
    spschool::charms | spschool::necromancy,
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
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    4, 4,
    6, 0,
    TILEG_IRON_SHOT,
},

{
    SPELL_STONE_ARROW, "Stone Arrow",
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer,
    3,
    50,
    4, 4,
    3, 0,
    TILEG_STONE_ARROW,
},

{
    SPELL_SHOCK, "Shock",
    spschool::conjuration | spschool::air,
    spflag::dir_or_target | spflag::needs_tracer,
    1,
    25,
    LOS_RADIUS, LOS_RADIUS,
    1, 0,
    TILEG_SHOCK,
},

{
    SPELL_SWIFTNESS, "Swiftness",
    spschool::charms | spschool::air,
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
    spschool::charms | spschool::air,
    spflag::utility,
    3,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},

{
    SPELL_INSULATION, "Insulation",
    spschool::charms | spschool::air,
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
    spschool::poison,
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
    spschool::charms | spschool::translocation,
    spflag::helpful | spflag::utility,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},

{
    SPELL_POISON_WEAPON, "Poison Weapon",
    spschool::charms | spschool::poison,
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
    spschool::conjuration,
    spflag::dir_or_target | spflag::testing,
    7,
    100,
    LOS_RADIUS, LOS_RADIUS,
    7, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_RECALL, "Recall",
    spschool::summoning | spschool::translocation,
    spflag::utility,
    3,
    0,
    -1, -1,
    3, 0,
    TILEG_RECALL,
},

{
    SPELL_AGONY, "Agony",
    spschool::necromancy,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::MR_check,
    5,
    200,
    1, 1,
    4, 0,
    TILEG_AGONY,
},

{
    SPELL_SPIDER_FORM, "Spider Form",
    spschool::transmutation | spschool::poison,
    spflag::helpful | spflag::chaotic | spflag::utility,
    3,
    200,
    -1, -1,
    2, 0,
    TILEG_SPIDER_FORM,
},

{
    SPELL_DISINTEGRATE, "Disintegrate",
    spschool::conjuration,
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
    spschool::transmutation,
    spflag::helpful | spflag::chaotic | spflag::utility,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_BLADE_HANDS,
},

{
    SPELL_STATUE_FORM, "Statue Form",
    spschool::transmutation | spschool::earth,
    spflag::helpful | spflag::chaotic | spflag::utility,
    6,
    150,
    -1, -1,
    5, 0,
    TILEG_STATUE_FORM,
},

{
    SPELL_ICE_FORM, "Ice Form",
    spschool::ice | spschool::transmutation,
    spflag::helpful | spflag::chaotic | spflag::utility,
    4,
    100,
    -1, -1,
    3, 0,
    TILEG_ICE_FORM,
},

{
    SPELL_DRAGON_FORM, "Dragon Form",
    spschool::transmutation,
    spflag::helpful | spflag::chaotic | spflag::utility,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_DRAGON_FORM,
},

{
    SPELL_HYDRA_FORM, "Hydra Form",
    spschool::transmutation,
    spflag::helpful | spflag::chaotic | spflag::utility,
    6,
    200,
    -1, -1,
    6, 0,
    TILEG_HYDRA_FORM,
},

{
    SPELL_NECROMUTATION, "Necromutation",
    spschool::transmutation | spschool::necromancy,
    spflag::helpful | spflag::chaotic,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_NECROMUTATION,
},

{
    SPELL_DEATH_CHANNEL, "Death Channel",
    spschool::necromancy,
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
    spschool::necromancy,
    spflag::area | spflag::monster,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_SYMBOL_OF_TORMENT,
},

{
    SPELL_DEFLECT_MISSILES, "Deflect Missiles",
    spschool::charms | spschool::air,
    spflag::helpful | spflag::utility,
    6,
    200,
    -1, -1,
    3, 0,
    TILEG_DEFLECT_MISSILES,
},

{
    SPELL_THROW_ICICLE, "Throw Icicle",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    4,
    100,
    5, 5,
    4, 0,
    TILEG_THROW_ICICLE,
},

{
    SPELL_AIRSTRIKE, "Airstrike",
    spschool::air,
    spflag::target | spflag::not_self,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    2, 4,
    TILEG_AIRSTRIKE,
},

{
    SPELL_SHADOW_CREATURES, "Shadow Creatures",
    spschool::summoning,
    spflag::mons_abjure,
    6,
    0,
    -1, -1,
    4, 0,
    TILEG_SUMMON_SHADOW_CREATURES,
},

{
    SPELL_CONFUSING_TOUCH, "Confusing Touch",
    spschool::hexes,
    spflag::none,
    3,
    100,
    -1, -1,
    2, 0,
    TILEG_CONFUSING_TOUCH,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_SURE_BLADE, "Sure Blade",
    spschool::hexes | spschool::charms,
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
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::monster,
    1,
    40, // cap for range; damage cap is at 25
    2, 5,
    1, 0,
    TILEG_FLAME_TONGUE,
},

{
    SPELL_PASSWALL, "Passwall",
    spschool::transmutation | spschool::earth,
    spflag::target | spflag::escape | spflag::not_self | spflag::utility,
    2,
    120,
    1, 7,
    0, 0, // make silent to keep passwall a viable stabbing spell [rob]
    TILEG_PASSWALL,
},

{
    SPELL_IGNITE_POISON, "Ignite Poison",
    spschool::fire | spschool::transmutation | spschool::poison,
    spflag::area,
    3,
    100,
    -1, -1,
    4, 0,
    TILEG_IGNITE_POISON,
},

{
    SPELL_STICKS_TO_SNAKES, "Sticks to Snakes",
    spschool::transmutation,
    spflag::no_ghost,
    2,
    100,
    -1, -1,
    2, 0,
    TILEG_STICKS_TO_SNAKES,
},

{
    SPELL_CALL_CANINE_FAMILIAR, "Call Canine Familiar",
    spschool::summoning,
    spflag::none,
    3,
    100,
    -1, -1,
    3, 0,
    TILEG_CALL_CANINE_FAMILIAR,
},

{
    SPELL_SUMMON_DRAGON, "Summon Dragon",
    spschool::summoning,
    spflag::mons_abjure,
    9,
    200,
    -1, -1,
    7, 0,
    TILEG_SUMMON_DRAGON,
},

{
    SPELL_HIBERNATION, "Ensorcelled Hibernation",
    spschool::hexes | spschool::ice,
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
    spschool::hexes | spschool::ice,
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
    spschool::charms,
    spflag::helpful,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},

{
    SPELL_PHASE_SHIFT, "Phase Shift",
    spschool::translocation,
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
    spschool::summoning,
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
    spschool::charms | spschool::translocation,
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
    spschool::hexes | spschool::air,
    spflag::area,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_SILENCE,
},

{
    SPELL_SHATTER, "Shatter",
    spschool::earth,
    spflag::area,
    9,
    200,
    -1, -1,
    7, 30,
    TILEG_SHATTER,
},

{
    SPELL_DISPERSAL, "Dispersal",
    spschool::translocation,
    spflag::area | spflag::escape,
    6,
    200,
    1, 4,
    5, 0,
    TILEG_DISPERSAL,
},

{
    SPELL_DISCHARGE, "Static Discharge",
    spschool::conjuration | spschool::air,
    spflag::area,
    3,
    100,
    1, 1,
    3, 0,
    TILEG_STATIC_DISCHARGE,
},

{
    SPELL_CORONA, "Corona",
    spschool::hexes,
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
    spschool::transmutation | spschool::poison,
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
    spschool::fire | spschool::transmutation,
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
    spschool::earth,
    spflag::target,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_LEES_RAPID_DECONSTRUCTION,
},

{
    SPELL_SANDBLAST, "Sandblast",
    spschool::earth,
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
    spschool::ice,
    spflag::helpful | spflag::utility,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_ERROR,
},

{
    SPELL_STONESKIN, "Stoneskin",
    spschool::earth | spschool::transmutation,
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
    spschool::ice | spschool::necromancy,
    spflag::none,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_SIMULACRUM,
},

{
    SPELL_CONJURE_BALL_LIGHTNING, "Conjure Ball Lightning",
    spschool::air | spschool::conjuration,
    spflag::selfench,
    6,
    200,
    -1, -1,
    6, 0,
    TILEG_CONJURE_BALL_LIGHTNING,
},

{
    SPELL_CHAIN_LIGHTNING, "Chain Lightning",
    spschool::air | spschool::conjuration,
    spflag::area,
    8,
    200,
    -1, -1,
    8, 25,
    TILEG_CHAIN_LIGHTNING,
},

{
    SPELL_EXCRUCIATING_WOUNDS, "Excruciating Wounds",
    spschool::charms | spschool::necromancy,
    spflag::helpful,
    5,
    200,
    -1, -1,
    4, 15,
    TILEG_EXCRUCIATING_WOUNDS,
},

{
    SPELL_PORTAL_PROJECTILE, "Portal Projectile",
    spschool::translocation | spschool::hexes,
    spflag::none,
    3,
    50,
    -1, -1,
    3, 0,
    TILEG_PORTAL_PROJECTILE,
},

{
    SPELL_MONSTROUS_MENAGERIE, "Monstrous Menagerie",
    spschool::summoning,
    spflag::mons_abjure,
    7,
    200,
    -1, -1,
    5, 0,
    TILEG_MONSTROUS_MENAGERIE,
},

{
    SPELL_GOLUBRIAS_PASSAGE, "Passage of Golubria",
    spschool::translocation,
    spflag::target | spflag::neutral | spflag::escape | spflag::selfench,
    4,
    0,
    LOS_RADIUS, LOS_RADIUS,
    3, 8, // when it closes
    TILEG_PASSAGE_OF_GOLUBRIA,
},

{
    SPELL_FULMINANT_PRISM, "Fulminant Prism",
    spschool::conjuration | spschool::hexes,
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
    spschool::translocation,
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
    spschool::hexes,
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
    spschool::necromancy,
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
    spschool::necromancy,
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
    spschool::conjuration,
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
    spschool::summoning,
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
    spschool::necromancy,
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
    spschool::summoning,
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
    spschool::conjuration | spschool::poison | spschool::air,
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
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    4,
    0,
    6, 6,
    4, 0,
    TILEG_STEAM_BALL,
},

{
    SPELL_SUMMON_UFETUBUS, "Summon Ufetubus",
    spschool::summoning,
    spflag::unholy | spflag::monster | spflag::selfench,
    4,
    0,
    -1, -1,
    3, 0,
    TILEG_SUMMON_UFETUBUS,
},

{
    SPELL_SUMMON_HELL_BEAST, "Summon Hell Beast",
    spschool::summoning,
    spflag::unholy | spflag::monster | spflag::selfench,
    4,
    0,
    -1, -1,
    3, 0,
    TILEG_SUMMON_HELL_BEAST,
},

{
    SPELL_ENERGY_BOLT, "Energy Bolt",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    4,
    0,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_ENERGY_BOLT,
},

{
    SPELL_SPIT_POISON, "Spit Poison",
    spschool::poison,
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
    spschool::summoning | spschool::necromancy,
    spflag::monster | spflag::mons_abjure,
    7,
    0,
    -1, -1,
    6, 0,
    TILEG_SUMMON_UNDEAD,
},

{
    SPELL_CANTRIP, "Cantrip",
    spschool::none,
    spflag::monster,
    1,
    0,
    -1, -1,
    2, 0,
    TILEG_CANTRIP,
},

{
    SPELL_QUICKSILVER_BOLT, "Quicksilver Bolt",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_QUICKSILVER_BOLT,
},

{
    SPELL_METAL_SPLINTERS, "Metal Splinters",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    0,
    4, 4,
    5, 0,
    TILEG_METAL_SPLINTERS,
},

{
    SPELL_MIASMA_BREATH, "Miasma Breath",
    spschool::conjuration,
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
    spschool::summoning | spschool::necromancy, // since it can summon shadow dragons
    spflag::unclean | spflag::monster | spflag::mons_abjure,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_SUMMON_DRAKES,
},

{
    SPELL_BLINK_OTHER, "Blink Other",
    spschool::translocation,
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
    spschool::translocation,
    spflag::target | spflag::not_self | spflag::monster | spflag::needs_tracer,
    2,
    0,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_BLINK_OTHER,
},

{
    SPELL_SUMMON_MUSHROOMS, "Summon Mushrooms",
    spschool::summoning,
    spflag::monster | spflag::selfench | spflag::mons_abjure,
    4,
    0,
    -1, -1,
    3, 0,
    TILEG_SUMMON_MUSHROOMS,
},

{
    SPELL_SPIT_ACID, "Spit Acid",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::noisy
        | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_SPIT_ACID,
},

{ SPELL_ACID_SPLASH, "Acid Splash",
    spschool::conjuration,
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
    spschool::conjuration | spschool::fire,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    4,
    100,
    4, 4,
    4, 0,
    TILEG_STICKY_FLAME_RANGE,
},

{
    SPELL_FIRE_BREATH, "Fire Breath",
    spschool::conjuration | spschool::fire,
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
    spschool::conjuration | spschool::fire,
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
    spschool::conjuration | spschool::random,
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
    spschool::conjuration | spschool::ice,
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
    spschool::conjuration | spschool::ice,
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
    spschool::conjuration,
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
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_WATER_ELEMENTALS,
},

{
    SPELL_PORKALATOR, "Porkalator",
    spschool::hexes | spschool::transmutation,
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
    spschool::none,
    spflag::monster | spflag::selfench,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_CREATE_TENTACLES,
},

{
    SPELL_TOMB_OF_DOROKLOHE, "Tomb of Doroklohe",
    spschool::earth,
    spflag::monster | spflag::emergency,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SUMMON_EYEBALLS, "Summon Eyeballs",
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_SUMMON_EYEBALLS,
},

{
    SPELL_HASTE_OTHER, "Haste Other",
    spschool::hexes,
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
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_EARTH_ELEMENTALS,
},

{
    SPELL_AIR_ELEMENTALS, "Summon Air Elementals",
    spschool::summoning,
    spflag::monster | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_AIR_ELEMENTALS,
},

{
    SPELL_FIRE_ELEMENTALS, "Summon Fire Elementals",
    spschool::summoning,
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
    spschool::summoning,
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
    spschool::hexes,
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
    spschool::summoning,
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
    spschool::summoning,
    spflag::monster,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_ERROR,
},

{
    SPELL_MISLEAD, "Mislead",
    spschool::hexes,
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
    spschool::summoning,
    spflag::monster,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_PRIMAL_WAVE, "Primal Wave",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    6, 6,
    6, 25,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CALL_TIDE, "Call Tide",
    spschool::translocation,
    spflag::monster,
    7,
    0,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_IOOD, "Orb of Destruction",
    spschool::conjuration,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer,
    7,
    200,
    8, 8,
    7, 0,
    TILEG_IOOD,
},

{
    SPELL_INK_CLOUD, "Ink Cloud",
    spschool::conjuration | spschool::ice, // it's a water spell
    spflag::monster | spflag::emergency | spflag::utility,
    7,
    0,
    -1, -1,
    7, 0,
    TILEG_INK_CLOUD,
},

{
    SPELL_MIGHT, "Might",
    spschool::charms,
    spflag::helpful | spflag::selfench | spflag::emergency | spflag::utility,
    3,
    200,
    -1, -1,
    3, 0,
    TILEG_MIGHT,
},

{
    SPELL_MIGHT_OTHER, "Might Other",
    spschool::charms,
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
    spschool::conjuration,
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
    spschool::hexes | spschool::summoning,
    spflag::area,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DRUIDS_CALL, "Druid's Call",
    spschool::charms,
    spflag::monster,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BROTHERS_IN_ARMS, "Brothers in Arms",
    spschool::summoning,
    spflag::monster | spflag::emergency,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_BROTHERS_IN_ARMS,
},

{
    SPELL_TROGS_HAND, "Trog's Hand",
    spschool::none,
    spflag::monster | spflag::selfench,
    3,
    0,
    -1, -1,
    3, 0,
    TILEG_TROGS_HAND,
},

{
    SPELL_SUMMON_SPECTRAL_ORCS, "Summon Spectral Orcs",
    spschool::necromancy | spschool::summoning,
    spflag::monster | spflag::target,
    4,
    0,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_SUMMON_SPECTRAL_ORCS,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_HOLY_LIGHT, "Holy Light",
    spschool::conjuration,
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
    spschool::summoning,
    spflag::monster | spflag::mons_abjure | spflag::holy,
    5,
    0,
    -1, -1,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_HEAL_OTHER, "Heal Other",
    spschool::necromancy,
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
    spschool::none,
    spflag::target | spflag::not_self | spflag::holy,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_HOLY_BREATH, "Holy Breath",
    spschool::conjuration,
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
    spschool::none,
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
    spschool::necromancy,
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
    spschool::earth | spschool::hexes,
    spflag::area,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_LEDAS_LIQUEFACTION,
},

{
    SPELL_SUMMON_HYDRA, "Summon Hydra",
    spschool::summoning,
    spflag::mons_abjure,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_SUMMON_HYDRA,
},

{
    SPELL_DARKNESS, "Darkness",
    spschool::hexes,
    spflag::none,
    6,
    200,
    -1, -1,
    3, 0,
    TILEG_DARKNESS,
},

{
    SPELL_MESMERISE, "Mesmerise",
    spschool::hexes,
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
    spschool::none,
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
    spschool::summoning | spschool::fire,
    spflag::monster | spflag::mons_abjure,
    8,
    0,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_PETRIFYING_CLOUD, "Petrifying Cloud",
    spschool::conjuration | spschool::earth | spschool::air,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_PETRIFYING_CLOUD,
},

{
    SPELL_SHROUD_OF_GOLUBRIA, "Shroud of Golubria",
    spschool::charms | spschool::translocation,
    spflag::selfench,
    2,
    50,
    -1, -1,
    2, 0,
    TILEG_SHROUD_OF_GOLUBRIA,
},

{
    SPELL_INNER_FLAME, "Inner Flame",
    spschool::hexes | spschool::fire,
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
    spschool::transmutation,
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
    spschool::conjuration,
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
    spschool::conjuration | spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    5, 5,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_THUNDERBOLT, "Thunderbolt",
    spschool::conjuration | spschool::air,
    spflag::dir_or_target | spflag::not_self,
    2, // 2-5, sort of
    200,
    5, 5,
    15, 0,
    TILEG_THUNDERBOLT,
},

{
    SPELL_BATTLESPHERE, "Iskenderun's Battlesphere",
    spschool::conjuration | spschool::charms,
    spflag::utility,
    5,
    100,
    -1, -1,
    5, 0,
    TILEG_BATTLESPHERE,
},

{
    SPELL_SUMMON_MINOR_DEMON, "Summon Minor Demon",
    spschool::summoning,
    spflag::unholy | spflag::selfench,
    2,
    200,
    -1, -1,
    2, 0,
    TILEG_SUMMON_MINOR_DEMON,
},

{
    SPELL_MALMUTATE, "Malmutate",
    spschool::transmutation | spschool::hexes,
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
    spschool::summoning | spschool::air,
    spflag::unclean | spflag::monster,
    9,
    0,
    -1, -1,
    0, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_DAZZLING_FLASH, "Dazzling Flash",
    spschool::conjuration | spschool::hexes,
    spflag::area | spflag::no_ghost,
    3,
    50,
    2, 3,
    0, 0,
    TILEG_DAZZLING_SPRAY,
},

{
    SPELL_FORCE_LANCE, "Force Lance",
    spschool::conjuration | spschool::translocation,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    4,
    100,
    3, 3,
    5, 0,
    TILEG_FORCE_LANCE,
},

{
    SPELL_SENTINEL_MARK, "Sentinel's Mark",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_SENTINEL_MARK,
},

// Ironbrand Convoker version (delayed activation, recalls only humanoids)
{
    SPELL_WORD_OF_RECALL, "Word of Recall",
    spschool::summoning | spschool::translocation,
    spflag::utility,
    3,
    0,
    -1, -1,
    3, 0,
    TILEG_RECALL,
},

{
    SPELL_INJURY_BOND, "Injury Bond",
    spschool::charms,
    spflag::area | spflag::helpful,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SPECTRAL_CLOUD, "Spectral Cloud",
    spschool::conjuration | spschool::necromancy,
    spflag::dir_or_target | spflag::monster | spflag::cloud,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_SPECTRAL_CLOUD,
},

{
    SPELL_GHOSTLY_FIREBALL, "Ghostly Fireball",
    spschool::conjuration | spschool::necromancy,
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
    spschool::summoning | spschool::necromancy,
    spflag::unholy | spflag::monster,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DIMENSION_ANCHOR, "Dimension Anchor",
    spschool::translocation | spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BLINK_ALLIES_ENCIRCLE, "Blink Allies Encircling",
    spschool::translocation,
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
    spschool::earth,
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
    spschool::hexes | spschool::summoning,
    spflag::area | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_AWAKEN_VINES,
},

#if TAG_MAJOR_VERSION == 34
{
    SPELL_CONTROL_WINDS, "Control Winds",
    spschool::charms | spschool::air,
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
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    100,
    5, 5,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_WALL_OF_BRAMBLES, "Wall of Brambles",
    spschool::conjuration | spschool::earth,
    spflag::area | spflag::monster,
    5,
    100,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_WALL_OF_BRAMBLES,
},

{
    SPELL_WATERSTRIKE, "Waterstrike",
    spschool::ice,
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
    spschool::charms,
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
    spschool::air,
    spflag::area,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_STRIP_RESISTANCE, "Strip Resistance",
    spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_STRIP_RESISTANCE,
},

{
    SPELL_INFUSION, "Infusion",
    spschool::charms,
    spflag::utility,
    1,
    25,
    -1, -1,
    1, 0,
    TILEG_INFUSION,
},

{
    SPELL_SONG_OF_SLAYING, "Song of Slaying",
    spschool::charms,
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
    spschool::charms,
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
    spschool::hexes | spschool::charms,
    spflag::selfench | spflag::utility | spflag::no_ghost,
    3,
    100,
    -1, -1,
    3, 0,
    TILEG_SPECTRAL_WEAPON,
},

{
    SPELL_SUMMON_VERMIN, "Summon Vermin",
    spschool::summoning,
    spflag::monster | spflag::unholy | spflag::selfench | spflag::mons_abjure,
    5,
    0,
    -1, -1,
    4, 0,
    TILEG_SUMMON_VERMIN,
},

{
    SPELL_MALIGN_OFFERING, "Malign Offering",
    spschool::necromancy,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 10,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SEARING_RAY, "Searing Ray",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    2,
    50,
    4, 4,
    2, 0,
    TILEG_SEARING_RAY,
},

{
    SPELL_DISCORD, "Discord",
    spschool::hexes,
    spflag::area | spflag::hasty,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_DISCORD,
},

{
    SPELL_INVISIBILITY_OTHER, "Invisibility Other",
    spschool::charms | spschool::hexes,
    spflag::dir_or_target | spflag::not_self | spflag::helpful,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_INVISIBILITY,
},

{
    SPELL_VIRULENCE, "Virulence",
    spschool::poison | spschool::hexes,
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
    spschool::fire | spschool::transmutation,
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
    spschool::conjuration | spschool::air,
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
    spschool::conjuration | spschool::fire,
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
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    7, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_LEGENDARY_DESTRUCTION, "Legendary Destruction",
    spschool::conjuration,
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
    spschool::charms | spschool::necromancy,
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
    spschool::summoning,
    spflag::monster,
    4,
    200,
    -1, -1,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_PLANEREND, "Plane Rend",
    spschool::summoning,
    spflag::monster,
    8,
    200,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CHAIN_OF_CHAOS, "Chain of Chaos",
    spschool::conjuration,
    spflag::area | spflag::monster | spflag::chaotic,
    8,
    200,
    -1, -1,
    8, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CALL_OF_CHAOS, "Call of Chaos",
    spschool::charms,
    spflag::area | spflag::chaotic | spflag::monster,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BLACK_MARK, "Black Mark",
    spschool::charms | spschool::necromancy,
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
    spschool::conjuration | spschool::charms | spschool::hexes,
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
    spschool::hexes,
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
    spschool::transmutation | spschool::hexes,
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
    spschool::hexes,
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
    spschool::conjuration,
    spflag::dir_or_target | spflag::chaotic | spflag::needs_tracer,
    7,
    200,
    6, 6,
    7, 0,
    TILEG_MAJOR_DESTRUCTION,
},

{
    SPELL_BLINK_ALLIES_AWAY, "Blink Allies Away",
    spschool::translocation,
    spflag::area,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SUMMON_FOREST, "Summon Forest",
    spschool::summoning | spschool::translocation,
    spflag::none,
    5,
    200,
    -1, -1,
    4, 10,
    TILEG_SUMMON_FOREST,
},

{
    SPELL_SUMMON_LIGHTNING_SPIRE, "Summon Lightning Spire",
    spschool::summoning | spschool::air,
    spflag::target | spflag::not_self | spflag::neutral,
    4,
    100,
    2, 2,
    2, 0,
    TILEG_SUMMON_LIGHTNING_SPIRE,
},

{
    SPELL_SUMMON_GUARDIAN_GOLEM, "Summon Guardian Golem",
    spschool::summoning | spschool::hexes,
    spflag::none,
    3,
    100,
    -1, -1,
    3, 0,
    TILEG_SUMMON_GUARDIAN_GOLEM,
},

{
    SPELL_SHADOW_SHARD, "Shadow Shard",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SHADOW_BOLT, "Shadow Bolt",
    spschool::conjuration,
    spflag::dir_or_target | spflag::monster | spflag::needs_tracer,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CRYSTAL_BOLT, "Crystal Bolt",
    spschool::conjuration | spschool::fire | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    6, 6,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_RANDOM_BOLT, "Random Bolt",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    200,
    5, 5,
    4, 0,
    TILEG_RANDOM_BOLT,
},

{
    SPELL_GLACIATE, "Glaciate",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::area | spflag::not_self
        | spflag:: monster,
    9,
    200,
    6, 6,
    9, 25,
    TILEG_ICE_STORM,
},

{
    SPELL_CLOUD_CONE, "Cloud Cone",
    spschool::conjuration | spschool::air,
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
    spschool::summoning,
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
    spschool::summoning,
    spflag::none,
    9,
    200,
    -1, -1,
    7, 15,
    TILEG_SUMMON_DRAGON,
},

{
    SPELL_SPELLFORGED_SERVITOR, "Spellforged Servitor",
    spschool::conjuration | spschool::summoning,
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
    spschool::summoning,
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
    spschool::summoning | spschool::hexes,
    spflag::mons_abjure,
    5,
    100,
    -1, -1,
    4, 0,
    TILEG_SUMMON_MANA_VIPER,
},

{
    SPELL_PHANTOM_MIRROR, "Phantom Mirror",
    spschool::charms | spschool::hexes,
    spflag::helpful | spflag::selfench,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_PHANTOM_MIRROR,
},

{
    SPELL_DRAIN_MAGIC, "Drain Magic",
    spschool::hexes,
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
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    6,
    200,
    5, 5,
    6, 0,
    TILEG_CORROSIVE_BOLT,
},

{
    SPELL_SERPENT_OF_HELL_GEH_BREATH, "gehenna serpent of hell breath",
    spschool::conjuration,
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
    spschool::conjuration,
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
    spschool::conjuration,
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
    spschool::conjuration,
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
    spschool::summoning | spschool::poison,
    spflag::mons_abjure,
    7,
    100,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_IRRADIATE, "Irradiate",
    spschool::conjuration | spschool::transmutation,
    spflag::area | spflag::chaotic,
    5,
    200,
    1, 1,
    4, 0,
    TILEG_IRRADIATE,
},

{
    SPELL_SPIT_LAVA, "Spit Lava",
    spschool::conjuration | spschool::fire | spschool::earth,
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
    spschool::conjuration | spschool::air,
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
    spschool::conjuration | spschool::fire,
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
    spschool::conjuration,
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
    spschool::charms,
    spflag::area | spflag::monster | spflag::selfench,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_WARNING_CRY, "Warning Cry",
    spschool::hexes,
    spflag::area | spflag::monster | spflag::selfench | spflag::noisy,
    6,
    0,
    -1, -1,
    25, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SEAL_DOORS, "Seal Doors",
    spschool::hexes,
    spflag::area | spflag::monster | spflag::selfench,
    6,
    0,
    -1, -1,
    5, 0,
    TILEG_SEAL_DOORS,
},

{
    SPELL_FLAY, "Flay",
    spschool::necromancy,
    spflag::target | spflag::not_self | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BERSERK_OTHER, "Berserk Other",
    spschool::charms,
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
    spschool::translocation,
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
    spschool::hexes | spschool::transmutation,
    spflag::area | spflag::monster,
    6,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SIREN_SONG, "Siren Song",
    spschool::hexes,
    spflag::area | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_AVATAR_SONG, "Avatar Song",
    spschool::hexes,
    spflag::area | spflag::MR_check,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_PARALYSIS_GAZE, "Paralysis Gaze",
    spschool::hexes,
    spflag::target | spflag::not_self | spflag::monster,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_CONFUSION_GAZE, "Confusion Gaze",
    spschool::hexes,
    spflag::target | spflag::not_self | spflag::monster | spflag::MR_check,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DRAINING_GAZE, "Draining Gaze",
    spschool::hexes,
    spflag::target | spflag::not_self | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DEATH_RATTLE, "Death Rattle",
    spschool::conjuration | spschool::necromancy | spschool::air,
    spflag::dir_or_target | spflag::monster,
    7,
    0,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SUMMON_SCARABS, "Summon Scarabs",
    spschool::summoning | spschool::necromancy,
    spflag::mons_abjure,
    7,
    100,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SCATTERSHOT, "Scattershot",
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::not_self,
    6,
    200,
    5, 5,
    6, 0,
    TILEG_SCATTERSHOT,
},

{
    SPELL_THROW_ALLY, "Throw Ally",
    spschool::translocation,
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
    spschool::hexes,
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
    spschool::none,
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
    spschool::necromancy,
    spflag::chaotic | spflag::utility | spflag::no_ghost,
    5,
    200,
    -1, -1,
    4, 0,
    TILEG_ERROR,
},
#endif

{
    SPELL_GRAVITAS, "Gell's Gravitas",
    spschool::translocation,
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
    spschool::conjuration | spschool::fire,
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
    spschool::hexes | spschool::transmutation,
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
    spschool::hexes,
    spflag::utility,
    5,
    200,
    -1, -1,
    3, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SUMMON_EXECUTIONERS, "Summon Executioners",
    spschool::summoning,
    spflag::unholy | spflag::selfench | spflag::mons_abjure,
    9,
    200,
    -1, -1,
    6, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DOOM_HOWL, "Doom Howl",
    spschool::translocation | spschool::hexes,
    spflag::dir_or_target,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    15, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_AWAKEN_EARTH, "Awaken Earth",
    spschool::summoning | spschool::earth,
    spflag::monster | spflag::target,
    7,
    0,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_AURA_OF_BRILLIANCE, "Aura of Brilliance",
    spschool::charms,
    spflag::area | spflag::monster,
    5,
    200,
    -1, -1,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_ICEBLAST, "Iceblast",
    spschool::conjuration | spschool::ice,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    5, 5,
    5, 0,
    TILEG_ICEBLAST,
},

{
    SPELL_SLUG_DART, "Slug Dart",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    1,
    25,
    LOS_RADIUS, LOS_RADIUS,
    1, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SPRINT, "Sprint",
    spschool::charms,
    spflag::hasty | spflag::selfench | spflag::utility,
    2,
    100,
    -1, -1,
    2, 0,
    TILEG_SPRINT,
},

{
    SPELL_GREATER_SERVANT_MAKHLEB, "Greater Servant of Makhleb",
    spschool::summoning,
    spflag::unholy | spflag::selfench | spflag::mons_abjure,
    7,
    200,
    -1, -1,
    6, 0,
    TILEG_ABILITY_MAKHLEB_GREATER_SERVANT,
},

{
    SPELL_BIND_SOULS, "Bind Souls",
    spschool::necromancy | spschool::ice,
    spflag::area | spflag::monster,
    6,
    200,
    -1, -1,
    5, 0,
    TILEG_DEATH_CHANNEL,
},

{
    SPELL_INFESTATION, "Infestation",
    spschool::necromancy,
    spflag::target | spflag::unclean,
    8,
    200,
    LOS_RADIUS, LOS_RADIUS,
    8, 4,
    TILEG_INFESTATION,
},

{
    SPELL_STILL_WINDS, "Still Winds",
    spschool::hexes | spschool::air,
    spflag::monster | spflag::selfench,
    6,
    200,
    -1, -1,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_RESONANCE_STRIKE, "Resonance Strike",
    spschool::earth,
    spflag::target | spflag::not_self | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_GHOSTLY_SACRIFICE, "Ghostly Sacrifice",
    spschool::necromancy,
    spflag::target | spflag::monster,
    7,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_DREAM_DUST, "Dream Dust",
    spschool::hexes,
    spflag::target | spflag::not_self | spflag::monster,
    3,
    200,
    LOS_RADIUS, LOS_RADIUS,
    0, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BECKONING, "Lesser Beckoning",
    spschool::translocation,
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
    spschool::conjuration,
    spflag::target | spflag::not_self | spflag::needs_tracer | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_ABILITY_QAZLAL_UPHEAVAL,
},

{
    SPELL_RANDOM_EFFECTS, "Random Effects",
    spschool::conjuration,
    spflag::dir_or_target | spflag::needs_tracer,
    4,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_ERROR,
},

{
    SPELL_POISONOUS_VAPOURS, "Poisonous Vapours",
    spschool::poison | spschool::air,
    spflag::target | spflag::not_self,
    2,
    50,
    LOS_RADIUS, LOS_RADIUS,
    2, 0,
    TILEG_POISONOUS_VAPOURS,
},

{
    SPELL_IGNITION, "Ignition",
    spschool::fire,
    spflag::area,
    8,
    200,
    -1, -1,
    8, 0,
    TILEG_IGNITION,
},

{
    SPELL_VORTEX, "Vortex",
    spschool::air,
    spflag::area,
    5,
    200,
    VORTEX_RADIUS, VORTEX_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_BORGNJORS_VILE_CLUTCH, "Borgnjor's Vile Clutch",
    spschool::necromancy | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer,
    5,
    200,
    6, 6,
    5, 4,
    TILEG_BORGNJORS_VILE_CLUTCH,
},

{
    SPELL_HARPOON_SHOT, "Harpoon Shot",
    spschool::conjuration | spschool::earth,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    4,
    200,
    6, 6,
    4, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_GRASPING_ROOTS, "Grasping Roots",
    spschool::earth,
    spflag::target | spflag::not_self | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GRASPING_ROOTS,
},

{
    SPELL_THROW_PIE, "Throw Klown Pie",
    spschool::conjuration | spschool::hexes,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    5, 0,
    TILEG_GENERIC_MONSTER_SPELL,
},

{
    SPELL_SPORULATE, "Sporulate",
    spschool::conjuration | spschool::earth,
    spflag::monster | spflag::selfench,
    5,
    200,
    -1, -1,
    5, 0,
    TILEG_SPORULATE,
},

{
    SPELL_STARBURST, "Starburst",
    spschool::conjuration | spschool::fire,
    spflag::area,
    6,
    200,
    5, 5,
    6, 0,
    TILEG_STARBURST,
},

{
    SPELL_FOXFIRE, "Foxfire",
    spschool::conjuration | spschool::fire,
    spflag::selfench,
    1,
    25,
    -1, -1,
    1, 0,
    TILEG_FOXFIRE,
},

{
    SPELL_HAILSTORM, "Hailstorm",
    spschool::conjuration | spschool::ice,
    spflag::area,
    4,
    100,
    3, 3, // Range special-cased in describe-spells
    4, 0,
    TILEG_HAILSTORM,
},

{
    SPELL_NOXIOUS_BOG, "Eringya's Noxious Bog",
    spschool::poison | spschool::transmutation,
    spflag::selfench,
    6,
    200,
    -1, -1,
    2, 0,
    TILEG_NOXIOUS_BOG,
},

{
    SPELL_AGONY_RANGE, "Agony Range",
    spschool::necromancy,
    spflag::dir_or_target | spflag::not_self | spflag::needs_tracer
        | spflag::monster | spflag::MR_check,
    5,
    200,
    LOS_RADIUS, LOS_RADIUS,
    4, 0,
    TILEG_AGONY,
},

{
    SPELL_DISPEL_UNDEAD_RANGE, "Dispel Undead Range",
    spschool::necromancy,
    spflag::dir_or_target | spflag::needs_tracer | spflag::monster,
    5,
    100,
    4, 4,
    4, 0,
    TILEG_DISPEL_UNDEAD,
},

{
    SPELL_FROZEN_RAMPARTS, "Frozen Ramparts",
    spschool::ice,
    spflag::area | spflag::no_ghost,
    3,
    50,
    3, 3,
    3, 8,
    TILEG_FREEZE,
},

{
    SPELL_ABSOLUTE_ZERO, "Absolute Zero",
    spschool::ice,
    spflag::no_ghost,
    9,
    200,
    1, LOS_RADIUS,
    9, 25,
    TILEG_ICE_STORM,
},

{
    SPELL_NO_SPELL, "nonexistent spell",
    spschool::none,
    spflag::testing,
    1,
    0,
    -1, -1,
    1, 0,
    TILEG_ERROR,
},

};
