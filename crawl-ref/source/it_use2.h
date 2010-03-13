/*
 *  File:       it_use2.h
 *  Summary:    Functions for using wands, potions, and weapon/armour removal.
 *  Written by: Linley Henzell
 */

#ifndef IT_USE2_H
#define IT_USE2_H


#include "externs.h"

// drank_it should be true for real potion effects (as opposed
// to abilities which duplicate such effects.)
bool potion_effect(potion_type pot_eff, int pow,
                   bool drank_it = false, bool was_known = true);

void unuse_artefact(const item_def &item, bool* show_msgs=NULL);
void unequip_armour_effect(item_def& item);
bool unwield_item(bool showMsgs = true);
void unequip_weapon_effect(item_def& item, bool msg);

#endif
