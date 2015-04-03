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

int recharge_wand(bool known = true, const string &pre_msg = "");

void wind_blast(actor* agent, int pow, coord_def target, bool card = false);

bool can_flood_feature(dungeon_feature_type feat);

bool evoke_item(int slot = -1, bool check_range = false);
int wand_mp_cost();
void zap_wand(int slot = -1);

void shadow_lantern_effect();
void expire_lantern_shadows();
bool disc_of_storms();
void black_drac_breath();

#endif
