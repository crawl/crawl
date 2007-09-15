/*
 *  File:       transfor.cc
 *  Summary:    Misc function related to player transformations.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
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

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - transfor
 * *********************************************************************** */
void untransform(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: item_use
 * *********************************************************************** */
bool can_equip(equipment_type use_which, bool ignore_temporary);
size_type transform_size(int psize = PSIZE_BODY);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
bool transform(int pow, transformation_type which_trans);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: mutation - transfor
 * *********************************************************************** */
bool remove_equipment( std::set<equipment_type> remove_stuff );
bool remove_one_equip(equipment_type eq);

bool transform_changed_physiology( bool phys_scales = false );

#endif
