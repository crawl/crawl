#pragma once

#include "tag-version.h"

enum class transformation
{
    none,
    spider,
    blade_hands,
    statue,
    ice_beast,
    dragon,
    lich,
    bat,
    pig,
    appendage,
    tree,
#if TAG_MAJOR_VERSION == 34
    porcupine,
#endif
    wisp,
#if TAG_MAJOR_VERSION == 34
    jelly,
#endif
    fungus,
    shadow,
#if TAG_MAJOR_VERSION == 34
    hydra,
#endif
    air,
    COUNT
};
constexpr int NUM_TRANSFORMS = static_cast<int>(transformation::COUNT);
