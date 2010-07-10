/*
 *  File:       transform.h
 *  Summary:    Misc function related to player transformations.
 *  Written by: Linley Henzell
 */


#ifndef TRANSFOR_H
#define TRANSFOR_H

#include <set>

#include "enum.h"

enum transformation_type
{
    TRAN_NONE,                         //    0
    TRAN_SPIDER,
    TRAN_BLADE_HANDS,
    TRAN_STATUE,
    TRAN_ICE_BEAST,
    TRAN_DRAGON,                       //    5
    TRAN_LICH,
    TRAN_BAT,
    TRAN_PIG,
    NUM_TRANSFORMATIONS                // must remain last member {dlb}
};

bool transformation_can_wield(transformation_type trans);
bool transform_can_butcher_barehanded(transformation_type tt);

// skip_move: don't make player re-enter current cell
void untransform(bool skip_wielding = false, bool skip_move = false);

bool can_equip(equipment_type use_which, bool ignore_temporary);
size_type transform_size(int psize = PSIZE_BODY);

bool transform(int pow, transformation_type which_trans, bool force = false,
               bool just_check = false);

void remove_one_equip(equipment_type eq, bool meld = true,
                      bool mutation = false);
void unmeld_one_equip(equipment_type eq);

bool transform_changed_physiology( bool phys_scales = false );
bool transform_allows_wearing_item(const item_def& item,
                                   transformation_type transform);
monster_type transform_mons();

// Check your current transformation.
bool transform_allows_wearing_item(const item_def& item);
bool transform_allows_wielding(transformation_type transform);

void transformation_expiration_warning();
std::string transform_desc(bool expire_pre);

#endif
