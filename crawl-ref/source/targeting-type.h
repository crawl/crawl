#pragma once

enum targeting_type
{
    DIR_NONE,           // positional or cardinal targeting
    DIR_TARGET,         // positional targeting only
    DIR_SHADOW_STEP,    // a shadow step target (prevents cardinal targeting as
                        // well as ! and @ targeting, uses a special hitfunc)
    DIR_ENFORCE_RANGE,  // like DIR_TARGET, but the range is a hard limit
                        // (prevents ! and @ targeting)
};
