/*
 *  File:       arena.h
 *  Summary:    Functions related to the monster arena (stage and watch fights).
 */

#ifndef ARENA_H
#define ARENA_H

#include "enum.h"

class level_id;
class monster;
struct mgen_data;

struct coord_def;

void run_arena(const std::string& teams);

monster_type arena_pick_random_monster(const level_id &place, int power,
                                       int &lev_mons);

bool arena_veto_random_monster(monster_type type);

bool arena_veto_place_monster(const mgen_data &mg, bool first_band_member,
                              const coord_def& pos);

void arena_placed_monster(monster* mons);

void arena_split_monster(monster* split_from, monster* split_to);

void arena_monster_died(monster* mons, killer_type killer,
                        int killer_index, bool silent, int corpse);

int arena_cull_items();
#endif
