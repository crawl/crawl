#pragma once

#include "tag-version.h"

enum daction_type
{
#if TAG_MAJOR_VERSION == 34
    DACT_ALLY_HOLY,
    DACT_ALLY_UNHOLY_EVIL,
    DACT_ALLY_UNCLEAN_CHAOTIC,
    DACT_ALLY_SPELLCASTER,
    DACT_ALLY_YRED_SLAVE,
#endif
    DACT_ALLY_BEOGH, // both orcs and demons summoned by high priests
    DACT_ALLY_SLIME,
    DACT_ALLY_PLANT,

    NUM_DACTION_COUNTERS,

    // Leave space for new counters, as they need to be at the start.
    DACT_OLD_CHARMD_SOULS_POOF = 16,
#if TAG_MAJOR_VERSION == 34
    DACT_HOLY_NEW_ATTEMPT,
#else
    DACT_SLIME_NEW_ATTEMPT,
#endif
#if TAG_MAJOR_VERSION == 34
    DACT_HOLY_PETS_GO_NEUTRAL,
    DACT_ALLY_TROG,
    DACT_RECLAIM_DECKS,
#endif
    DACT_REAUTOMAP,
    DACT_JIYVA_DEAD,
    DACT_PIKEL_MINIONS,
    DACT_ROT_CORPSES,
#if TAG_MAJOR_VERSION == 34
    DACT_TOMB_CTELE,
    DACT_SLIME_NEW_ATTEMPT,
#endif
    DACT_KIRKE_HOGS,
#if TAG_MAJOR_VERSION == 34
    DACT_END_SPIRIT_HOWL,
#endif
    DACT_GOLD_ON_TOP,
    DACT_BRIBE_TIMEOUT,
    DACT_REMOVE_GOZAG_SHOPS,
    DACT_SET_BRIBES,
#if TAG_MAJOR_VERSION == 34
    DACT_ALLY_MAKHLEB,
    DACT_ALLY_SACRIFICE_LOVE,
#endif
    DACT_ALLY_HEPLIAKLQANA,
    DACT_UPGRADE_ANCESTOR,
    DACT_REMOVE_IGNIS_ALTARS,
    NUM_DACTIONS,
    // If you want to add a new daction, you need to
    // add a corresponding entry to *daction_names[]
    // of dactions.cc to avoid breaking the debug build
};
