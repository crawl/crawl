#ifndef NEWGAME_DEF_H
#define NEWGAME_DEF_H

#include "itemprop-enum.h"

enum startup_book_type
{
    SBT_NO_SELECTION = 0,
    SBT_FIRE,
    SBT_COLD,
    SBT_SUMM,
    SBT_RANDOM
};

enum startup_wand_type
{
    SWT_NO_SELECTION = 0,
    SWT_ENSLAVEMENT,
    SWT_CONFUSION,
    SWT_MAGIC_DARTS,
    SWT_FROST,
    SWT_FLAME,
    SWT_STRIKING, // actually a rod
    SWT_RANDOM
};

// Either a character definition, with real species, job, and
// weapon book, religion, wand as appropriate.
// Or a character choice, with possibly random/viable entries.
struct newgame_def
{
    std::string name;

    species_type species;
    job_type job;

    weapon_type weapon;
    startup_book_type book;
    god_type religion;
    startup_wand_type wand;

    // Only relevant for character choice, where the entire
    // character was randomly picked in one step.
    // If this is true, the species field encodes whether
    // the choice was for a viable character or not.
    bool fully_random;

    newgame_def();
};

#endif

