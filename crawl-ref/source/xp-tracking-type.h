#pragma once

// XP tracking state for the xp_by_level dump table.
enum xp_tracking_type
{
    XP_GENERATED,  // Monster generated at level creation.
    XP_SPAWNED,    // Monster spawned as turns were spent on the level.
    XP_UNTRACKED,  // Don't track this monster's XP (e.g. Orb spawns).
};
