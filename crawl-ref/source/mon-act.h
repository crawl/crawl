/**
 * @file
 * @brief Monsters doing stuff (monsters acting).
**/

#pragma once

struct bolt;

class MonsterActionQueueCompare
{
public:
    bool operator() (pair<monster*, int> m1, pair<monster*, int> m2)
    {
        return m1.second < m2.second;
    }
};

bool mon_can_move_to_pos(const monster* mons, const coord_def& delta,
                         bool just_check = false);
bool mons_can_move_towards_target(const monster* mon);
bool monster_swaps_places(monster* mon, const coord_def& delta,
                          bool takes_time = true, bool apply_effects = true);

bool handle_throw(monster* mons, bolt &beem, bool teleport, bool check_only);

void handle_monsters(bool with_noise = false);
void handle_monster_move(monster* mon);

void queue_monster_for_action(monster* mons);

#define ENERGY_SUBMERGE(entry) (max(entry->energy_usage.swim / 2, 1))
