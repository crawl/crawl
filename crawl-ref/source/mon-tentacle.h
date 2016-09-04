/**
 * @file
 * @brief Monster tentacle-related code.
**/

#ifndef MONTENTACLE_H
#define MONTENTACLE_H

bool mons_is_tentacle_head(monster_type mc);
bool mons_is_child_tentacle(monster_type mc);
bool mons_is_child_tentacle_segment(monster_type mc);
bool mons_is_solo_tentacle(monster_type mc);
bool mons_is_tentacle(monster_type mc);
bool mons_is_tentacle_segment(monster_type mc);
bool mons_is_tentacle_or_tentacle_segment(monster_type mc);

monster_type mons_tentacle_parent_type(const monster* mons);
monster_type mons_tentacle_child_type(const monster* mons);

bool mons_tentacle_adjacent(const monster* parent, const monster* child);
const monster& get_tentacle_head(const monster& mon);

void move_solo_tentacle(monster* tentacle);
void move_child_tentacles(monster * kraken);
bool destroy_tentacles(monster* head);
bool destroy_tentacle(monster* head);

int mons_available_tentacles(monster* head);
void mons_create_tentacles(monster* head);

#endif
