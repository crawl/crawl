#pragma once

enum targ_mode_type
{
    TARG_ANY,
    TARG_FRIEND,
    TARG_INJURED_FRIEND, // for healing
    TARG_HOSTILE,
    TARG_MOVABLE_OBJECT,    // Movable objects only
    TARG_MOBILE_MONSTER,    // Non-stationary monsters
    TARG_NON_ACTOR,         // For things which target features or directions
    TARG_HOSTILE_OR_EMPTY,  // Perfer aiming at enemies, but aim at nearby
                            // space if none are in range. (Platinum Paragon)
    TARG_NUM_MODES
};
