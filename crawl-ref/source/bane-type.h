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
#if TAG_MAJOR_VERSION == 34
    BANE_RECKLESS_REMOVED,
#endif
    BANE_SUCCOUR,
    BANE_MULTIPLICITY,
    BANE_DILETTANTE,
    BANE_PARADOX,
    BANE_WARDING,
    BANE_HUNTED,
    BANE_MORTALITY,

    NUM_BANES,
};

constexpr int BANE_DUR_LONG   = 4000;
constexpr int BANE_DUR_MEDIUM = 2500;
constexpr int BANE_DUR_SHORT  = 1600;

#define CLAUSTROPHOBIA_KEY "claustrophia_stacks"
#define MULTIPLICITY_TIME_KEY "multiplicity_time"
#define DILETTANTE_SKILL_KEY "dilettante_skills"
#define MORTALITY_TIME_KEY "mortality_time"
