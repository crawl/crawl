/**
 * @file
 * @brief Prayer and sacrifice.
**/

#pragma once

#include "god-type.h"

string god_prayer_reaction();
void try_god_conversion(god_type god);

int zin_tithe(const item_def& item, int quant, bool converting = false);
