/*
 *  File:       items.h
 *  Summary:    Misc (mostly) inventory related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <2>     6/9/99         DML             Autopickup
 *               <1>     -/--/--        LRH             Created
 */


#ifndef ITEMS_H
#define ITEMS_H

#include "externs.h"

// Various ways to get the acquirement effect.
enum acquirement_agent_type
{
    AQ_SCROLL   = 0,
    
    // Empty space for the gods
    
    AQ_CARD_GENIE = 100,

    AQ_WIZMODE          = 200
};

bool is_valid_item( const item_def &item );

bool dec_inv_item_quantity( int obj, int amount );
bool dec_mitm_item_quantity( int obj, int amount );

void inc_inv_item_quantity( int obj, int amount );
void inc_mitm_item_quantity( int obj, int amount );

bool move_item_to_grid( int *const obj, int x, int y );
void move_item_stack_to_grid( int x, int y, int targ_x, int targ_y );
int  move_item_to_player( int obj, int quant_got, bool quiet = false );
void mark_items_non_pickup_at(const coord_def &pos);
bool is_stackable_item( const item_def &item );
bool items_stack( const item_def &item1, const item_def &item2,
                  bool force = false );

item_def find_item_type(object_class_type base_type, std::string name);

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

// last updated: 16oct2001 -- bwr
int get_item_slot( int reserve = 50 );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - files - food - items - misc - monstuff -
 *              religion - spells2 - spells3 - spells4
 * *********************************************************************** */
void unlink_item(int dest);
void destroy_item( item_def &item, bool never_created = false );
void destroy_item(int dest, bool never_created = false);
void destroy_item_stack( int x, int y, int cause = -1 );
void lose_item_stack( int x, int y );

// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void item_check(bool verbose);
void request_autopickup(bool do_pickup = true);

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
                        int quant_drop = -1,    // item.quantity by default
                        bool mark_dropped = false);

// last updated Oct 15, 2000 -- bwr
/* ***********************************************************************
 * called from: spells4.cc
 * *********************************************************************** */
bool move_top_item( const coord_def &src, const coord_def &dest );

// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void drop(void);

// last updated: 08jun2000 {dlb}
/* ***********************************************************************
 * called from: command food item_use shopping spl-book transfor
 * *********************************************************************** */
int inv_count(void);

bool pickup_single_item(int link, int qty);

bool drop_item( int item_dropped, int quant_drop, bool try_offer = false );

int get_equip_slot(const item_def *item);

void origin_set(int x, int y);
void origin_set_monster(item_def &item, const monsters *monster);
bool origin_known(const item_def &item);
bool origin_describable(const item_def &item);
std::string origin_desc(const item_def &item);
void origin_purchased(item_def &item);
void origin_acquired(item_def &item, int agent);
void origin_set_startequip(item_def &item);
void origin_set_unknown(item_def &item);
void origin_set_inventory( void (*oset)(item_def &item) );

bool item_needs_autopickup(const item_def &);
bool can_autopickup();

bool need_to_autopickup();
void autopickup();

int find_free_slot(const item_def &i);
bool is_rune(const item_def &item);
bool is_unique_rune(const item_def &item);

bool need_to_autoinscribe();
void request_autoinscribe(bool do_inscribe = true);
void autoinscribe();

bool item_is_equipped(const item_def &item);

void item_was_lost(const item_def &item);
void item_was_destroyed(const item_def &item, int cause = -1);

#endif
