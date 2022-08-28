#pragma once

#include "tag-version.h"

enum mon_attitude_type
{
    ATT_HOSTILE,                       // 0, default in most cases
    ATT_NEUTRAL,                       // neutral
#if TAG_MAJOR_VERSION == 34
    ATT_OLD_STRICT_NEUTRAL,            // neutral, won't attack player. Was used by Jiyva.
#endif
    ATT_GOOD_NEUTRAL,                  // neutral, but won't attack friendlies
    ATT_FRIENDLY,                      // created friendly (or tamed?)
};
