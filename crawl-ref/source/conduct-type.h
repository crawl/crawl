#pragma once
#include "tag-version.h"
enum conduct_type
{
    DID_NOTHING,
#if TAG_MAJOR_VERSION == 34
    DID_EVIL,                             // now forbidden
    DID_HOLY,                             // now forbidden
#endif
    DID_ATTACK_HOLY,
    DID_ATTACK_NEUTRAL,
    DID_ATTACK_FRIEND,
    DID_KILL_LIVING,
    DID_KILL_UNDEAD,
    DID_KILL_DEMON,
    DID_KILL_NATURAL_EVIL,                // TSO
    DID_KILL_UNCLEAN,                     // Zin
    DID_KILL_CHAOTIC,                     // Zin
    DID_KILL_WIZARD,                      // Trog
    DID_KILL_PRIEST,                      // Beogh
    DID_KILL_HOLY,
    DID_KILL_FAST,                        // Cheibriados
    DID_BANISH,
#if TAG_MAJOR_VERSION == 34
    DID_SPELL_MEMORISE,                   // now forbidden
    DID_SPELL_CASTING,                    // now forbidden
    DID_SPELL_PRACTISE,                   // now forbidden
    DID_CANNIBALISM,
    DID_DELIBERATE_MUTATING,              // Zin
#endif
    DID_CAUSE_GLOWING,                    // Zin
#if TAG_MAJOR_VERSION == 34
    DID_UNCLEAN,                          // now forbidden
    DID_CHAOS,                            // now forbidden
    DID_DESECRATE_ORCISH_REMAINS,         // Beogh
    DID_KILL_SLIME,                       // Jiyva
    DID_HASTY,                            // now forbidden
    DID_ATTACK_IN_SANCTUARY,              // Zin
#endif
    DID_KILL_NONLIVING,
    DID_EXPLORATION,                      // Ashenzari, wrath timers
    DID_SEE_MONSTER,                      // TSO
#if TAG_MAJOR_VERSION == 34
    DID_SACRIFICE_LOVE,                   // Ru
#endif
    DID_HURT_FOE,                         // Uskayaw
#if TAG_MAJOR_VERSION == 34
    DID_WIZARDLY_ITEM,                    // now forbidden
#endif
    DID_TITHE,                            // Zin
    NUM_CONDUCTS
};
