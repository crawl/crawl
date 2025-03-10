#pragma once

enum targeting_type
{
    DIR_NONE,           // positional or cardinal targeting
    DIR_TARGET,         // positional targeting only
    DIR_ENFORCE_RANGE,  // like DIR_TARGET, but the range is a hard limit
                        // (prevents ! and @ targeting)
};
