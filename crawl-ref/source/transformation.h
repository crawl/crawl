#pragma once

#include "tag-version.h"

enum class transformation
{
    none,
#if TAG_MAJOR_VERSION == 34
    spider,
#endif
    blade_hands,
    statue,
    serpent,
    dragon,
    death,
    bat,
    pig,
#if TAG_MAJOR_VERSION == 34
    appendage,
#endif
    tree,
#if TAG_MAJOR_VERSION == 34
    porcupine,
#endif
    wisp,
#if TAG_MAJOR_VERSION == 34
    jelly,
#endif
    fungus,
#if TAG_MAJOR_VERSION == 34
    shadow,
    hydra,
#endif
    storm,
    beast,
    maw,
    flux,
    slaughter,
    COUNT
};
constexpr int NUM_TRANSFORMS = static_cast<int>(transformation::COUNT);
