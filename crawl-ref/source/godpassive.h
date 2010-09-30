/*
 * File:     godpassive.h
 * Summary:  Passive god effects.
 */

#ifndef GODPASSIVE_H
#define GODPASSIVE_H

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

int che_boost_level();
int che_boost(che_boost_type bt, int level = che_boost_level());
void che_handle_change(che_change_type ct, int diff);
void jiyva_eat_offlevel_items();

#endif
