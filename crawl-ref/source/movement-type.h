#pragma once

#include "enum.h"

enum movement_type_flags
{
    // No additional flags.
    MV_DEFAULT                     = 0,

    // Movement is a translocation, rather than a physical movement.
    MV_TRANSLOCATION            = 1<<0,

    // Movement was done deliberately by the actor themselves (ie: will trigger
    // Barbs and reduce Sticky Flame duration, unless also a translocation).
    MV_DELIBERATE               = 1<<1,

    // Don't update or cancel constriction being performed by the moving actor.
    MV_PRESERVE_CONSTRICTION    = 1<<2,

    // Allow monsters to move to a tile currently occupied by the player and
    // the player to move to a tile currently occupied by a monster (that they
    // would otherwise not be able to stand on).
    MV_ALLOW_OVERLAP            = 1<<3,

    // Movement done by some internal process that should not result in any
    // post-movement effects. For players, also implies MV_ALLOW_OVERLAP and
    // bypasses some sanity checks.
    MV_INTERNAL                 = 1<<4,

    // Movement should not actually update the monster grid, if performed on a
    // monster. **This is very dangerous and should never be done unless you're
    // confident in what you're doing!**
    MV_NO_MGRID_UPDATE          = 1<<5,

    // Movement should not interrupt travel on its own.
    MV_NO_TRAVEL_STOP           = 1<<6,

    MV_LAST = MV_NO_TRAVEL_STOP,
};
DEF_BITFIELD(movement_type, movement_type_flags, 6);
COMPILE_CHECK(movement_type::exponent(movement_type::last_exponent)
                                            == movement_type_flags::MV_LAST);
