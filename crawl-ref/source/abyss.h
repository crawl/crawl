/*
 *  File:       abyss.h
 *  Summary:    Misc functions (most of which don't appear to be related to priests).
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef ABYSS_H
#define ABYSS_H


// last updated 02apr2001 {gdl}
/* ***********************************************************************
 * called from: dungeon
 * *********************************************************************** */
void generate_abyss(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void area_shift(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: spells1 - spells3 - spells4
 * *********************************************************************** */
void abyss_teleport( bool new_area );


#endif
