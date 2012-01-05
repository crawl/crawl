/**
 * @file
 * @brief Monsters' initial equipment.
**/

#ifndef MON_GEAR_H
#define MON_GEAR_H

void give_specific_item(monster* mon, const item_def& tpl);
void give_item(monster *mon, int level_number,
               bool mons_summoned, bool spectral_orcs = false);
void give_weapon(monster *mon, int level_number, bool mons_summoned,
                 bool spectral_orcs = false);
#endif
