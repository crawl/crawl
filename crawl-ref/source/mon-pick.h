/*
 *  File:       mon-pick.h
 *  Summary:    Functions used to help determine which monsters should appear.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MONPICK_H
#define MONPICK_H


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - fight
 * *********************************************************************** */
int mons_rarity(int mcls);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon
 * *********************************************************************** */
int mons_level(int mcls);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - mon-pick
 * *********************************************************************** */
bool mons_abyss(int mcls);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: dungeon - mon-pick
 * *********************************************************************** */
int mons_rare_abyss(int mcls);


// last updated 10jun2000 {dlb}
/* ***********************************************************************
 * called from: levels - mon-pick
 * *********************************************************************** */
bool mons_pan(int mcls);

// [ds] Why in the name of all that's holy are these in mon-pick.cc?
int branch_depth(int branch);
int branch_stair(int branch);

#endif
