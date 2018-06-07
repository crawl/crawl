#pragma once

enum ac_type
{
    AC_NONE,
    // These types block small amounts of damage, hardly affecting big hits.
    AC_NORMAL,
    AC_HALF,
    AC_TRIPLE,
    // This one stays fair over arbitrary splits.
    AC_PROPORTIONAL,
};
