#pragma once

#include "bane-type.h"

struct bane_def
{
    bane_type   type;
    short       duration;    ///< Duration of bane in XP units
    const char* name;        ///< Name of this bane.
    const char* description; ///< What appears on the 'A' screen.
};

static const bane_def bane_data[] =
{
    {
        BANE_LETHARGY,
        BANE_DUR_LONG,
        "Lethargy",
        "You cover ground slowly.",
    },

    {
        BANE_HEATSTROKE,
        BANE_DUR_MEDIUM,
        "Heatstroke",
        "You sometimes become slowed when damaged by fire.",
    },

    {
        BANE_SNOW_BLINDNESS,
        BANE_DUR_MEDIUM,
        "Snow-blindness",
        "You sometimes become weak and blind when damaged by cold.",
    },
};
