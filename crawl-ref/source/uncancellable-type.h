#pragma once

enum uncancellable_type
{
#if TAG_MAJOR_VERSION == 34
    UNC_ACQUIREMENT,           // arg is AQ_SCROLL
#endif
    UNC_DRAW_THREE,            // arg is ignored
    UNC_STACK_FIVE,            // arg is number of cards to stack
#if TAG_MAJOR_VERSION == 34
    UNC_MERCENARY,             // arg is mid of the monster
#endif
    UNC_POTION_PETITION,       // arg is ignored
    UNC_CALL_MERCHANT,         // arg is ignored
};
