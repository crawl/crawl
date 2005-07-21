/*
 *  File:       misc.cc
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MISC_H
#define MISC_H


#include "externs.h"

// last updated 08jan2001 {gdl}
/* ***********************************************************************
 * called from: ability - decks - fight - it_use2 - spells1
 * *********************************************************************** */
bool go_berserk(bool intentional);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void search_around(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void manage_clouds(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void disarm_trap(struct dist &disa);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - effects - spells3
 * *********************************************************************** */
void down_stairs(bool remove_stairs, int old_level);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void fall_into_a_pool(bool place, unsigned char grype);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc
 * *********************************************************************** */
void handle_traps(char trt, int i, bool trap_known);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void in_a_cloud(void);

// Created Sept 1, 2000 -- bwr
/* ***********************************************************************
 * called from: acr misc
 * *********************************************************************** */
void merfolk_start_swimming(void);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: misc - mstuff2
 * *********************************************************************** */
void itrap(struct bolt &pbolt, int trapped);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc - player - stuff
 * *********************************************************************** */
void new_level(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: delay
 * *********************************************************************** */
void turn_corpse_into_chunks( item_def &item );


// last updated 3jun2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc - mstuff2 - spells3
 * *********************************************************************** */
int trap_at_xy(int which_x, int which_y);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void up_stairs(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - effects
 * *********************************************************************** */
void weird_colours(unsigned char coll, char wc[30]);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3
 * *********************************************************************** */
void weird_writing(char stringy[40]);


// last updated 3jun2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc - mstuff2 - spells2 - spells3
 * *********************************************************************** */
unsigned char trap_category(unsigned char trap_type);


#endif
