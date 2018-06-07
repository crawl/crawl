#pragma once

enum montravel_target_type
{
    MTRAV_NONE = 0,
    MTRAV_FOE,         // Travelling to reach its foe.
    MTRAV_PATROL,      // Travelling to reach the patrol point.
    MTRAV_MERFOLK_AVATAR, // Merfolk avatars travelling towards deep water.
    MTRAV_UNREACHABLE, // Not travelling because target is unreachable.
    MTRAV_KNOWN_UNREACHABLE, // As above, and the player knows this.
};
