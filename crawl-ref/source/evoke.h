/**
 * @file
 * @brief Functions for using some of the wackier inventory items.
**/


#ifndef EVOKE_H
#define EVOKE_H

void stop_studying_manual(bool finish = false);
void skill_manual(int slot);

void tome_of_power(int slot);

bool evoke_item(int slot = -1);

void shadow_lantern_effect();
bool disc_of_storms(bool drac_breath = false);

#endif
