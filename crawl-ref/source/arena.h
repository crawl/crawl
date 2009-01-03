#ifndef ARENA_H
#define ARENA_H

#include "AppHdr.h"
#include "enum.h"

class level_id;
class monsters;
class mgen_data;

void run_arena();

monster_type arena_pick_random_monster(const level_id &place, int power,
                                       int &lev_mons);

bool arena_veto_random_monster(monster_type type);

void arena_placed_monster(monsters *monster, const mgen_data &mg,
                          bool first_band_member);

void arena_monster_died(monsters *monster, killer_type killer,
                        int killer_index, bool silent);
#endif
