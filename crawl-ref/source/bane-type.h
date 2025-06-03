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
    BANE_SUCCOR,
    BANE_MULTIPLICITY,
    BANE_DILETTANTE,
    BANE_PARADOX,
    BANE_WARDING,
    BANE_HUNTED,
    BANE_MORTALITY,

    NUM_BANES,
};

constexpr int BANE_DUR_LONG   = 2200;
constexpr int BANE_DUR_MEDIUM = 1500;
constexpr int BANE_DUR_SHORT  = 800;

#define CLAUSTROPHOBIA_KEY "claustrophia_stacks"
#define MULTIPLICITY_TIME_KEY "multiplicity_time"
#define DILETTANTE_SKILL_KEY "dilettante_skills"
#define MORTALITY_TIME_KEY "mortality_time"
