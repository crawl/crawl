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
    SPELL_BLINK,
    SPELL_CALL_IMP,
},

{   // Book of Conjurations
    SPELL_MAGIC_DART,
    SPELL_SEARING_RAY,
    SPELL_FULMINANT_PRISM,
},

{   // Book of Flames
    SPELL_INNER_FLAME,
    SPELL_STICKY_FLAME,
    SPELL_FLAME_WAVE,
},

{   // Book of Frost
    SPELL_FREEZE,
    SPELL_FROZEN_RAMPARTS,
    SPELL_HAILSTORM,
},

{   // Book of the Wilderness
    SPELL_SUMMON_SEISMOSAURUS_EGG,
    SPELL_SUMMON_FOREST,
    SPELL_SUMMON_MANA_VIPER,
    SPELL_SUMMON_CACTUS,
},

{   // Book of Fire
    SPELL_FOXFIRE,
    SPELL_PLASMA_BEAM,
    SPELL_STARBURST,
},

{   // Book of Ice
    SPELL_FREEZING_CLOUD,
    SPELL_OZOCUBUS_REFRIGERATION,
    SPELL_POLAR_VORTEX,
},

{   // Book of Spatial Translocations
    SPELL_BLINK,
    SPELL_MOMENTUM_STRIKE,
    SPELL_BECKONING,
},

{   // Book of Hexes
    SPELL_ANGUISH,
    SPELL_CAUSE_FEAR,
    SPELL_ENFEEBLE,
},

{   // Young Poisoner's Handbook
    SPELL_POISONOUS_VAPOURS,
    SPELL_MERCURY_ARROW,
    SPELL_OLGREBS_TOXIC_RADIANCE,
},

{   // Book of Lightning
    SPELL_ELECTRIC_CHARGE,
    SPELL_ARCJOLT,
    SPELL_PLASMA_BEAM,
},

{   // Book of Death
    SPELL_ANIMATE_DEAD,
    SPELL_HAUNT,
    SPELL_INFESTATION,
},

{   // Book of Misfortune
    SPELL_JINXBITE,
    SPELL_VIOLENT_UNRAVELLING,
    SPELL_ENFEEBLE,
},

{   // Book of Spontaneous Combustion
    SPELL_INNER_FLAME,
    SPELL_DETONATION_CATALYST,
    SPELL_IRRADIATE,
    SPELL_IGNITION,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Transfigurations
    SPELL_IRRADIATE,
},
#endif

{   // Fen Folio
    SPELL_SUMMON_FOREST,
    SPELL_NOXIOUS_BOG,
    SPELL_SUMMON_HYDRA,
},

{   // My Sojourn Through Swamps
    SPELL_MERCURY_ARROW,
    SPELL_CLOCKWORK_BEE,
    SPELL_SURPRISING_CROCODILE,
},

{   // Book of Necromancy
    SPELL_SOUL_SPLINTER,
    SPELL_GRAVE_CLAW,
    SPELL_VAMPIRIC_DRAINING,
},

{   // Book of Callings
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_CALL_IMP,
    SPELL_CALL_CANINE_FAMILIAR,
    SPELL_SURPRISING_CROCODILE,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Maledictions
    SPELL_ANGUISH,
    SPELL_ENFEEBLE,
},
#endif

{   // Book of Air
    SPELL_SHOCK,
    SPELL_SWIFTNESS,
    SPELL_AIRSTRIKE,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of the Sky
    SPELL_MAXWELLS_COUPLING,
},
#endif

{   // Book of the Warp
    SPELL_DISPERSAL,
    SPELL_MANIFOLD_ASSAULT,
    SPELL_DISJUNCTION,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Envenomations
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_INTOXICATE,
},
#endif

{   // Book of Unlife
    SPELL_SOUL_SPLINTER,
    SPELL_MARTYRS_KNELL,
    SPELL_DEATH_CHANNEL,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Control
    SPELL_ENGLACIATION,
},
#endif

{   // Book of Battle
    SPELL_FUGUE_OF_THE_FALLEN,
    SPELL_OZOCUBUS_ARMOUR,
    SPELL_DETONATION_CATALYST,
    SPELL_MANIFOLD_ASSAULT,
},

{   // Book of Geomancy
    SPELL_SANDBLAST,
    SPELL_STONE_ARROW,
    SPELL_PASSWALL,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Stone
    SPELL_LEDAS_LIQUEFACTION,
},

{   // Book of Wizardry
    SPELL_CURSE_OF_AGONY,
    SPELL_SPELLSPARK_SERVITOR,
},
#endif

{   // Book of Power
    SPELL_BATTLESPHERE,
    SPELL_BOMBARD,
    SPELL_FORTRESS_BLAST,
    SPELL_SPELLSPARK_SERVITOR,
},

{   // Book of Cantrips
    SPELL_POISONOUS_VAPOURS,
    SPELL_KINETIC_GRAPNEL,
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_APPORTATION,
},

{   // Book of Party Tricks
    SPELL_APPORTATION,
    SPELL_JINXBITE,
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
    SPELL_CONFUSING_TOUCH,
    SPELL_PUTREFACTION,
},

{   // Book of the Dragon
    SPELL_CAUSE_FEAR,
    SPELL_FIREBALL,
    SPELL_DRAGON_CALL,
},

{   // Book of Burglary
    SPELL_HIBERNATION,
    SPELL_SWIFTNESS,
    SPELL_PASSWALL,
},

{   // Book of Dreams
    SPELL_HIBERNATION,
    SPELL_FUGUE_OF_THE_FALLEN,
    SPELL_ANGUISH,
},

{   // Book of Transmutation
    SPELL_PETRIFY,
    SPELL_IRRADIATE,
    SPELL_FULSOME_FUSILLADE,
},

{   // Book of Beasts
    SPELL_SUMMON_ICE_BEAST,
    SPELL_SUMMON_SEISMOSAURUS_EGG,
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
    SPELL_HAUNT,
    SPELL_BORGNJORS_REVIVIFICATION,
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
    SPELL_STARBURST,
    SPELL_FULSOME_FUSILLADE,
},

{ // Book of Winter
    SPELL_FROZEN_RAMPARTS,
    SPELL_OZOCUBUS_ARMOUR,
    SPELL_ENGLACIATION,
},

{ // Book of Spheres
    SPELL_BOULDER,
    SPELL_BATTLESPHERE,
    SPELL_FIREBALL,
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_IOOD,
},

{ // Book of Armaments
    SPELL_STONE_ARROW,
    SPELL_RENDING_BLADE,
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
},

#if TAG_MAJOR_VERSION == 34
{ // Book of Pain
    SPELL_SOUL_SPLINTER,
    SPELL_CURSE_OF_AGONY,
},
#endif

{ // Book of Decay
    SPELL_PUTREFACTION,
    SPELL_DISPEL_UNDEAD,
    SPELL_MARTYRS_KNELL,
},

{ // Book of Displacement
    SPELL_BECKONING,
    SPELL_TELEPORT_OTHER,
    SPELL_PILEDRIVER,
},

#if TAG_MAJOR_VERSION == 34
{ // Book of Rime
    SPELL_FROZEN_RAMPARTS,
    SPELL_SUMMON_ICE_BEAST,
},
#endif

{ // Everburning Encyclopedia
    SPELL_IGNITE_POISON,
    SPELL_STICKY_FLAME,
    SPELL_IGNITION,
},

{ // Book of Earth
    SPELL_LEDAS_LIQUEFACTION,
    SPELL_LRD,
    SPELL_PERMAFROST_ERUPTION,
},

#if TAG_MAJOR_VERSION == 34
{ // Ozocubu's Autobio
    SPELL_OZOCUBUS_ARMOUR,
    SPELL_OZOCUBUS_REFRIGERATION,
},
#endif

{ // Book of the Senses
    SPELL_DAZZLING_FLASH,
    SPELL_MEPHITIC_CLOUD,
    SPELL_SILENCE,
    SPELL_CURSE_OF_AGONY,
},

{ // Book of the Moon
    SPELL_GOLUBRIAS_PASSAGE,
    SPELL_SILENCE,
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
},

{ // Book of Blasting
    SPELL_BLASTMOTE,
    SPELL_FULMINANT_PRISM,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_MONARCH_BOMB,
},

{ // Book of Iron
    SPELL_AWAKEN_ARMOUR,
    SPELL_LRD,
    SPELL_MAGNAVOLT,
},

#if TAG_MAJOR_VERSION == 34
{ // Inescapable Atlas
    SPELL_BLINK,
    SPELL_MANIFOLD_ASSAULT,
},
#endif

{ // Book of the Tundra
    SPELL_HAILSTORM,
    SPELL_SUMMON_ICE_BEAST,
    SPELL_PERMAFROST_ERUPTION,
},

{ // Book of Storms
    SPELL_DISCHARGE,
    SPELL_AIRSTRIKE,
    SPELL_FORGE_LIGHTNING_SPIRE,
},

{ // Book of Weapons
    SPELL_TUKIMAS_DANCE,
    SPELL_DIMENSIONAL_BULLSEYE,
    SPELL_RENDING_BLADE,
    SPELL_PLATINUM_PARAGON,
},

{ // Book of Sloth
    SPELL_LEDAS_LIQUEFACTION,
    SPELL_PETRIFY,
    SPELL_ENGLACIATION,
},

{ // Book of Blood
    SPELL_SUBLIMATION_OF_BLOOD,
    SPELL_IGNITE_POISON,
    SPELL_SUMMON_HYDRA,
},

{ // There-And-Back Book
    SPELL_TELEPORT_OTHER,
    SPELL_GELLS_GAVOTTE,
    SPELL_DISPERSAL,
},

{ // Book of Dangerous Friends
    SPELL_FORGE_BLAZEHEART_GOLEM,
    SPELL_IOOD,
    SPELL_SPELLSPARK_SERVITOR,
},

{ // Book of Touch
    SPELL_KISS_OF_DEATH,
    SPELL_CONFUSING_TOUCH,
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
    SPELL_DISCORD,
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
    SPELL_MEPHITIC_CLOUD,
    SPELL_FREEZING_CLOUD,
    SPELL_HELLFIRE_MORTAR,
},

{ // Book of the Hunter
    SPELL_SIGIL_OF_BINDING,
    SPELL_CALL_CANINE_FAMILIAR,
    SPELL_DIMENSIONAL_BULLSEYE,
},

{ // Book of Scorching
    SPELL_SCORCH,
    SPELL_FLAME_WAVE,
    SPELL_SUMMON_CACTUS,
},

{ // Maxwell's Memoranda
    SPELL_PILEDRIVER,
    SPELL_MAGNAVOLT,
    SPELL_MAXWELLS_COUPLING,
},

{ // Book of Movement
    SPELL_BLASTMOTE,
    SPELL_ELECTRIC_CHARGE,
    SPELL_GELLS_GAVOTTE,
},

{ // Book of Wicked Creation
    SPELL_SIMULACRUM,
    SPELL_DEATH_CHANNEL,
    SPELL_RIMEBLIGHT,
},

{ // Compendium of Siegecraft
    SPELL_BOULDER,
    SPELL_HOARFROST_CANNONADE,
    SPELL_BOMBARD,
    SPELL_HELLFIRE_MORTAR,
},

{ // Book of Maladies
    SPELL_SUBLIMATION_OF_BLOOD,
    SPELL_CURSE_OF_AGONY,
    SPELL_RIMEBLIGHT,
},

{ // Codex of Conductivity
    SPELL_FORGE_LIGHTNING_SPIRE,
    SPELL_ARCJOLT,
    SPELL_MAXWELLS_COUPLING,
},

{ // Handbook of Applied Construction
    SPELL_SPIKE_LAUNCHER,
    SPELL_CLOCKWORK_BEE,
    SPELL_FORGE_BLAZEHEART_GOLEM,
},

{ // Book of the Fortress
    SPELL_AWAKEN_ARMOUR,
    SPELL_PHALANX_BEETLE,
    SPELL_FORTRESS_BLAST,
},

{ // Treatise on Traps
    SPELL_SPIKE_LAUNCHER,
    SPELL_SIGIL_OF_BINDING,
    SPELL_DIAMOND_SAWBLADES,
},

{ // Book of Cryoformation
    SPELL_HOARFROST_CANNONADE,
    SPELL_SIMULACRUM,
    SPELL_SPLINTERFROST_SHELL,
},

{ // Book of the Grave
    SPELL_GRAVE_CLAW,
    SPELL_ANIMATE_DEAD,
    SPELL_BORGNJORS_VILE_CLUTCH,
},

{ // Book of Metalworking
    SPELL_PERCUSSIVE_TEMPERING,
    SPELL_WALKING_ALEMBIC,
    SPELL_PHALANX_BEETLE,
},

{ // Book of Duality
    SPELL_VAMPIRIC_DRAINING,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_PERCUSSIVE_TEMPERING,
    SPELL_SPLINTERFROST_SHELL,
},

{ // Book of Contraptions
    SPELL_WALKING_ALEMBIC,
    SPELL_MONARCH_BOMB,
    SPELL_DIAMOND_SAWBLADES,
}

};

COMPILE_CHECK(ARRAYSZ(spellbook_templates) == NUM_BOOKS);
