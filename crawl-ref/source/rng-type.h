#pragma once
#include "branch-type.h"

enum rng_type {
    RNG_GAMEPLAY,
    RNG_UI,
    RNG_SPARE1,
    RNG_SPARE2,
    RNG_SPARE3,
    RNG_LEVELGEN, // branch 0
    NUM_RNGS = RNG_LEVELGEN + NUM_BRANCHES,
};
