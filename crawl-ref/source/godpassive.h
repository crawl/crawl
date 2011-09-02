/**
 * @file
 * @brief Passive god effects.
**/

#ifndef GODPASSIVE_H
#define GODPASSIVE_H

#include "mon-info.h"
#include "religion-enum.h"

enum che_boost_type
{
    CB_RNEG,
    CB_RCOLD,
    CB_RFIRE,
    CB_STATS,
    NUM_BOOSTS
};

enum che_change_type
{
    CB_PIETY,     // Change in piety_rank.
    CB_PONDEROUSNESS,   // Change in ponderousness.
    CB_PONDEROUS_COUNT, // Change in number of worn ponderous items.
    CB_SLOTS      // Change in number of slots.
};

enum jiyva_slurp_results
{
        JS_NONE = 0,
        JS_FOOD = 1,
        JS_HP   = 2,
        JS_MP   = 4,
};

int che_boost_level();
int che_boost(che_boost_type bt, int level = che_boost_level());
void che_handle_change(che_change_type ct, int diff);
void jiyva_eat_offlevel_items();
void jiyva_slurp_bonus(int item_value, int *js);
void jiyva_slurp_message(int js);
void ash_init_bondage(player *y);
void ash_check_bondage(bool msg = true);
std::string ash_describe_bondage(int flags, bool level);
bool ash_id_item(item_def& item, bool silent = true);
void ash_id_inventory();
void ash_id_monster_equipment(monster* mon);
int ash_detect_portals(bool all);
monster_type ash_monster_tier(const monster *mon);
int ash_skill_boost(skill_type sk);
std::map<skill_type, int8_t> ash_get_boosted_skills(eq_type type);

#endif
