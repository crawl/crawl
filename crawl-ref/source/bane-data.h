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

    {
        BANE_ELECTROSPASM,
        BANE_DUR_MEDIUM,
        "Electrospasm",
        "You sometimes become unable to move when damaged by electricity.",
    },

    {
        BANE_CLAUSTROPHOBIA,
        BANE_DUR_MEDIUM,
        "Claustrophobia",
        "Your damage and accuracy is decreased for each wall adjacent to you.",
    },

    {
        BANE_STUMBLING,
        BANE_DUR_SHORT,
        "Stumbling",
        "Your evasion is greatly reduced on turns you move or wait in place.",
    },

    {
        BANE_RECKLESS,
        BANE_DUR_LONG,
        "the Reckless",
        "Your SH is set to 0.",
    },

    {
        BANE_SUCCOUR,
        BANE_DUR_MEDIUM,
        "Succour",
        "You heal other nearby enemies whenever you kill a monster.",
    },

    {
        BANE_MULTIPLICITY,
        BANE_DUR_MEDIUM,
        "Multiplicity",
        "Enemies in your sight sometimes split into clones of themselves.",
    },

    {
        BANE_DILETTANTE,
        BANE_DUR_LONG,
        "the Dilettante",
        "You are less proficient with several skills.", // Overriden
    },

    {
        BANE_PARADOX,
        BANE_DUR_MEDIUM,
        "Paradox",
        "Enemies you spot sometimes become touched by paradox.",
    },

    {
        BANE_WARDING,
        BANE_DUR_SHORT,
        "Warding",
        "Enemies you encounter may be immune to damage from range.",
    },

    {
        BANE_HUNTED,
        BANE_DUR_LONG,
        "the Hunted",
        "When enemies are summoned, they will appear beside you instead.",
    },

    {
        BANE_MORTALITY,
        BANE_DUR_MEDIUM,
        "Mortality",
        "When alone and injured, reapers sometimes come to claim you.",
    },
};
