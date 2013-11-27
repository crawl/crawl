/**
 * @file
 * @brief Potion and potion-like effects.
**/

#ifndef POTION_H
#define POTION_H


#include "externs.h"

// drank_it should be true for real potion effects (as opposed
// to abilities which duplicate such effects.)
void potion_effect(potion_type pot_eff, int pow,
                   item_def *potion = nullptr, bool was_known = true,
                   bool from_fountain = false);

#endif
