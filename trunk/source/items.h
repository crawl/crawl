/*
 *  File:       items.cc
 *  Summary:    Misc (mostly) inventory related functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <2>     6/9/99         DML             Autopickup
 *               <1>     -/--/--        LRH             Created
 */


#ifndef ITEMS_H
#define ITEMS_H

#include "externs.h"

// used in acr.cc {dlb}:
extern int autopickup_on;

// used in initfile.cc {dlb}:
extern long autopickups;

bool is_valid_item( const item_def &item );

bool dec_inv_item_quantity( int obj, int amount );
bool dec_mitm_item_quantity( int obj, int amount );

void inc_inv_item_quantity( int obj, int amount );
void inc_mitm_item_quantity( int obj, int amount );

void move_item_to_grid( int *const obj, int x, int y );
void move_item_stack_to_grid( int x, int y, int targ_x, int targ_y );
int  move_item_to_player( int obj, int quant_got, bool quiet = false );
bool items_stack( const item_def &item1, const item_def &item2 );

void init_item( int item );

// last updated 13mar2001 {gdl}
/* ***********************************************************************
 * called from: dungeon files
 * *********************************************************************** */
void link_items(void);

// last updated 13mar2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void fix_item_coordinates(void);

// last updated: 19apr2001 {gdl}
/* ***********************************************************************
 * called from: dungeon
 * *********************************************************************** */
int cull_items(void);

// last updated: 16oct2001 -- bwr
int get_item_slot( int reserve = 50 );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - files - food - items - misc - monstuff -
 *              religion - spells2 - spells3 - spells4
 * *********************************************************************** */
void unlink_item(int dest);
void destroy_item(int dest);
void destroy_item_stack( int x, int y );

// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void item_check(char keyin);


// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void pickup(void);


// last updated 08jun2000 {dlb}
/* ***********************************************************************
 * called from: beam - items - transfor
 * *********************************************************************** */
bool copy_item_to_grid( const item_def &item, int x_plos, int y_plos, 
                        int quant_drop = -1 ); // item.quantity by default

// last updated Oct 15, 2000 -- bwr
/* ***********************************************************************
 * called from: spells4.cc
 * *********************************************************************** */
bool move_top_item( int src_x, int src_y, int dest_x, int dest_y );

// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void drop(void);


// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: files - items
 * *********************************************************************** */
void update_corpses(double elapsedTime);
void update_level(double elapsedTime);


// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void handle_time( long time_delta );

// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: command food item_use shopping spl-book transfor
 * *********************************************************************** */
int inv_count(void);

void cmd_destroy_item( void );

#endif
