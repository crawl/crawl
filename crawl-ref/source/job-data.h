#pragma once

#include <vector>

#include "tag-version.h"

using std::vector;

enum weapon_choice
{
    WCHOICE_NONE,   ///< No weapon choice
    WCHOICE_PLAIN,  ///< Normal weapon choice
    WCHOICE_GOOD,   ///< Chooses from "good" weapons
};

struct job_def
{
    const char* abbrev; ///< Two-letter abbreviation
    const char* name; ///< Long name
    int s, i, d; ///< Starting Str, Int, and Dex
    /// Which species are good at it
    /// No recommended species = job is disabled
    vector<species_type> recommended_species;
    /// What spells start out in their library?
    /// The first spell in the list will be memorised at the start of the game,
    /// if it's level 1 and not useless.
    vector<spell_type> library;
    /// Guaranteed starting equipment. Uses vault spec syntax, with the plus:,
    /// charges:, q:, and ego: tags supported.
    vector<string> equipment;
    weapon_choice wchoice; ///< how the weapon is chosen, if any
    vector<pair<skill_type, int>> skills; ///< starting skills
};

static const map<job_type, job_def> job_data =
{

{ JOB_AIR_ELEMENTALIST, {
    "AE", "Air Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_TENGU, SP_BASE_DRACONIAN, SP_NAGA, SP_VINE_STALKER,
      SP_DJINNI, },
    {
        SPELL_SHOCK,
        SPELL_DISCHARGE,
        SPELL_SWIFTNESS,
        SPELL_AIRSTRIKE,
    },
    { "robe", "potion of magic" },
    WCHOICE_NONE,
    { { SK_CONJURATIONS, 1 }, { SK_AIR_MAGIC, 3 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_ARTIFICER, {
    "Ar", "Artificer",
    4, 3, 5,
    { SP_KOBOLD, SP_SPRIGGAN, SP_BASE_DRACONIAN, SP_DEMONSPAWN, SP_COGLIN },
    { },
    { "club", "leather armour", "wand of flame charges:15",
      "wand of charming charges:15 no_exclude",
      "wand of iceblast charges:5 no_exclude" },
    WCHOICE_NONE,
    { { SK_EVOCATIONS, 3 }, { SK_DODGING, 1 }, { SK_FIGHTING, 1 },
      { SK_ARMOUR, 1 }, { SK_STEALTH, 1 }, },
} },

{ JOB_BERSERKER, {
    "Be", "Berserker",
    9, -1, 4,
    { SP_MOUNTAIN_DWARF, SP_ONI, SP_MERFOLK, SP_MINOTAUR, SP_GARGOYLE, SP_ARMATAUR, },
    { },
    { "animal skin" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_DODGING, 2 }, { SK_WEAPON, 3 }, },
} },

{ JOB_BRIGAND, {
    "Br", "Brigand",
    3, 3, 6,
    { SP_TROLL, SP_SPRIGGAN, SP_DEMONSPAWN, SP_VAMPIRE, SP_VINE_STALKER, SP_GNOLL, },
    { },
    { "dagger plus:2", "robe", "cloak", "dart ego:poisoned q:9",
      "dart ego:curare q:3" },
    WCHOICE_NONE,
    { { SK_FIGHTING, 2 }, { SK_DODGING, 1 }, { SK_STEALTH, 4 },
      { SK_THROWING, 2 }, { SK_WEAPON, 2 }, },
} },

{ JOB_CHAOS_KNIGHT, {
    "CK", "Chaos Knight",
    4, 4, 4,
    { SP_SPRIGGAN, SP_TROLL, SP_GNOLL, SP_MERFOLK, SP_MINOTAUR,
      SP_BASE_DRACONIAN, SP_DEMONSPAWN, },
    { },
    { "leather armour plus:2", "scroll of butterflies no_exclude" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_WEAPON, 3 } },
} },

{ JOB_CINDER_ACOLYTE, {
    "CA", "Cinder Acolyte",
    6, 6, 0,
    { SP_MOUNTAIN_DWARF, SP_BASE_DRACONIAN, SP_ONI, SP_DJINNI, SP_GNOLL },
    { SPELL_SCORCH },
    { "robe" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_WEAPON, 3 },
      { SK_FIRE_MAGIC, 3 }, {SK_SPELLCASTING, 1} },
} },

{ JOB_CONJURER, {
    "Cj", "Conjurer",
    -1, 10, 3,
    { SP_DEEP_ELF, SP_NAGA, SP_TENGU, SP_BASE_DRACONIAN, SP_DEMIGOD, SP_DJINNI, },
    {
        SPELL_MAGIC_DART,
        SPELL_SEARING_RAY,
        SPELL_FULMINANT_PRISM,
        SPELL_ISKENDERUNS_MYSTIC_BLAST,
    },
    { "robe", "potion of magic" },
    WCHOICE_NONE,
    { { SK_CONJURATIONS, 4 }, { SK_SPELLCASTING, 2 }, { SK_DODGING, 2 },
      { SK_STEALTH, 2 }, },
} },

{ JOB_EARTH_ELEMENTALIST, {
    "EE", "Earth Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_SPRIGGAN, SP_GARGOYLE, SP_DEMIGOD, SP_GHOUL,
      SP_OCTOPODE, },
    {
        SPELL_SANDBLAST,
        SPELL_PASSWALL,
        SPELL_STONE_ARROW,
        SPELL_PETRIFY,
        SPELL_BOULDER,
    },
    { "robe", "potion of magic", },
    WCHOICE_NONE,
    { { SK_CONJURATIONS, 1 }, { SK_EARTH_MAGIC, 3 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, }
} },

{ JOB_ENCHANTER, {
    "En", "Enchanter",
    0, 7, 5,
    { SP_DEEP_ELF, SP_FELID, SP_KOBOLD, SP_SPRIGGAN, SP_NAGA, SP_VAMPIRE, },
    {
        SPELL_HIBERNATION,
        SPELL_CONFUSING_TOUCH,
        SPELL_TUKIMAS_DANCE,
        SPELL_DAZZLING_FLASH,
    },
    { "dagger plus:1", "robe", "potion of invisibility q:2" },
    WCHOICE_NONE,
    { { SK_WEAPON, 1 }, { SK_HEXES, 3 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 3 }, },
} },

{ JOB_FIGHTER, {
    "Fi", "Fighter",
    8, 0, 4,
    { SP_MOUNTAIN_DWARF, SP_TROLL, SP_MINOTAUR, SP_GARGOYLE, SP_ARMATAUR, SP_FORMICID, },
    { },
    { "scale mail", "buckler", "potion of might" },
    WCHOICE_GOOD,
    { { SK_FIGHTING, 3 }, { SK_SHIELDS, 3 }, { SK_ARMOUR, 3 },
      { SK_WEAPON, 2 } },
} },

{ JOB_FIRE_ELEMENTALIST, {
    "FE", "Fire Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_MOUNTAIN_DWARF, SP_NAGA, SP_TENGU, SP_DEMIGOD, SP_GARGOYLE,
      SP_DJINNI, },
    {
        SPELL_FOXFIRE,
        SPELL_SCORCH,
        SPELL_BLASTMOTE,
        SPELL_INNER_FLAME,
        SPELL_FLAME_WAVE,
    },
    { "robe", "potion of magic" },
    WCHOICE_NONE,
    { { SK_CONJURATIONS, 1 }, { SK_FIRE_MAGIC, 3 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_GLADIATOR, {
    "Gl", "Gladiator",
    6, 0, 6,
    { SP_MOUNTAIN_DWARF, SP_MERFOLK, SP_TROLL, SP_GARGOYLE, SP_COGLIN, SP_VINE_STALKER, },
    { },
    { "leather armour", "helmet", "throwing net q:3" },
    WCHOICE_GOOD,
    { { SK_FIGHTING, 2 }, { SK_THROWING, 2 }, { SK_DODGING, 3 },
      { SK_WEAPON, 3}, },
} },

{ JOB_HEXSLINGER, {
    "Hs", "Hexslinger",
    0, 5, 7,
    { SP_FORMICID, SP_DEEP_ELF, SP_KOBOLD, SP_SPRIGGAN, SP_GNOLL },
    {
        SPELL_JINXBITE,
        SPELL_SIGIL_OF_BINDING,
        SPELL_INNER_FLAME,
        SPELL_CAUSE_FEAR,
        SPELL_DIMENSIONAL_BULLSEYE,
    },
    { "robe", "scroll of poison", "sling plus:1" },
    WCHOICE_NONE,
    { { SK_FIGHTING, 1 }, { SK_DODGING, 2 }, { SK_SPELLCASTING, 1 },
      { SK_FIRE_MAGIC, 1}, { SK_HEXES, 3 }, { SK_WEAPON, 2 }, },
} },

{ JOB_HUNTER, {
    "Hu", "Hunter",
    3, 1, 8,
    { SP_MINOTAUR, SP_GNOLL, SP_BARACHI, SP_KOBOLD, SP_SPRIGGAN, },
    { },
    { "leather armour", "scroll of butterflies no_exclude", "shortbow" },
    WCHOICE_NONE,
    { { SK_FIGHTING, 2 }, { SK_DODGING, 2 }, { SK_STEALTH, 1 },
      { SK_WEAPON, 4 }, },
} },

{ JOB_ICE_ELEMENTALIST, {
    "IE", "Ice Elementalist",
    0, 7, 5,
    { SP_MERFOLK, SP_BARACHI, SP_BASE_DRACONIAN, SP_DEMIGOD,
      SP_GARGOYLE, SP_DJINNI, },
    {
        SPELL_FREEZE,
        SPELL_FROZEN_RAMPARTS,
        SPELL_OZOCUBUS_ARMOUR,
        SPELL_SUMMON_ICE_BEAST,
    },
    { "robe", "potion of magic" },
    WCHOICE_NONE,
    { { SK_ICE_MAGIC, 4 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_DELVER, {
    "De", "Delver",
    4, 2, 6,
    { SP_FELID, SP_SPRIGGAN, SP_KOBOLD, SP_VAMPIRE, SP_GNOLL },
    { },
    { "leather armour", "scroll of fog no_exclude", "scroll of revelation",
      "scroll of fear", "potion of haste", "wand of digging charges:3" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_DODGING, 2 }, { SK_STEALTH, 5 }, { SK_WEAPON, 2 }, },
} },

{ JOB_MONK, {
    "Mo", "Monk",
    3, 2, 7,
    { SP_MOUNTAIN_DWARF, SP_TROLL, SP_ARMATAUR, SP_MERFOLK, SP_GARGOYLE, SP_DEMONSPAWN, },
    { },
    { "robe", "potion of ambrosia", "orb ego:light" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_WEAPON, 3 }, { SK_DODGING, 3 } },
} },

{ JOB_NECROMANCER, {
    "Ne", "Necromancer",
    0, 7, 5,
    { SP_DEEP_ELF, SP_DJINNI, SP_MOUNTAIN_DWARF, SP_DEMONSPAWN, SP_MUMMY,
      SP_VAMPIRE, },
    {
        SPELL_NECROTISE,
        SPELL_VAMPIRIC_DRAINING,
        SPELL_ANIMATE_DEAD,
        SPELL_CURSE_OF_AGONY,
    },
    { "robe", "potion of magic" },
    WCHOICE_NONE,
    { { SK_SPELLCASTING, 2 }, { SK_NECROMANCY, 4 }, { SK_DODGING, 2 },
      { SK_STEALTH, 2 }, },
} },

{ JOB_REAVER, {
    "Re", "Reaver",
    4, 5, 3,
    { SP_GNOLL, SP_TENGU, SP_BARACHI, SP_DEMONSPAWN, SP_BASE_DRACONIAN,
      SP_MOUNTAIN_DWARF, },
    {
        SPELL_KISS_OF_DEATH,
        SPELL_MOMENTUM_STRIKE,
        SPELL_HAILSTORM,
    },
    { "leather armour" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 2 }, { SK_WEAPON, 3 }, { SK_DODGING, 2 },
      { SK_SPELLCASTING, 2 }, { SK_CONJURATIONS, 3 }, },
} },

{ JOB_SUMMONER, {
    "Su", "Summoner",
    0, 7, 5,
    { SP_DEEP_ELF, SP_MOUNTAIN_DWARF, SP_VINE_STALKER, SP_MERFOLK, SP_TENGU,
      SP_VAMPIRE, },
    {
        SPELL_SUMMON_SMALL_MAMMAL,
        SPELL_CALL_IMP,
        SPELL_CALL_CANINE_FAMILIAR,
        SPELL_SUMMON_BLAZEHEART_GOLEM,
        SPELL_SUMMON_LIGHTNING_SPIRE,
    },
    { "robe", "potion of magic" },
    WCHOICE_NONE,
    { { SK_SUMMONINGS, 4 }, { SK_SPELLCASTING, 2 }, { SK_DODGING, 2 },
      { SK_STEALTH, 2 }, },
} },

{ JOB_SHAPESHIFTER, {
    "Sh", "Shapeshifter",
    6, 2, 4,
    { SP_NAGA, SP_MERFOLK, SP_BASE_DRACONIAN, SP_DEMIGOD, SP_DEMONSPAWN,
      SP_TROLL },
    { },
    { "animal skin", "potion of lignification", "beast talisman", "flux talisman" },
    WCHOICE_NONE,
    { { SK_FIGHTING, 1 }, { SK_UNARMED_COMBAT, 3 }, { SK_DODGING, 2 },
      { SK_SHAPESHIFTING, 3 }, },
} },

{ JOB_ALCHEMIST, {
    "Al", "Alchemist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_SPRIGGAN, SP_NAGA, SP_MERFOLK, SP_OCTOPODE, SP_DJINNI,
      SP_DEMONSPAWN, },
    {
        SPELL_STING,
        SPELL_MERCURY_VAPOURS,
        SPELL_MEPHITIC_CLOUD,
        SPELL_OLGREBS_TOXIC_RADIANCE,
        SPELL_STICKY_FLAME
    },
    { "robe", "potion of magic" },
    WCHOICE_NONE,
    { { SK_ALCHEMY, 3 }, { SK_CONJURATIONS, 1}, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_WANDERER, {
    "Wn", "Wanderer",
    0, 0, 0, // Randomised
    { SP_MOUNTAIN_DWARF, SP_GNOLL, SP_MERFOLK, SP_BASE_DRACONIAN, SP_HUMAN,
      SP_DEMONSPAWN, SP_BARACHI, },
    { }, // Randomised
    { }, // Randomised
    WCHOICE_NONE,
    { }, // Randomised
} },

{ JOB_WARPER, {
    "Wr", "Warper",
    3, 5, 4,
    { SP_FELID, SP_SPRIGGAN, SP_ARMATAUR, SP_BASE_DRACONIAN, SP_COGLIN, },
    {
        SPELL_BLINK,
        SPELL_BECKONING,
        SPELL_TELEPORT_OTHER,
        SPELL_MANIFOLD_ASSAULT,
        SPELL_ELECTRIC_CHARGE,
    },
    { "leather armour", "scroll of blinking", "dart ego:dispersal q:7" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 2 }, { SK_ARMOUR, 1 }, { SK_DODGING, 2 },
      { SK_SPELLCASTING, 2 }, { SK_TRANSLOCATIONS, 3 }, { SK_THROWING, 1 },
      { SK_WEAPON, 2 }, },
} },

{ JOB_HEDGE_WIZARD, {
    "HW", "Hedge Wizard",
    2, 6, 4,
    { SP_DEEP_ELF, SP_NAGA, SP_BASE_DRACONIAN, SP_OCTOPODE, SP_HUMAN,
      SP_DJINNI, },
    {
        SPELL_MAGIC_DART,
        SPELL_SLOW,
        SPELL_BLINK,
        SPELL_CALL_IMP,
        SPELL_ROT,
        SPELL_MEPHITIC_CLOUD,
    },
    { "dagger", "robe", "hat", "potion of magic" },
    WCHOICE_NONE,
    { { SK_DODGING, 2 }, { SK_STEALTH, 2 }, { SK_SPELLCASTING, 3 },
      { SK_TRANSLOCATIONS, 1 }, { SK_CONJURATIONS, 1 }, { SK_SUMMONINGS, 1 }, },
} },
#if TAG_MAJOR_VERSION == 34
{ JOB_ABYSSAL_KNIGHT, {
    "AK", "Abyssal Knight",
} },

{ JOB_SKALD, {
    "Sk", "Skald",
    0, 0, 0,
    { },
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_DEATH_KNIGHT, {
    "DK", "Death Knight",
    0, 0, 0,
    { },
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_HEALER, {
    "He", "Healer",
    0, 0, 0,
    { },
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_JESTER, {
    "Jr", "Jester",
    0, 0, 0,
    { },
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_PRIEST, {
    "Pr", "Priest",
    0, 0, 0,
    { },
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_STALKER, {
    "St", "Stalker",
    0, 0, 0,
    { },
    { },
    { },
    WCHOICE_NONE,
    { },
} },
#endif
};
