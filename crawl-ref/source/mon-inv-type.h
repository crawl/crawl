#pragma once

#include "tag-version.h"

// Adding slots breaks saves. YHBW.
enum mon_inv_type           // env.mons[].inv[]
{
    MSLOT_WEAPON,           // Primary weapon (melee)
    MSLOT_ALT_WEAPON,       // Alternate weapon, ranged or second melee weapon
                            // for monsters that can use two weapons.
    MSLOT_MISSILE,
#if TAG_MAJOR_VERSION == 34
    MSLOT_ALT_MISSILE,
#endif
    MSLOT_ARMOUR,
    MSLOT_SHIELD,
    MSLOT_WAND,
    MSLOT_JEWELLERY,
    MSLOT_MISCELLANY,

    // [ds] Last monster gear slot that the player can observe by examining
    // the monster; i.e. the last slot that goes into monster_info.
    MSLOT_LAST_VISIBLE_SLOT = MSLOT_MISCELLANY,

#if TAG_MAJOR_VERSION == 34
    MSLOT_POTION,
    MSLOT_SCROLL,
#endif
    MSLOT_GOLD,
    NUM_MONSTER_SLOTS
};
