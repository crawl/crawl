#pragma once

#include "tag-version.h"

enum bane_type
{
    BANE_LETHARGY,
    BANE_HEATSTROKE,

    NUM_BANES,
};

constexpr int BANE_DUR_LONG   = 2200;
constexpr int BANE_DUR_MEDIUM = 1500;
constexpr int BANE_DUR_SHORT  = 800;
