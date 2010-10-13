/*
 *  File:       items.h
 *  Summary:    Misc (mostly) inventory related functions.
 *  Written by: Linley Henzell
 */


#ifndef ITEMS_H
#define ITEMS_H

#include "externs.h"

// Ways to get items, other than finding them on the ground or looting them
// from slain monsters.
enum item_source_type
{
    IT_SRC_NONE   = 0,

    // Empty space for the gods

    AQ_SCROLL     = 100,
    AQ_CARD_GENIE,
    IT_SRC_START,
    IT_SRC_SHOP,

    // Empty space for new non-wizmode acquisition methods

    AQ_WIZMODE    = 200,
};

int get_max_subtype(object_class_type base_type);

bool dec_inv_item_quantity(int obj, int amount, bool suppress_burden = false);
bool dec_mitm_item_quantity(int obj, int amount);

void inc_inv_item_quantity(int obj, int amount, bool suppress_burden = false);
void inc_mitm_item_quantity(int obj, int amount);

bool move_item_to_grid( int *const obj, const coord_def& p,
                        bool silent = false );
void move_item_stack_to_grid( const coord_def& from, const coord_def& to );
void note_inscribe_item(item_def &item);
int  move_item_to_player(int obj, int quant_got, bool quiet = false,
                         bool ignore_burden = false);
void mark_items_non_pickup_at(const coord_def &pos);
bool is_stackable_item( const item_def &item );
bool items_similar( const item_def &item1, const item_def &item2,
                    bool ignore_ident = false );
bool items_stack( const item_def &item1, const item_def &item2,
                  bool force_merge = false, bool ignore_ident = false );
void merge_item_stacks(item_def &source, item_def &dest,
                       int quant = -1);

item_def find_item_type(object_class_type base_type, std::string name);
item_def *find_floor_item(object_class_type cls, int sub_type);
int item_on_floor(const item_def &item, const coord_def& where);

void init_item( int item );

void link_items(void);

void fix_item_coordinates(void);

int get_item_slot( int reserve = 50 );

void unlink_item(int dest);
void destroy_item( item_def &item, bool never_created = false );
void destroy_item(int dest, bool never_created = false);
void lose_item_stack( const coord_def& where );

void item_check(bool verbose);
void request_autopickup(bool do_pickup = true);

void pickup(void);

int item_name_specialness(const item_def& item);
void item_list_on_square( std::vector<const item_def*>& items,
                          int obj, bool force_squelch = false );

bool copy_item_to_grid( const item_def &item, const coord_def& p,
                        int quant_drop = -1,    // item.quantity by default
                        bool mark_dropped = false,
                        bool silent = false );

bool move_top_item( const coord_def &src, const coord_def &dest );

// Get the top item in a given cell. If allow_mimic_item is true and
// an unknown mimic is inhabiting the square, get that mimic's item.
// If there are no items, return NULL.
const item_def* top_item_at(const coord_def& where, bool allow_mimic_item);
item_def *corpse_at(coord_def pos, int *num_corpses = NULL);

// Returns whether there is more than one item in a given cell.
bool multiple_items_at(const coord_def& where, bool allow_mimic_item);

void drop(void);

int inv_count(void);

bool pickup_single_item(int link, int qty);

bool drop_item( int item_dropped, int quant_drop, bool try_offer = false );

int          get_equip_slot(const item_def *item);
mon_inv_type get_mon_equip_slot(const monster* mon, const item_def &item);

void origin_reset(item_def &item);
void origin_set(const coord_def& where);
void origin_set_monster(item_def &item, const monster* mons);
bool origin_known(const item_def &item);
bool origin_describable(const item_def &item);
std::string origin_desc(const item_def &item);
void origin_purchased(item_def &item);
void origin_acquired(item_def &item, int agent);
void origin_set_startequip(item_def &item);
void origin_set_unknown(item_def &item);
void origin_set_inventory( void (*oset)(item_def &item) );
bool origin_is_god_gift(const item_def& item, god_type *god = NULL);
bool origin_is_acquirement(const item_def& item,
                           item_source_type *type = NULL);
std::string origin_monster_name(const item_def &item);

bool item_needs_autopickup(const item_def &);
bool can_autopickup();

bool need_to_autopickup();
void autopickup();

int find_free_slot(const item_def &i);

bool need_to_autoinscribe();
void request_autoinscribe(bool do_inscribe = true);
void autoinscribe();

bool item_is_equipped(const item_def &item, bool quiver_too = false);
bool item_is_melded(const item_def& item);
equipment_type item_equip_slot(const item_def &item);

void item_was_lost(const item_def &item);
void item_was_destroyed(const item_def &item, int cause = -1);

bool get_item_by_name(item_def *item, char* specs,
                      object_class_type class_wanted,
                      bool create_for_real = false);

void move_items(const coord_def r, const coord_def p);
#endif
