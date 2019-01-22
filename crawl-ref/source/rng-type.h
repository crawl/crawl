#pragma once
#include "branch-type.h"

enum rng_type {
    RNG_GAMEPLAY,
    RNG_UI,
    RNG_LEVELGEN, // branch 0
    NUM_RNGS = RNG_LEVELGEN + NUM_BRANCHES,
};
