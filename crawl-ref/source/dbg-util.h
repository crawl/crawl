/**
 * @file
 * @brief Miscellaneous debugging functions.
**/

#pragma once

monster_type debug_prompt_for_monster();
skill_type debug_prompt_for_skill(const char *prompt);
skill_type skill_from_name(const char *name);

int debug_cap_stat(int stat);

void debug_dump_levgen();

struct item_def;
string debug_art_val_str(const item_def& item);

class monster;
struct coord_def;

string debug_coord_str(const coord_def &pos);

void debug_dump_constriction(const actor *act);
void debug_dump_mon(const monster* mon, bool recurse);

string debug_mon_str(const monster* mon);

void wizard_toggle_dprf();

