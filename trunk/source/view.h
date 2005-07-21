/*
 *  File:       view.cc
 *  Summary:    Misc function used to render the dungeon.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *     <2>     9/29/99     BCR     Added the BORDER_COLOR define
 *     <1>     -/--/--     LRH     Created
 */


#ifndef VIEW_H
#define VIEW_H


#include "externs.h"


#define BORDER_COLOR BROWN

int get_number_of_lines(void);

// last updated 29may2000 {dlb}
/* ***********************************************************************
 * called from: bang - beam - direct - effects - fight - monstuff -
 *              mstuff2 - spells1 - spells2
 * *********************************************************************** */
bool mons_near(struct monsters *monster, unsigned int foe = MHITYOU);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - player - stuff
 * *********************************************************************** */
void draw_border(void);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - view
 * *********************************************************************** */
void item(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: direct - monstufff - view
 * *********************************************************************** */
void losight(FixedArray<unsigned int, 19, 19>& sh, FixedArray<unsigned char, 80, 70>& gr, int x_p, int y_p);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - acr - it_use3 - item_use - spell
 * *********************************************************************** */
void magic_mapping(int map_radius, int proportion);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - effects - it_use2 - it_use3 - item_use - spell -
 *              spells - spells3 - spells4
 * *********************************************************************** */
void noisy( int loudness, int nois_x, int nois_y );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - spells3
 * *********************************************************************** */
void show_map( FixedVector<int, 2>& spec_place );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void viewwindow2(char draw_it, bool do_updates);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void viewwindow3(char draw_it, bool do_updates);        // non-IBM graphics

// last updated 19jun2000 (gdl)
/* ***********************************************************************
 * called from: acr view
 * *********************************************************************** */
void setLOSRadius(int newLR);

// last updated 02apr2001 (gdl)
/* ***********************************************************************
 * called from: view monstuff
 * *********************************************************************** */
bool check_awaken(int mons_aw);


#endif
