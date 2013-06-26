/**
 * @file
 * @brief Monsters doing stuff (monsters acting).
**/

#ifndef MONACT_H
#define MONACT_H

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

void handle_monsters(bool with_noise = false);
void handle_monster_move(monster* mon);

void queue_monster_for_action(monster* mons);

#define ENERGY_SUBMERGE(entry) (max(entry->energy_usage.swim / 2, 1))

#endif
