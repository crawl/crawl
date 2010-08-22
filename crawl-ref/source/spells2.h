/*
 *  File:       spells2.h
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 */

#ifndef SPELLS2_H
#define SPELLS2_H

#include "itemprop-enum.h"

class dist;

bool brand_weapon(brand_type which_brand, int power);

bool cast_selective_amnesia(bool force);

void cast_see_invisible(int pow);
void cast_silence(int pow);

#endif
