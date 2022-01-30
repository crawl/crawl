#pragma once

#include "monster-type.h"

class unique_creature_list : public FixedBitVector<NUM_MONSTERS>
{
public:
    inline void set(unsigned int i, bool value = true) override
    {
        switch (i)
        {
        case MONS_MARGERY:
        case MONS_MAGGIE:
            FixedBitVector<NUM_MONSTERS>::set(MONS_MARGERY, value);
            FixedBitVector<NUM_MONSTERS>::set(MONS_MAGGIE, value);
            break;
        case MONS_PARGI:
        case MONS_PARGHIT:
            FixedBitVector<NUM_MONSTERS>::set(MONS_PARGI, value);
            FixedBitVector<NUM_MONSTERS>::set(MONS_PARGHIT, value);
            break;
        default:
            FixedBitVector<NUM_MONSTERS>::set(i, value);
            break;
        }
    }
};
