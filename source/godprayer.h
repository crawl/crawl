/**
 * @file
 * @brief Prayer and sacrifice.
**/

#ifndef GODPRAYER_H
#define GODPRAYER_H

#include "religion-enum.h"

string god_prayer_reaction();
bool can_do_capstone_ability(god_type god);
void pray(bool allow_altar_prayer = true);

piety_gain_t sacrifice_item_stack(const item_def& item, int *js = 0,
                                  int quantity = 0);
int zin_tithe(const item_def& item, int quant, bool quiet,
              bool converting = false);

#endif
