/**
 * @file
 * @brief Prayer and sacrifice.
**/

#ifndef GODPRAYER_H
#define GODPRAYER_H

#include "religion-enum.h"

string god_prayer_reaction();
void pray();

piety_gain_t sacrifice_item_stack(const item_def& item, int *js = 0,
                                  int quantity = 0);
bool check_nemelex_sacrificing_item_type(const item_def& item);
int zin_tithe(item_def& item, int quant, bool quiet, bool converting = false);

#endif
