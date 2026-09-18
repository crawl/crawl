#pragma once

#include "tag-version.h"

// Can't change this order without breaking saves.
enum map_marker_type
{
    MAT_FEATURE,              // Stock marker.
    MAT_LUA_MARKER,
    MAT_CORRUPTION_NEXUS,
    MAT_WIZ_PROPS,
    MAT_TOMB,
#if TAG_MAJOR_VERSION == 34
    MAT_MALIGN_OLD,
    MAT_PHOENIX,
#endif
    MAT_POSITION,
#if TAG_MAJOR_VERSION == 34
    MAT_DOOR_SEAL,
#endif
    MAT_TERRAIN_CHANGE,
    MAT_CLOUD_SPREADER,
    MAT_HELLFIRE_MORTAR_LAVA,
    MAT_MALIGN_GATEWAY,
    MAT_ACTIVE_FEATURE,
    NUM_MAP_MARKER_TYPES,
    MAT_ANY,
};
