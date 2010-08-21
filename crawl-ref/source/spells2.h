/*
 *  File:       spells2.h
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 */

#ifndef SPELLS2_H
#define SPELLS2_H

#include "enum.h"
#include "itemprop-enum.h"

class dist;

bool brand_weapon(brand_type which_brand, int power);
bool burn_freeze(int pow, beam_type flavour, monsters *monster);

bool vampiric_drain(int pow, monsters *monster);
int detect_creatures(int pow, bool telepathic = false);
int detect_items(int pow);
int detect_traps(int pow);
void cast_refrigeration(int pow, bool non_player = false);
void cast_toxic_radiance(bool non_player = false);

#endif
