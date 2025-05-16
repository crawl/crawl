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
        BANE_PLACEHOLDER_1,
        1000,
        "placeholder",
        "You are afflicted by a placeholder.",
    },

    {
        BANE_PLACEHOLDER_2,
        500,
        "placeholder 2",
        "You are afflicted by a very different placeholder.",
    },
};
