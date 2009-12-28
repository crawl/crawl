/*  File:       mislead.h
 *  Summary:    Handling of the Mislead spell and stats
 */

bool unsuitable_misled_monster(monster_type mons);
monster_type get_misled_monster (monsters *monster);
int update_mislead_monsters(monsters* caster);
bool update_mislead_monster(monsters* monster);
