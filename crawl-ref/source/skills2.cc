/*
 *  File:       skills2.cc
 *  Summary:    More skill related functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "skills2.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <sstream>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "artefact.h"
#include "cio.h"
#include "describe.h"
#include "externs.h"
#include "fight.h"
#include "godabil.h"
#include "itemprop.h"
#include "menu.h"
#include "player.h"
#include "species.h"
#include "skills.h"
#include "stuff.h"
#include "hints.h"

typedef std::string (*string_fn)();
typedef std::map<std::string, string_fn> skill_op_map;

static skill_op_map Skill_Op_Map;

// The species for which the skill title is being worked out.
static species_type Skill_Species = SP_UNKNOWN;

class skill_title_key_t
{
public:
    skill_title_key_t(const char *k, string_fn o) : key(k), op(o)
    {
        Skill_Op_Map[k] = o;
    }

    static std::string get(const std::string &_key)
    {
        skill_op_map::const_iterator i = Skill_Op_Map.find(_key);
        return (i == Skill_Op_Map.end()? std::string() : (i->second)());
    }
private:
    const char *key;
    string_fn op;
};

typedef skill_title_key_t stk;

// Basic goals for titles:
// The higher titles must come last.
// Referring to the skill itself is fine ("Transmuter") but not impressive.
// No overlaps, high diversity.

// Replace @Adj@ with uppercase adjective form, @genus@ with lowercase genus,
// @Genus@ with uppercase genus, and %s with special cases defined below,
// including but not limited to species.

// NOTE:  Even though %s could be used with most of these, remember that
// the character's race will be listed on the next line.  It's only really
// intended for cases where things might be really awkward without it. -- bwr

// NOTE: If a skill name is changed, remember to also adapt the database entry.
const char *skills[50][6] =
{
  //  Skill name        levels 1-7       levels 8-14        levels 15-20       levels 21-26      level 27
    {"Fighting",       "Skirmisher",    "Fighter",         "Warrior",         "Slayer",         "Conqueror"},
    {"Short Blades",   "Cutter",        "Slicer",          "Swashbuckler",    "Blademaster",    "Eviscerator"},
    {"Long Blades",    "Slasher",       "Carver",          "Fencer",          "@Adj@ Blade",    "Swordmaster"},
    {"Axes",           "Chopper",       "Cleaver",         "Severer",         "Executioner",    "Axe Maniac"},
    {"Maces & Flails", "Cudgeler",      "Basher",          "Bludgeoner",      "Shatterer",      "Skullcrusher"},
    {"Polearms",       "Poker",         "Spear-Bearer",    "Impaler",         "Phalangite",     "@Adj@ Porcupine"},
    {"Staves",         "Twirler",       "Cruncher",        "Stickfighter",    "Pulveriser",     "Chief of Staff"},
    {"Slings",         "Vandal",        "Slinger",         "Whirler",         "Slingshot",      "@Adj@ Catapult"},
    {"Bows",           "Shooter",       "Archer",          "Marks@genus@",    "Crack Shot",     "Merry @Genus@"},
    {"Crossbows",      "Bolt Thrower",  "Quickloader",     "Sharpshooter",    "Sniper",         "@Adj@ Arbalest"},
    {"Throwing",       "Chucker",       "Thrower",         "Deadly Accurate", "Hawkeye",        "@Adj@ Ballista"},
    {"Armour",         "Covered",       "Protected",       "Tortoise",        "Impregnable",    "Invulnerable"},
    {"Dodging",        "Ducker",        "Nimble",          "Spry",            "Acrobat",        "Intangible"},
    {"Stealth",        "Sneak",         "Covert",          "Unseen",          "Imperceptible",  "Ninja"},
    {"Stabbing",       "Miscreant",     "Blackguard",      "Backstabber",     "Cutthroat",      "Politician"},
    {"Shields",        "Shield-Bearer", "Hoplite",         "Blocker",         "Peltast",        "@Adj@ Barricade"},
    {"Traps & Doors",  "Scout",         "Disarmer",        "Vigilant",        "Perceptive",     "Dungeon Master"},
    // STR based fighters, for DEX/martial arts titles see below.  Felids get their own cathegory, too.
    {"Unarmed Combat", "Ruffian",       "Grappler",        "Brawler",         "Wrestler",       "@Weight@weight Champion"},

    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},

    {"Spellcasting",   "Magician",      "Thaumaturge",     "Eclecticist",     "Sorcerer",       "Archmage"},
    {"Conjurations",   "Ruinous",       "Conjurer",        "Destroyer",       "Devastator",     "Annihilator"},
    {"Enchantments",   "Charm-Maker",   "Infuser",         "Bewitcher",       "Enchanter",      "Spellbinder"},
    {"Summonings",     "Caller",        "Summoner",        "Convoker",        "Demonologist",   "Hellbinder"},
    {"Necromancy",     "Grave Robber",  "Reanimator",      "Necromancer",     "Thanatomancer",  "@Genus_Short@ of Death"},
    {"Translocations", "Grasshopper",   "Placeless @Genus@", "Blinker",       "Portalist",      "Plane @Walker@"},
    {"Transmutations", "Changer",       "Transmogrifier",  "Alchemist",       "Malleable",      "Shapeless @Genus@"},

    {"Fire Magic",     "Firebug",       "Arsonist",        "Scorcher",        "Pyromancer",     "Infernalist"},
    {"Ice Magic",      "Chiller",       "Frost Mage",      "Gelid",           "Cryomancer",     "Englaciator"},
    {"Air Magic",      "Gusty",         "Cloud Mage",      "Aerator",         "Anemomancer",    "Meteorologist"},
    {"Earth Magic",    "Digger",        "Geomancer",       "Earth Mage",      "Metallomancer",  "Petrodigitator"},
    {"Poison Magic",   "Stinger",       "Tainter",         "Polluter",        "Contaminator",   "Envenomancer"},

    // These titles apply to atheists only, worshippers of the various gods
    // use the god titles instead, depending on piety or, in Xom's case, mood.
    {"Invocations",    "Unbeliever",    "Agnostic",        "Dissident",       "Heretic",        "Apostate"},
    {"Evocations",     "Charlatan",     "Prestidigitator", "Fetichist",       "Evocator",       "Talismancer"},

    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL},
    {NULL}
};

const char *martial_arts_titles[6] =
    {"Unarmed Combat", "Insei", "Martial Artist", "Black Belt", "Sensei", "Grand Master"};
const char *claw_and_tooth_titles[6] =
    {"Unarmed Combat", "Scratcher", "Gouger", "Ripper", "Eviscerator", "Sabretooth"};

struct species_skill_aptitude
{
    species_type species;
    skill_type   skill;
    int aptitude;          // -50..50, with 0 for humans

    species_skill_aptitude(species_type _species,
                           skill_type _skill,
                           int _aptitude)
        : species(_species), skill(_skill), aptitude(_aptitude)
    {
    }
};

static inline species_skill_aptitude APT(species_type sp,
                                         skill_type sk,
                                         int aptitude)
{
    return species_skill_aptitude(sp, sk, aptitude);
}

static const species_skill_aptitude species_skill_aptitudes[] =
{
    // SP_HUMAN
    APT(SP_HUMAN,           SK_FIGHTING,        0),
    APT(SP_HUMAN,           SK_SHORT_BLADES,    0),
    APT(SP_HUMAN,           SK_LONG_BLADES,     0),
    APT(SP_HUMAN,           SK_AXES,            0),
    APT(SP_HUMAN,           SK_MACES_FLAILS,    0),
    APT(SP_HUMAN,           SK_POLEARMS,        0),
    APT(SP_HUMAN,           SK_STAVES,          0),
    APT(SP_HUMAN,           SK_SLINGS,          0),
    APT(SP_HUMAN,           SK_BOWS,            0),
    APT(SP_HUMAN,           SK_CROSSBOWS,       0),
    APT(SP_HUMAN,           SK_THROWING,        0),
    APT(SP_HUMAN,           SK_ARMOUR,          0),
    APT(SP_HUMAN,           SK_DODGING,         0),
    APT(SP_HUMAN,           SK_STEALTH,         0),
    APT(SP_HUMAN,           SK_STABBING,        0),
    APT(SP_HUMAN,           SK_SHIELDS,         0),
    APT(SP_HUMAN,           SK_TRAPS_DOORS,     0),
    APT(SP_HUMAN,           SK_UNARMED_COMBAT,  0),
    APT(SP_HUMAN,           SK_SPELLCASTING,    0),
    APT(SP_HUMAN,           SK_CONJURATIONS,    0),
    APT(SP_HUMAN,           SK_ENCHANTMENTS,    0),
    APT(SP_HUMAN,           SK_SUMMONINGS,      0),
    APT(SP_HUMAN,           SK_NECROMANCY,      0),
    APT(SP_HUMAN,           SK_TRANSLOCATIONS,  0),
    APT(SP_HUMAN,           SK_TRANSMUTATIONS,  0),
    APT(SP_HUMAN,           SK_FIRE_MAGIC,      0),
    APT(SP_HUMAN,           SK_ICE_MAGIC,       0),
    APT(SP_HUMAN,           SK_AIR_MAGIC,       0),
    APT(SP_HUMAN,           SK_EARTH_MAGIC,     0),
    APT(SP_HUMAN,           SK_POISON_MAGIC,    0),
    APT(SP_HUMAN,           SK_INVOCATIONS,     0),
    APT(SP_HUMAN,           SK_EVOCATIONS,      0),

    // SP_HIGH_ELF
    APT(SP_HIGH_ELF,        SK_FIGHTING,        0),
    APT(SP_HIGH_ELF,        SK_SHORT_BLADES,    2),
    APT(SP_HIGH_ELF,        SK_LONG_BLADES,     2),
    APT(SP_HIGH_ELF,        SK_AXES,           -2),
    APT(SP_HIGH_ELF,        SK_MACES_FLAILS,   -2),
    APT(SP_HIGH_ELF,        SK_POLEARMS,       -2),
    APT(SP_HIGH_ELF,        SK_STAVES,          0),
    APT(SP_HIGH_ELF,        SK_SLINGS,         -2),
    APT(SP_HIGH_ELF,        SK_BOWS,            3),
    APT(SP_HIGH_ELF,        SK_CROSSBOWS,       0),
    APT(SP_HIGH_ELF,        SK_THROWING,        1),
    APT(SP_HIGH_ELF,        SK_ARMOUR,         -1),
    APT(SP_HIGH_ELF,        SK_DODGING,         1),
    APT(SP_HIGH_ELF,        SK_STEALTH,         1),
    APT(SP_HIGH_ELF,        SK_STABBING,       -1),
    APT(SP_HIGH_ELF,        SK_SHIELDS,        -1),
    APT(SP_HIGH_ELF,        SK_TRAPS_DOORS,     0),
    APT(SP_HIGH_ELF,        SK_UNARMED_COMBAT, -2),
    APT(SP_HIGH_ELF,        SK_SPELLCASTING,    2),
    APT(SP_HIGH_ELF,        SK_CONJURATIONS,    1),
    APT(SP_HIGH_ELF,        SK_ENCHANTMENTS,    2),
    APT(SP_HIGH_ELF,        SK_SUMMONINGS,     -1),
    APT(SP_HIGH_ELF,        SK_NECROMANCY,     -2),
    APT(SP_HIGH_ELF,        SK_TRANSLOCATIONS,  1),
    APT(SP_HIGH_ELF,        SK_TRANSMUTATIONS,  1),
    APT(SP_HIGH_ELF,        SK_FIRE_MAGIC,      0),
    APT(SP_HIGH_ELF,        SK_ICE_MAGIC,       0),
    APT(SP_HIGH_ELF,        SK_AIR_MAGIC,       2),
    APT(SP_HIGH_ELF,        SK_EARTH_MAGIC,    -2),
    APT(SP_HIGH_ELF,        SK_POISON_MAGIC,   -2),
    APT(SP_HIGH_ELF,        SK_INVOCATIONS,     0),
    APT(SP_HIGH_ELF,        SK_EVOCATIONS,      0),

    // SP_DEEP_ELF
    APT(SP_DEEP_ELF,        SK_FIGHTING,       -2),
    APT(SP_DEEP_ELF,        SK_SHORT_BLADES,    0),
    APT(SP_DEEP_ELF,        SK_LONG_BLADES,    -1),
    APT(SP_DEEP_ELF,        SK_AXES,           -2),
    APT(SP_DEEP_ELF,        SK_MACES_FLAILS,   -3),
    APT(SP_DEEP_ELF,        SK_POLEARMS,       -3),
    APT(SP_DEEP_ELF,        SK_STAVES,          0),
    APT(SP_DEEP_ELF,        SK_SLINGS,         -2),
    APT(SP_DEEP_ELF,        SK_BOWS,            1),
    APT(SP_DEEP_ELF,        SK_CROSSBOWS,      -1),
    APT(SP_DEEP_ELF,        SK_THROWING,        1),
    APT(SP_DEEP_ELF,        SK_ARMOUR,         -2),
    APT(SP_DEEP_ELF,        SK_DODGING,         2),
    APT(SP_DEEP_ELF,        SK_STEALTH,         2),
    APT(SP_DEEP_ELF,        SK_STABBING,        1),
    APT(SP_DEEP_ELF,        SK_SHIELDS,        -2),
    APT(SP_DEEP_ELF,        SK_TRAPS_DOORS,     0),
    APT(SP_DEEP_ELF,        SK_UNARMED_COMBAT, -2),
    APT(SP_DEEP_ELF,        SK_SPELLCASTING,    4),
    APT(SP_DEEP_ELF,        SK_CONJURATIONS,    1),
    APT(SP_DEEP_ELF,        SK_ENCHANTMENTS,    4),
    APT(SP_DEEP_ELF,        SK_SUMMONINGS,      1),
    APT(SP_DEEP_ELF,        SK_NECROMANCY,      2),
    APT(SP_DEEP_ELF,        SK_TRANSLOCATIONS,  1),
    APT(SP_DEEP_ELF,        SK_TRANSMUTATIONS,  1),
    APT(SP_DEEP_ELF,        SK_FIRE_MAGIC,      1),
    APT(SP_DEEP_ELF,        SK_ICE_MAGIC,       1),
    APT(SP_DEEP_ELF,        SK_AIR_MAGIC,       1),
    APT(SP_DEEP_ELF,        SK_EARTH_MAGIC,     0),
    APT(SP_DEEP_ELF,        SK_POISON_MAGIC,    1),
    APT(SP_DEEP_ELF,        SK_INVOCATIONS,     0),
    APT(SP_DEEP_ELF,        SK_EVOCATIONS,      1),

    // SP_SLUDGE_ELF
    APT(SP_SLUDGE_ELF,      SK_FIGHTING,        1),
    APT(SP_SLUDGE_ELF,      SK_SHORT_BLADES,   -1),
    APT(SP_SLUDGE_ELF,      SK_LONG_BLADES,    -1),
    APT(SP_SLUDGE_ELF,      SK_AXES,           -2),
    APT(SP_SLUDGE_ELF,      SK_MACES_FLAILS,   -2),
    APT(SP_SLUDGE_ELF,      SK_POLEARMS,       -2),
    APT(SP_SLUDGE_ELF,      SK_STAVES,          0),
    APT(SP_SLUDGE_ELF,      SK_SLINGS,          0),
    APT(SP_SLUDGE_ELF,      SK_BOWS,            0),
    APT(SP_SLUDGE_ELF,      SK_CROSSBOWS,       0),
    APT(SP_SLUDGE_ELF,      SK_THROWING,        2),
    APT(SP_SLUDGE_ELF,      SK_ARMOUR,         -2),
    APT(SP_SLUDGE_ELF,      SK_DODGING,         2),
    APT(SP_SLUDGE_ELF,      SK_STEALTH,         1),
    APT(SP_SLUDGE_ELF,      SK_STABBING,        0),
    APT(SP_SLUDGE_ELF,      SK_SHIELDS,        -2),
    APT(SP_SLUDGE_ELF,      SK_TRAPS_DOORS,     0),
    APT(SP_SLUDGE_ELF,      SK_UNARMED_COMBAT,  1),
    APT(SP_SLUDGE_ELF,      SK_SPELLCASTING,    2),
    APT(SP_SLUDGE_ELF,      SK_CONJURATIONS,   -2),
    APT(SP_SLUDGE_ELF,      SK_ENCHANTMENTS,   -2),
    APT(SP_SLUDGE_ELF,      SK_SUMMONINGS,      1),
    APT(SP_SLUDGE_ELF,      SK_NECROMANCY,      1),
    APT(SP_SLUDGE_ELF,      SK_TRANSLOCATIONS,  0),
    APT(SP_SLUDGE_ELF,      SK_TRANSMUTATIONS,  3),
    APT(SP_SLUDGE_ELF,      SK_FIRE_MAGIC,      1),
    APT(SP_SLUDGE_ELF,      SK_ICE_MAGIC,       1),
    APT(SP_SLUDGE_ELF,      SK_AIR_MAGIC,       1),
    APT(SP_SLUDGE_ELF,      SK_EARTH_MAGIC,     1),
    APT(SP_SLUDGE_ELF,      SK_POISON_MAGIC,    1),
    APT(SP_SLUDGE_ELF,      SK_INVOCATIONS,     0),
    APT(SP_SLUDGE_ELF,      SK_EVOCATIONS,      0),

    // SP_MOUNTAIN_DWARF
    APT(SP_MOUNTAIN_DWARF,  SK_FIGHTING,        2),
    APT(SP_MOUNTAIN_DWARF,  SK_SHORT_BLADES,    1),
    APT(SP_MOUNTAIN_DWARF,  SK_LONG_BLADES,     0),
    APT(SP_MOUNTAIN_DWARF,  SK_AXES,            2),
    APT(SP_MOUNTAIN_DWARF,  SK_MACES_FLAILS,    2),
    APT(SP_MOUNTAIN_DWARF,  SK_POLEARMS,       -1),
    APT(SP_MOUNTAIN_DWARF,  SK_STAVES,         -1),
    APT(SP_MOUNTAIN_DWARF,  SK_SLINGS,         -1),
    APT(SP_MOUNTAIN_DWARF,  SK_BOWS,           -2),
    APT(SP_MOUNTAIN_DWARF,  SK_CROSSBOWS,       1),
    APT(SP_MOUNTAIN_DWARF,  SK_THROWING,       -1),
    APT(SP_MOUNTAIN_DWARF,  SK_ARMOUR,          3),
    APT(SP_MOUNTAIN_DWARF,  SK_DODGING,        -1),
    APT(SP_MOUNTAIN_DWARF,  SK_STEALTH,        -3),
    APT(SP_MOUNTAIN_DWARF,  SK_STABBING,       -2),
    APT(SP_MOUNTAIN_DWARF,  SK_SHIELDS,         2),
    APT(SP_MOUNTAIN_DWARF,  SK_TRAPS_DOORS,     1),
    APT(SP_MOUNTAIN_DWARF,  SK_UNARMED_COMBAT,  0),
    APT(SP_MOUNTAIN_DWARF,  SK_SPELLCASTING,   -3),
    APT(SP_MOUNTAIN_DWARF,  SK_CONJURATIONS,   -1),
    APT(SP_MOUNTAIN_DWARF,  SK_ENCHANTMENTS,   -2),
    APT(SP_MOUNTAIN_DWARF,  SK_SUMMONINGS,     -2),
    APT(SP_MOUNTAIN_DWARF,  SK_NECROMANCY,     -3),
    APT(SP_MOUNTAIN_DWARF,  SK_TRANSLOCATIONS, -2),
    APT(SP_MOUNTAIN_DWARF,  SK_TRANSMUTATIONS, -1),
    APT(SP_MOUNTAIN_DWARF,  SK_FIRE_MAGIC,      2),
    APT(SP_MOUNTAIN_DWARF,  SK_ICE_MAGIC,      -2),
    APT(SP_MOUNTAIN_DWARF,  SK_AIR_MAGIC,      -2),
    APT(SP_MOUNTAIN_DWARF,  SK_EARTH_MAGIC,     2),
    APT(SP_MOUNTAIN_DWARF,  SK_POISON_MAGIC,   -2),
    APT(SP_MOUNTAIN_DWARF,  SK_INVOCATIONS,     0),
    APT(SP_MOUNTAIN_DWARF,  SK_EVOCATIONS,      1),

    // SP_HALFLING
    APT(SP_HALFLING,        SK_FIGHTING,       -1),
    APT(SP_HALFLING,        SK_SHORT_BLADES,    3),
    APT(SP_HALFLING,        SK_LONG_BLADES,     0),
    APT(SP_HALFLING,        SK_AXES,           -1),
    APT(SP_HALFLING,        SK_MACES_FLAILS,   -2),
    APT(SP_HALFLING,        SK_POLEARMS,       -3),
    APT(SP_HALFLING,        SK_STAVES,         -2),
    APT(SP_HALFLING,        SK_SLINGS,          4),
    APT(SP_HALFLING,        SK_BOWS,            2),
    APT(SP_HALFLING,        SK_CROSSBOWS,       1),
    APT(SP_HALFLING,        SK_THROWING,        3),
    APT(SP_HALFLING,        SK_ARMOUR,         -2),
    APT(SP_HALFLING,        SK_DODGING,         2),
    APT(SP_HALFLING,        SK_STEALTH,         3),
    APT(SP_HALFLING,        SK_STABBING,        2),
    APT(SP_HALFLING,        SK_SHIELDS,         1),
    APT(SP_HALFLING,        SK_TRAPS_DOORS,     0),
    APT(SP_HALFLING,        SK_UNARMED_COMBAT, -2),
    APT(SP_HALFLING,        SK_SPELLCASTING,   -1),
    APT(SP_HALFLING,        SK_CONJURATIONS,   -2),
    APT(SP_HALFLING,        SK_ENCHANTMENTS,    0),
    APT(SP_HALFLING,        SK_SUMMONINGS,     -1),
    APT(SP_HALFLING,        SK_NECROMANCY,     -2),
    APT(SP_HALFLING,        SK_TRANSLOCATIONS,  0),
    APT(SP_HALFLING,        SK_TRANSMUTATIONS, -2),
    APT(SP_HALFLING,        SK_FIRE_MAGIC,      0),
    APT(SP_HALFLING,        SK_ICE_MAGIC,       0),
    APT(SP_HALFLING,        SK_AIR_MAGIC,       1),
    APT(SP_HALFLING,        SK_EARTH_MAGIC,     0),
    APT(SP_HALFLING,        SK_POISON_MAGIC,   -1),
    APT(SP_HALFLING,        SK_INVOCATIONS,     0),
    APT(SP_HALFLING,        SK_EVOCATIONS,      1),

    // SP_HILL_ORC
    APT(SP_HILL_ORC,        SK_FIGHTING,        2),
    APT(SP_HILL_ORC,        SK_SHORT_BLADES,    0),
    APT(SP_HILL_ORC,        SK_LONG_BLADES,     1),
    APT(SP_HILL_ORC,        SK_AXES,            2),
    APT(SP_HILL_ORC,        SK_MACES_FLAILS,    1),
    APT(SP_HILL_ORC,        SK_POLEARMS,        1),
    APT(SP_HILL_ORC,        SK_STAVES,         -1),
    APT(SP_HILL_ORC,        SK_SLINGS,         -1),
    APT(SP_HILL_ORC,        SK_BOWS,           -1),
    APT(SP_HILL_ORC,        SK_CROSSBOWS,      -1),
    APT(SP_HILL_ORC,        SK_THROWING,        0),
    APT(SP_HILL_ORC,        SK_ARMOUR,          1),
    APT(SP_HILL_ORC,        SK_DODGING,        -2),
    APT(SP_HILL_ORC,        SK_STEALTH,        -2),
    APT(SP_HILL_ORC,        SK_STABBING,        2),
    APT(SP_HILL_ORC,        SK_SHIELDS,         1),
    APT(SP_HILL_ORC,        SK_TRAPS_DOORS,     0),
    APT(SP_HILL_ORC,        SK_UNARMED_COMBAT,  1),
    APT(SP_HILL_ORC,        SK_SPELLCASTING,   -3),
    APT(SP_HILL_ORC,        SK_CONJURATIONS,    0),
    APT(SP_HILL_ORC,        SK_ENCHANTMENTS,   -1),
    APT(SP_HILL_ORC,        SK_SUMMONINGS,      0),
    APT(SP_HILL_ORC,        SK_NECROMANCY,      0),
    APT(SP_HILL_ORC,        SK_TRANSLOCATIONS, -2),
    APT(SP_HILL_ORC,        SK_TRANSMUTATIONS, -3),
    APT(SP_HILL_ORC,        SK_FIRE_MAGIC,      0),
    APT(SP_HILL_ORC,        SK_ICE_MAGIC,       0),
    APT(SP_HILL_ORC,        SK_AIR_MAGIC,      -2),
    APT(SP_HILL_ORC,        SK_EARTH_MAGIC,     0),
    APT(SP_HILL_ORC,        SK_POISON_MAGIC,   -1),
    APT(SP_HILL_ORC,        SK_INVOCATIONS,     0),
    APT(SP_HILL_ORC,        SK_EVOCATIONS,      0),

    // SP_KOBOLD
    APT(SP_KOBOLD,          SK_FIGHTING,        1),
    APT(SP_KOBOLD,          SK_SHORT_BLADES,    3),
    APT(SP_KOBOLD,          SK_LONG_BLADES,    -2),
    APT(SP_KOBOLD,          SK_AXES,           -1),
    APT(SP_KOBOLD,          SK_MACES_FLAILS,    0),
    APT(SP_KOBOLD,          SK_POLEARMS,       -2),
    APT(SP_KOBOLD,          SK_STAVES,         -1),
    APT(SP_KOBOLD,          SK_SLINGS,          2),
    APT(SP_KOBOLD,          SK_BOWS,            1),
    APT(SP_KOBOLD,          SK_CROSSBOWS,       1),
    APT(SP_KOBOLD,          SK_THROWING,        3),
    APT(SP_KOBOLD,          SK_ARMOUR,         -2),
    APT(SP_KOBOLD,          SK_DODGING,         2),
    APT(SP_KOBOLD,          SK_STEALTH,         3),
    APT(SP_KOBOLD,          SK_STABBING,        2),
    APT(SP_KOBOLD,          SK_SHIELDS,        -2),
    APT(SP_KOBOLD,          SK_TRAPS_DOORS,     0),
    APT(SP_KOBOLD,          SK_UNARMED_COMBAT,  0),
    APT(SP_KOBOLD,          SK_SPELLCASTING,    0),
    APT(SP_KOBOLD,          SK_CONJURATIONS,   -1),
    APT(SP_KOBOLD,          SK_ENCHANTMENTS,   -1),
    APT(SP_KOBOLD,          SK_SUMMONINGS,     -1),
    APT(SP_KOBOLD,          SK_NECROMANCY,     -1),
    APT(SP_KOBOLD,          SK_TRANSLOCATIONS,  0),
    APT(SP_KOBOLD,          SK_TRANSMUTATIONS, -1),
    APT(SP_KOBOLD,          SK_FIRE_MAGIC,      0),
    APT(SP_KOBOLD,          SK_ICE_MAGIC,       0),
    APT(SP_KOBOLD,          SK_AIR_MAGIC,       0),
    APT(SP_KOBOLD,          SK_EARTH_MAGIC,     0),
    APT(SP_KOBOLD,          SK_POISON_MAGIC,    0),
    APT(SP_KOBOLD,          SK_INVOCATIONS,     0),
    APT(SP_KOBOLD,          SK_EVOCATIONS,      2),

    // SP_MUMMY
    APT(SP_MUMMY,           SK_FIGHTING,        0),
    APT(SP_MUMMY,           SK_SHORT_BLADES,   -2),
    APT(SP_MUMMY,           SK_LONG_BLADES,    -2),
    APT(SP_MUMMY,           SK_AXES,           -2),
    APT(SP_MUMMY,           SK_MACES_FLAILS,   -2),
    APT(SP_MUMMY,           SK_POLEARMS,       -2),
    APT(SP_MUMMY,           SK_STAVES,         -2),
    APT(SP_MUMMY,           SK_SLINGS,         -2),
    APT(SP_MUMMY,           SK_BOWS,           -2),
    APT(SP_MUMMY,           SK_CROSSBOWS,      -2),
    APT(SP_MUMMY,           SK_THROWING,       -2),
    APT(SP_MUMMY,           SK_ARMOUR,         -2),
    APT(SP_MUMMY,           SK_DODGING,        -2),
    APT(SP_MUMMY,           SK_STEALTH,        -2),
    APT(SP_MUMMY,           SK_STABBING,       -2),
    APT(SP_MUMMY,           SK_SHIELDS,        -2),
    APT(SP_MUMMY,           SK_TRAPS_DOORS,    -2),
    APT(SP_MUMMY,           SK_UNARMED_COMBAT, -2),
    APT(SP_MUMMY,           SK_SPELLCASTING,    0),
    APT(SP_MUMMY,           SK_CONJURATIONS,   -2),
    APT(SP_MUMMY,           SK_ENCHANTMENTS,   -2),
    APT(SP_MUMMY,           SK_SUMMONINGS,     -2),
    APT(SP_MUMMY,           SK_NECROMANCY,      0),
    APT(SP_MUMMY,           SK_TRANSLOCATIONS, -2),
    APT(SP_MUMMY,           SK_TRANSMUTATIONS, -2),
    APT(SP_MUMMY,           SK_FIRE_MAGIC,     -2),
    APT(SP_MUMMY,           SK_ICE_MAGIC,      -2),
    APT(SP_MUMMY,           SK_AIR_MAGIC,      -2),
    APT(SP_MUMMY,           SK_EARTH_MAGIC,    -2),
    APT(SP_MUMMY,           SK_POISON_MAGIC,   -2),
    APT(SP_MUMMY,           SK_INVOCATIONS,    -2),
    APT(SP_MUMMY,           SK_EVOCATIONS,     -2),

    // SP_NAGA
    APT(SP_NAGA,            SK_FIGHTING,        0),
    APT(SP_NAGA,            SK_SHORT_BLADES,    0),
    APT(SP_NAGA,            SK_LONG_BLADES,     0),
    APT(SP_NAGA,            SK_AXES,            0),
    APT(SP_NAGA,            SK_MACES_FLAILS,    0),
    APT(SP_NAGA,            SK_POLEARMS,        0),
    APT(SP_NAGA,            SK_STAVES,         -1),
    APT(SP_NAGA,            SK_SLINGS,         -1),
    APT(SP_NAGA,            SK_BOWS,           -1),
    APT(SP_NAGA,            SK_CROSSBOWS,      -1),
    APT(SP_NAGA,            SK_THROWING,       -1),
    APT(SP_NAGA,            SK_ARMOUR,         -2),
    APT(SP_NAGA,            SK_DODGING,        -2),
    APT(SP_NAGA,            SK_STEALTH,         5),
    APT(SP_NAGA,            SK_STABBING,        0),
    APT(SP_NAGA,            SK_SHIELDS,        -2),
    APT(SP_NAGA,            SK_TRAPS_DOORS,     0),
    APT(SP_NAGA,            SK_UNARMED_COMBAT,  0),
    APT(SP_NAGA,            SK_SPELLCASTING,    0),
    APT(SP_NAGA,            SK_CONJURATIONS,    0),
    APT(SP_NAGA,            SK_ENCHANTMENTS,    0),
    APT(SP_NAGA,            SK_SUMMONINGS,      0),
    APT(SP_NAGA,            SK_NECROMANCY,      0),
    APT(SP_NAGA,            SK_TRANSLOCATIONS,  0),
    APT(SP_NAGA,            SK_TRANSMUTATIONS,  0),
    APT(SP_NAGA,            SK_FIRE_MAGIC,      0),
    APT(SP_NAGA,            SK_ICE_MAGIC,       0),
    APT(SP_NAGA,            SK_AIR_MAGIC,       0),
    APT(SP_NAGA,            SK_EARTH_MAGIC,     0),
    APT(SP_NAGA,            SK_POISON_MAGIC,    3),
    APT(SP_NAGA,            SK_INVOCATIONS,     0),
    APT(SP_NAGA,            SK_EVOCATIONS,      0),

    // SP_OGRE
    APT(SP_OGRE,            SK_FIGHTING,        2),
    APT(SP_OGRE,            SK_SHORT_BLADES,   -4),
    APT(SP_OGRE,            SK_LONG_BLADES,    -3),
    APT(SP_OGRE,            SK_AXES,           -3),
    APT(SP_OGRE,            SK_MACES_FLAILS,    1),
    APT(SP_OGRE,            SK_POLEARMS,        0),
    APT(SP_OGRE,            SK_STAVES,         -1),
    APT(SP_OGRE,            SK_SLINGS,         -3),
    APT(SP_OGRE,            SK_BOWS,           -3),
    APT(SP_OGRE,            SK_CROSSBOWS,      -3),
    APT(SP_OGRE,            SK_THROWING,        1),
    APT(SP_OGRE,            SK_ARMOUR,         -2),
    APT(SP_OGRE,            SK_DODGING,        -1),
    APT(SP_OGRE,            SK_STEALTH,        -2),
    APT(SP_OGRE,            SK_STABBING,       -2),
    APT(SP_OGRE,            SK_SHIELDS,        -1),
    APT(SP_OGRE,            SK_TRAPS_DOORS,    -2),
    APT(SP_OGRE,            SK_UNARMED_COMBAT, -1),
    APT(SP_OGRE,            SK_SPELLCASTING,    2),
    APT(SP_OGRE,            SK_CONJURATIONS,   -3),
    APT(SP_OGRE,            SK_ENCHANTMENTS,   -3),
    APT(SP_OGRE,            SK_SUMMONINGS,     -3),
    APT(SP_OGRE,            SK_NECROMANCY,     -3),
    APT(SP_OGRE,            SK_TRANSLOCATIONS, -3),
    APT(SP_OGRE,            SK_TRANSMUTATIONS, -3),
    APT(SP_OGRE,            SK_FIRE_MAGIC,     -3),
    APT(SP_OGRE,            SK_ICE_MAGIC,      -3),
    APT(SP_OGRE,            SK_AIR_MAGIC,      -3),
    APT(SP_OGRE,            SK_EARTH_MAGIC,    -3),
    APT(SP_OGRE,            SK_POISON_MAGIC,   -3),
    APT(SP_OGRE,            SK_INVOCATIONS,     0),
    APT(SP_OGRE,            SK_EVOCATIONS,     -2),

    // SP_TROLL
    APT(SP_TROLL,           SK_FIGHTING,       -2),
    APT(SP_TROLL,           SK_SHORT_BLADES,   -2),
    APT(SP_TROLL,           SK_LONG_BLADES,    -2),
    APT(SP_TROLL,           SK_AXES,           -2),
    APT(SP_TROLL,           SK_MACES_FLAILS,   -1),
    APT(SP_TROLL,           SK_POLEARMS,       -2),
    APT(SP_TROLL,           SK_STAVES,         -2),
    APT(SP_TROLL,           SK_SLINGS,         -4),
    APT(SP_TROLL,           SK_BOWS,           -4),
    APT(SP_TROLL,           SK_CROSSBOWS,      -4),
    APT(SP_TROLL,           SK_THROWING,       -1),
    APT(SP_TROLL,           SK_ARMOUR,         -2),
    APT(SP_TROLL,           SK_DODGING,        -2),
    APT(SP_TROLL,           SK_STEALTH,        -5),
    APT(SP_TROLL,           SK_STABBING,       -2),
    APT(SP_TROLL,           SK_SHIELDS,        -2),
    APT(SP_TROLL,           SK_TRAPS_DOORS,    -4),
    APT(SP_TROLL,           SK_UNARMED_COMBAT,  0),
    APT(SP_TROLL,           SK_SPELLCASTING,   -4),
    APT(SP_TROLL,           SK_CONJURATIONS,   -3),
    APT(SP_TROLL,           SK_ENCHANTMENTS,   -4),
    APT(SP_TROLL,           SK_SUMMONINGS,     -3),
    APT(SP_TROLL,           SK_NECROMANCY,     -2),
    APT(SP_TROLL,           SK_TRANSLOCATIONS, -3),
    APT(SP_TROLL,           SK_TRANSMUTATIONS, -3),
    APT(SP_TROLL,           SK_FIRE_MAGIC,     -3),
    APT(SP_TROLL,           SK_ICE_MAGIC,      -3),
    APT(SP_TROLL,           SK_AIR_MAGIC,      -4),
    APT(SP_TROLL,           SK_EARTH_MAGIC,    -1),
    APT(SP_TROLL,           SK_POISON_MAGIC,   -3),
    APT(SP_TROLL,           SK_INVOCATIONS,    -2),
    APT(SP_TROLL,           SK_EVOCATIONS,     -3),

    // SP_RED_DRACONIAN
    APT(SP_RED_DRACONIAN,   SK_FIGHTING,        1),
    APT(SP_RED_DRACONIAN,   SK_SHORT_BLADES,    0),
    APT(SP_RED_DRACONIAN,   SK_LONG_BLADES,     0),
    APT(SP_RED_DRACONIAN,   SK_AXES,            0),
    APT(SP_RED_DRACONIAN,   SK_MACES_FLAILS,    0),
    APT(SP_RED_DRACONIAN,   SK_POLEARMS,        0),
    APT(SP_RED_DRACONIAN,   SK_STAVES,          0),
    APT(SP_RED_DRACONIAN,   SK_SLINGS,         -1),
    APT(SP_RED_DRACONIAN,   SK_BOWS,           -1),
    APT(SP_RED_DRACONIAN,   SK_CROSSBOWS,      -1),
    APT(SP_RED_DRACONIAN,   SK_THROWING,       -1),
    APT(SP_RED_DRACONIAN,   SK_ARMOUR,         -4),
    APT(SP_RED_DRACONIAN,   SK_DODGING,        -1),
    APT(SP_RED_DRACONIAN,   SK_STEALTH,        -1),
    APT(SP_RED_DRACONIAN,   SK_STABBING,        0),
    APT(SP_RED_DRACONIAN,   SK_SHIELDS,         0),
    APT(SP_RED_DRACONIAN,   SK_TRAPS_DOORS,     0),
    APT(SP_RED_DRACONIAN,   SK_UNARMED_COMBAT,  0),
    APT(SP_RED_DRACONIAN,   SK_SPELLCASTING,    0),
    APT(SP_RED_DRACONIAN,   SK_CONJURATIONS,    0),
    APT(SP_RED_DRACONIAN,   SK_ENCHANTMENTS,   -1),
    APT(SP_RED_DRACONIAN,   SK_SUMMONINGS,      0),
    APT(SP_RED_DRACONIAN,   SK_NECROMANCY,      0),
    APT(SP_RED_DRACONIAN,   SK_TRANSLOCATIONS,  0),
    APT(SP_RED_DRACONIAN,   SK_TRANSMUTATIONS,  0),
    APT(SP_RED_DRACONIAN,   SK_FIRE_MAGIC,      2),
    APT(SP_RED_DRACONIAN,   SK_ICE_MAGIC,      -2),
    APT(SP_RED_DRACONIAN,   SK_AIR_MAGIC,       0),
    APT(SP_RED_DRACONIAN,   SK_EARTH_MAGIC,     0),
    APT(SP_RED_DRACONIAN,   SK_POISON_MAGIC,    0),
    APT(SP_RED_DRACONIAN,   SK_INVOCATIONS,     0),
    APT(SP_RED_DRACONIAN,   SK_EVOCATIONS,      0),

    // SP_WHITE_DRACONIAN
    APT(SP_WHITE_DRACONIAN, SK_FIGHTING,        1),
    APT(SP_WHITE_DRACONIAN, SK_SHORT_BLADES,    0),
    APT(SP_WHITE_DRACONIAN, SK_LONG_BLADES,     0),
    APT(SP_WHITE_DRACONIAN, SK_AXES,            0),
    APT(SP_WHITE_DRACONIAN, SK_MACES_FLAILS,    0),
    APT(SP_WHITE_DRACONIAN, SK_POLEARMS,        0),
    APT(SP_WHITE_DRACONIAN, SK_STAVES,          0),
    APT(SP_WHITE_DRACONIAN, SK_SLINGS,         -1),
    APT(SP_WHITE_DRACONIAN, SK_BOWS,           -1),
    APT(SP_WHITE_DRACONIAN, SK_CROSSBOWS,      -1),
    APT(SP_WHITE_DRACONIAN, SK_THROWING,       -1),
    APT(SP_WHITE_DRACONIAN, SK_ARMOUR,         -4),
    APT(SP_WHITE_DRACONIAN, SK_DODGING,        -1),
    APT(SP_WHITE_DRACONIAN, SK_STEALTH,        -1),
    APT(SP_WHITE_DRACONIAN, SK_STABBING,        0),
    APT(SP_WHITE_DRACONIAN, SK_SHIELDS,         0),
    APT(SP_WHITE_DRACONIAN, SK_TRAPS_DOORS,     0),
    APT(SP_WHITE_DRACONIAN, SK_UNARMED_COMBAT,  0),
    APT(SP_WHITE_DRACONIAN, SK_SPELLCASTING,    0),
    APT(SP_WHITE_DRACONIAN, SK_CONJURATIONS,    0),
    APT(SP_WHITE_DRACONIAN, SK_ENCHANTMENTS,   -1),
    APT(SP_WHITE_DRACONIAN, SK_SUMMONINGS,      0),
    APT(SP_WHITE_DRACONIAN, SK_NECROMANCY,      0),
    APT(SP_WHITE_DRACONIAN, SK_TRANSLOCATIONS,  0),
    APT(SP_WHITE_DRACONIAN, SK_TRANSMUTATIONS,  0),
    APT(SP_WHITE_DRACONIAN, SK_FIRE_MAGIC,     -2),
    APT(SP_WHITE_DRACONIAN, SK_ICE_MAGIC,       2),
    APT(SP_WHITE_DRACONIAN, SK_AIR_MAGIC,       0),
    APT(SP_WHITE_DRACONIAN, SK_EARTH_MAGIC,     0),
    APT(SP_WHITE_DRACONIAN, SK_POISON_MAGIC,    0),
    APT(SP_WHITE_DRACONIAN, SK_INVOCATIONS,     0),
    APT(SP_WHITE_DRACONIAN, SK_EVOCATIONS,      0),

    // SP_GREEN_DRACONIAN
    APT(SP_GREEN_DRACONIAN, SK_FIGHTING,        1),
    APT(SP_GREEN_DRACONIAN, SK_SHORT_BLADES,    0),
    APT(SP_GREEN_DRACONIAN, SK_LONG_BLADES,     0),
    APT(SP_GREEN_DRACONIAN, SK_AXES,            0),
    APT(SP_GREEN_DRACONIAN, SK_MACES_FLAILS,    0),
    APT(SP_GREEN_DRACONIAN, SK_POLEARMS,        0),
    APT(SP_GREEN_DRACONIAN, SK_STAVES,          0),
    APT(SP_GREEN_DRACONIAN, SK_SLINGS,         -1),
    APT(SP_GREEN_DRACONIAN, SK_BOWS,           -1),
    APT(SP_GREEN_DRACONIAN, SK_CROSSBOWS,      -1),
    APT(SP_GREEN_DRACONIAN, SK_THROWING,       -1),
    APT(SP_GREEN_DRACONIAN, SK_ARMOUR,         -4),
    APT(SP_GREEN_DRACONIAN, SK_DODGING,        -1),
    APT(SP_GREEN_DRACONIAN, SK_STEALTH,        -1),
    APT(SP_GREEN_DRACONIAN, SK_STABBING,        0),
    APT(SP_GREEN_DRACONIAN, SK_SHIELDS,         0),
    APT(SP_GREEN_DRACONIAN, SK_TRAPS_DOORS,     0),
    APT(SP_GREEN_DRACONIAN, SK_UNARMED_COMBAT,  0),
    APT(SP_GREEN_DRACONIAN, SK_SPELLCASTING,    0),
    APT(SP_GREEN_DRACONIAN, SK_CONJURATIONS,    0),
    APT(SP_GREEN_DRACONIAN, SK_ENCHANTMENTS,   -1),
    APT(SP_GREEN_DRACONIAN, SK_SUMMONINGS,      0),
    APT(SP_GREEN_DRACONIAN, SK_NECROMANCY,      0),
    APT(SP_GREEN_DRACONIAN, SK_TRANSLOCATIONS,  0),
    APT(SP_GREEN_DRACONIAN, SK_TRANSMUTATIONS,  0),
    APT(SP_GREEN_DRACONIAN, SK_FIRE_MAGIC,      0),
    APT(SP_GREEN_DRACONIAN, SK_ICE_MAGIC,       0),
    APT(SP_GREEN_DRACONIAN, SK_AIR_MAGIC,       0),
    APT(SP_GREEN_DRACONIAN, SK_EARTH_MAGIC,     0),
    APT(SP_GREEN_DRACONIAN, SK_POISON_MAGIC,    2),
    APT(SP_GREEN_DRACONIAN, SK_INVOCATIONS,     0),
    APT(SP_GREEN_DRACONIAN, SK_EVOCATIONS,      0),

    // SP_YELLOW_DRACONIAN
    APT(SP_YELLOW_DRACONIAN,SK_FIGHTING,        1),
    APT(SP_YELLOW_DRACONIAN,SK_SHORT_BLADES,    0),
    APT(SP_YELLOW_DRACONIAN,SK_LONG_BLADES,     0),
    APT(SP_YELLOW_DRACONIAN,SK_AXES,            0),
    APT(SP_YELLOW_DRACONIAN,SK_MACES_FLAILS,    0),
    APT(SP_YELLOW_DRACONIAN,SK_POLEARMS,        0),
    APT(SP_YELLOW_DRACONIAN,SK_STAVES,          0),
    APT(SP_YELLOW_DRACONIAN,SK_SLINGS,         -1),
    APT(SP_YELLOW_DRACONIAN,SK_BOWS,           -1),
    APT(SP_YELLOW_DRACONIAN,SK_CROSSBOWS,      -1),
    APT(SP_YELLOW_DRACONIAN,SK_THROWING,       -1),
    APT(SP_YELLOW_DRACONIAN,SK_ARMOUR,         -4),
    APT(SP_YELLOW_DRACONIAN,SK_DODGING,        -1),
    APT(SP_YELLOW_DRACONIAN,SK_STEALTH,        -1),
    APT(SP_YELLOW_DRACONIAN,SK_STABBING,        0),
    APT(SP_YELLOW_DRACONIAN,SK_SHIELDS,         0),
    APT(SP_YELLOW_DRACONIAN,SK_TRAPS_DOORS,     0),
    APT(SP_YELLOW_DRACONIAN,SK_UNARMED_COMBAT,  0),
    APT(SP_YELLOW_DRACONIAN,SK_SPELLCASTING,    0),
    APT(SP_YELLOW_DRACONIAN,SK_CONJURATIONS,    0),
    APT(SP_YELLOW_DRACONIAN,SK_ENCHANTMENTS,   -1),
    APT(SP_YELLOW_DRACONIAN,SK_SUMMONINGS,      0),
    APT(SP_YELLOW_DRACONIAN,SK_NECROMANCY,      0),
    APT(SP_YELLOW_DRACONIAN,SK_TRANSLOCATIONS,  0),
    APT(SP_YELLOW_DRACONIAN,SK_TRANSMUTATIONS,  0),
    APT(SP_YELLOW_DRACONIAN,SK_FIRE_MAGIC,      0),
    APT(SP_YELLOW_DRACONIAN,SK_ICE_MAGIC,       0),
    APT(SP_YELLOW_DRACONIAN,SK_AIR_MAGIC,       0),
    APT(SP_YELLOW_DRACONIAN,SK_EARTH_MAGIC,     0),
    APT(SP_YELLOW_DRACONIAN,SK_POISON_MAGIC,    0),
    APT(SP_YELLOW_DRACONIAN,SK_INVOCATIONS,     0),
    APT(SP_YELLOW_DRACONIAN,SK_EVOCATIONS,      0),

    // SP_GREY_DRACONIAN
    APT(SP_GREY_DRACONIAN,  SK_FIGHTING,       -1),
    APT(SP_GREY_DRACONIAN,  SK_SHORT_BLADES,    0),
    APT(SP_GREY_DRACONIAN,  SK_LONG_BLADES,     0),
    APT(SP_GREY_DRACONIAN,  SK_AXES,            0),
    APT(SP_GREY_DRACONIAN,  SK_MACES_FLAILS,    0),
    APT(SP_GREY_DRACONIAN,  SK_POLEARMS,        0),
    APT(SP_GREY_DRACONIAN,  SK_STAVES,          0),
    APT(SP_GREY_DRACONIAN,  SK_SLINGS,         -1),
    APT(SP_GREY_DRACONIAN,  SK_BOWS,           -1),
    APT(SP_GREY_DRACONIAN,  SK_CROSSBOWS,      -1),
    APT(SP_GREY_DRACONIAN,  SK_THROWING,        0),
    APT(SP_GREY_DRACONIAN,  SK_ARMOUR,         -4),
    APT(SP_GREY_DRACONIAN,  SK_DODGING,        -1),
    APT(SP_GREY_DRACONIAN,  SK_STEALTH,         2),
    APT(SP_GREY_DRACONIAN,  SK_STABBING,        2),
    APT(SP_GREY_DRACONIAN,  SK_SHIELDS,         0),
    APT(SP_GREY_DRACONIAN,  SK_TRAPS_DOORS,     0),
    APT(SP_GREY_DRACONIAN,  SK_UNARMED_COMBAT,  0),
    APT(SP_GREY_DRACONIAN,  SK_SPELLCASTING,    0),
    APT(SP_GREY_DRACONIAN,  SK_CONJURATIONS,    0),
    APT(SP_GREY_DRACONIAN,  SK_ENCHANTMENTS,   -1),
    APT(SP_GREY_DRACONIAN,  SK_SUMMONINGS,      0),
    APT(SP_GREY_DRACONIAN,  SK_NECROMANCY,      0),
    APT(SP_GREY_DRACONIAN,  SK_TRANSLOCATIONS,  0),
    APT(SP_GREY_DRACONIAN,  SK_TRANSMUTATIONS,  0),
    APT(SP_GREY_DRACONIAN,  SK_FIRE_MAGIC,      0),
    APT(SP_GREY_DRACONIAN,  SK_ICE_MAGIC,       0),
    APT(SP_GREY_DRACONIAN,  SK_AIR_MAGIC,       0),
    APT(SP_GREY_DRACONIAN,  SK_EARTH_MAGIC,     0),
    APT(SP_GREY_DRACONIAN,  SK_POISON_MAGIC,    0),
    APT(SP_GREY_DRACONIAN,  SK_INVOCATIONS,     0),
    APT(SP_GREY_DRACONIAN,  SK_EVOCATIONS,      0),

    // SP_BLACK_DRACONIAN
    APT(SP_BLACK_DRACONIAN, SK_FIGHTING,        1),
    APT(SP_BLACK_DRACONIAN, SK_SHORT_BLADES,    0),
    APT(SP_BLACK_DRACONIAN, SK_LONG_BLADES,     0),
    APT(SP_BLACK_DRACONIAN, SK_AXES,            0),
    APT(SP_BLACK_DRACONIAN, SK_MACES_FLAILS,    0),
    APT(SP_BLACK_DRACONIAN, SK_POLEARMS,        0),
    APT(SP_BLACK_DRACONIAN, SK_STAVES,          0),
    APT(SP_BLACK_DRACONIAN, SK_SLINGS,         -1),
    APT(SP_BLACK_DRACONIAN, SK_BOWS,           -1),
    APT(SP_BLACK_DRACONIAN, SK_CROSSBOWS,      -1),
    APT(SP_BLACK_DRACONIAN, SK_THROWING,       -1),
    APT(SP_BLACK_DRACONIAN, SK_ARMOUR,         -4),
    APT(SP_BLACK_DRACONIAN, SK_DODGING,        -1),
    APT(SP_BLACK_DRACONIAN, SK_STEALTH,        -1),
    APT(SP_BLACK_DRACONIAN, SK_STABBING,        0),
    APT(SP_BLACK_DRACONIAN, SK_SHIELDS,         0),
    APT(SP_BLACK_DRACONIAN, SK_TRAPS_DOORS,     0),
    APT(SP_BLACK_DRACONIAN, SK_UNARMED_COMBAT,  0),
    APT(SP_BLACK_DRACONIAN, SK_SPELLCASTING,    0),
    APT(SP_BLACK_DRACONIAN, SK_CONJURATIONS,    0),
    APT(SP_BLACK_DRACONIAN, SK_ENCHANTMENTS,   -1),
    APT(SP_BLACK_DRACONIAN, SK_SUMMONINGS,      0),
    APT(SP_BLACK_DRACONIAN, SK_NECROMANCY,      0),
    APT(SP_BLACK_DRACONIAN, SK_TRANSLOCATIONS,  0),
    APT(SP_BLACK_DRACONIAN, SK_TRANSMUTATIONS,  0),
    APT(SP_BLACK_DRACONIAN, SK_FIRE_MAGIC,      0),
    APT(SP_BLACK_DRACONIAN, SK_ICE_MAGIC,       0),
    APT(SP_BLACK_DRACONIAN, SK_AIR_MAGIC,       2),
    APT(SP_BLACK_DRACONIAN, SK_EARTH_MAGIC,    -2),
    APT(SP_BLACK_DRACONIAN, SK_POISON_MAGIC,    0),
    APT(SP_BLACK_DRACONIAN, SK_INVOCATIONS,     0),
    APT(SP_BLACK_DRACONIAN, SK_EVOCATIONS,      0),

    // SP_PURPLE_DRACONIAN
    APT(SP_PURPLE_DRACONIAN,SK_FIGHTING,        1),
    APT(SP_PURPLE_DRACONIAN,SK_SHORT_BLADES,    0),
    APT(SP_PURPLE_DRACONIAN,SK_LONG_BLADES,     0),
    APT(SP_PURPLE_DRACONIAN,SK_AXES,            0),
    APT(SP_PURPLE_DRACONIAN,SK_MACES_FLAILS,    0),
    APT(SP_PURPLE_DRACONIAN,SK_POLEARMS,        0),
    APT(SP_PURPLE_DRACONIAN,SK_STAVES,          0),
    APT(SP_PURPLE_DRACONIAN,SK_SLINGS,         -1),
    APT(SP_PURPLE_DRACONIAN,SK_BOWS,           -1),
    APT(SP_PURPLE_DRACONIAN,SK_CROSSBOWS,      -1),
    APT(SP_PURPLE_DRACONIAN,SK_THROWING,       -1),
    APT(SP_PURPLE_DRACONIAN,SK_ARMOUR,         -4),
    APT(SP_PURPLE_DRACONIAN,SK_DODGING,        -1),
    APT(SP_PURPLE_DRACONIAN,SK_STEALTH,        -1),
    APT(SP_PURPLE_DRACONIAN,SK_STABBING,        0),
    APT(SP_PURPLE_DRACONIAN,SK_SHIELDS,         0),
    APT(SP_PURPLE_DRACONIAN,SK_TRAPS_DOORS,     0),
    APT(SP_PURPLE_DRACONIAN,SK_UNARMED_COMBAT,  0),
    APT(SP_PURPLE_DRACONIAN,SK_SPELLCASTING,    2),
    APT(SP_PURPLE_DRACONIAN,SK_CONJURATIONS,    0),
    APT(SP_PURPLE_DRACONIAN,SK_ENCHANTMENTS,    1),
    APT(SP_PURPLE_DRACONIAN,SK_SUMMONINGS,      0),
    APT(SP_PURPLE_DRACONIAN,SK_NECROMANCY,      0),
    APT(SP_PURPLE_DRACONIAN,SK_TRANSLOCATIONS,  0),
    APT(SP_PURPLE_DRACONIAN,SK_TRANSMUTATIONS,  0),
    APT(SP_PURPLE_DRACONIAN,SK_FIRE_MAGIC,      0),
    APT(SP_PURPLE_DRACONIAN,SK_ICE_MAGIC,       0),
    APT(SP_PURPLE_DRACONIAN,SK_AIR_MAGIC,       0),
    APT(SP_PURPLE_DRACONIAN,SK_EARTH_MAGIC,     0),
    APT(SP_PURPLE_DRACONIAN,SK_POISON_MAGIC,    0),
    APT(SP_PURPLE_DRACONIAN,SK_INVOCATIONS,     0),
    APT(SP_PURPLE_DRACONIAN,SK_EVOCATIONS,      1),

    // SP_MOTTLED_DRACONIAN
    APT(SP_MOTTLED_DRACONIAN,SK_FIGHTING,        1),
    APT(SP_MOTTLED_DRACONIAN,SK_SHORT_BLADES,    0),
    APT(SP_MOTTLED_DRACONIAN,SK_LONG_BLADES,     0),
    APT(SP_MOTTLED_DRACONIAN,SK_AXES,            0),
    APT(SP_MOTTLED_DRACONIAN,SK_MACES_FLAILS,    0),
    APT(SP_MOTTLED_DRACONIAN,SK_POLEARMS,        0),
    APT(SP_MOTTLED_DRACONIAN,SK_STAVES,          0),
    APT(SP_MOTTLED_DRACONIAN,SK_SLINGS,         -1),
    APT(SP_MOTTLED_DRACONIAN,SK_BOWS,           -1),
    APT(SP_MOTTLED_DRACONIAN,SK_CROSSBOWS,      -1),
    APT(SP_MOTTLED_DRACONIAN,SK_THROWING,       -1),
    APT(SP_MOTTLED_DRACONIAN,SK_ARMOUR,         -4),
    APT(SP_MOTTLED_DRACONIAN,SK_DODGING,        -1),
    APT(SP_MOTTLED_DRACONIAN,SK_STEALTH,        -1),
    APT(SP_MOTTLED_DRACONIAN,SK_STABBING,        0),
    APT(SP_MOTTLED_DRACONIAN,SK_SHIELDS,         0),
    APT(SP_MOTTLED_DRACONIAN,SK_TRAPS_DOORS,     0),
    APT(SP_MOTTLED_DRACONIAN,SK_UNARMED_COMBAT,  0),
    APT(SP_MOTTLED_DRACONIAN,SK_SPELLCASTING,    0),
    APT(SP_MOTTLED_DRACONIAN,SK_CONJURATIONS,    0),
    APT(SP_MOTTLED_DRACONIAN,SK_ENCHANTMENTS,   -1),
    APT(SP_MOTTLED_DRACONIAN,SK_SUMMONINGS,      0),
    APT(SP_MOTTLED_DRACONIAN,SK_NECROMANCY,      0),
    APT(SP_MOTTLED_DRACONIAN,SK_TRANSLOCATIONS,  0),
    APT(SP_MOTTLED_DRACONIAN,SK_TRANSMUTATIONS,  0),
    APT(SP_MOTTLED_DRACONIAN,SK_FIRE_MAGIC,      1),
    APT(SP_MOTTLED_DRACONIAN,SK_ICE_MAGIC,       0),
    APT(SP_MOTTLED_DRACONIAN,SK_AIR_MAGIC,       0),
    APT(SP_MOTTLED_DRACONIAN,SK_EARTH_MAGIC,     0),
    APT(SP_MOTTLED_DRACONIAN,SK_POISON_MAGIC,    0),
    APT(SP_MOTTLED_DRACONIAN,SK_INVOCATIONS,     0),
    APT(SP_MOTTLED_DRACONIAN,SK_EVOCATIONS,      0),

    // SP_PALE_DRACONIAN
    APT(SP_PALE_DRACONIAN,  SK_FIGHTING,        1),
    APT(SP_PALE_DRACONIAN,  SK_SHORT_BLADES,    0),
    APT(SP_PALE_DRACONIAN,  SK_LONG_BLADES,     0),
    APT(SP_PALE_DRACONIAN,  SK_AXES,            0),
    APT(SP_PALE_DRACONIAN,  SK_MACES_FLAILS,    0),
    APT(SP_PALE_DRACONIAN,  SK_POLEARMS,        0),
    APT(SP_PALE_DRACONIAN,  SK_STAVES,          0),
    APT(SP_PALE_DRACONIAN,  SK_SLINGS,         -1),
    APT(SP_PALE_DRACONIAN,  SK_BOWS,           -1),
    APT(SP_PALE_DRACONIAN,  SK_CROSSBOWS,      -1),
    APT(SP_PALE_DRACONIAN,  SK_THROWING,       -1),
    APT(SP_PALE_DRACONIAN,  SK_ARMOUR,         -4),
    APT(SP_PALE_DRACONIAN,  SK_DODGING,        -1),
    APT(SP_PALE_DRACONIAN,  SK_STEALTH,        -1),
    APT(SP_PALE_DRACONIAN,  SK_STABBING,        0),
    APT(SP_PALE_DRACONIAN,  SK_SHIELDS,         0),
    APT(SP_PALE_DRACONIAN,  SK_TRAPS_DOORS,     0),
    APT(SP_PALE_DRACONIAN,  SK_UNARMED_COMBAT,  0),
    APT(SP_PALE_DRACONIAN,  SK_SPELLCASTING,    0),
    APT(SP_PALE_DRACONIAN,  SK_CONJURATIONS,    0),
    APT(SP_PALE_DRACONIAN,  SK_ENCHANTMENTS,   -1),
    APT(SP_PALE_DRACONIAN,  SK_SUMMONINGS,      0),
    APT(SP_PALE_DRACONIAN,  SK_NECROMANCY,      0),
    APT(SP_PALE_DRACONIAN,  SK_TRANSLOCATIONS,  0),
    APT(SP_PALE_DRACONIAN,  SK_TRANSMUTATIONS,  0),
    APT(SP_PALE_DRACONIAN,  SK_FIRE_MAGIC,      1),
    APT(SP_PALE_DRACONIAN,  SK_ICE_MAGIC,       0),
    APT(SP_PALE_DRACONIAN,  SK_AIR_MAGIC,       1),
    APT(SP_PALE_DRACONIAN,  SK_EARTH_MAGIC,     0),
    APT(SP_PALE_DRACONIAN,  SK_POISON_MAGIC,    0),
    APT(SP_PALE_DRACONIAN,  SK_INVOCATIONS,     0),
    APT(SP_PALE_DRACONIAN,  SK_EVOCATIONS,      1),

    // SP_BASE_DRACONIAN
    APT(SP_BASE_DRACONIAN,  SK_FIGHTING,        1),
    APT(SP_BASE_DRACONIAN,  SK_SHORT_BLADES,    0),
    APT(SP_BASE_DRACONIAN,  SK_LONG_BLADES,     0),
    APT(SP_BASE_DRACONIAN,  SK_AXES,            0),
    APT(SP_BASE_DRACONIAN,  SK_MACES_FLAILS,    0),
    APT(SP_BASE_DRACONIAN,  SK_POLEARMS,        0),
    APT(SP_BASE_DRACONIAN,  SK_STAVES,          0),
    APT(SP_BASE_DRACONIAN,  SK_SLINGS,         -1),
    APT(SP_BASE_DRACONIAN,  SK_BOWS,           -1),
    APT(SP_BASE_DRACONIAN,  SK_CROSSBOWS,      -1),
    APT(SP_BASE_DRACONIAN,  SK_THROWING,       -1),
    APT(SP_BASE_DRACONIAN,  SK_ARMOUR,         -4),
    APT(SP_BASE_DRACONIAN,  SK_DODGING,        -1),
    APT(SP_BASE_DRACONIAN,  SK_STEALTH,        -1),
    APT(SP_BASE_DRACONIAN,  SK_STABBING,        0),
    APT(SP_BASE_DRACONIAN,  SK_SHIELDS,         0),
    APT(SP_BASE_DRACONIAN,  SK_TRAPS_DOORS,     0),
    APT(SP_BASE_DRACONIAN,  SK_UNARMED_COMBAT,  0),
    APT(SP_BASE_DRACONIAN,  SK_SPELLCASTING,    0),
    APT(SP_BASE_DRACONIAN,  SK_CONJURATIONS,    0),
    APT(SP_BASE_DRACONIAN,  SK_ENCHANTMENTS,   -1),
    APT(SP_BASE_DRACONIAN,  SK_SUMMONINGS,      0),
    APT(SP_BASE_DRACONIAN,  SK_NECROMANCY,      0),
    APT(SP_BASE_DRACONIAN,  SK_TRANSLOCATIONS,  0),
    APT(SP_BASE_DRACONIAN,  SK_TRANSMUTATIONS,  0),
    APT(SP_BASE_DRACONIAN,  SK_FIRE_MAGIC,      0),
    APT(SP_BASE_DRACONIAN,  SK_ICE_MAGIC,       0),
    APT(SP_BASE_DRACONIAN,  SK_AIR_MAGIC,       0),
    APT(SP_BASE_DRACONIAN,  SK_EARTH_MAGIC,     0),
    APT(SP_BASE_DRACONIAN,  SK_POISON_MAGIC,    0),
    APT(SP_BASE_DRACONIAN,  SK_INVOCATIONS,     0),
    APT(SP_BASE_DRACONIAN,  SK_EVOCATIONS,      0),

    // SP_CENTAUR
    APT(SP_CENTAUR,         SK_FIGHTING,        0),
    APT(SP_CENTAUR,         SK_SHORT_BLADES,   -1),
    APT(SP_CENTAUR,         SK_LONG_BLADES,    -1),
    APT(SP_CENTAUR,         SK_AXES,           -1),
    APT(SP_CENTAUR,         SK_MACES_FLAILS,   -1),
    APT(SP_CENTAUR,         SK_POLEARMS,       -1),
    APT(SP_CENTAUR,         SK_STAVES,         -1),
    APT(SP_CENTAUR,         SK_SLINGS,          1),
    APT(SP_CENTAUR,         SK_BOWS,            3),
    APT(SP_CENTAUR,         SK_CROSSBOWS,       1),
    APT(SP_CENTAUR,         SK_THROWING,        3),
    APT(SP_CENTAUR,         SK_ARMOUR,         -3),
    APT(SP_CENTAUR,         SK_DODGING,        -3),
    APT(SP_CENTAUR,         SK_STEALTH,        -4),
    APT(SP_CENTAUR,         SK_STABBING,       -3),
    APT(SP_CENTAUR,         SK_SHIELDS,        -3),
    APT(SP_CENTAUR,         SK_TRAPS_DOORS,    -2),
    APT(SP_CENTAUR,         SK_UNARMED_COMBAT,  0),
    APT(SP_CENTAUR,         SK_SPELLCASTING,   -2),
    APT(SP_CENTAUR,         SK_CONJURATIONS,   -1),
    APT(SP_CENTAUR,         SK_ENCHANTMENTS,   -1),
    APT(SP_CENTAUR,         SK_SUMMONINGS,     -1),
    APT(SP_CENTAUR,         SK_NECROMANCY,     -1),
    APT(SP_CENTAUR,         SK_TRANSLOCATIONS, -1),
    APT(SP_CENTAUR,         SK_TRANSMUTATIONS, -1),
    APT(SP_CENTAUR,         SK_FIRE_MAGIC,     -1),
    APT(SP_CENTAUR,         SK_ICE_MAGIC,      -1),
    APT(SP_CENTAUR,         SK_AIR_MAGIC,      -1),
    APT(SP_CENTAUR,         SK_EARTH_MAGIC,    -1),
    APT(SP_CENTAUR,         SK_POISON_MAGIC,   -2),
    APT(SP_CENTAUR,         SK_INVOCATIONS,     0),
    APT(SP_CENTAUR,         SK_EVOCATIONS,     -1),

    // SP_DEMIGOD
    APT(SP_DEMIGOD,         SK_FIGHTING,       -1),
    APT(SP_DEMIGOD,         SK_SHORT_BLADES,   -1),
    APT(SP_DEMIGOD,         SK_LONG_BLADES,    -1),
    APT(SP_DEMIGOD,         SK_AXES,           -1),
    APT(SP_DEMIGOD,         SK_MACES_FLAILS,   -1),
    APT(SP_DEMIGOD,         SK_POLEARMS,       -1),
    APT(SP_DEMIGOD,         SK_STAVES,         -1),
    APT(SP_DEMIGOD,         SK_SLINGS,         -1),
    APT(SP_DEMIGOD,         SK_BOWS,           -1),
    APT(SP_DEMIGOD,         SK_CROSSBOWS,      -1),
    APT(SP_DEMIGOD,         SK_THROWING,       -1),
    APT(SP_DEMIGOD,         SK_ARMOUR,         -1),
    APT(SP_DEMIGOD,         SK_DODGING,        -1),
    APT(SP_DEMIGOD,         SK_STEALTH,        -1),
    APT(SP_DEMIGOD,         SK_STABBING,       -1),
    APT(SP_DEMIGOD,         SK_SHIELDS,        -1),
    APT(SP_DEMIGOD,         SK_TRAPS_DOORS,    -1),
    APT(SP_DEMIGOD,         SK_UNARMED_COMBAT, -1),
    APT(SP_DEMIGOD,         SK_SPELLCASTING,   -1),
    APT(SP_DEMIGOD,         SK_CONJURATIONS,   -1),
    APT(SP_DEMIGOD,         SK_ENCHANTMENTS,   -1),
    APT(SP_DEMIGOD,         SK_SUMMONINGS,     -1),
    APT(SP_DEMIGOD,         SK_NECROMANCY,     -1),
    APT(SP_DEMIGOD,         SK_TRANSLOCATIONS, -1),
    APT(SP_DEMIGOD,         SK_TRANSMUTATIONS, -1),
    APT(SP_DEMIGOD,         SK_FIRE_MAGIC,     -1),
    APT(SP_DEMIGOD,         SK_ICE_MAGIC,      -1),
    APT(SP_DEMIGOD,         SK_AIR_MAGIC,      -1),
    APT(SP_DEMIGOD,         SK_EARTH_MAGIC,    -1),
    APT(SP_DEMIGOD,         SK_POISON_MAGIC,   -1),
    APT(SP_DEMIGOD,         SK_INVOCATIONS,     0),
    APT(SP_DEMIGOD,         SK_EVOCATIONS,     -1),

    // SP_SPRIGGAN
    APT(SP_SPRIGGAN,        SK_FIGHTING,       -2),
    APT(SP_SPRIGGAN,        SK_SHORT_BLADES,    1),
    APT(SP_SPRIGGAN,        SK_LONG_BLADES,    -2),
    APT(SP_SPRIGGAN,        SK_AXES,           -2),
    APT(SP_SPRIGGAN,        SK_MACES_FLAILS,   -3),
    APT(SP_SPRIGGAN,        SK_POLEARMS,       -3),
    APT(SP_SPRIGGAN,        SK_STAVES,         -3),
    APT(SP_SPRIGGAN,        SK_SLINGS,          2),
    APT(SP_SPRIGGAN,        SK_BOWS,            2),
    APT(SP_SPRIGGAN,        SK_CROSSBOWS,       0),
    APT(SP_SPRIGGAN,        SK_THROWING,        1),
    APT(SP_SPRIGGAN,        SK_ARMOUR,         -3),
    APT(SP_SPRIGGAN,        SK_DODGING,         4),
    APT(SP_SPRIGGAN,        SK_STEALTH,         4),
    APT(SP_SPRIGGAN,        SK_STABBING,        4),
    APT(SP_SPRIGGAN,        SK_SHIELDS,        -3),
    APT(SP_SPRIGGAN,        SK_TRAPS_DOORS,     3),
    APT(SP_SPRIGGAN,        SK_UNARMED_COMBAT, -2),
    APT(SP_SPRIGGAN,        SK_SPELLCASTING,    3),
    APT(SP_SPRIGGAN,        SK_CONJURATIONS,   -3),
    APT(SP_SPRIGGAN,        SK_ENCHANTMENTS,    4),
    APT(SP_SPRIGGAN,        SK_SUMMONINGS,     -2),
    APT(SP_SPRIGGAN,        SK_NECROMANCY,     -1),
    APT(SP_SPRIGGAN,        SK_TRANSLOCATIONS,  4),
    APT(SP_SPRIGGAN,        SK_TRANSMUTATIONS,  3),
    APT(SP_SPRIGGAN,        SK_FIRE_MAGIC,     -2),
    APT(SP_SPRIGGAN,        SK_ICE_MAGIC,      -2),
    APT(SP_SPRIGGAN,        SK_AIR_MAGIC,      -1),
    APT(SP_SPRIGGAN,        SK_EARTH_MAGIC,    -1),
    APT(SP_SPRIGGAN,        SK_POISON_MAGIC,    0),
    APT(SP_SPRIGGAN,        SK_INVOCATIONS,    -1),
    APT(SP_SPRIGGAN,        SK_EVOCATIONS,      3),

    // SP_MINOTAUR
    APT(SP_MINOTAUR,        SK_FIGHTING,        2),
    APT(SP_MINOTAUR,        SK_SHORT_BLADES,    2),
    APT(SP_MINOTAUR,        SK_LONG_BLADES,     2),
    APT(SP_MINOTAUR,        SK_AXES,            2),
    APT(SP_MINOTAUR,        SK_MACES_FLAILS,    2),
    APT(SP_MINOTAUR,        SK_POLEARMS,        2),
    APT(SP_MINOTAUR,        SK_STAVES,          2),
    APT(SP_MINOTAUR,        SK_SLINGS,          1),
    APT(SP_MINOTAUR,        SK_BOWS,            1),
    APT(SP_MINOTAUR,        SK_CROSSBOWS,       1),
    APT(SP_MINOTAUR,        SK_THROWING,        1),
    APT(SP_MINOTAUR,        SK_ARMOUR,          1),
    APT(SP_MINOTAUR,        SK_DODGING,         1),
    APT(SP_MINOTAUR,        SK_STEALTH,        -2),
    APT(SP_MINOTAUR,        SK_STABBING,        0),
    APT(SP_MINOTAUR,        SK_SHIELDS,         1),
    APT(SP_MINOTAUR,        SK_TRAPS_DOORS,    -1),
    APT(SP_MINOTAUR,        SK_UNARMED_COMBAT,  1),
    APT(SP_MINOTAUR,        SK_SPELLCASTING,   -3),
    APT(SP_MINOTAUR,        SK_CONJURATIONS,   -3),
    APT(SP_MINOTAUR,        SK_ENCHANTMENTS,   -3),
    APT(SP_MINOTAUR,        SK_SUMMONINGS,     -3),
    APT(SP_MINOTAUR,        SK_NECROMANCY,     -3),
    APT(SP_MINOTAUR,        SK_TRANSLOCATIONS, -3),
    APT(SP_MINOTAUR,        SK_TRANSMUTATIONS, -3),
    APT(SP_MINOTAUR,        SK_FIRE_MAGIC,     -3),
    APT(SP_MINOTAUR,        SK_ICE_MAGIC,      -3),
    APT(SP_MINOTAUR,        SK_AIR_MAGIC,      -3),
    APT(SP_MINOTAUR,        SK_EARTH_MAGIC,    -3),
    APT(SP_MINOTAUR,        SK_POISON_MAGIC,   -3),
    APT(SP_MINOTAUR,        SK_INVOCATIONS,    -1),
    APT(SP_MINOTAUR,        SK_EVOCATIONS,     -3),

    // SP_DEMONSPAWN
    APT(SP_DEMONSPAWN,      SK_FIGHTING,        0),
    APT(SP_DEMONSPAWN,      SK_SHORT_BLADES,   -1),
    APT(SP_DEMONSPAWN,      SK_LONG_BLADES,    -1),
    APT(SP_DEMONSPAWN,      SK_AXES,           -1),
    APT(SP_DEMONSPAWN,      SK_MACES_FLAILS,   -1),
    APT(SP_DEMONSPAWN,      SK_POLEARMS,       -1),
    APT(SP_DEMONSPAWN,      SK_STAVES,         -1),
    APT(SP_DEMONSPAWN,      SK_SLINGS,         -1),
    APT(SP_DEMONSPAWN,      SK_BOWS,           -1),
    APT(SP_DEMONSPAWN,      SK_CROSSBOWS,      -1),
    APT(SP_DEMONSPAWN,      SK_THROWING,       -1),
    APT(SP_DEMONSPAWN,      SK_ARMOUR,         -1),
    APT(SP_DEMONSPAWN,      SK_DODGING,        -1),
    APT(SP_DEMONSPAWN,      SK_STEALTH,        -1),
    APT(SP_DEMONSPAWN,      SK_STABBING,       -1),
    APT(SP_DEMONSPAWN,      SK_SHIELDS,        -1),
    APT(SP_DEMONSPAWN,      SK_TRAPS_DOORS,    -1),
    APT(SP_DEMONSPAWN,      SK_UNARMED_COMBAT, -1),
    APT(SP_DEMONSPAWN,      SK_SPELLCASTING,    0),
    APT(SP_DEMONSPAWN,      SK_CONJURATIONS,    0),
    APT(SP_DEMONSPAWN,      SK_ENCHANTMENTS,   -1),
    APT(SP_DEMONSPAWN,      SK_SUMMONINGS,      0),
    APT(SP_DEMONSPAWN,      SK_NECROMANCY,      1),
    APT(SP_DEMONSPAWN,      SK_TRANSLOCATIONS, -1),
    APT(SP_DEMONSPAWN,      SK_TRANSMUTATIONS, -1),
    APT(SP_DEMONSPAWN,      SK_FIRE_MAGIC,     -1),
    APT(SP_DEMONSPAWN,      SK_ICE_MAGIC,      -1),
    APT(SP_DEMONSPAWN,      SK_AIR_MAGIC,      -1),
    APT(SP_DEMONSPAWN,      SK_EARTH_MAGIC,    -1),
    APT(SP_DEMONSPAWN,      SK_POISON_MAGIC,    0),
    APT(SP_DEMONSPAWN,      SK_INVOCATIONS,     2),
    APT(SP_DEMONSPAWN,      SK_EVOCATIONS,      0),

    // SP_GHOUL
    APT(SP_GHOUL,           SK_FIGHTING,        1),
    APT(SP_GHOUL,           SK_SHORT_BLADES,   -1),
    APT(SP_GHOUL,           SK_LONG_BLADES,    -1),
    APT(SP_GHOUL,           SK_AXES,           -1),
    APT(SP_GHOUL,           SK_MACES_FLAILS,   -1),
    APT(SP_GHOUL,           SK_POLEARMS,       -1),
    APT(SP_GHOUL,           SK_STAVES,         -1),
    APT(SP_GHOUL,           SK_SLINGS,         -1),
    APT(SP_GHOUL,           SK_BOWS,           -1),
    APT(SP_GHOUL,           SK_CROSSBOWS,      -1),
    APT(SP_GHOUL,           SK_THROWING,       -1),
    APT(SP_GHOUL,           SK_ARMOUR,         -1),
    APT(SP_GHOUL,           SK_DODGING,        -1),
    APT(SP_GHOUL,           SK_STEALTH,         1),
    APT(SP_GHOUL,           SK_STABBING,        0),
    APT(SP_GHOUL,           SK_SHIELDS,        -1),
    APT(SP_GHOUL,           SK_TRAPS_DOORS,    -1),
    APT(SP_GHOUL,           SK_UNARMED_COMBAT,  1),
    APT(SP_GHOUL,           SK_SPELLCASTING,   -1),
    APT(SP_GHOUL,           SK_CONJURATIONS,   -2),
    APT(SP_GHOUL,           SK_ENCHANTMENTS,   -2),
    APT(SP_GHOUL,           SK_SUMMONINGS,     -1),
    APT(SP_GHOUL,           SK_NECROMANCY,      0),
    APT(SP_GHOUL,           SK_TRANSLOCATIONS, -1),
    APT(SP_GHOUL,           SK_TRANSMUTATIONS, -1),
    APT(SP_GHOUL,           SK_FIRE_MAGIC,     -2),
    APT(SP_GHOUL,           SK_ICE_MAGIC,       1),
    APT(SP_GHOUL,           SK_AIR_MAGIC,      -2),
    APT(SP_GHOUL,           SK_EARTH_MAGIC,     1),
    APT(SP_GHOUL,           SK_POISON_MAGIC,    0),
    APT(SP_GHOUL,           SK_INVOCATIONS,     0),
    APT(SP_GHOUL,           SK_EVOCATIONS,     -1),

    // SP_KENKU
    APT(SP_KENKU,           SK_FIGHTING,        0),
    APT(SP_KENKU,           SK_SHORT_BLADES,    1),
    APT(SP_KENKU,           SK_LONG_BLADES,     1),
    APT(SP_KENKU,           SK_AXES,            1),
    APT(SP_KENKU,           SK_MACES_FLAILS,    1),
    APT(SP_KENKU,           SK_POLEARMS,        1),
    APT(SP_KENKU,           SK_STAVES,          1),
    APT(SP_KENKU,           SK_SLINGS,          0),
    APT(SP_KENKU,           SK_BOWS,            1),
    APT(SP_KENKU,           SK_CROSSBOWS,       1),
    APT(SP_KENKU,           SK_THROWING,        1),
    APT(SP_KENKU,           SK_ARMOUR,          1),
    APT(SP_KENKU,           SK_DODGING,         1),
    APT(SP_KENKU,           SK_STEALTH,         0),
    APT(SP_KENKU,           SK_STABBING,        1),
    APT(SP_KENKU,           SK_SHIELDS,         0),
    APT(SP_KENKU,           SK_TRAPS_DOORS,     0),
    APT(SP_KENKU,           SK_UNARMED_COMBAT,  1),
    APT(SP_KENKU,           SK_SPELLCASTING,    0),
    APT(SP_KENKU,           SK_CONJURATIONS,    3),
    APT(SP_KENKU,           SK_ENCHANTMENTS,   -3),
    APT(SP_KENKU,           SK_SUMMONINGS,      2),
    APT(SP_KENKU,           SK_NECROMANCY,      1),
    APT(SP_KENKU,           SK_TRANSLOCATIONS, -2),
    APT(SP_KENKU,           SK_TRANSMUTATIONS, -2),
    APT(SP_KENKU,           SK_FIRE_MAGIC,      1),
    APT(SP_KENKU,           SK_ICE_MAGIC,      -1),
    APT(SP_KENKU,           SK_AIR_MAGIC,       1),
    APT(SP_KENKU,           SK_EARTH_MAGIC,    -1),
    APT(SP_KENKU,           SK_POISON_MAGIC,    0),
    APT(SP_KENKU,           SK_INVOCATIONS,    -2),
    APT(SP_KENKU,           SK_EVOCATIONS,      0),

    // SP_MERFOLK
    APT(SP_MERFOLK,         SK_FIGHTING,        1),
    APT(SP_MERFOLK,         SK_SHORT_BLADES,    2),
    APT(SP_MERFOLK,         SK_LONG_BLADES,     1),
    APT(SP_MERFOLK,         SK_AXES,           -2),
    APT(SP_MERFOLK,         SK_MACES_FLAILS,   -2),
    APT(SP_MERFOLK,         SK_POLEARMS,        4),
    APT(SP_MERFOLK,         SK_STAVES,         -2),
    APT(SP_MERFOLK,         SK_SLINGS,         -2),
    APT(SP_MERFOLK,         SK_BOWS,           -2),
    APT(SP_MERFOLK,         SK_CROSSBOWS,      -2),
    APT(SP_MERFOLK,         SK_THROWING,        0),
    APT(SP_MERFOLK,         SK_ARMOUR,         -3),
    APT(SP_MERFOLK,         SK_DODGING,         3),
    APT(SP_MERFOLK,         SK_STEALTH,         1),
    APT(SP_MERFOLK,         SK_STABBING,        2),
    APT(SP_MERFOLK,         SK_SHIELDS,         0),
    APT(SP_MERFOLK,         SK_TRAPS_DOORS,    -1),
    APT(SP_MERFOLK,         SK_UNARMED_COMBAT,  1),
    APT(SP_MERFOLK,         SK_SPELLCASTING,    0),
    APT(SP_MERFOLK,         SK_CONJURATIONS,   -2),
    APT(SP_MERFOLK,         SK_ENCHANTMENTS,    1),
    APT(SP_MERFOLK,         SK_SUMMONINGS,      0),
    APT(SP_MERFOLK,         SK_NECROMANCY,     -2),
    APT(SP_MERFOLK,         SK_TRANSLOCATIONS, -2),
    APT(SP_MERFOLK,         SK_TRANSMUTATIONS,  3),
    APT(SP_MERFOLK,         SK_FIRE_MAGIC,     -3),
    APT(SP_MERFOLK,         SK_ICE_MAGIC,       1),
    APT(SP_MERFOLK,         SK_AIR_MAGIC,      -2),
    APT(SP_MERFOLK,         SK_EARTH_MAGIC,    -2),
    APT(SP_MERFOLK,         SK_POISON_MAGIC,    1),
    APT(SP_MERFOLK,         SK_INVOCATIONS,     0),
    APT(SP_MERFOLK,         SK_EVOCATIONS,      0),

    // SP_VAMPIRE
    APT(SP_VAMPIRE,         SK_FIGHTING,       -1),
    APT(SP_VAMPIRE,         SK_SHORT_BLADES,    1),
    APT(SP_VAMPIRE,         SK_LONG_BLADES,     0),
    APT(SP_VAMPIRE,         SK_AXES,           -1),
    APT(SP_VAMPIRE,         SK_MACES_FLAILS,   -2),
    APT(SP_VAMPIRE,         SK_POLEARMS,       -1),
    APT(SP_VAMPIRE,         SK_STAVES,         -2),
    APT(SP_VAMPIRE,         SK_SLINGS,         -2),
    APT(SP_VAMPIRE,         SK_BOWS,           -2),
    APT(SP_VAMPIRE,         SK_CROSSBOWS,      -2),
    APT(SP_VAMPIRE,         SK_THROWING,       -2),
    APT(SP_VAMPIRE,         SK_ARMOUR,         -2),
    APT(SP_VAMPIRE,         SK_DODGING,         1),
    APT(SP_VAMPIRE,         SK_STEALTH,         4),
    APT(SP_VAMPIRE,         SK_STABBING,        1),
    APT(SP_VAMPIRE,         SK_SHIELDS,        -1),
    APT(SP_VAMPIRE,         SK_TRAPS_DOORS,     0),
    APT(SP_VAMPIRE,         SK_UNARMED_COMBAT,  1),
    APT(SP_VAMPIRE,         SK_SPELLCASTING,    0),
    APT(SP_VAMPIRE,         SK_CONJURATIONS,   -3),
    APT(SP_VAMPIRE,         SK_ENCHANTMENTS,    1),
    APT(SP_VAMPIRE,         SK_SUMMONINGS,      0),
    APT(SP_VAMPIRE,         SK_NECROMANCY,      1),
    APT(SP_VAMPIRE,         SK_TRANSLOCATIONS, -2),
    APT(SP_VAMPIRE,         SK_TRANSMUTATIONS,  1),
    APT(SP_VAMPIRE,         SK_FIRE_MAGIC,     -2),
    APT(SP_VAMPIRE,         SK_ICE_MAGIC,       0),
    APT(SP_VAMPIRE,         SK_AIR_MAGIC,       0),
    APT(SP_VAMPIRE,         SK_EARTH_MAGIC,    -1),
    APT(SP_VAMPIRE,         SK_POISON_MAGIC,   -1),
    APT(SP_VAMPIRE,         SK_INVOCATIONS,    -2),
    APT(SP_VAMPIRE,         SK_EVOCATIONS,     -1),

    // SP_DEEP_DWARF
    APT(SP_DEEP_DWARF,      SK_FIGHTING,       -1),
    APT(SP_DEEP_DWARF,      SK_SHORT_BLADES,   -1),
    APT(SP_DEEP_DWARF,      SK_LONG_BLADES,     0),
    APT(SP_DEEP_DWARF,      SK_AXES,            1),
    APT(SP_DEEP_DWARF,      SK_MACES_FLAILS,    0),
    APT(SP_DEEP_DWARF,      SK_POLEARMS,       -1),
    APT(SP_DEEP_DWARF,      SK_STAVES,         -1),
    APT(SP_DEEP_DWARF,      SK_SLINGS,          1),
    APT(SP_DEEP_DWARF,      SK_BOWS,           -3),
    APT(SP_DEEP_DWARF,      SK_CROSSBOWS,       1),
    APT(SP_DEEP_DWARF,      SK_THROWING,       -1),
    APT(SP_DEEP_DWARF,      SK_ARMOUR,          1),
    APT(SP_DEEP_DWARF,      SK_DODGING,         1),
    APT(SP_DEEP_DWARF,      SK_STEALTH,         2),
    APT(SP_DEEP_DWARF,      SK_STABBING,       -1),
    APT(SP_DEEP_DWARF,      SK_SHIELDS,         1),
    APT(SP_DEEP_DWARF,      SK_TRAPS_DOORS,     1),
    APT(SP_DEEP_DWARF,      SK_UNARMED_COMBAT, -1),
    APT(SP_DEEP_DWARF,      SK_SPELLCASTING,   -1),
    APT(SP_DEEP_DWARF,      SK_CONJURATIONS,   -1),
    APT(SP_DEEP_DWARF,      SK_ENCHANTMENTS,   -1),
    APT(SP_DEEP_DWARF,      SK_SUMMONINGS,     -1),
    APT(SP_DEEP_DWARF,      SK_NECROMANCY,      1),
    APT(SP_DEEP_DWARF,      SK_TRANSLOCATIONS,  1),
    APT(SP_DEEP_DWARF,      SK_TRANSMUTATIONS, -1),
    APT(SP_DEEP_DWARF,      SK_FIRE_MAGIC,     -1),
    APT(SP_DEEP_DWARF,      SK_ICE_MAGIC,      -1),
    APT(SP_DEEP_DWARF,      SK_AIR_MAGIC,      -3),
    APT(SP_DEEP_DWARF,      SK_EARTH_MAGIC,     3),
    APT(SP_DEEP_DWARF,      SK_POISON_MAGIC,   -2),
    APT(SP_DEEP_DWARF,      SK_INVOCATIONS,     2),
    APT(SP_DEEP_DWARF,      SK_EVOCATIONS,      3),

    // SP_CAT
    APT(SP_CAT,             SK_FIGHTING,        0),
    APT(SP_CAT,             SK_SHORT_BLADES,    0),
    APT(SP_CAT,             SK_LONG_BLADES,     0),
    APT(SP_CAT,             SK_AXES,            0),
    APT(SP_CAT,             SK_MACES_FLAILS,    0),
    APT(SP_CAT,             SK_POLEARMS,        0),
    APT(SP_CAT,             SK_STAVES,          0),
    APT(SP_CAT,             SK_SLINGS,          0),
    APT(SP_CAT,             SK_BOWS,            0),
    APT(SP_CAT,             SK_CROSSBOWS,       0),
    APT(SP_CAT,             SK_THROWING,        0),
    APT(SP_CAT,             SK_ARMOUR,          0),
    APT(SP_CAT,             SK_DODGING,         2),
    APT(SP_CAT,             SK_STEALTH,         2),
    APT(SP_CAT,             SK_STABBING,        3),
    APT(SP_CAT,             SK_SHIELDS,         0),
    APT(SP_CAT,             SK_TRAPS_DOORS,     2),
    APT(SP_CAT,             SK_UNARMED_COMBAT,  0),
    APT(SP_CAT,             SK_SPELLCASTING,    0),
    APT(SP_CAT,             SK_CONJURATIONS,   -1),
    APT(SP_CAT,             SK_ENCHANTMENTS,    2),
    APT(SP_CAT,             SK_SUMMONINGS,      1),
    APT(SP_CAT,             SK_NECROMANCY,      0),
    APT(SP_CAT,             SK_TRANSLOCATIONS,  2),
    APT(SP_CAT,             SK_TRANSMUTATIONS,  1),
    APT(SP_CAT,             SK_FIRE_MAGIC,     -1),
    APT(SP_CAT,             SK_ICE_MAGIC,      -1),
    APT(SP_CAT,             SK_AIR_MAGIC,      -1),
    APT(SP_CAT,             SK_EARTH_MAGIC,    -1),
    APT(SP_CAT,             SK_POISON_MAGIC,   -1),
    APT(SP_CAT,             SK_INVOCATIONS,    -1),
    APT(SP_CAT,             SK_EVOCATIONS,     -2),
};

// Traditionally, Spellcasting and In/Evocations formed the exceptions here:
// Spellcasting skill was more expensive with about 130%, the other two got
// a discount with about 75%.
static int _spec_skills[NUM_SPECIES][NUM_SKILLS];

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

    SK_BOWS, SK_CROSSBOWS, SK_THROWING, SK_SLINGS,

    SK_BLANK_LINE,

    SK_ARMOUR, SK_DODGING, SK_STEALTH, SK_SHIELDS,

    SK_COLUMN_BREAK,

    SK_STABBING, SK_TRAPS_DOORS,

    SK_BLANK_LINE,

    SK_SPELLCASTING, SK_CONJURATIONS, SK_ENCHANTMENTS, SK_SUMMONINGS,
    SK_NECROMANCY, SK_TRANSLOCATIONS, SK_TRANSMUTATIONS,
    SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC, SK_POISON_MAGIC,

    SK_BLANK_LINE,

    SK_INVOCATIONS, SK_EVOCATIONS,
};

static const int ndisplayed_skills =
            sizeof(skill_display_order) / sizeof(*skill_display_order);

static bool _skill_is_selectable(skill_type sk, int flags)
{
    if (is_invalid_skill(sk))
        return false;

    if (you.skills[sk] == 0 && !(flags & SK_MENU_SHOW_ALL))
        return false;

    if (flags & SK_MENU_SHOW_DESC)
        return true;

    if (flags & SK_MENU_RESKILL && you.transfer_from_skill == sk)
        return false;

    if (you.skills[sk] == 0
        && !(flags & SK_MENU_RESKILL && you.transfer_from_skill != SK_NONE))
    {
        return false;
    }

    if (you.skills[sk] == 27
        && !(flags & SK_MENU_RESKILL && you.transfer_from_skill == SK_NONE))
    {
        return false;
    }

    return true;
}

static void _display_skill_table(int flags)
{
    menu_letter lcount = 'a';

    if (!(flags & SK_MENU_RESKILL))
    {
        cgotoxy(1, 1);
        textcolor(LIGHTGREY);

#ifdef DEBUG_DIAGNOSTICS
        cprintf("You have %d points of unallocated experience "
                " (cost lvl %d; total %d).\n\n",
                you.exp_available, you.skill_cost_level,
                you.total_skill_points);
#else
        cprintf(" You have %s unallocated experience.\n\n",
                you.exp_available == 0? "no" :
                make_stringf("%d point%s of",
                             you.exp_available,
                             you.exp_available == 1? "" : "s").c_str());
#endif
    }

    int scrln = 3, scrcol = 1;
    skill_type x;
    int maxln = scrln;

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
        if (you.skills[x] > 0 || flags & SK_MENU_SHOW_ALL)
#endif
        {
            maxln = std::max(maxln, scrln);
            skill_type sx = static_cast<skill_type>(x);
            int ct_bonus = crosstrain_bonus(sx);

            if (flags & SK_MENU_SHOW_RESKILL && (x == you.transfer_from_skill
                                                || x == you.transfer_to_skill))
            {
                textcolor(GREEN);
            }
            else if (flags & SK_MENU_RESKILL && x == you.transfer_from_skill)
                textcolor(WHITE);
            else if (you.practise_skill[x] == 0 || you.skills[x] == 0)
                textcolor(DARKGREY);
            else if (ct_bonus > 1 && flags & SK_MENU_SHOW_APT)
                textcolor(LIGHTBLUE);
            else if (is_antitrained(sx) && flags & SK_MENU_SHOW_APT)
                textcolor(MAGENTA);
            else
                textcolor(LIGHTGREY);

            if (you.skills[x] == 27)
                textcolor(YELLOW);


            if (_skill_is_selectable(x, flags))
                putch(lcount++);
            else
                putch(' ');

            cprintf(" %c %-14s Skill %2d",
                     (you.skills[x] == 0 || you.skills[x] == 27) ? ' ' :
                     (you.practise_skill[x]) ? '+' : '-',
                     skill_name(x), you.skills[x]);

#ifdef DEBUG_DIAGNOSTICS
            cprintf(" %5d", you.skill_points[x]);
#endif

            if (you.skills[x] < 27)
            {
                if (flags & SK_MENU_RESKILL
                    && you.transfer_from_skill != SK_NONE)
                {
                    textcolor(CYAN);
                    cprintf (" ->%2d",
                             transfer_skill_points(you.transfer_from_skill, sx,
                                             you.transfer_skill_points, true));
                }
                if (flags & SK_MENU_SHOW_APT)
                {
                    int apt = species_apt(x, you.species);
                    std::string apt_str(" <red>");
                    if (apt != 0)
                        apt_str += make_stringf("%+d", apt);
                    else
                        apt_str += make_stringf(" %d", apt);

                    if (crosstrain_other(sx))
                        apt_str += "<lightblue>*</lightblue>";
                    else if (antitrain_other(sx))
                        apt_str += "<magenta>*</magenta>";
                    else
                        apt_str += " ";

                    if ( ct_bonus > 1)
                    {
                        apt_str += make_stringf("<lightblue>%+d </lightblue>",
                                                ct_bonus * 2);
                    }
                    else if (is_antitrained(sx))
                        apt_str += "<magenta>-4 </magenta>";
                    else
                        apt_str += "   ";

                    formatted_string::parse_string(apt_str).display();
                }
                else if (flags & SK_MENU_SHOW_RESKILL)
                {
                    textcolor(GREEN);
                    if (sx == you.transfer_from_skill)
                        cprintf("  *  ");
                    else if (sx == you.transfer_to_skill)
                    {
                        cprintf(" (%2d%%)", (you.transfer_total_skill_points
                                             - you.transfer_skill_points) * 100
                                            / you.transfer_total_skill_points);
                    }
                    else
                        cprintf("      ");

                }
                else
                {
                    const int needed = skill_exp_needed(you.skills[x] + 1, x);
                    const int prev_needed = skill_exp_needed(you.skills[x], x);

                    const int amt_done = you.skill_points[x] - prev_needed;
                    int percent_done = (amt_done*100) / (needed - prev_needed);

                    if (percent_done >= 100) // paranoia (1)
                        percent_done = 99;

                    if (percent_done < 0)    // paranoia (2)
                        percent_done = 0;

                    textcolor(CYAN);
                    // Round down to multiple of 5.
                    cprintf(" (%2d%%)", (percent_done / 5) * 5);
                }
            }

            scrln++;
        }
    }

    if (flags & SK_MENU_RESKILL && you.transfer_from_skill != SK_NONE
        && !(flags & SK_MENU_SHOW_ALL))
    {
        textcolor(WHITE);
        cgotoxy(1, bottom_line);
        cprintf("Press * to show all.");
    }

    if (flags & SK_MENU_RESKILL)
        return;

    if (Hints.hints_left)
    {
        if (flags & SK_MENU_SHOW_DESC || maxln >= bottom_line - 5)
        {
            cgotoxy(1, bottom_line-2);
            // Doesn't mention the toggle between progress/aptitudes.
            print_hints_skills_description_info();
        }
        else
        {
            cgotoxy(1, bottom_line-5);
            // Doesn't mention the toggle between progress/aptitudes.
            print_hints_skills_info();
        }
    }
    else
    {
        // NOTE: If any more skills added, must adapt letters to go into caps.
        cgotoxy(1, bottom_line-3);
        textcolor(LIGHTGREY);

        if (flags & SK_MENU_SHOW_DESC)
        {
            // We need the extra spaces to override the alternative sentence.
            cprintf("Press the letter of a skill to read its description.      "
                    "            ");
        }
        else
        {
            cprintf("Press the letter of a skill to choose whether you want to "
                    "practise it.\n" "Skills marked with '+' will train more "
                    "quickly than those with '-'.");
        }

        cgotoxy(1, bottom_line-1);
        if (flags & SK_MENU_SHOW_DESC)
        {
            formatted_string::parse_string("Press '<w>?</w>' to choose which "
                                           "skills to train.  ").display();
        }
        else
        {
            formatted_string::parse_string("Press '<w>?</w>' to read the "
                                           "skills' descriptions.").display();
        }

        cgotoxy(1, bottom_line);
        formatted_string::parse_string(
#ifndef USE_TILE
            "Press '<w>!</w>'"
#else
            "<w>Right-click</w>"
#endif
            " to toggle between <cyan>progress</cyan>").display();
        if (you.transfer_skill_points > 0)
        {
            formatted_string::parse_string(", <red>aptitude</red> and "
                    "<green>transfer knowledge</green> display.").display();
        }
        else
        {
            formatted_string::parse_string(" and <red>aptitude</red> "
                                           "display.").display();
        }
    }
}

void show_skills()
{
    int flags = SK_MENU_NONE;
    clrscr();
    while (true)
    {
        _display_skill_table(flags);

        mouse_control mc(MOUSE_MODE_MORE);
        const int keyin = getch();
        if ((keyin == '!' || keyin == CK_MOUSE_CMD))
        {
            if (you.transfer_skill_points == 0)
                flags ^= SK_MENU_SHOW_APT;
            else if (!(flags & (SK_MENU_SHOW_APT | SK_MENU_SHOW_RESKILL)))
                flags |= SK_MENU_SHOW_APT;
            else if (flags & SK_MENU_SHOW_APT)
            {
                flags ^= SK_MENU_SHOW_APT;
                flags |= SK_MENU_SHOW_RESKILL;
            }
            else if (flags & SK_MENU_SHOW_RESKILL)
                flags ^= SK_MENU_SHOW_RESKILL;

            continue;
        }

        if (keyin == '?')
        {
            // Show skill description.
            flags ^= SK_MENU_SHOW_DESC;
            if (Hints.hints_left)
                clrscr();
            continue;
        }

        if (!isaalpha(keyin))
            break;

        menu_letter lcount = 'a';       // toggle skill practise

        for (int i = 0; i < ndisplayed_skills; i++)
        {
            const skill_type x = skill_display_order[i];
            if (!_skill_is_selectable(x, flags))
                continue;

            if (keyin == lcount)
            {
                if (!(flags & SK_MENU_SHOW_DESC))
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

const char *skill_name(skill_type which_skill)
{
    return (skills[which_skill][0]);
}

skill_type str_to_skill(const std::string &skill)
{
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
        if (skills[i][0] && skill == skills[i][0])
            return (static_cast<skill_type>(i));

    return (SK_FIGHTING);
}

static std::string _stk_adj_cap()
{
    return species_name(Skill_Species, false, true);
}

static std::string _stk_genus_cap()
{
    if (Skill_Species == SP_CAT)
        return "Cat";
    return species_name(Skill_Species, true, false);
}

static std::string _stk_genus_nocap()
{
    std::string s = _stk_genus_cap();
    return (lowercase(s));
}

static std::string _stk_genus_short_cap()
{
    return (Skill_Species == SP_DEMIGOD ? "God" :
            Skill_Species == SP_CAT     ? "Cat" :
            _stk_genus_cap());
}

static std::string _stk_walker()
{
    return (Skill_Species == SP_NAGA  ? "Slider" :
            Skill_Species == SP_KENKU ? "Glider"
                                      : "Walker");
}

static std::string _stk_weight()
{
    switch (Skill_Species)
    {
    case SP_OGRE:
    case SP_TROLL:
        return "Heavy";

    case SP_NAGA:
    case SP_CENTAUR:
        return "Cruiser";

    default:
        return "Middle";

    case SP_HIGH_ELF:
    case SP_DEEP_ELF:
    case SP_SLUDGE_ELF:
    case SP_KENKU:
        return "Light";

    case SP_HALFLING:
    case SP_KOBOLD:
        return "Feather";

    case SP_SPRIGGAN:
        return "Fly";

    case SP_CAT:
        return "Bacteria"; // not used
    }
}

static skill_title_key_t _skill_title_keys[] = {
    stk("Adj", _stk_adj_cap),
    stk("Genus", _stk_genus_cap),
    stk("genus", _stk_genus_nocap),
    stk("Genus_Short", _stk_genus_short_cap),
    stk("Walker", _stk_walker),
    stk("Weight", _stk_weight),
};

static std::string _replace_skill_keys(const std::string &text)
{
    std::string::size_type at = 0, last = 0;
    std::ostringstream res;
    while ((at = text.find('@', last)) != std::string::npos)
    {
        res << text.substr(last, at - last);
        const std::string::size_type end = text.find('@', at + 1);
        if (end == std::string::npos)
            break;

        const std::string key = text.substr(at + 1, end - at - 1);
        const std::string value = stk::get(key);

        ASSERT(!value.empty());

        res << value;

        last = end + 1;
    }
    if (!last)
        return text;

    res << text.substr(last);
    return res.str();
}

unsigned get_skill_rank(unsigned skill_lev)
{
    // Translate skill level into skill ranking {dlb}:
    return ((skill_lev <= 7)  ? 0 :
                            (skill_lev <= 14) ? 1 :
                            (skill_lev <= 20) ? 2 :
                            (skill_lev <= 26) ? 3
                            /* level 27 */    : 4);
}

std::string skill_title_by_rank(skill_type best_skill, uint8_t skill_rank,
                                int species, int str, int dex, int god)
{
    // paranoia
    if (is_invalid_skill(best_skill))
        return ("Adventurer");

    if (species == -1)
        species = you.species;

    if (str == -1)
        str = you.base_stats[STAT_STR];

    if (dex == -1)
        dex = you.base_stats[STAT_DEX];

    if (god == -1)
        god = you.religion;

    // Increment rank by one to "skip" skill name in array {dlb}:
    ++skill_rank;

    std::string result;

    if (best_skill < NUM_SKILLS)
    {
        // Note that ghosts default to (dex == str) and god == no_god, due
        // to a current lack of that information... the god case is probably
        // suitable for most cases (TSO/Zin/Ely at the very least). -- bwr
        switch (best_skill)
        {
        case SK_UNARMED_COMBAT:
            if (species == SP_CAT)
            {
                result = claw_and_tooth_titles[skill_rank];
                break;
            }
            result = (dex >= str) ? martial_arts_titles[skill_rank]
                                  : skills[best_skill][skill_rank];

            break;

        case SK_INVOCATIONS:
            if (god == GOD_NO_GOD)
                result = skills[best_skill][skill_rank];
            else
                result = god_title((god_type)god, (species_type)species);
            break;

        case SK_BOWS:
            if (player_genus(GENPC_ELVEN, static_cast<species_type>(species))
                && skill_rank == 5)
            {
                result = "Master Archer";
                break;
            }
            else
                result = skills[best_skill][skill_rank];
            break;

        case SK_SPELLCASTING:
            if (player_genus(GENPC_OGREISH, static_cast<species_type>(species)))
            {
                result = "Ogre Mage";
                break;
            }
            // else fall-through
        default:
            result = skills[best_skill][skill_rank];
            break;
        }
    }

    {
        unwind_var<species_type> sp(Skill_Species,
                                    static_cast<species_type>(species));
        result = _replace_skill_keys(result);
    }

    return (result.empty() ? std::string("Invalid Title")
                           : result);
}

std::string skill_title(skill_type best_skill, uint8_t skill_lev,
                        int species, int str, int dex, int god)
{
    return skill_title_by_rank(best_skill, get_skill_rank(skill_lev), species, str, dex, god);
}

std::string player_title()
{
    const skill_type best = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    return (skill_title(best, you.skills[ best ]));
}

skill_type best_skill(skill_type min_skill, skill_type max_skill,
                      skill_type excl_skill)
{
    skill_type ret = SK_FIGHTING;
    unsigned int best_skill_level = 0;
    unsigned int best_position = 1000;

    for (int i = min_skill; i <= max_skill; i++)    // careful!!!
    {
        skill_type sk = static_cast<skill_type>(i);
        if (sk == excl_skill || is_invalid_skill(sk))
            continue;

        if (you.skills[sk] > best_skill_level)
        {
            ret = sk;
            best_skill_level = you.skills[sk];
            best_position = you.skill_order[sk];

        }
        else if (you.skills[sk] == best_skill_level
                && you.skill_order[sk] < best_position)
        {
            ret = sk;
            best_position = you.skill_order[sk];
        }
    }

    return static_cast<skill_type>(ret);
}

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
void init_skill_order(void)
{
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; i++)
    {
        skill_type si = static_cast<skill_type>(i);
        if (is_invalid_skill(si))
        {
            you.skill_order[si] = MAX_SKILL_ORDER;
            continue;
        }

        const unsigned int i_points = you.skill_points[si]
                                      / species_apt_factor(si);

        you.skill_order[si] = 0;

        for (int j = SK_FIRST_SKILL; j < NUM_SKILLS; j++)
        {
            skill_type sj = static_cast<skill_type>(j);
            if (si == sj || is_invalid_skill(sj))
                continue;

            const unsigned int j_points = you.skill_points[sj]
                                          / species_apt_factor(sj);

            if (you.skills[sj] == you.skills[si]
                && (j_points > i_points
                    || (j_points == i_points && sj > si)))
            {
                you.skill_order[si]++;
            }
        }
    }
}

void calc_hp()
{
    you.hp_max = get_real_hp(true, false);
    deflate_hp(you.hp_max, false);
}

void calc_mp()
{
    you.max_magic_points = get_real_mp(true);
    you.magic_points = std::min(you.magic_points, you.max_magic_points);
    you.redraw_magic_points = true;
}

// What aptitude value corresponds to doubled skill learning
// (i.e., old-style aptitude 50).
#define APT_DOUBLE 4

static float _apt_to_factor(int apt)
{
    return (1 / exp(log(2) * apt / APT_DOUBLE));
}

// Base skill cost, i.e. old-style human aptitudes.
static int _base_cost(skill_type sk)
{
    switch (sk)
    {
    case SK_SPELLCASTING:
        return 130;
    case SK_INVOCATIONS:
    case SK_EVOCATIONS:
        return 80;
    // Quick fix for the fact that stealth can't be gained fast enough
    // to keep up with the monster levels. This was a skill points bonus
    // in _exercise2 and was changed to a reduced base_cost to keep
    // total_skill_points progression the same for all skills.
    case SK_STEALTH:
        return 50;
    default:
        return 100;
    }
}

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

unsigned int skill_exp_needed(int lev, skill_type sk, species_type sp)
{
    return skill_exp_needed(lev) * species_apt_factor(sk, sp)
           * _base_cost(sk) / 100;
}

int species_apt(skill_type skill, species_type species)
{
    static bool spec_skills_initialised = false;
    if (!spec_skills_initialised)
    {
        // Setup sentinel values to find errors more easily.
        const int sentinel = -20; // this gives cost 3200
        for (int sp = 0; sp < NUM_SPECIES; ++sp)
            for (int sk = 0; sk < NUM_SKILLS; ++sk)
                _spec_skills[sp][sk] = sentinel;
        for (unsigned i = 0; i < ARRAYSZ(species_skill_aptitudes); ++i)
        {
            const species_skill_aptitude &ssa(species_skill_aptitudes[i]);
            ASSERT(_spec_skills[ssa.species][ssa.skill] == sentinel);
            _spec_skills[ssa.species][ssa.skill] = ssa.aptitude;
        }
        spec_skills_initialised = true;
    }

    return _spec_skills[species][skill];
}

float species_apt_factor(skill_type sk, species_type sp)
{
    return _apt_to_factor(species_apt(sk, sp));
}

static std::vector<skill_type> _get_crosstrain_skills(skill_type sk)
{
    std::vector<skill_type> ret;

    switch (sk)
    {
    case SK_SHORT_BLADES:
        ret.push_back(SK_LONG_BLADES);
        return ret;
    case SK_LONG_BLADES:
        ret.push_back(SK_SHORT_BLADES);
        return ret;
    case SK_AXES:
    case SK_STAVES:
        ret.push_back(SK_POLEARMS);
        ret.push_back(SK_MACES_FLAILS);
        return ret;
    case SK_MACES_FLAILS:
    case SK_POLEARMS:
        ret.push_back(SK_AXES);
        ret.push_back(SK_STAVES);
        return ret;
    case SK_SLINGS:
        ret.push_back(SK_THROWING);
        return ret;
    case SK_THROWING:
        ret.push_back(SK_SLINGS);
        return ret;
    default:
        return ret;
    }
}


float crosstrain_bonus(skill_type sk)
{
    int bonus = 1;

    std::vector<skill_type> crosstrain_skills = _get_crosstrain_skills(sk);

    for (unsigned int i = 0; i < crosstrain_skills.size(); ++i)
        if (you.skills[crosstrain_skills[i]] > you.skills[sk])
            bonus *= 2;

    return bonus;
}

bool crosstrain_other(skill_type sk)
{
    std::vector<skill_type> crosstrain_skills = _get_crosstrain_skills(sk);

    for (unsigned int i = 0; i < crosstrain_skills.size(); ++i)
        if (you.skills[crosstrain_skills[i]] < you.skills[sk]
            && you.skills[crosstrain_skills[i]] != 0)
        {
            return true;
        }

    return false;
}

static skill_type _get_opposite(skill_type sk)
{
    switch (sk)
    {
    case SK_FIRE_MAGIC  : return SK_ICE_MAGIC;   break;
    case SK_ICE_MAGIC   : return SK_FIRE_MAGIC;  break;
    case SK_AIR_MAGIC   : return SK_EARTH_MAGIC; break;
    case SK_EARTH_MAGIC : return SK_AIR_MAGIC;   break;
    default: return SK_NONE;
    }
}

bool is_antitrained(skill_type sk)
{
    skill_type opposite = _get_opposite(sk);
    if (opposite == SK_NONE)
        return false;

    return (you.skills[sk] < you.skills[opposite]
            || you.skills[sk] == you.skills[opposite]
               && you.skill_order[sk] > you.skill_order[opposite]
               && you.skills[sk] != 0);
}

bool antitrain_other(skill_type sk)
{
    skill_type opposite = _get_opposite(sk);
    if (opposite == SK_NONE)
        return false;

    return (you.skills[opposite] != 0
            && (you.skills[sk] > you.skills[opposite]
                || you.skills[sk] == you.skills[opposite]
                   && you.skill_order[sk] < you.skill_order[opposite]));
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

    std::string msg;

    // We know if it's an artefact because we just wielded
    // it, so no information leak.
    if (is_artefact(wep))
        msg = "the";
    else if (newWeapon)
        msg = "this";
    else
        msg = "your";
    msg += " " + wep.name(DESC_BASENAME);
    const char* mstr = msg.c_str();

    if (you.strength() < you.dex())
    {
        if (you.strength() < 11)
        {
            mprf(MSGCH_WARN, "You have %strouble swinging %s.",
                 (you.strength() < 7) ? "" : "a little ", mstr);
        }
        else
        {
            mprf(MSGCH_WARN, "You'd be more effective with "
                 "%s if you were stronger.", mstr);
        }
    }
    else
    {
        if (you.dex() < 11)
        {
            mprf(MSGCH_WARN, "Wielding %s is %s awkward.",
                 mstr, (you.dex() < 7) ? "fairly" : "a little");
        }
        else
        {
            mprf(MSGCH_WARN, "You'd be more effective with "
                 "%s if you were nimbler.", mstr);
        }
    }
}

bool is_invalid_skill(skill_type skill)
{
    if (skill < 0 || skill >= NUM_SKILLS)
        return (true);

    if (skill > SK_UNARMED_COMBAT && skill < SK_SPELLCASTING)
        return (true);

    return (false);
}

void dump_skills(std::string &text)
{
    char tmp_quant[20];
    for (uint8_t i = 0; i < NUM_SKILLS; i++)
    {
        if (you.skills[i] > 0)
        {
            text += ((you.skills[i] == 27)   ? " * " :
                     (you.practise_skill[i]) ? " + "
                                             : " - ");

            text += "Level ";
            itoa(you.skills[i], tmp_quant, 10);
            text += tmp_quant;
            text += " ";
            text += skill_name(static_cast<skill_type>(i));
            text += "\n";
        }
    }
}

skill_type select_skill(bool show_all)
{
    clrscr();
    cgotoxy(1, 1);
    textcolor(WHITE);
    if (you.transfer_from_skill == SK_NONE)
        cprintf("Select the source skill.");
    else
        cprintf("Select the destination skill.");

    int flags = SK_MENU_SHOW_APT | SK_MENU_RESKILL;
    if (show_all)
        flags |= SK_MENU_SHOW_ALL;

    _display_skill_table(flags);

    mouse_control mc(MOUSE_MODE_MORE);
    const int keyin = getch();

    if (keyin == '*' && you.transfer_from_skill != SK_NONE)
        return select_skill(true);

    if (!isaalpha(keyin))
        return SK_NONE;

    menu_letter lcount = 'a';

    for (int i = 0; i < ndisplayed_skills; i++)
    {
        const skill_type x = skill_display_order[i];
        if (!_skill_is_selectable(x, flags))
            continue;

        if (keyin == lcount)
            return x;

        ++lcount;
    }
    return SK_NONE;
}

// Transfer skill points from one skill to another (Ashenzari transfer
// knowledge ability). If simu, it just simulates the transfer and don't
// change anything. It returns the new level of tsk.
int transfer_skill_points(skill_type fsk, skill_type tsk, int skp_max,
                          bool simu)
{
    const int penalty = 90; // 10% XP penalty
    int total_skp_lost   = 0; // skill points lost in fsk.
    int total_skp_gained = 0; // skill points gained in tsk.
    int fsk_level = you.skills[fsk];
    int tsk_level = you.skills[tsk];
    int fsk_points = you.skill_points[fsk];
    int tsk_points = you.skill_points[tsk];
    int fsk_ct_points = you.ct_skill_points[fsk];
    int tsk_ct_points = you.ct_skill_points[tsk];

#ifdef DEBUG_DIAGNOSTICS
    if (!simu && you.ct_skill_points[fsk] > 0)
        dprf("ct_skill_points[%s]: %d", skill_name(fsk), you.ct_skill_points[fsk]);
#endif

    // We need to transfer by small steps and updating skill levels each time
    // so that cross/anti-training are handled properly.
    while (total_skp_lost < skp_max && you.skills[tsk] < 27
           && (simu || total_skp_lost < (int)you.transfer_skill_points))
    {
        int skp_lost = std::min(20, skp_max - total_skp_lost);
        int skp_gained = skp_lost * penalty / 100;

        float ct_bonus = crosstrain_bonus(tsk);
        if (ct_bonus > 1)
        {
            skp_gained *= ct_bonus;
            you.ct_skill_points[tsk] += (1 - 1 / ct_bonus) * skp_gained;
        }
        else if (is_antitrained(tsk))
            skp_gained /= ANTITRAIN_PENALTY;

        int ct_penalty = skp_lost * you.ct_skill_points[fsk]
                          / (you.skill_points[fsk] - you.ct_skill_points[fsk]);
        ct_penalty = std::min<int>(ct_penalty, you.ct_skill_points[fsk]);
        you.ct_skill_points[fsk] -= ct_penalty;
        skp_lost += ct_penalty;

        if (!simu)
        {
            skp_lost = std::min<int>(skp_lost, you.transfer_skill_points
                                               - total_skp_lost);
        }

        change_skill_points(fsk, -skp_lost, false);
        if (fsk != tsk)
            change_skill_points(tsk, skp_gained, false);
        total_skp_lost += skp_lost;
        total_skp_gained += skp_gained;
    }

    int new_level = you.skills[tsk];
    // Restore the level
    you.skills[fsk] = fsk_level;
    you.skills[tsk] = tsk_level;

    if (simu)
    {
        you.skill_points[fsk] = fsk_points;
        you.skill_points[tsk] = tsk_points;
        you.ct_skill_points[fsk] = fsk_ct_points;
        you.ct_skill_points[tsk] = tsk_ct_points;
    }
    else
    {
        // Perform the real level up
        check_skill_level_change(fsk);
        check_skill_level_change(tsk);
        you.transfer_skill_points -= total_skp_lost;

        dprf("skill %s lost %d points", skill_name(fsk), total_skp_lost);
        dprf("skill %s gained %d points", skill_name(tsk), total_skp_gained);
#ifdef DEBUG_DIAGNOSTICS
        if (you.ct_skill_points[fsk] > 0)
            dprf("ct_skill_points[%s]: %d", skill_name(fsk), you.ct_skill_points[fsk]);
#endif

        if (you.transfer_skill_points <= 0 || you.skills[tsk] == 27)
            ashenzari_end_transfer(true);
        else if (you.transfer_skill_points > 0)
            dprf("%d skill points left to transfer", you.transfer_skill_points);
    }
    return new_level;
}
