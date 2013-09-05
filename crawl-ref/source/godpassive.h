/**
 * @file
 * @brief Passive god effects.
**/

#ifndef GODPASSIVE_H
#define GODPASSIVE_H

#include "enum.h"
#include "player.h"

class monster;

enum jiyva_slurp_results
{
        JS_NONE = 0,
        JS_FOOD = 1,
        JS_HP   = 2,
        JS_MP   = 4,
};

int chei_stat_boost(int piety = you.piety);
void jiyva_eat_offlevel_items();
void jiyva_slurp_bonus(int item_value, int *js);
void jiyva_slurp_message(int js);
void ash_init_bondage(player *y);
void ash_check_bondage(bool msg = true);
string ash_describe_bondage(int flags, bool level);
bool god_id_item(item_def& item, bool silent = true);
void ash_id_monster_equipment(monster* mon);
int ash_detect_portals(bool all);
monster_type ash_monster_tier(const monster *mon);
int ash_skill_boost(skill_type sk, int scale);
map<skill_type, int8_t> ash_get_boosted_skills(eq_type type);

#endif
