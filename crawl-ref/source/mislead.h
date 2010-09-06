/*  File:       mislead.h
 *  Summary:    Handling of Mara's Mislead spell and stats, plus fakes.
 */

bool unsuitable_misled_monster(monster_type mons);
monster_type get_misled_monster (monster* mons);
int update_mislead_monsters(monster* caster);
bool update_mislead_monster(monster* mons);
int count_mara_fakes();
