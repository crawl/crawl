/**
 * @file
 * @brief Functions for using some of the wackier inventory items.
**/

#ifndef EVOKE_H
#define EVOKE_H

int manual_slot_for_skill(skill_type skill);
bool skill_has_manual(skill_type skill);
void finish_manual(int slot);
void get_all_manual_charges(vector<int> &charges);
void set_all_manual_charges(const vector<int> &charges);
string manual_skill_names(bool short_text=false);

void wind_blast(actor* agent, int pow, coord_def target);

void tome_of_power(int slot);

bool evoke_item(int slot = -1);

void shadow_lantern_effect();
bool disc_of_storms(bool drac_breath = false);

#endif
