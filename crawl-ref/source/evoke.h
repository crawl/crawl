/**
 * @file
 * @brief Functions for using some of the wackier inventory items.
**/

#pragma once

#include <vector>

using std::vector;

string manual_skill_names(bool short_text=false);

void wind_blast(actor* agent, int pow, coord_def target, bool card = false);

bool evoke_check(int slot, bool quiet = false);
bool evoke_item(int slot = -1, dist *target=nullptr);
int wand_mp_cost();
int wand_power();
void zap_wand(int slot = -1, dist *target=nullptr);

void black_drac_breath();
