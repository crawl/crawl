/*
 *  File:       maps.cc
 *  Summary:    Functions used to create vaults.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MAPS_H
#define MAPS_H

#include "FixVec.h"


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon
 * *********************************************************************** */
char vault_main(char vgrid[81][81], FixedVector<int, 7>& mons_array, int vault_force, int many_many);


#endif
