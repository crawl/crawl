/*
 * File:     godpassive.h
 * Summary:  Passive god effects.
 */

#ifndef GODPASSIVE_H
#define GODPASSIVE_H

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
    CB_PONDEROUS, // Change in number of worn ponderous items.
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
int ash_bondage_level(int type_only = 0);
void ash_check_bondage();
bool ash_id_item(item_def& item, bool silent = true);
void ash_id_inventory();
int ash_detect_portals(bool all);

#endif
