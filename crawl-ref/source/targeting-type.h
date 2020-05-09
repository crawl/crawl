#pragma once

enum targeting_type
{
    DIR_NONE,           // smite or in a cardinal direction
    DIR_TARGET,         // smite targeting
    DIR_SHADOW_STEP,    // a shadow step target
    DIR_LEAP,           // like DIR_TARGET, but the range is a hard limit
};
