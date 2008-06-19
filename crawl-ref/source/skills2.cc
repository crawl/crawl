/*
 *  File:       skills2.cc
 *  Summary:    More skill related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *     01aug2000 jmf             RESTORED TITLES TO THEIR FORMER GLORY! MUA-HA!
 * <4> 22Jul2000 GDL             added warning for low throwing skill
 *                               Changed a few titles.
 * <3> 5/20/99   BWR             Changed Trapper titles, avoided
 *                               overflow on the weapon skill
 *                               column.
 * <2> -/--/--   WL              Extensive mods from Wladimir van der Laan.
 * <1> -/--/--   LRH             Created
 */

#include "AppHdr.h"
#include "skills2.h"

#include <algorithm>
#include <string>
#include <sstream>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef DOS
 #include <conio.h>
#endif

#include "describe.h"
#include "externs.h"
#include "fight.h"
#include "itemprop.h"
#include "menu.h"
#include "player.h"
#include "randart.h"
#include "stuff.h"
#include "transfor.h"
#include "tutorial.h"
#include "view.h"

// Basic goals for titles:
// The higher titles must come last.
// Referring to the skill itself is fine ("Transmuter") but not impressive.
// No overlaps, high diversity.

// Replace @genus@ with lowercase genus, @Genus@ with uppercase, and %s
// with special cases defined below, including but not limited to species.

// NOTE:  Even though %s could be used with most of these, remember that
// the character's race will be listed on the next line.  It's only really
// intended for cases where things might be really awkward without it. -- bwr

// NOTE: If a skill name is changed, remember to also adapt the database entry.
const char *skills[50][6] =
{
  //  Skill name        levels 1-7       levels 8-14        levels 15-20       levels 21-26      level 27
    {"Fighting",       "Skirmisher",    "Fighter",         "Warrior",         "Slayer",         "Conqueror"},          //  0
    {"Short Blades",   "Cutter",        "Slicer",          "Swashbuckler",    "Blademaster",    "Eviscerator"},
    {"Long Blades",    "Slasher",       "Carver",          "Fencer",          "@Genus@ Blade",  "Swordmaster"},
    {NULL},  //  3- was: great swords {dlb}
    {"Axes",           "Chopper",       "Cleaver",         "Hacker",          "Severer",        "Executioner"},
    {"Maces & Flails", "Cudgeler",      "Basher",          "Bludgeoner",      "Shatterer",      "Skullcrusher"},       //  5
    {"Polearms",       "Poker",         "Spear-Bearer",    "Impaler",         "Phalangite",     "@Genus@ Porcupine"},
    {"Staves",         "Twirler",       "Cruncher",        "Stickfighter",    "Pulverizer",     "Chief of Staff"},
    {"Slings",         "Vandal",        "Slinger",         "Whirler",         "Slingshot",      "@Genus@ Catapult"},
    {"Bows",           "Shooter",       "Archer",          "Marks@genus@",    "Crack Shot",     "Merry %s"},
    {"Crossbows",      "Bolt Thrower",  "Quickloader",     "Sharpshooter",    "Sniper",         "@Genus@ Arbalest"},   // 10
    {"Darts",          "Dart Thrower",  "Hurler",          "Hedgehog",        "Darts Champion", "Perforator"},
    {"Throwing",       "Chucker",       "Thrower",         "Deadly Accurate", "Hawkeye",        "@Genus@ Ballista"},
    {"Armour",         "Covered",       "Protected",       "Tortoise",        "Impregnable",    "Invulnerable"},
    {"Dodging",        "Ducker",        "Nimble",          "Spry",            "Acrobat",        "Intangible"},
    {"Stealth",        "Sneak",         "Covert",          "Unseen",          "Imperceptible",  "Ninja"},              // 15
    {"Stabbing",       "Miscreant",     "Blackguard",      "Backstabber",     "Cutthroat",      "Politician"},
    {"Shields",        "Shield-Bearer", "Hoplite",         "Blocker",         "Peltast",        "@Genus@ Barricade"},
    {"Traps & Doors",  "Scout",         "Disarmer",        "Vigilant",        "Perceptive",     "Dungeon Master"},
    // STR based fighters, for DEX/martial arts titles see below
    {"Unarmed Combat", "Ruffian",       "Grappler",        "Brawler",         "Wrestler",       "%sweight Champion"},

    {NULL},   // 20- empty
    {NULL},   // 21- empty
    {NULL},   // 22- empty
    {NULL},   // 23- empty
    {NULL},   // 24- empty

    {"Spellcasting",   "Magician",      "Thaumaturge",     "Eclecticist",     "Sorcerer",       "Archmage"},           // 25
    {"Conjurations",   "Ruinous",       "Conjurer",        "Destroyer",       "Devastator",     "Annihilator"},
    {"Enchantments",   "Charm-Maker",   "Infuser",         "Bewitcher",       "Enchanter",      "Spellbinder"},
    {"Summonings",     "Caller",        "Summoner",        "Convoker",        "Demonologist",   "Hellbinder"},
    {"Necromancy",     "Grave Robber",  "Reanimator",      "Necromancer",     "Thanatomancer",  "%s of Death"},
    {"Translocations", "Grasshopper",   "Placeless %s",    "Blinker",         "Portalist",      "Plane %s"},           // 30
    {"Transmigration", "Changer",       "Transmogrifier",  "Alchemist",       "Malleable",      "Shapeless %s"},
    {"Divinations",    "Seer",          "Soothsayer",      "Diviner",         "Augur",          "Oracle"},

    {"Fire Magic",     "Firebug",       "Arsonist",        "Scorcher",        "Pyromancer",     "Infernalist"},
    {"Ice Magic",      "Chiller",       "Frost Mage",      "Gelid",           "Cryomancer",     "Englaciator"},
    {"Air Magic",      "Gusty",         "Cloud Mage",      "Aerator",         "Anemomancer",    "Meteorologist"},      // 35
    {"Earth Magic",    "Digger",        "Geomancer",       "Earth Mage",      "Metallomancer",  "Petrodigitator"},
    {"Poison Magic",   "Stinger",       "Tainter",         "Polluter",        "Contaminator",   "Envenomancer"},

    // for titles for godless characters, see below
    {"Invocations",    "Believer",      "Agitator",        "Worldly Agent",   "Theurge",        "Avatar"},
    {"Evocations",     "Charlatan",     "Prestidigitator", "Fetichist",       "Evocator",       "Talismancer"},        // 39

/*NOTE: If more skills are added, must change ranges in level_change() in player.cc */

    {NULL},   // 40- empty
    {NULL},   // 41- empty
    {NULL},   // 42- empty
    {NULL},   // 43- empty
    {NULL},   // 44- empty
    {NULL},   // 45- empty
    {NULL},   // 46- empty
    {NULL},   // 47- empty
    {NULL},   // 48- empty
    {NULL}    // 49- empty  {end of array}
};

const char *martial_arts_titles[6] =
    {"Unarmed Combat", "Insei", "Martial Artist", "Black Belt", "Sensei", "Grand Master"};

const char *atheist_inv_titles[6] =
    {"Invocations", "Unbeliever", "Agnostic", "Dissident", "Heretic", "Apostate"};

// The Human aptitude set of 100 for all skills allows to define all other
// species relative to Humans.

// Spellcasting and In/Evocations form the exceptions here:
// Spellcasting skill is actually about 130%, the other two about 75%.
const int spec_skills[ NUM_SPECIES ][40] =
{
    {                           // SP_HUMAN (1)
     100,                       // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     100,                       // SK_SLINGS
     100,                       // SK_BOWS
     100,                       // SK_CROSSBOWS
     100,                       // SK_DARTS
     100,                       // SK_THROWING
     100,                       // SK_ARMOUR
     100,                       // SK_DODGING
     100,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     100,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_HIGH_ELF (3)
     100,                       // SK_FIGHTING
     70,                        // SK_SHORT_BLADES
     70,                        // SK_LONG_BLADES
     115,                       // SK_UNUSED_1
     130,                       // SK_AXES
     150,                       // SK_MACES_FLAILS
     150,                       // SK_POLEARMS
     100,                       // SK_STAVES
     140,                       // SK_SLINGS
     60,                        // SK_BOWS
     100,                       // SK_CROSSBOWS
     90,                        // SK_DARTS
     80,                        // SK_THROWING
     110,                       // SK_ARMOUR
     90,                        // SK_DODGING
     90,                        // SK_STEALTH
     110,                       // SK_STABBING
     110,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     130,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (70 * 130) / 100,          // SK_SPELLCASTING
     90,                        // SK_CONJURATIONS
     70,                        // SK_ENCHANTMENTS
     110,                       // SK_SUMMONINGS
     130,                       // SK_NECROMANCY
     90,                        // SK_TRANSLOCATIONS
     90,                        // SK_TRANSMIGRATION
     110,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     70,                        // SK_AIR_MAGIC
     130,                       // SK_EARTH_MAGIC
     130,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_GREY_ELF (4)
     140,                       // SK_FIGHTING
     90,                        // SK_SHORT_BLADES
     95,                        // SK_LONG_BLADES
     120,                       // SK_UNUSED_1
     140,                       // SK_AXES
     160,                       // SK_MACES_FLAILS
     160,                       // SK_POLEARMS
     100,                       // SK_STAVES
     130,                       // SK_SLINGS
     70,                        // SK_BOWS
     100,                       // SK_CROSSBOWS
     90,                        // SK_DARTS
     80,                        // SK_THROWING
     140,                       // SK_ARMOUR
     75,                        // SK_DODGING
     70,                        // SK_STEALTH
     100,                       // SK_STABBING
     140,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     130,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (60 * 130) / 100,          // SK_SPELLCASTING
     90,                        // SK_CONJURATIONS
     50,                        // SK_ENCHANTMENTS
     90,                        // SK_SUMMONINGS
     130,                       // SK_NECROMANCY
     80,                        // SK_TRANSLOCATIONS
     80,                        // SK_TRANSMIGRATION
     80,                        // SK_DIVINATIONS
     90,                        // SK_FIRE_MAGIC
     90,                        // SK_ICE_MAGIC
     60,                        // SK_AIR_MAGIC
     150,                       // SK_EARTH_MAGIC
     110,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (90 * 75) / 100,           // SK_EVOCATIONS
     },

    {                           // SP_DEEP_ELF (5)
     150,                       // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     105,                       // SK_LONG_BLADES
     120,                       // SK_UNUSED_1
     150,                       // SK_AXES
     165,                       // SK_MACES_FLAILS
     165,                       // SK_POLEARMS
     100,                       // SK_STAVES
     135,                       // SK_SLINGS
     75,                        // SK_BOWS
     75,                        // SK_CROSSBOWS
     75,                        // SK_DARTS
     80,                        // SK_THROWING
     140,                       // SK_ARMOUR
     70,                        // SK_DODGING
     65,                        // SK_STEALTH
     80,                        // SK_STABBING
     140,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     130,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (55 * 130) / 100,          // SK_SPELLCASTING
     80,                        // SK_CONJURATIONS
     50,                        // SK_ENCHANTMENTS
     80,                        // SK_SUMMONINGS
     70,                        // SK_NECROMANCY
     75,                        // SK_TRANSLOCATIONS
     75,                        // SK_TRANSMIGRATION
     75,                        // SK_DIVINATIONS
     90,                        // SK_FIRE_MAGIC
     90,                        // SK_ICE_MAGIC
     80,                        // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     80,                        // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (90 * 75) / 100,           // SK_EVOCATIONS
     },

    {                           // SP_SLUDGE_ELF (6)
     80,                        // SK_FIGHTING
     110,                       // SK_SHORT_BLADES
     110,                       // SK_LONG_BLADES
     110,                       // SK_UNUSED_1
     130,                       // SK_AXES
     140,                       // SK_MACES_FLAILS
     140,                       // SK_POLEARMS
     100,                       // SK_STAVES
     100,                       // SK_SLINGS
     100,                       // SK_BOWS
     100,                       // SK_CROSSBOWS
     100,                       // SK_DARTS
     70,                        // SK_THROWING
     140,                       // SK_ARMOUR
     70,                        // SK_DODGING
     75,                        // SK_STEALTH
     100,                       // SK_STABBING
     130,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     80,                        // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (70 * 130) / 100,          // SK_SPELLCASTING
     130,                       // SK_CONJURATIONS
     130,                       // SK_ENCHANTMENTS
     90,                        // SK_SUMMONINGS
     90,                        // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     60,                        // SK_TRANSMIGRATION
     130,                       // SK_DIVINATIONS
     80,                        // SK_FIRE_MAGIC
     80,                        // SK_ICE_MAGIC
     80,                        // SK_AIR_MAGIC
     80,                        // SK_EARTH_MAGIC
     80,                        // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (110 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_MOUNTAIN_DWARF (8)
     70,                        // SK_FIGHTING
     80,                        // SK_SHORT_BLADES
     90,                        // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     65,                        // SK_AXES
     70,                        // SK_MACES_FLAILS
     110,                       // SK_POLEARMS
     120,                       // SK_STAVES
     120,                       // SK_SLINGS
     150,                       // SK_BOWS
     90,                        // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     60,                        // SK_ARMOUR
     110,                       // SK_DODGING
     150,                       // SK_STEALTH
     130,                       // SK_STABBING
     70,                        // SK_SHIELDS
     80,                        // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (160 * 130) / 100,         // SK_SPELLCASTING
     120,                       // SK_CONJURATIONS
     150,                       // SK_ENCHANTMENTS
     150,                       // SK_SUMMONINGS
     160,                       // SK_NECROMANCY
     150,                       // SK_TRANSLOCATIONS
     120,                       // SK_TRANSMIGRATION
     130,                       // SK_DIVINATIONS
     70,                        // SK_FIRE_MAGIC
     130,                       // SK_ICE_MAGIC
     150,                       // SK_AIR_MAGIC
     70,                        // SK_EARTH_MAGIC
     130,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (60 * 75) / 100,           // SK_EVOCATIONS
     },

    {                           // SP_HALFLING (9)
     120,                       // SK_FIGHTING
     60,                        // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     130,                       // SK_UNUSED_1
     120,                       // SK_AXES
     150,                       // SK_MACES_FLAILS
     160,                       // SK_POLEARMS
     130,                       // SK_STAVES
     50,                        // SK_SLINGS
     70,                        // SK_BOWS
     90,                        // SK_CROSSBOWS
     50,                        // SK_DARTS
     60,                        // SK_THROWING
     150,                       // SK_ARMOUR
     70,                        // SK_DODGING
     60,                        // SK_STEALTH
     70,                        // SK_STABBING
     130,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     140,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (130 * 130) / 100,         // SK_SPELLCASTING
     130,                       // SK_CONJURATIONS
     100,                       // SK_ENCHANTMENTS
     120,                       // SK_SUMMONINGS
     150,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     150,                       // SK_TRANSMIGRATION
     140,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     90,                        // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     120,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (90 * 75) / 100,           // SK_EVOCATIONS
     },

    {                           // SP_HILL_ORC (10)
     70,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     80,                        // SK_LONG_BLADES
     70,                        // SK_UNUSED_1
     70,                        // SK_AXES
     80,                        // SK_MACES_FLAILS
     80,                        // SK_POLEARMS
     110,                       // SK_STAVES
     130,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     130,                       // SK_DARTS
     100,                       // SK_THROWING
     90,                        // SK_ARMOUR
     140,                       // SK_DODGING
     150,                       // SK_STEALTH
     100,                       // SK_STABBING
     80,                        // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     90,                        // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (150 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     120,                       // SK_ENCHANTMENTS
     120,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     150,                       // SK_TRANSLOCATIONS
     160,                       // SK_TRANSMIGRATION
     160,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     150,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     110,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_KOBOLD (11)
     80,                        // SK_FIGHTING
     60,                        // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     120,                       // SK_UNUSED_1
     110,                       // SK_AXES
     140,                       // SK_MACES_FLAILS
     150,                       // SK_POLEARMS
     110,                       // SK_STAVES
     70,                        // SK_SLINGS
     80,                        // SK_BOWS
     90,                        // SK_CROSSBOWS
     50,                        // SK_DARTS
     60,                        // SK_THROWING
     140,                       // SK_ARMOUR
     70,                        // SK_DODGING
     60,                        // SK_STEALTH
     70,                        // SK_STABBING
     130,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (110 * 130) / 100,         // SK_SPELLCASTING
     110,                       // SK_CONJURATIONS
     110,                       // SK_ENCHANTMENTS
     105,                       // SK_SUMMONINGS
     105,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     110,                       // SK_TRANSMIGRATION
     130,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (80 * 75) / 100,           // SK_EVOCATIONS
     },

    {                           // SP_MUMMY (12)
     100,                       // SK_FIGHTING
     140,                       // SK_SHORT_BLADES
     140,                       // SK_LONG_BLADES
     140,                       // SK_UNUSED_1
     140,                       // SK_AXES
     140,                       // SK_MACES_FLAILS
     140,                       // SK_POLEARMS
     140,                       // SK_STAVES
     140,                       // SK_SLINGS
     140,                       // SK_BOWS
     140,                       // SK_CROSSBOWS
     140,                       // SK_DARTS
     140,                       // SK_THROWING
     140,                       // SK_ARMOUR
     140,                       // SK_DODGING
     140,                       // SK_STEALTH
     140,                       // SK_STABBING
     140,                       // SK_SHIELDS
     140,                       // SK_TRAPS_DOORS
     140,                       // SK_UNARMED_COMBAT
     140,                       // undefined
     140,                       // undefined
     140,                       // undefined
     140,                       // undefined
     140,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     140,                       // SK_CONJURATIONS
     140,                       // SK_ENCHANTMENTS
     140,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     140,                       // SK_TRANSLOCATIONS
     140,                       // SK_TRANSMIGRATION
     140,                       // SK_DIVINATIONS
     140,                       // SK_FIRE_MAGIC
     140,                       // SK_ICE_MAGIC
     140,                       // SK_AIR_MAGIC
     140,                       // SK_EARTH_MAGIC
     140,                       // SK_POISON_MAGIC
     (140 * 75) / 100,          // SK_INVOCATIONS
     (140 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_NAGA (13)
     100,                       // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     120,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     150,                       // SK_ARMOUR
     150,                       // SK_DODGING
     40,                        // SK_STEALTH
     100,                       // SK_STABBING
     140,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     100,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     60,                        // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_GNOME (14)
     100,                       // SK_FIGHTING
     75,                        // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     130,                       // SK_UNUSED_1
     100,                       // SK_AXES
     130,                       // SK_MACES_FLAILS
     140,                       // SK_POLEARMS
     130,                       // SK_STAVES
     80,                        // SK_SLINGS
     100,                       // SK_BOWS
     90,                        // SK_CROSSBOWS
     60,                        // SK_DARTS
     100,                       // SK_THROWING
     150,                       // SK_ARMOUR
     70,                        // SK_DODGING
     70,                        // SK_STEALTH
     80,                        // SK_STABBING
     120,                       // SK_SHIELDS
     70,                        // SK_TRAPS_DOORS
     110,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (120 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     100,                       // SK_ENCHANTMENTS
     110,                       // SK_SUMMONINGS
     130,                       // SK_NECROMANCY
     130,                       // SK_TRANSLOCATIONS
     120,                       // SK_TRANSMIGRATION
     120,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     170,                       // SK_AIR_MAGIC
     60,                        // SK_EARTH_MAGIC
     130,                       // SK_POISON_MAGIC
     (120 * 75) / 100,          // SK_INVOCATIONS
     (60 * 75) / 100,           // SK_EVOCATIONS
     },

    {                           // SP_OGRE (15)
     100,                       // SK_FIGHTING
     140,                       // SK_SHORT_BLADES
     120,                       // SK_LONG_BLADES
     110,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     110,                       // SK_POLEARMS
     120,                       // SK_STAVES
     150,                       // SK_SLINGS
     150,                       // SK_BOWS
     180,                       // SK_CROSSBOWS
     150,                       // SK_DARTS
     100,                       // SK_THROWING
     140,                       // SK_ARMOUR
     150,                       // SK_DODGING
     200,                       // SK_STEALTH
     150,                       // SK_STABBING
     110,                       // SK_SHIELDS
     200,                       // SK_TRAPS_DOORS
     130,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (220 * 130) / 100,         // SK_SPELLCASTING
     180,                       // SK_CONJURATIONS
     220,                       // SK_ENCHANTMENTS
     200,                       // SK_SUMMONINGS
     150,                       // SK_NECROMANCY
     200,                       // SK_TRANSLOCATIONS
     200,                       // SK_TRANSMIGRATION
     200,                       // SK_DIVINATIONS
     150,                       // SK_FIRE_MAGIC
     150,                       // SK_ICE_MAGIC
     200,                       // SK_AIR_MAGIC
     120,                       // SK_EARTH_MAGIC
     150,                       // SK_POISON_MAGIC
     (130 * 75) / 100,          // SK_INVOCATIONS
     (170 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_TROLL (16)
     140,                       // SK_FIGHTING
     150,                       // SK_SHORT_BLADES
     150,                       // SK_LONG_BLADES
     150,                       // SK_UNUSED_1
     150,                       // SK_AXES
     130,                       // SK_MACES_FLAILS
     150,                       // SK_POLEARMS
     150,                       // SK_STAVES
     180,                       // SK_SLINGS
     180,                       // SK_BOWS
     180,                       // SK_CROSSBOWS
     180,                       // SK_DARTS
     130,                       // SK_THROWING
     150,                       // SK_ARMOUR
     130,                       // SK_DODGING
     250,                       // SK_STEALTH
     150,                       // SK_STABBING
     150,                       // SK_SHIELDS
     200,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (200 * 130) / 100,         // SK_SPELLCASTING
     160,                       // SK_CONJURATIONS
     200,                       // SK_ENCHANTMENTS
     160,                       // SK_SUMMONINGS
     150,                       // SK_NECROMANCY
     160,                       // SK_TRANSLOCATIONS
     160,                       // SK_TRANSMIGRATION
     200,                       // SK_DIVINATIONS
     160,                       // SK_FIRE_MAGIC
     160,                       // SK_ICE_MAGIC
     200,                       // SK_AIR_MAGIC
     120,                       // SK_EARTH_MAGIC
     160,                       // SK_POISON_MAGIC
     (150 * 75) / 100,          // SK_INVOCATIONS
     (180 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_OGRE_MAGE (17)
     100,                       // SK_FIGHTING
     110,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     150,                       // SK_SLINGS
     150,                       // SK_BOWS
     150,                       // SK_CROSSBOWS
     150,                       // SK_DARTS
     150,                       // SK_THROWING
     170,                       // SK_ARMOUR
     130,                       // SK_DODGING
     100,                       // SK_STEALTH
     130,                       // SK_STABBING
     150,                       // SK_SHIELDS
     150,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (70 * 130) / 100,          // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     80,                        // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,           // SK_EVOCATIONS
     },

    {                           // SP_RED_DRACONIAN (18)
     90,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     200,                       // SK_ARMOUR
     120,                       // SK_DODGING
     120,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     120,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     70,                        // SK_FIRE_MAGIC
     135,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_WHITE_DRACONIAN (19)
     90,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     200,                       // SK_ARMOUR
     120,                       // SK_DODGING
     120,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     120,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     135,                       // SK_FIRE_MAGIC
     70,                        // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_GREEN_DRACONIAN (20)
     90,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     200,                       // SK_ARMOUR
     120,                       // SK_DODGING
     120,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     120,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     70,                        // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_YELLOW_DRACONIAN (21)
     90,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     200,                       // SK_ARMOUR
     120,                       // SK_DODGING
     120,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     120,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_GREY_DRACONIAN (22)
     90,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     200,                       // SK_ARMOUR
     120,                       // SK_DODGING
     120,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     120,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_BLACK_DRACONIAN (23)
     90,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     200,                       // SK_ARMOUR
     120,                       // SK_DODGING
     120,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     120,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     70,                        // SK_AIR_MAGIC
     135,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_PURPLE_DRACONIAN (24)
     90,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     200,                       // SK_ARMOUR
     120,                       // SK_DODGING
     120,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (70 * 130) / 100,          // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     90,                        // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (90 * 75) / 100,           // SK_EVOCATIONS
     },

    {                           // SP_MOTTLED_DRACONIAN (25)
     90,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     200,                       // SK_ARMOUR
     120,                       // SK_DODGING
     120,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     120,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     80,                        // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_PALE_DRACONIAN (26)
     90,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     200,                       // SK_ARMOUR
     120,                       // SK_DODGING
     120,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     120,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     90,                        // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     90,                        // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (90 * 75) / 100,           // SK_EVOCATIONS
     },

    {                           // SP_BASE_DRACONIAN (29)
     90,                        // SK_FIGHTING
     100,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     100,                       // SK_AXES
     100,                       // SK_MACES_FLAILS
     100,                       // SK_POLEARMS
     100,                       // SK_STAVES
     120,                       // SK_SLINGS
     120,                       // SK_BOWS
     120,                       // SK_CROSSBOWS
     120,                       // SK_DARTS
     120,                       // SK_THROWING
     200,                       // SK_ARMOUR
     120,                       // SK_DODGING
     120,                       // SK_STEALTH
     100,                       // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     120,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     100,                       // SK_TRANSLOCATIONS
     100,                       // SK_TRANSMIGRATION
     100,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     100,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_CENTAUR (30)
     100,                       // SK_FIGHTING
     120,                       // SK_SHORT_BLADES
     110,                       // SK_LONG_BLADES
     110,                       // SK_UNUSED_1
     110,                       // SK_AXES
     110,                       // SK_MACES_FLAILS
     110,                       // SK_POLEARMS
     110,                       // SK_STAVES
     75,                        // SK_SLINGS
     60,                        // SK_BOWS
     85,                        // SK_CROSSBOWS
     80,                        // SK_DARTS
     60,                        // SK_THROWING
     180,                       // SK_ARMOUR
     170,                       // SK_DODGING
     200,                       // SK_STEALTH
     170,                       // SK_STABBING
     180,                       // SK_SHIELDS
     150,                       // SK_TRAPS_DOORS
     100,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (140 * 130) / 100,         // SK_SPELLCASTING
     120,                       // SK_CONJURATIONS
     110,                       // SK_ENCHANTMENTS
     120,                       // SK_SUMMONINGS
     120,                       // SK_NECROMANCY
     120,                       // SK_TRANSLOCATIONS
     120,                       // SK_TRANSMIGRATION
     130,                       // SK_DIVINATIONS
     120,                       // SK_FIRE_MAGIC
     120,                       // SK_ICE_MAGIC
     120,                       // SK_AIR_MAGIC
     120,                       // SK_EARTH_MAGIC
     130,                       // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (130 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_DEMIGOD (31)
     110,                       // SK_FIGHTING
     110,                       // SK_SHORT_BLADES
     110,                       // SK_LONG_BLADES
     110,                       // SK_UNUSED_1
     110,                       // SK_AXES
     110,                       // SK_MACES_FLAILS
     110,                       // SK_POLEARMS
     110,                       // SK_STAVES
     110,                       // SK_SLINGS
     110,                       // SK_BOWS
     110,                       // SK_CROSSBOWS
     110,                       // SK_DARTS
     110,                       // SK_THROWING
     110,                       // SK_ARMOUR
     110,                       // SK_DODGING
     110,                       // SK_STEALTH
     110,                       // SK_STABBING
     110,                       // SK_SHIELDS
     110,                       // SK_TRAPS_DOORS
     110,                       // SK_UNARMED_COMBAT
     110,                       // undefined
     110,                       // undefined
     110,                       // undefined
     110,                       // undefined
     110,                       // undefined
     (110 * 130) / 100,         // SK_SPELLCASTING
     110,                       // SK_CONJURATIONS
     110,                       // SK_ENCHANTMENTS
     110,                       // SK_SUMMONINGS
     110,                       // SK_NECROMANCY
     110,                       // SK_TRANSLOCATIONS
     110,                       // SK_TRANSMIGRATION
     110,                       // SK_DIVINATIONS
     110,                       // SK_FIRE_MAGIC
     110,                       // SK_ICE_MAGIC
     110,                       // SK_AIR_MAGIC
     110,                       // SK_EARTH_MAGIC
     110,                       // SK_POISON_MAGIC
     (110 * 75) / 100,          // SK_INVOCATIONS
     (110 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_SPRIGGAN (32)
     150,                       // SK_FIGHTING
     90,                        // SK_SHORT_BLADES
     140,                       // SK_LONG_BLADES
     160,                       // SK_UNUSED_1
     150,                       // SK_AXES
     160,                       // SK_MACES_FLAILS
     180,                       // SK_POLEARMS
     150,                       // SK_STAVES
     70,                        // SK_SLINGS
     70,                        // SK_BOWS
     100,                       // SK_CROSSBOWS
     70,                        // SK_DARTS
     90,                        // SK_THROWING
     170,                       // SK_ARMOUR
     50,                        // SK_DODGING
     50,                        // SK_STEALTH
     50,                        // SK_STABBING
     180,                       // SK_SHIELDS
     60,                        // SK_TRAPS_DOORS
     130,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (60 * 130) / 100,          // SK_SPELLCASTING
     160,                       // SK_CONJURATIONS
     50,                        // SK_ENCHANTMENTS
     150,                       // SK_SUMMONINGS
     120,                       // SK_NECROMANCY
     50,                        // SK_TRANSLOCATIONS
     60,                        // SK_TRANSMIGRATION
     70,                        // SK_DIVINATIONS
     140,                       // SK_FIRE_MAGIC
     140,                       // SK_ICE_MAGIC
     120,                       // SK_AIR_MAGIC
     120,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (130 * 75) / 100,          // SK_INVOCATIONS
     (70 * 75) / 100,           // SK_EVOCATIONS
     },

    {                           // SP_MINOTAUR (33)
     70,                        // SK_FIGHTING
     70,                        // SK_SHORT_BLADES
     70,                        // SK_LONG_BLADES
     70,                        // SK_UNUSED_1
     70,                        // SK_AXES
     70,                        // SK_MACES_FLAILS
     70,                        // SK_POLEARMS
     70,                        // SK_STAVES
     90,                        // SK_SLINGS
     90,                        // SK_BOWS
     90,                        // SK_CROSSBOWS
     90,                        // SK_DARTS
     90,                        // SK_THROWING
     80,                        // SK_ARMOUR
     80,                        // SK_DODGING
     130,                       // SK_STEALTH
     100,                       // SK_STABBING
     80,                        // SK_SHIELDS
     120,                       // SK_TRAPS_DOORS
     80,                        // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (180 * 130) / 100,         // SK_SPELLCASTING
     170,                       // SK_CONJURATIONS
     170,                       // SK_ENCHANTMENTS
     170,                       // SK_SUMMONINGS
     170,                       // SK_NECROMANCY
     170,                       // SK_TRANSLOCATIONS
     170,                       // SK_TRANSMIGRATION
     170,                       // SK_DIVINATIONS
     170,                       // SK_FIRE_MAGIC
     170,                       // SK_ICE_MAGIC
     170,                       // SK_AIR_MAGIC
     170,                       // SK_EARTH_MAGIC
     170,                       // SK_POISON_MAGIC
     (130 * 75) / 100,          // SK_INVOCATIONS
     (170 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_DEMONSPAWN (34)
     100,                       // SK_FIGHTING
     110,                       // SK_SHORT_BLADES
     110,                       // SK_LONG_BLADES
     110,                       // SK_UNUSED_1
     110,                       // SK_AXES
     110,                       // SK_MACES_FLAILS
     110,                       // SK_POLEARMS
     110,                       // SK_STAVES
     110,                       // SK_SLINGS
     110,                       // SK_BOWS
     110,                       // SK_CROSSBOWS
     110,                       // SK_DARTS
     110,                       // SK_THROWING
     110,                       // SK_ARMOUR
     110,                       // SK_DODGING
     110,                       // SK_STEALTH
     110,                       // SK_STABBING
     110,                       // SK_SHIELDS
     110,                       // SK_TRAPS_DOORS
     110,                       // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     100,                       // SK_CONJURATIONS
     110,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     90,                        // SK_NECROMANCY
     110,                       // SK_TRANSLOCATIONS
     110,                       // SK_TRANSMIGRATION
     110,                       // SK_DIVINATIONS
     100,                       // SK_FIRE_MAGIC
     110,                       // SK_ICE_MAGIC
     110,                       // SK_AIR_MAGIC
     110,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (80 * 75) / 100,           // SK_INVOCATIONS
     (110 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_GHOUL (35)
     80,                        // SK_FIGHTING
     110,                       // SK_SHORT_BLADES
     110,                       // SK_LONG_BLADES
     110,                       // SK_UNUSED_1
     110,                       // SK_AXES
     110,                       // SK_MACES_FLAILS
     110,                       // SK_POLEARMS
     110,                       // SK_STAVES
     130,                       // SK_SLINGS
     130,                       // SK_BOWS
     130,                       // SK_CROSSBOWS
     130,                       // SK_DARTS
     130,                       // SK_THROWING
     110,                       // SK_ARMOUR
     110,                       // SK_DODGING
     80,                        // SK_STEALTH
     100,                       // SK_STABBING
     110,                       // SK_SHIELDS
     120,                       // SK_TRAPS_DOORS
     80,                        // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (120 * 130) / 100,         // SK_SPELLCASTING
     130,                       // SK_CONJURATIONS
     130,                       // SK_ENCHANTMENTS
     120,                       // SK_SUMMONINGS
     100,                       // SK_NECROMANCY
     120,                       // SK_TRANSLOCATIONS
     120,                       // SK_TRANSMIGRATION
     120,                       // SK_DIVINATIONS
     150,                       // SK_FIRE_MAGIC
     90,                        // SK_ICE_MAGIC
     150,                       // SK_AIR_MAGIC
     90,                        // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (110 * 75) / 100,          // SK_INVOCATIONS
     (130 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_KENKU (36)
     100,                       // SK_FIGHTING
     75,                        // SK_SHORT_BLADES
     75,                        // SK_LONG_BLADES
     75,                        // SK_UNUSED_1
     75,                        // SK_AXES
     75,                        // SK_MACES_FLAILS
     75,                        // SK_POLEARMS
     75,                        // SK_STAVES
     100,                       // SK_SLINGS
     80,                        // SK_BOWS
     80,                        // SK_CROSSBOWS
     90,                        // SK_DARTS
     90,                        // SK_THROWING
     90,                        // SK_ARMOUR
     90,                        // SK_DODGING
     100,                       // SK_STEALTH
     80,                        // SK_STABBING
     100,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
     80,                        // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     60,                        // SK_CONJURATIONS
     160,                       // SK_ENCHANTMENTS
     70,                        // SK_SUMMONINGS
     80,                        // SK_NECROMANCY
     150,                       // SK_TRANSLOCATIONS
     150,                       // SK_TRANSMIGRATION
     180,                       // SK_DIVINATIONS
     90,                        // SK_FIRE_MAGIC
     120,                       // SK_ICE_MAGIC
     90,                        // SK_AIR_MAGIC
     120,                       // SK_EARTH_MAGIC
     100,                       // SK_POISON_MAGIC
     (160 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_MERFOLK (37)
     80,                        // SK_FIGHTING
     70,                        // SK_SHORT_BLADES
     90,                        // SK_LONG_BLADES
     100,                       // SK_UNUSED_1
     140,                       // SK_AXES
     150,                       // SK_MACES_FLAILS
     50,                        // SK_POLEARMS
     130,                       // SK_STAVES
     150,                       // SK_SLINGS
     140,                       // SK_BOWS
     140,                       // SK_CROSSBOWS
     100,                       // SK_DARTS
     100,                       // SK_THROWING
     160,                       // SK_ARMOUR
     60,                        // SK_DODGING
     90,                        // SK_STEALTH
     70,                        // SK_STABBING
     100,                       // SK_SHIELDS
     120,                       // SK_TRAPS_DOORS
     90,                        // SK_UNARMED_COMBAT
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     100,                       // undefined
     (100 * 130) / 100,         // SK_SPELLCASTING
     140,                       // SK_CONJURATIONS
     90,                        // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
     150,                       // SK_NECROMANCY
     140,                       // SK_TRANSLOCATIONS
     60,                        // SK_TRANSMIGRATION
     80,                        // SK_DIVINATIONS
     160,                       // SK_FIRE_MAGIC
     80,                        // SK_ICE_MAGIC
     150,                       // SK_AIR_MAGIC
     150,                       // SK_EARTH_MAGIC
     80,                        // SK_POISON_MAGIC
     (100 * 75) / 100,          // SK_INVOCATIONS
     (100 * 75) / 100,          // SK_EVOCATIONS
     },

    {                           // SP_VAMPIRE (38)
     110,                       // SK_FIGHTING
      90,                       // SK_SHORT_BLADES
     100,                       // SK_LONG_BLADES
     110,                       // SK_UNUSED_1
     110,                       // SK_AXES
     140,                       // SK_MACES_FLAILS
     110,                       // SK_POLEARMS
     140,                       // SK_STAVES
     140,                       // SK_SLINGS
     140,                       // SK_BOWS
     140,                       // SK_CROSSBOWS
     140,                       // SK_DARTS
     140,                       // SK_THROWING
     140,                       // SK_ARMOUR
      90,                       // SK_DODGING
      50,                       // SK_STEALTH
      90,                       // SK_STABBING
     110,                       // SK_SHIELDS
     100,                       // SK_TRAPS_DOORS
      90,                       // SK_UNARMED_COMBAT
     140,                       // undefined
     140,                       // undefined
     140,                       // undefined
     140,                       // undefined
     140,                       // undefined
     (100 * 130)/100,           // SK_SPELLCASTING
     160,                       // SK_CONJURATIONS
      90,                       // SK_ENCHANTMENTS
     100,                       // SK_SUMMONINGS
      90,                       // SK_NECROMANCY
     140,                       // SK_TRANSLOCATIONS
      90,                       // SK_TRANSMIGRATION
     120,                       // SK_DIVINATIONS
     140,                       // SK_FIRE_MAGIC
     100,                       // SK_ICE_MAGIC
     100,                       // SK_AIR_MAGIC
     120,                       // SK_EARTH_MAGIC
     120,                       // SK_POISON_MAGIC
     (160 * 75)/100,            // SK_INVOCATIONS
     (120 * 75)/100,            // SK_EVOCATIONS
    },

    // SP_HILL_DWARF placeholder.
    {
    },

    // SP_ELF placeholder.
    {
    },


/* ******************************************************

// base draconian
    {
         90,               // SK_FIGHTING
        100,               // SK_SHORT_BLADES
        100,               // SK_LONG_BLADES
        100,               // SK_UNUSED_1
        100,               // SK_AXES
        100,               // SK_MACES_FLAILS
        100,               // SK_POLEARMS
        100,               // SK_STAVES
        120,               // SK_SLINGS
        120,               // SK_BOWS
        120,               // SK_CROSSBOWS
        120,               // SK_DARTS
        120,               // SK_THROWING
        200,               // SK_ARMOUR
        120,               // SK_DODGING
        120,               // SK_STEALTH
        100,               // SK_STABBING
        100,               // SK_SHIELDS
        100,               // SK_TRAPS_DOORS
        100,               // SK_UNARMED_COMBAT
        100,               // undefined
        100,               // undefined
        100,               // undefined
        100,               // undefined
        100,               // undefined
        100,               // SK_SPELLCASTING
        100,               // SK_CONJURATIONS
        120,               // SK_ENCHANTMENTS
        100,               // SK_SUMMONINGS
        100,               // SK_NECROMANCY
        100,               // SK_TRANSLOCATIONS
        100,               // SK_TRANSMIGRATION
        100,               // SK_DIVINATIONS
        100,               // SK_FIRE_MAGIC
        100,               // SK_ICE_MAGIC
        100,               // SK_AIR_MAGIC
        100,               // SK_EARTH_MAGIC
        100,               // SK_POISON_MAGIC
        100,               // SK_INVOCATIONS
        100,               // SK_EVOCATIONS
    },

****************************************************** */

};




/* *************************************************************

// these were unimplemented "level titles" for two classes {dlb}

JOB_PRIEST
   "Preacher";
   "Priest";
   "Evangelist";
   "Pontifex";

JOB_PALADIN:
   "Holy Warrior";
   "Holy Crusader";
   "Paladin";
   "Scourge of Evil";

************************************************************* */

static const skill_type skill_display_order[] =
{
    SK_FIGHTING, SK_SHORT_BLADES, SK_LONG_BLADES, SK_AXES,
    SK_MACES_FLAILS, SK_POLEARMS, SK_STAVES, SK_UNARMED_COMBAT,

    SK_BLANK_LINE,

    SK_BOWS, SK_CROSSBOWS, SK_THROWING, SK_SLINGS, SK_DARTS,

    SK_BLANK_LINE,

    SK_ARMOUR, SK_DODGING, SK_STEALTH, SK_STABBING, SK_SHIELDS, SK_TRAPS_DOORS,

    SK_BLANK_LINE,
    SK_COLUMN_BREAK,

    SK_SPELLCASTING, SK_CONJURATIONS, SK_ENCHANTMENTS, SK_SUMMONINGS,
    SK_NECROMANCY, SK_TRANSLOCATIONS, SK_TRANSMIGRATION, SK_DIVINATIONS,
    SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC, SK_POISON_MAGIC,

    SK_BLANK_LINE,

    SK_INVOCATIONS, SK_EVOCATIONS,
};

static const int ndisplayed_skills =
            sizeof(skill_display_order) / sizeof(*skill_display_order);

static bool _player_knows_aptitudes()
{
    return !player_genus(GENPC_DRACONIAN) || you.max_level >= 7;

}

static void _display_skill_table(bool show_aptitudes, bool show_description)
{
    menu_letter lcount = 'a';

    cgotoxy(1, 1);
    textcolor(LIGHTGREY);

#if DEBUG_DIAGNOSTICS
    cprintf("You have %d points of unallocated experience "
            " (cost lvl %d; total %d)." EOL EOL,
            you.exp_available, you.skill_cost_level,
            you.total_skill_points);
#else
    cprintf(" You have %s unallocated experience." EOL EOL,
            you.exp_available == 0? "no" :
            make_stringf("%d point%s of",
                         you.exp_available,
                         you.exp_available == 1? "" : "s").c_str());
#endif

    int scrln = 3, scrcol = 1;
    int x;

    // Don't want the help line to appear too far down a big window.
    const int bottom_line = std::min(30, get_number_of_lines());

    for (int i = 0; i < ndisplayed_skills; ++i)
    {
        x = skill_display_order[i];

        if (scrln > bottom_line - 3 || x == SK_COLUMN_BREAK)
        {
            if (scrcol != 40)
            {
                scrln = 3;
                scrcol = 40;
            }
            if (x == SK_COLUMN_BREAK)
                continue;
        }

        if (x == SK_BLANK_LINE)
        {
            scrln++;
            continue;
        }

        cgotoxy(scrcol, scrln);

#ifndef DEBUG_DIAGNOSTICS
        if (you.skills[x] > 0)
#endif
        {
            if (you.practise_skill[x] == 0 || you.skills[x] == 0)
                textcolor(DARKGREY);
            else
                textcolor(LIGHTGREY);

            if (you.skills[x] == 27)
                textcolor(YELLOW);

#if DEBUG_DIAGNOSTICS
            if (you.skills[x] == 0)
                putch(' ');
            else
                putch(lcount++);
#else
            putch(lcount++);
#endif

            cprintf( " %c %-14s Skill %2d",
                     (you.skills[x] == 0)    ? ' ' :
                     (you.practise_skill[x]) ? '+' : '-',
                     skill_name(x), you.skills[x] );

#if DEBUG_DIAGNOSTICS
            cprintf( " %5d", you.skill_points[x] );
#endif

            if (you.skills[x] < 27)
            {
                const int spec_abil = species_skills(x, you.species);

                if (!show_aptitudes)
                {
                    const int needed =
                        (skill_exp_needed(you.skills[x] + 1) * spec_abil) / 100;
                    const int prev_needed =
                        (skill_exp_needed(you.skills[x]    ) * spec_abil) / 100;

                    const int amt_done = you.skill_points[x] - prev_needed;
                    int percent_done = (amt_done*100) / (needed - prev_needed);

                    if (percent_done >= 100) // paranoia (1)
                        percent_done = 99;

                    if (percent_done < 0)    // paranoia (2)
                        percent_done = 0;

                    textcolor(CYAN);
                    // Round down to multiple of 5.
                    cprintf( " (%2d%%)", (percent_done / 5) * 5 );
                }
                else
                {
                    textcolor(RED);
                    cprintf(" %3d  ", spec_abil);
                }
            }

            scrln++;
        }
    }

    if (Options.tutorial_left)
    {
        if (show_description)
        {
            cgotoxy(1, bottom_line-2);
            // Doesn't mention the toggle between progress/aptitudes.
            print_tut_skills_description_info();
        }
        else
        {
            cgotoxy(1, bottom_line-5);
            // Doesn't mention the toggle between progress/aptitudes.
            print_tut_skills_info();
        }
    }
    else
    {
        // NOTE: If any more skills added, must adapt letters to go into caps.
        cgotoxy(1, bottom_line-2);
        textcolor(LIGHTGREY);

        if (show_description)
        {
            // We need the extra spaces to override the alternative sentence.
            cprintf("Press the letter of a skill to read its description.      "
                    "            ");
        }
        else
        {
            cprintf("Press the letter of a skill to choose whether you want to "
                    "practise it.");
        }

        cgotoxy(1, bottom_line-1);
        if (show_description)
        {
            formatted_string::parse_string("Press '<w>?</w>' to choose which "
                                           "skills to train.  ").display();
        }
        else
        {
            formatted_string::parse_string("Press '<w>?</w>' to read the "
                                           "skills' descriptions.").display();
        }

        if (_player_knows_aptitudes())
        {
            cgotoxy(1, bottom_line);
            formatted_string::parse_string("Press '<w>!</w>' to toggle between "
                                           "<cyan>progress</cyan> and "
                                           "<red>aptitude</red> "
                                           "display.").display();
        }
    }
}

void show_skills()
{
    bool show_aptitudes   = false;
    bool show_description = false;
    clrscr();
    while (true)
    {
        _display_skill_table(show_aptitudes, show_description);

        const int keyin = getch();
        if (keyin == '!' && _player_knows_aptitudes())
        {
            show_aptitudes = !show_aptitudes;
            continue;
        }

        if (keyin == '?')
        {
            // Show skill description.
            show_description = !show_description;
            if (Options.tutorial_left)
                clrscr();
            continue;
        }

        if (!isalpha(keyin))
            break;

        menu_letter lcount = 'a';       // toggle skill practise

        for (int i = 0; i < ndisplayed_skills; i++)
        {
            const skill_type x = skill_display_order[i];
            if (x == SK_BLANK_LINE || x == SK_COLUMN_BREAK)
                continue;

            if (you.skills[x] == 0)
                continue;

            if (keyin == lcount)
            {
                if (!show_description)
                    you.practise_skill[x] = !you.practise_skill[x];
                else
                {
                    describe_skill(x);
                    clrscr();
                }
                break;
            }
            ++lcount;
        }
    }
}

const char *skill_name(int which_skill)
{
    return (skills[which_skill][0]);
}                               // end skill_name()

int str_to_skill(const std::string &skill)
{
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (skills[i][0] && skill == skills[i][0])
            return (i);
    }
    return (SK_FIGHTING);
}

std::string skill_title( unsigned char best_skill, unsigned char skill_lev,
                         int species, int str, int dex, int god )
{
    // paranoia
    if (best_skill == SK_UNUSED_1
        || best_skill > SK_UNARMED_COMBAT && best_skill < SK_SPELLCASTING
        || best_skill >= NUM_SKILLS)
    {
        return ("Adventurer");
    }

    if (species == -1)
        species = you.species;

    if (str == -1)
        str = std::max(you.strength - stat_modifier(STAT_STRENGTH), 1);

    if (dex == -1)
        dex = std::max(you.dex - stat_modifier(STAT_DEXTERITY), 1);

    if (god == -1)
        god = you.religion;

    // Translate skill level into skill ranking {dlb}:
    // Increment rank by one to "skip" skill name in array {dlb}:
    const int skill_rank = ((skill_lev <= 7)  ? 1 :
                            (skill_lev <= 14) ? 2 :
                            (skill_lev <= 20) ? 3 :
                            (skill_lev <= 26) ? 4
                            /* level 27 */    : 5);

    std::string result;

    if (best_skill < NUM_SKILLS)
    {
        // Note that ghosts default to (dex == str) and god == no_god, due
        // to a current lack of that information... the god case is probably
        // suitable for most cases (TSO/Zin/Ely at the very least). -- bwr
        switch (best_skill)
        {
        case SK_UNARMED_COMBAT:
            result = (dex >= str) ? martial_arts_titles[skill_rank]
                                  : skills[best_skill][skill_rank];

            break;

        case SK_INVOCATIONS:
            if (god == GOD_NO_GOD)
                result = atheist_inv_titles[skill_rank];
            else if (god == GOD_TROG || god == GOD_XOM
                     || god == GOD_NEMELEX_XOBEH)
            {
                // don't care about Invocations
                result = "Prodigal";
            }
            else
                result = skills[best_skill][skill_rank];
            break;

        default:
            result = skills[best_skill][skill_rank];
            break;
        }
    }

    // Replace all occurrences of @genus@ with the player genus.
    std::string genus = species_name(you.species, 1, true, false, true);
    std::string::size_type where = result.find("@genus@");
    if (where != std::string::npos)
    {
        lowercase(genus);
        result = replace_all(result, "@genus@", genus);
    }
    else
    {
        if (you.species == SP_OGRE_MAGE)
            genus = species_name(you.species, 1, true, false, false);
        result = replace_all(result, "@Genus@", genus);
    }

    where = result.find("%s");
    if (where != std::string::npos)
    {
        if (best_skill == SK_UNARMED_COMBAT)
        {
            std::string weightclass = "";
            switch (species)
            {
            case SP_OGRE:
            case SP_OGRE_MAGE:
            case SP_TROLL:
                weightclass = "Heavy";
                break;

            case SP_NAGA:
            case SP_CENTAUR:
                weightclass = "Cruiser";
                break;

            default:
                weightclass = "Middle";
                break;

            case SP_HIGH_ELF:
            case SP_GREY_ELF:
            case SP_DEEP_ELF:
            case SP_SLUDGE_ELF:
            case SP_KENKU:
                weightclass = "Little";
                break;

            case SP_HALFLING:
            case SP_GNOME:
            case SP_KOBOLD:
                weightclass = "Feather";
                break;

            case SP_SPRIGGAN:
                weightclass = "Fly";
                break;
            }
            result.replace(where, 2, weightclass);
        }
        else if (best_skill == SK_TRANSLOCATIONS && skill_rank == 5)
        {
            result.replace(where, 2, (species == SP_NAGA  ? "Slider" :
                                      species == SP_KENKU ? "Glider"
                                                          : "Walker"));
        }
        else
        {
            const bool need_cap = (best_skill != SK_BOWS);
            const std::string sp =
                species_name(static_cast<species_type>(species), 0, true,
                             need_cap);
            result.replace(where, 2, sp);
        }
    }
    return (result.empty() ? std::string("Invalid Title")
                           : result);
}

std::string player_title()
{
    const unsigned char best = best_skill( SK_FIGHTING, (NUM_SKILLS - 1), 99 );
    return (skill_title( best, you.skills[ best ] ));
}                               // end player_title()

skill_type best_skill( int min_skill, int max_skill, int excl_skill )
{
    int ret = SK_FIGHTING;
    unsigned int best_skill_level = 0;
    unsigned int best_position = 1000;

    for (int i = min_skill; i <= max_skill; i++)    // careful!!!
    {
        if (i == excl_skill
            || i == SK_UNUSED_1
            || (i > SK_UNARMED_COMBAT && i < SK_SPELLCASTING))
        {
            continue;
        }

        if (you.skills[i] > best_skill_level)
        {
            ret = i;
            best_skill_level = you.skills[i];
            best_position = you.skill_order[i];

        }
        else if (you.skills[i] == best_skill_level
                && you.skill_order[i] < best_position)
        {
            ret = i;
            best_position = you.skill_order[i];
        }
    }

    return static_cast<skill_type>(ret);
}                               // end best_skill()

// Calculate the skill_order array from scratch.
//
// The skill order array is used for breaking ties in best_skill.
// This is done by ranking each skill by the order in which it
// has attained its current level (the values are the number of
// skills at or above that level when the current skill reached it).
//
// In this way, the skill which has been at a level for the longest
// is judged to be the best skill (thus, nicknames are sticky)...
// other skills will have to attain the next level higher to be
// considered a better skill (thus, the first skill to reach level 27
// becomes the characters final nickname).
//
// As for other uses of best_skill:  this method is still appropriate
// in that there is no additional advantage anywhere else in the game
// for partial skill levels.  Besides, its probably best if the player
// isn't able to micromanage at that level.  -- bwr
void init_skill_order( void )
{
    for (int i = SK_FIGHTING; i < NUM_SKILLS; i++)
    {
        if (i == SK_UNUSED_1
            || (i > SK_UNARMED_COMBAT && i < SK_SPELLCASTING))
        {
            you.skill_order[i] = MAX_SKILL_ORDER;
            continue;
        }

        const int i_diff = species_skills( i, you.species );
        const unsigned int i_points = (you.skill_points[i] * 100) / i_diff;

        you.skill_order[i] = 0;

        for (int j = SK_FIGHTING; j < NUM_SKILLS; j++)
        {
            if (i == j
                || j == SK_UNUSED_1
                || (j > SK_UNARMED_COMBAT && j < SK_SPELLCASTING))
            {
                continue;
            }

            const int j_diff = species_skills( j, you.species );
            const unsigned int j_points = (you.skill_points[j] * 100) / j_diff;

            if (you.skills[j] == you.skills[i]
                && (j_points > i_points
                    || (j_points == i_points && j > i)))
            {
                you.skill_order[i]++;
            }
        }
    }
}

int calc_hp(bool real_hp)
{
    int hitp;

    hitp = (you.base_hp - 5000) + (you.base_hp2 - 5000);
    hitp += (you.experience_level * you.skills[SK_FIGHTING]) / 5;

    // being berserk makes you resistant to damage. I don't know why.
    if (you.duration[DUR_BERSERKER] && !real_hp)
    {
        hitp *= 15;
        hitp /= 10;
    }

    if (!real_hp)
    {
        // some transformations give you extra hp
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_STATUE:
            hitp *= 15;
            hitp /= 10;
            break;
        case TRAN_ICE_BEAST:
            hitp *= 12;
            hitp /= 10;
            break;
        case TRAN_DRAGON:
            hitp *= 16;
            hitp /= 10;
            break;
        }
    }

    // frail and robust mutations, and divine robustness
    hitp *= (10 + player_mutation_level(MUT_ROBUST)
                + you.attribute[ATTR_DIVINE_ROBUSTNESS]
                - player_mutation_level(MUT_FRAIL));
    hitp /= 10;

    you.hp_max = hitp;

    deflate_hp( you.hp_max, false );

    return (hitp);
}                               // end calc_hp()

int calc_mp(bool real_mp)
{
    int enp;

    // base_magic_points2 accounts for species and magic potions
    enp = (you.base_magic_points2 - 5000);

    int spell_extra = (you.experience_level * you.skills[SK_SPELLCASTING]) / 4;
    int invoc_extra = (you.experience_level * you.skills[SK_INVOCATIONS]) / 6;

    if (spell_extra > invoc_extra)
        enp += spell_extra;
    else
        enp += invoc_extra;

    you.max_magic_points = stepdown_value( enp, 9, 18, 45, 100 );

    // this is our "rotted" base (applied after scaling):
    you.max_magic_points += (you.base_magic_points - 5000);

    // Yes, we really do want this duplication... this is so the stepdown
    // doesn't truncate before we apply the rotted base.  We're doing this
    // the nice way. -- bwr
    if (you.max_magic_points > 50)
        you.max_magic_points = 50;

    // now applied after scaling so that power items are more useful -- bwr
    if (!real_mp)
        you.max_magic_points += player_magical_power();

    // analogous to ROBUST/FRAIL
    you.max_magic_points *= (10 + player_mutation_level(MUT_HIGH_MAGIC)
                                - player_mutation_level(MUT_LOW_MAGIC));
    you.max_magic_points /= 10;

    if (you.max_magic_points > 50)
        you.max_magic_points = 50 + ((you.max_magic_points - 50) / 2);

    if (you.max_magic_points < 0)
        you.max_magic_points = 0;

    if (you.magic_points > you.max_magic_points)
        you.magic_points = you.max_magic_points;

    you.redraw_magic_points = true;

    return (you.max_magic_points);
}                               // end calc_mp()

unsigned int skill_exp_needed(int lev)
{
    switch (lev)
    {
    case 0:  return 0;
    case 1:  return 200;
    case 2:  return 300;
    case 3:  return 500;
    case 4:  return 750;
    case 5:  return 1050;
    case 6:  return 1350;
    case 7:  return 1700;
    case 8:  return 2100;
    case 9:  return 2550;
    case 10: return 3150;
    case 11: return 3750;
    case 12: return 4400;
    case 13: return 5250;
    default: return 6200 + 1800 * (lev - 14);
    }
    return 0;
}

int species_skills(int skill, species_type species)
{
    return spec_skills[species - 1][skill];
}

void wield_warning(bool newWeapon)
{
    // Early out - no weapon.
    if (!you.weapon())
         return;

    const item_def& wep = *you.weapon();

    // Early out - don't warn for non-weapons or launchers.
    if (wep.base_type != OBJ_WEAPONS || is_range_weapon(wep))
        return;

    // Don't warn if the weapon is OK, of course.
    if (effective_stat_bonus() > -4)
        return;

    std::string msg = (newWeapon ? "this " : "your ") + wep.name(DESC_BASENAME);
    const char* mstr = msg.c_str();

    if (you.strength < you.dex)
    {
        if (you.strength < 11)
        {
            mprf(MSGCH_WARN, "You have %strouble swinging %s.",
                 (you.strength < 7) ? "" : "a little ", mstr);
        }
        else
        {
            mprf(MSGCH_WARN, "You'd be more effective with "
                 "%s if you were stronger.", mstr);
        }
    }
    else
    {
        if (you.dex < 11)
        {
            mprf(MSGCH_WARN, "Wielding %s is %s awkward.",
                 mstr, (you.dex < 7) ? "fairly" : "a little" );
        }
        else
        {
            mprf(MSGCH_WARN, "You'd be more effective with "
                 "%s if you were nimbler.", mstr);
        }
    }
}
