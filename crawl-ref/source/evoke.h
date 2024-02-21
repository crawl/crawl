/**
 * @file
 * @brief Functions for using some of the wackier inventory items.
**/

#pragma once

#include <vector>

using std::vector;

struct dice_def;

string manual_skill_names(bool short_text=false);

void wind_blast(actor* agent, int pow, coord_def target);

string cannot_evoke_item_reason(const item_def *item=nullptr,
                                bool temp=true, bool ident=true);
bool item_currently_evokable(const item_def *item);
bool item_ever_evokable(const item_def &item);
bool evoke_item(item_def &item, dist *target=nullptr);
int wand_mp_cost();
int wand_power(spell_type spell);
void zap_wand(int slot = -1, dist *target=nullptr);

int tremorstone_count(int pow);

string target_evoke_desc(const monster_info& mi, const item_def& item);
string evoke_damage_string(const item_def& item);
string evoke_noise_string(const item_def& item);
