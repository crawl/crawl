#pragma once

#include <cstdint>

#include "tag-version.h"

enum object_class_type : uint8_t           // env.item[].base_type
{
    OBJ_WEAPONS,
    OBJ_MISSILES,
    OBJ_ARMOUR,
    OBJ_WANDS,
#if TAG_MAJOR_VERSION == 34
    OBJ_FOOD,
#endif
    OBJ_SCROLLS,
    OBJ_JEWELLERY,
    OBJ_POTIONS,
    OBJ_BOOKS,
    OBJ_STAVES,
    OBJ_ORBS,
    OBJ_MISCELLANY,
    OBJ_CORPSES,
    OBJ_GOLD,
#if TAG_MAJOR_VERSION == 34
    OBJ_RODS,
#endif
    OBJ_RUNES,
    OBJ_TALISMANS,
    OBJ_GEMS,
    OBJ_GIZMOS,
    NUM_OBJECT_CLASSES,
    OBJ_UNASSIGNED = 100,
    OBJ_RANDOM,      // used for blanket random sub_type .. see dungeon::items()
    OBJ_DETECTED,    // unknown item; pseudo-items on the map only
};
