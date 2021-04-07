#pragma once

#include "monster-type.h"

class unique_creature_list : public FixedBitVector<NUM_MONSTERS>
{
public:
    inline void set(unsigned int i, bool value = true) override
    {
        if (i != MONS_MAGGIE && i != MONS_MARGERY)
            FixedBitVector<NUM_MONSTERS>::set(i, value);
        else
        {
            FixedBitVector<NUM_MONSTERS>::set(MONS_MARGERY, value);
            FixedBitVector<NUM_MONSTERS>::set(MONS_MAGGIE, value);
        }
    }
};
