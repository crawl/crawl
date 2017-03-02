/**
 * @file
 * @brief Misc (mostly) inventory related functions.
**/

#pragma once

#include "equipment-type.h"
#include "god-type.h"
#include "mon-inv-type.h"

// Ways to get items, other than finding them on the ground or looting them
// from slain monsters.
enum item_source_type
{
    IT_SRC_NONE   = 0,

    // Empty space for the gods

    AQ_SCROLL     = 100,
#if TAG_MAJOR_VERSION == 34
    AQ_CARD_GENIE,
#endif
    IT_SRC_START,
    IT_SRC_SHOP,

    // Empty space for new non-wizmode acquisition methods

    AQ_WIZMODE    = 200,
};

/// top-priority item override colour
#define FORCED_ITEM_COLOUR_KEY "forced_item_colour"

pair<bool, int> item_int(item_def &item);
item_def& item_from_int(bool, int);

int get_max_subtype(object_class_type base_type);
bool item_type_has_unidentified(object_class_type base_type);

bool dec_inv_item_quantity(int obj, int amount);
bool dec_mitm_item_quantity(int obj, int amount);

void inc_inv_item_quantity(int obj, int amount);
void inc_mitm_item_quantity(int obj, int amount);

bool move_item_to_grid(int *const obj, const coord_def& p,
                        bool silent = false);
void move_item_stack_to_grid(const coord_def& from, const coord_def& to);
void note_inscribe_item(item_def &item);
bool move_item_to_inv(item_def& item);
bool move_item_to_inv(int obj, int quant_got, bool quiet = false);
item_def* auto_assign_item_slot(item_def& item);
void mark_items_non_pickup_at(const coord_def &pos);
void clear_item_pickup_flags(item_def &item);
bool is_stackable_item(const item_def &item);
bool items_similar(const item_def &item1, const item_def &item2);
bool items_stack(const item_def &item1, const item_def &item2);
void merge_item_stacks(const item_def &source, item_def &dest, int quant = -1);
void get_gold(const item_def& item, int quant, bool quiet);

item_def *find_floor_item(object_class_type cls, int sub_type = -1);
int item_on_floor(const item_def &item, const coord_def& where);

void init_item(int item);

void add_held_books_to_library();

void link_items();

void fix_item_coordinates();

int get_mitm_slot(int reserve = 50);

void unlink_item(int dest);
void destroy_item(item_def &item, bool never_created = false);
void destroy_item(int dest, bool never_created = false);
void lose_item_stack(const coord_def& where);

void item_check();
void request_autopickup(bool do_pickup = true);
void id_floor_items();

bool player_on_single_stack();
void pickup_menu(int item_link);
void pickup(bool partial_quantity = false);

bool item_is_branded(const item_def& item);
vector<const item_def*> item_list_on_square(int obj);

bool copy_item_to_grid(item_def &item, const coord_def& p,
                       int quant_drop = -1,    // item.quantity by default
                       bool mark_dropped = false,
                       bool silent = false);
coord_def item_pos(const item_def &item);

bool move_top_item(const coord_def &src, const coord_def &dest);

// Get the top item in a given cell. If there are no items, return nullptr.
const item_def* top_item_at(const coord_def& where);

// Returns whether there is more than one item in a given cell.
bool multiple_items_at(const coord_def& where);

void drop();

int inv_count();
int runes_in_pack();

bool pickup_single_item(int link, int qty);

bool drop_item(int item_dropped, int quant_drop);
void drop_last();

int          get_equip_slot(const item_def *item);
mon_inv_type get_mon_equip_slot(const monster* mon, const item_def &item);

void origin_reset(item_def &item);
void origin_set(const coord_def& where);
void origin_set_monster(item_def &item, const monster* mons);
bool origin_known(const item_def &item);
bool origin_describable(const item_def &item);
string origin_desc(const item_def &item);
void origin_purchased(item_def &item);
void origin_acquired(item_def &item, int agent);
void origin_set_startequip(item_def &item);
void origin_set_unknown(item_def &item);
god_type origin_as_god_gift(const item_def& item);
bool origin_is_acquirement(const item_def& item,
                           item_source_type *type = nullptr);

bool item_needs_autopickup(const item_def &, bool ignore_force = false);
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
void item_was_destroyed(const item_def &item);

bool get_item_by_name(item_def *item, const char* specs,
                      object_class_type class_wanted,
                      bool create_for_real = false);

bool get_item_by_exact_name(item_def &item, const char* name);

void move_items(const coord_def r, const coord_def p);
object_class_type get_random_item_mimic_type();

bool maybe_identify_base_type(item_def &item);
int count_movable_items(int obj);

// stack_iterator guarantees validity so long as you don't manually
// mess with item_def.link: i.e., you can kill the item you're
// examining but you can't kill the item linked to it.
class stack_iterator : public iterator<forward_iterator_tag, item_def>
{
public:
    explicit stack_iterator(const coord_def& pos, bool accessible = false);
    explicit stack_iterator(int start_link);

    operator bool() const;
    item_def& operator *() const;
    item_def* operator->() const;
    int index() const;

    const stack_iterator& operator ++ ();
    stack_iterator operator ++ (int);
private:
    int cur_link;
    int next_link;
};

class mon_inv_iterator : public iterator<forward_iterator_tag, item_def>
{
public:
    explicit mon_inv_iterator(monster& _mon);

    mon_inv_type slot() const
    {
        return type;
    }

    operator bool() const;
    item_def& operator *() const;
    item_def* operator->() const;

    mon_inv_iterator& operator ++ ();
    mon_inv_iterator operator ++ (int);
private:
    monster& mon;
    mon_inv_type type;
};
