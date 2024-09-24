#pragma once

#include "tag-version.h"

enum killer_type                       // monster_die(), thing_thrown
{
    KILL_NONE,                         // no killer
    KILL_YOU,                          // you are the killer
    KILL_MON,                          // no, it was a monster!
    KILL_YOU_MISSILE,                  // in the library, with a dart
    KILL_MON_MISSILE,                  // in the dungeon, with a club
    KILL_YOU_CONF,                     // died while confused as caused by you
#if TAG_MAJOR_VERSION == 34
    KILL_MISCAST,                      // as a result of a spell miscast
#endif
    KILL_NON_ACTOR,                    // Killed directly by something that was
                                       // not the player or a monster (eg:
                                       // neutral cloud generators or god effects)
    KILL_RESET,                        // excised from existence
    KILL_RESET_KEEP_ITEMS,             // like KILL_RESET, but drops inventory
    KILL_BANISHED,                     // monsters what got banished
#if TAG_MAJOR_VERSION == 34
    KILL_UNSUMMONED,                   // summoned monsters whose timers ran out
#endif
    KILL_TIMEOUT,                      // non-summoned monsters whose times ran out
    KILL_PACIFIED,                     // only used by milestones and notes
    KILL_BOUND,                        // only used by milestones and notes
    KILL_SLIMIFIED,                    // only used by milestones and notes
    KILL_TENTACLE_CLEANUP,             // Used to prevent infinite recursion when
                                       // the death of a tentacle segment kills
                                       // the rest of the tentacle. (Otherwise
                                       // identical to KILL_RESET)
};
