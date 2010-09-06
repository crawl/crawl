/*
 * File:       mon-gear.h
 * Summary:    Monsters' initial equipment.
 */

#ifndef MON_GEAR_H
#define MON_GEAR_H


void give_item(int mid, int level_number, bool mons_summoned, bool spectral_orcs = false);
void give_weapon(int mid, int level_number, bool mons_summoned, bool spectral_orcs = false);
#endif
