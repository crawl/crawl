#pragma once

#include <string>

#include "species-type.h"
#include "spell-type.h"
#include "skill-type.h"

enum class weapon_choice
{
    none,   ///< No weapon choice
    plain,  ///< Normal weapon choice
    good,   ///< Chooses from "good" weapons
};

struct job_def
{
    const char* abbrev; ///< Two-letter abbreviation
    const char* name; ///< Long name
    int s, i, d; ///< Starting Str, Dex, and Int
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
