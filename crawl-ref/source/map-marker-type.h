#pragma once

// Can't change this order without breaking saves.
enum map_marker_type
{
    MAT_FEATURE,              // Stock marker.
    MAT_LUA_MARKER,
    MAT_CORRUPTION_NEXUS,
    MAT_WIZ_PROPS,
    MAT_TOMB,
    MAT_MALIGN,
#if TAG_MAJOR_VERSION == 34
    MAT_PHOENIX,
#endif
    MAT_POSITION,
#if TAG_MAJOR_VERSION == 34
    MAT_DOOR_SEAL,
#endif
    MAT_TERRAIN_CHANGE,
    MAT_CLOUD_SPREADER,
    NUM_MAP_MARKER_TYPES,
    MAT_ANY,
};
