/*
 *  File:       overmap.cc
 *  Summary:    "Overmap" functionality
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>    --/--/--        LRH             Created
 */


#ifndef OVERMAP_H
#define OVERMAP_H


void seen_notable_thing( int which_thing );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: view
 * *********************************************************************** */
void seen_altar(unsigned char which_altar);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void init_overmap(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void display_overmap(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: view
 * *********************************************************************** */
void seen_staircase(unsigned char which_staircase);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: view
 * *********************************************************************** */
void seen_other_thing(unsigned char which_thing);


#endif
