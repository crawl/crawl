#pragma once

#include "tag-version.h"

enum bane_type
{
    BANE_LETHARGY,
    BANE_HEATSTROKE,
    BANE_SNOW_BLINDNESS,
    BANE_ELECTROSPASM,
    BANE_CLAUSTROPHOBIA,
    BANE_STUMBLING,
    BANE_RECKLESS,

    NUM_BANES,
};

constexpr int BANE_DUR_LONG   = 2200;
constexpr int BANE_DUR_MEDIUM = 1500;
constexpr int BANE_DUR_SHORT  = 800;

#define CLAUSTROPHOBIA_KEY "claustrophia_stacks"
