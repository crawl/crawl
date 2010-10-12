/*
 *  File:       it_use3.h
 *  Summary:    Functions for using some of the wackier inventory items.
 *  Written by: Linley Henzell
 */


#ifndef IT_USE3_H
#define IT_USE3_H

void skill_manual(int slot);

void tome_of_power(int slot);

bool evoke_item(int slot = -1);

void noisy_equipment();
void shadow_lantern_effect();
void unrand_reacts();
bool _disc_of_storms(bool drac_breath);

#endif
