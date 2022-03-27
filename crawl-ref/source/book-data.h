#pragma once

#include <vector>

#include "tag-version.h"

using std::vector;

/*
 * These books have a few goals:
 * 1. Have a variety of spell levels and types, so they're useful to
 *    players who pick them up in different contexts. For example, a book
 *    with a level 5, level 6 and level 9 spell might be helpful to an
 *    mid-game character or a late-game one, or even just provide goals
 *    for an early character.
 * 2. Group spells in a thematic or meaningful fashion. The Book of Pain,
 *    Book of Storms, Wizard Biographies... these tell stories, and players
 *    love stories.
 *
 * Randbooks can play a role in increasing variety, but I do think that
 * fixed books provide an overall better player experience in general.
 *
 * These on average have 3 spells each, so that we can hand them out fairly
 * frequently without giving the player every spell too quickly. Occasionally
 * they can have 2 or 4 spells, and that's fine too. It'd also be fine to have
 * 5 spells for a really special book.
 *
 * They're designed so that almost every spell shows up in 2 books. A few show
 * up in three books, a few in just one (especially lower-level spells that
 * will rarely be useful to players, level 9 spells, and some level 8 spells
 * which are intentionally rarer).
 * A spreadsheet accurate as of June 2021 can be found here:
 * https://docs.google.com/spreadsheets/d/1RCWRO_fltNQDAlbF2h1wx8ZZyulUsyGmNNvkkogxRoo/edit#gid=0
 */

// This needs to be re-ordered when TAG_MAJOR_VERSION changes!
static const vector<spell_type> spellbook_templates[] =
{

{   // Book of Minor Magic
    SPELL_MAGIC_DART,
    SPELL_CALL_IMP,
    SPELL_MEPHITIC_CLOUD,
},

{   // Book of Conjurations
    SPELL_MAGIC_DART,
    SPELL_SEARING_RAY,
    SPELL_FULMINANT_PRISM,
},

{   // Book of Flames
    SPELL_FOXFIRE,
    SPELL_CONJURE_FLAME,
    SPELL_INNER_FLAME,
    SPELL_FLAME_WAVE,
},

{   // Book of Frost
    SPELL_FREEZE,
    SPELL_FROZEN_RAMPARTS,
    SPELL_HAILSTORM,
},

{   // Book of the Wilderness
    SPELL_SUMMON_FOREST,
    SPELL_SUMMON_MANA_VIPER,
    SPELL_SUMMON_CACTUS,
},

{   // Book of Fire
    SPELL_FIREBALL,
    SPELL_STARBURST,
    SPELL_IGNITION,
},

{   // Book of Ice
    SPELL_FREEZING_CLOUD,
    SPELL_SIMULACRUM,
    SPELL_POLAR_VORTEX,
},

{   // Book of Spatial Translocations
    SPELL_BLINK,
    SPELL_BECKONING,
    SPELL_MANIFOLD_ASSAULT,
},

{   // Book of Hexes
    SPELL_INNER_FLAME,
    SPELL_CAUSE_FEAR,
    SPELL_DISCORD,
},

{   // Young Poisoner's Handbook
    SPELL_STING,
    SPELL_POISONOUS_VAPOURS,
    SPELL_OLGREBS_TOXIC_RADIANCE,
},

{   // Book of Lightning
    SPELL_DISCHARGE,
    SPELL_LIGHTNING_BOLT,
    SPELL_MAXWELLS_COUPLING,
},

{   // Book of Death
    SPELL_ANIMATE_DEAD,
    SPELL_HAUNT,
    SPELL_INFESTATION,
},

{   // Book of Misfortune
    SPELL_CONFUSING_TOUCH,
    SPELL_VIOLENT_UNRAVELLING,
    SPELL_ENFEEBLE,
},

{   // Book of Changes
    SPELL_BEASTLY_APPENDAGE,
    SPELL_WEREBLOOD,
    SPELL_SPIDER_FORM,
},

{   // Book of Transfigurations
    SPELL_IRRADIATE,
    SPELL_BLADE_HANDS,
    SPELL_DRAGON_FORM,
},

{   // Fen Folio
    SPELL_SUMMON_FOREST,
    SPELL_NOXIOUS_BOG,
    SPELL_SUMMON_HYDRA,
},

{   // Book of Vapours
    SPELL_POISONOUS_VAPOURS,
    SPELL_CORPSE_ROT,
},

{   // Book of Necromancy
    SPELL_NECROTIZE,
    SPELL_VAMPIRIC_DRAINING,
    SPELL_AGONY,
},

{   // Book of Callings
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_CALL_CANINE_FAMILIAR,
    SPELL_SUMMON_GUARDIAN_GOLEM,
},

{   // Book of Maledictions
    SPELL_TUKIMAS_DANCE,
    SPELL_ANGUISH,
    SPELL_ENFEEBLE,
},

{   // Book of Air
    SPELL_SHOCK,
    SPELL_SWIFTNESS,
    SPELL_AIRSTRIKE,
},

{   // Book of the Sky
    SPELL_SUMMON_LIGHTNING_SPIRE,
    SPELL_STORM_FORM,
    SPELL_MAXWELLS_COUPLING,
},

{   // Book of the Warp
    SPELL_DISPERSAL,
    SPELL_DISJUNCTION,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Envenomations
    SPELL_SPIDER_FORM,
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_INTOXICATE,
},
#endif

{   // Book of Unlife
    SPELL_ANIMATE_DEAD,
    SPELL_BORGNJORS_VILE_CLUTCH,
    SPELL_DEATH_CHANNEL,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Control
    SPELL_ENGLACIATION,
},

{   // Book of Battle
    SPELL_WEREBLOOD,
    SPELL_OZOCUBUS_ARMOUR,
},
#endif

{   // Book of Geomancy
    SPELL_SANDBLAST,
    SPELL_PASSWALL,
    SPELL_STONE_ARROW,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Stone
    SPELL_LEDAS_LIQUEFACTION,
    SPELL_STATUE_FORM,
},

{   // Book of Wizardry
    SPELL_AGONY,
    SPELL_SPELLFORGED_SERVITOR,
},
#endif

{   // Book of Power
    SPELL_BATTLESPHERE,
    SPELL_IRON_SHOT,
    SPELL_SPELLFORGED_SERVITOR,
},

{   // Book of Cantrips
    SPELL_NECROTIZE,
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_APPORTATION,
},

{   // Book of Party Tricks
    SPELL_APPORTATION,
    SPELL_INTOXICATE,
},

#if TAG_MAJOR_VERSION == 34
{   // Akashic Record
    SPELL_DISPERSAL,
    SPELL_MALIGN_GATEWAY,
    SPELL_DISJUNCTION,
},
#endif

{   // Book of Debilitation
    SPELL_SLOW,
    SPELL_VAMPIRIC_DRAINING,
    SPELL_CONFUSING_TOUCH,
},

{   // Book of the Dragon
    SPELL_CAUSE_FEAR,
    SPELL_DRAGON_FORM,
    SPELL_DRAGON_CALL,
},

{   // Book of Burglary
    SPELL_PASSWALL,
    SPELL_HIBERNATION,
    SPELL_SWIFTNESS,
},

{   // Book of Dreams
    SPELL_HIBERNATION,
    SPELL_SPIDER_FORM,
    SPELL_ANGUISH,
},

{   // Book of Alchemy
    SPELL_SUBLIMATION_OF_BLOOD,
    SPELL_PETRIFY,
    SPELL_IRRADIATE,
},

{   // Book of Beasts
    SPELL_SUMMON_ICE_BEAST,
    SPELL_SUMMON_MANA_VIPER,
    SPELL_MONSTROUS_MENAGERIE,
},

{   // Book of Annihilations
    SPELL_CHAIN_LIGHTNING,
    SPELL_FIRE_STORM,
    SPELL_SHATTER,
},

{   // Grand Grimoire
    SPELL_MALIGN_GATEWAY,
    SPELL_SUMMON_HORRIBLE_THINGS,
},

{   // Necronomicon
    SPELL_BORGNJORS_REVIVIFICATION,
    SPELL_NECROMUTATION,
    SPELL_DEATHS_DOOR,
},

{ }, // BOOK_RANDART_LEVEL
{ }, // BOOK_RANDART_THEME
{ }, // BOOK_MANUAL
#if TAG_MAJOR_VERSION == 34
{ }, // BOOK_BUGGY_DESTRUCTION
#endif

{ // Book of Spectacle
    SPELL_DAZZLING_FLASH,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_STARBURST,
},

{ // Book of Winter
    SPELL_OZOCUBUS_ARMOUR,
    SPELL_ENGLACIATION,
    SPELL_SIMULACRUM,
},

{ // Book of Spheres
    SPELL_BATTLESPHERE,
    SPELL_FIREBALL,
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_IOOD,
},

{ // Book of Armaments
    SPELL_STONE_ARROW,
    SPELL_ANIMATE_ARMOUR,
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
},

#if TAG_MAJOR_VERSION == 34
{ // Book of Pain
    SPELL_NECROTIZE,
    SPELL_AGONY,
},
#endif

{ // Book of Decay
    SPELL_CORPSE_ROT,
    SPELL_DISPEL_UNDEAD,
    SPELL_DEATH_CHANNEL,
},

{ // Book of Displacement
    SPELL_BECKONING,
    SPELL_GRAVITAS,
    SPELL_TELEPORT_OTHER,
},

{ // Book of Rime
    SPELL_FROZEN_RAMPARTS,
    SPELL_ICE_FORM,
    SPELL_SUMMON_ICE_BEAST,
},

{ // Everburning Encyclopedia
    SPELL_CONJURE_FLAME,
    SPELL_IGNITE_POISON,
    SPELL_STICKY_FLAME,
},

{ // Book of Earth
    SPELL_LEDAS_LIQUEFACTION,
    SPELL_LRD,
    SPELL_STATUE_FORM,
},

{ // Ozocubu's Autobio
    SPELL_OZOCUBUS_ARMOUR,
    SPELL_OZOCUBUS_REFRIGERATION,
},

{ // Book of the Senses
    SPELL_DAZZLING_FLASH,
    SPELL_AGONY,
    SPELL_SILENCE,
},

{ // Book of the Moon
    SPELL_GOLUBRIAS_PASSAGE,
    SPELL_SILENCE,
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
},

{ // Book of Blasting
    SPELL_FULMINANT_PRISM,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_LRD,
},

{ // Book of Iron
    SPELL_ANIMATE_ARMOUR,
    SPELL_BLADE_HANDS,
    SPELL_IRON_SHOT,
},

{ // Inescapable Atlas
    SPELL_BLINK,
    SPELL_MANIFOLD_ASSAULT,
    SPELL_STORM_FORM,
},

{ // Book of the Tundra
    SPELL_HAILSTORM,
    SPELL_ICE_FORM,
    SPELL_SIMULACRUM,
},

{ // Book of Storms
    SPELL_AIRSTRIKE,
    SPELL_SUMMON_LIGHTNING_SPIRE,
    SPELL_LIGHTNING_BOLT,
},

{ // Book of Weapons
    SPELL_PORTAL_PROJECTILE,
    SPELL_BLADE_HANDS,
},

{ // Book of Sloth
    SPELL_PETRIFY,
    SPELL_ENGLACIATION,
    SPELL_STATUE_FORM,
},

{ // Book of Blood
    SPELL_SUBLIMATION_OF_BLOOD,
    SPELL_WEREBLOOD,
    SPELL_SUMMON_HYDRA,
},

{ // There-And-Back Book
    SPELL_GRAVITAS,
    SPELL_TELEPORT_OTHER,
    SPELL_DISPERSAL,
},

{ // Book of Dangerous Friends
    SPELL_SUMMON_GUARDIAN_GOLEM,
    SPELL_IOOD,
    SPELL_SPELLFORGED_SERVITOR,
},

{ // Book of Touch
    SPELL_DISCHARGE,
    SPELL_STICKY_FLAME,
    SPELL_DISPEL_UNDEAD,
},

{ // Book of Chaos
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_DISJUNCTION,
    SPELL_DISCORD,
},

{ // Unrestrained Analects
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_OZOCUBUS_REFRIGERATION,
    SPELL_IGNITION,
},

{ // Great Wizards, Vol. II
    SPELL_INTOXICATE,
    SPELL_BORGNJORS_VILE_CLUTCH,
    SPELL_NOXIOUS_BOG,
},

{ // Great Wizards, Vol. VII
    SPELL_TUKIMAS_DANCE,
    SPELL_GOLUBRIAS_PASSAGE,
    SPELL_VIOLENT_UNRAVELLING,
},

{ // Trismegistus Codex
    SPELL_IGNITE_POISON,
    SPELL_MEPHITIC_CLOUD,
    SPELL_FREEZING_CLOUD,
},

{ // Book of the Hunter
    SPELL_CALL_CANINE_FAMILIAR,
    SPELL_PORTAL_PROJECTILE,
    SPELL_LEDAS_LIQUEFACTION,
},

{ // Book of Scorching
    SPELL_SCORCH,
    SPELL_FLAME_WAVE,
    SPELL_SUMMON_CACTUS,
},

};

COMPILE_CHECK(ARRAYSZ(spellbook_templates) == NUM_BOOKS);
