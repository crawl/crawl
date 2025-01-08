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
    TARG_NUM_MODES
};
