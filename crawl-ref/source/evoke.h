/**
 * @file
 * @brief Functions for using some of the wackier inventory items.
**/

#pragma once

#define BAG_PROPS_KEY  "bag_props"

#define MERCENARY_UNIT_KEY  "mercenary_unit_props"
#define MERCENARY_NAME_KEY  "mercenary_name_props"

bool put_bag_item(int bag_slot, int item_dropped, int quant_drop, bool fail_message, bool message);

int manual_slot_for_skill(skill_type skill);
int get_all_manual_charges_for_skill(skill_type skill);
bool skill_has_manual(skill_type skill);
void finish_manual(int slot);
void get_all_manual_charges(vector<int> &charges);
void set_all_manual_charges(const vector<int> &charges);
string manual_skill_names(bool short_text=false);
bool sack_of_spiders(int power, int count, coord_def pos);

void wind_blast(actor* agent, int pow, coord_def target, bool card = false);

bool evoke_check(int slot, bool quiet = false);
bool evoke_item(int slot = -1);
bool evoke_auto_item();
int wand_mp_cost();
int wand_power();
void zap_wand(int slot = -1);

bool disc_of_storms();
void black_drac_breath();

int recharge_wand(item_def& wand, bool known = true, std::string * pre_msg = nullptr);