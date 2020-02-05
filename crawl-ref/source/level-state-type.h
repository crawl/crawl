#pragma once

// Volatile state and cache.
enum level_state_type
{
    LSTATE_NONE = 0,

    LSTATE_GOLUBRIA       = (1 << 0), // A Golubria trap exists.
#if TAG_MAJOR_VERSION == 34
    LSTATE_GLOW_MOLD      = (1 << 1), // Any glowing mold exists.
#endif
    LSTATE_DELETED        = (1 << 2), // The level won't be saved.
    LSTATE_BEOGH          = (1 << 3), // Possibly an orcish priest around.
    LSTATE_SLIMY_WALL     = (1 << 4), // Any slime walls exist.
    LSTATE_STILL_WINDS    = (1 << 5), // Cloud generation is disabled
    LSTATE_ICY_WALL       = (1 << 6), // Any icy walls exist.
};
