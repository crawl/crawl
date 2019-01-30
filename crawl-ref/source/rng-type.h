#pragma once
#include "branch-type.h"

enum rng_type {
    RNG_GAMEPLAY,        // gameplay effects in general
    RNG_UI,              // UI randomization
    RNG_SYSTEM_SPECIFIC, // anything that may vary from system to system, e.g. ghosts
    RNG_SPARE2,
    RNG_SPARE3,
    RNG_LEVELGEN,        // branch 0, i.e. the dungeon
    NUM_RNGS = RNG_LEVELGEN + NUM_BRANCHES, // and then one for each other branch
};
