/**
 * @file
 * @brief Functions for corpse butchering & bottling.
 **/

#ifndef BUTCHER_H
#define BUTCHER_H

#define NEVER_HIDE_KEY "never_hide"

bool butchery(int which_corpse = -1);

void maybe_drop_monster_hide(const item_def corpse);
int get_max_corpse_chunks(monster_type mons_class);
bool turn_corpse_into_skeleton(item_def &item);
void turn_corpse_into_chunks(item_def &item, bool bloodspatter = true,
                             bool make_hide = true);
void butcher_corpse(item_def &item, maybe_bool skeleton = MB_MAYBE,
                    bool chunks = true);
bool can_bottle_blood_from_corpse(monster_type mons_class);
int num_blood_potions_from_corpse(monster_type mons_class);
void turn_corpse_into_blood_potions(item_def &item);
void turn_corpse_into_skeleton_and_blood_potions(item_def &item);

#endif
