#pragma once

// XP tracking state for the xp_by_level dump table.
enum xp_tracking_type
{
    XP_NON_VAULT,  // Monster was generated as level monster or spawn.
    XP_VAULT,      // Monster was placed by a vault.
    XP_UNTRACKED,  // Don't track this monster's XP (e.g. Orb spawns).
};
