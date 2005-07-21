/*
 *  File:       transfor.cc
 *  Summary:    Misc function related to player transformations.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef TRANSFOR_H
#define TRANSFOR_H

#include "FixVec.h"


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - transfor
 * *********************************************************************** */
void untransform(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: item_use
 * *********************************************************************** */
bool can_equip(char use_which);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
bool transform(int pow, char which_trans);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: mutation - transfor
 * *********************************************************************** */
bool remove_equipment( FixedVector<char, 8>& remove_stuff );


#endif
