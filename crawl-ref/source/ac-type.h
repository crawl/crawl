#pragma once

enum class ac_type
{
    none,
    // These types block small amounts of damage, hardly affecting big hits.
    normal,
    half,
    triple,
    // This one stays fair over arbitrary splits.
    proportional,
};
