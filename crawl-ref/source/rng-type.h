#pragma once

#include "branch-type.h"

namespace rng
{
    enum rng_type {
        GAMEPLAY,        // gameplay effects in general
        UI,              // UI randomization
        SYSTEM_SPECIFIC, // anything that may vary from system to system, e.g. ghosts
        SPARE2,
        SPARE3,
        LEVELGEN,        // branch 0, i.e. the dungeon
        NUM_RNGS = LEVELGEN + NUM_BRANCHES, // and then one for each other branch
        SUB_GENERATOR,   // unsaved -- past NUM_RNGS
        ASSERT_NO_RNG,   // debugging tool
    };
}
