/*
 *  File:       transform.h
 *  Summary:    Misc function related to player transformations.
 *  Written by: Linley Henzell
 */


#ifndef TRANSFOR_H
#define TRANSFOR_H

#include <set>

#include "enum.h"
#include "player.h"

bool form_can_wield(transformation_type trans = you.form);
bool form_can_fly(transformation_type trans = you.form);
bool form_can_swim(transformation_type trans = you.form);
bool form_likes_water(transformation_type trans = you.form);
bool form_can_butcher_barehanded(transformation_type trans = you.form);
bool form_changed_physiology(bool phys_scales = false,
                             transformation_type trans = you.form);
bool form_can_wear_item(const item_def& item,
                        transformation_type trans = you.form);

bool can_equip(equipment_type use_which, bool ignore_temporary);

bool transform(int pow, transformation_type which_trans, bool force = false,
               bool just_check = false);

// skip_move: don't make player re-enter current cell
void untransform(bool skip_wielding = false, bool skip_move = false);

size_type transform_size(int psize = PSIZE_BODY);

void remove_one_equip(equipment_type eq, bool meld = true,
                      bool mutation = false);
void unmeld_one_equip(equipment_type eq);

monster_type transform_mons();
std::string blade_parts(bool terse = false);
monster_type dragon_form_dragon_type();

void transformation_expiration_warning();

#endif
