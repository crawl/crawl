/*
 *  File:       traps.h
 *  Summary:    Traps related functions.
 *  Written by: Linley Henzell
 */

#ifndef TRAPS_H
#define TRAPS_H

#include "enum.h"
#include "externs.h"

struct bolt;
class  monsters;
struct trap_def;

void disarm_trap(const coord_def& where);
void remove_net_from( monsters *mon );
void free_self_from_net(void);

void handle_traps(trap_type trt, int i, bool trap_known);
int get_trapping_net(const coord_def& where, bool trapped = true);
void mark_net_trapping(const coord_def& where);
void monster_caught_in_net(monsters *mon, bolt &pbolt);
bool player_caught_in_net();
void clear_trapping_net();
void check_net_will_hold_monster(monsters *mon);

dungeon_feature_type trap_category(trap_type type);

int reveal_traps(const int range);
void destroy_trap(const coord_def& pos);
trap_def* find_trap(const coord_def& where);
trap_type get_trap_type(const coord_def& where);

bool     is_valid_shaft_level(const level_id &place = level_id::current());
bool     shaft_known(int depth, bool randomly_placed);
level_id generic_shaft_dest(coord_def pos, bool known);
void     handle_items_on_shaft(const coord_def& where, bool open_shaft);

int       num_traps_for_place(int level_number = -1,
                              const level_id &place = level_id::current());
trap_type random_trap_for_place(int level_number = -1,
                                const level_id &place = level_id::current());

trap_type random_trap_slime(int level_number = -1);

int traps_zero_number(int level_number = -1);

int       traps_pan_number(int level_number = -1);
trap_type traps_pan_type(int level_number = -1);

int       traps_abyss_number(int level_number = -1);
trap_type traps_abyss_type(int level_number = -1);

int       traps_lab_number(int level_number = -1);
trap_type traps_lab_type(int level_number = -1);

int count_traps(trap_type ttyp);

#endif
