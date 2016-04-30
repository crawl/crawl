/**
 * @file
 * @brief Prayer and sacrifice.
**/

#ifndef GODPRAYER_H
#define GODPRAYER_H

#include "religion-enum.h"

string god_prayer_reaction();
void pray(bool allow_altar_prayer = true);

void jiyva_slurp_item_stack(const item_def& item, int quant = 0);
int zin_tithe(const item_def& item, int quant, bool quiet,
              bool converting = false);

#endif
