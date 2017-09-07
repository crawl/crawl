#pragma once

enum mon_attitude_type
{
    ATT_HOSTILE,                       // 0, default in most cases
    ATT_NEUTRAL,                       // neutral
    ATT_STRICT_NEUTRAL,                // neutral, won't attack player. Used by Jiyva.
    ATT_GOOD_NEUTRAL,                  // neutral, but won't attack friendlies
    ATT_FRIENDLY,                      // created friendly (or tamed?)
};
