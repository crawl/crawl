/**
 * @file
 * @brief Potion and potion-like effects.
**/

#ifndef POTION_H
#define POTION_H

#include "externs.h"

bool potion_effect(potion_type pot_eff, int pow,
                   item_def *potion = nullptr, bool was_known = true);

#endif
