/**
 * @file
 * @brief Handling of Mara's Mislead spell and stats, plus fakes.
**/

bool unsuitable_misled_monster(monster_type mons);
void mons_cast_mislead(monster* mons);
bool update_mislead_monster(monster* mons);
int count_mara_fakes();
void end_mislead();
