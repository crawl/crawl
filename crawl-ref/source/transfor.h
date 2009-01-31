/*
 *  File:       transfor.h
 *  Summary:    Misc function related to player transformations.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef TRANSFOR_H
#define TRANSFOR_H

#include <set>

#include "FixVec.h"
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
    TRAN_SERPENT_OF_HELL,
    TRAN_AIR,
    TRAN_BAT,
    NUM_TRANSFORMATIONS                // must remain last member {dlb}
};

bool transform_can_butcher_barehanded(transformation_type tt);

void untransform(void);

bool can_equip(equipment_type use_which, bool ignore_temporary);
bool check_transformation_stat_loss(const std::set<equipment_type> &remove,
                                    bool quiet = false, int str_loss = 0,
                                    int dex_loss = 0, int int_loss = 0);
size_type transform_size(int psize = PSIZE_BODY);

bool transform(int pow, transformation_type which_trans, bool quiet = false);

bool remove_one_equip(equipment_type eq, bool meld = true);
bool unmeld_one_equip(equipment_type eq);

bool transform_changed_physiology( bool phys_scales = false );

#endif
