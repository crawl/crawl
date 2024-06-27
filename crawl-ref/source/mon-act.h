/**
 * @file
 * @brief Monsters doing stuff (monsters acting).
**/

#pragma once

#include <map>

#include "coord-def.h"

using std::pair;

class monster;
struct bolt;

class MonsterActionQueueCompare
{
public:
    bool operator() (pair<monster*, int> m1, pair<monster*, int> m2)
    {
        return m1.second < m2.second;
    }
};

void mons_set_just_seen(monster *mon);
void mons_reset_just_seen();

bool mon_can_move_to_pos(const monster* mons, const coord_def& delta,
                         bool just_check = false);

bool handle_throw(monster* mons, bolt &beem, bool teleport, bool check_only);

void handle_monsters(bool with_noise = false);
void handle_monster_move(monster* mon);

void queue_monster_for_action(monster* mons);

void clear_monster_flags();
