/**
 * @file
 * @brief Shop keeper functions.
**/


#ifndef SHOPPING_H
#define SHOPPING_H

#include "externs.h"

int artefact_value(const item_def &item);

// ident == true overrides the item ident level and gives the price
// as if the item was fully id'd
unsigned int item_value(item_def item, bool ident = false);
// Return true if an item is classified as a worthless consumable.
// Note that this does not take into account the player's condition:
// curse scrolls are worthless for everyone, most potions aren't worthless
// for mummies, etcetera.
bool is_worthless_consumable(const item_def &item);
void shop();

shop_struct *get_shop(const coord_def& where);

void destroy_shop_at(coord_def p);

string shop_name(const coord_def& where);
string shop_name(const coord_def& where, bool add_stop);

bool shoptype_identifies_stock(shop_type type);

bool is_shop_item(const item_def &item);
bool shop_item_unknown(const item_def &item);

/////////////////////////////////////////////////////////////////////

struct level_pos;
class  Menu;

class ShoppingList
{
public:
    ShoppingList();

    bool add_thing(const item_def &item, int cost,
                   const level_pos* pos = NULL);
    bool add_thing(string desc, string buy_verb, int cost,
                   const level_pos* pos = NULL);

    bool is_on_list(const item_def &item, const level_pos* pos = NULL) const;
    bool is_on_list(string desc, const level_pos* pos = NULL) const;

    bool del_thing(const item_def &item, const level_pos* pos = NULL);
    bool del_thing(string desc, const level_pos* pos = NULL);

    void del_things_from(const level_id &lid);

    void item_type_identified(object_class_type base_type, int sub_type);
    unsigned int cull_identical_items(const item_def& item, int cost = -1);

    void gold_changed(int old_amount, int new_amount);

    void move_things(const coord_def &src, const coord_def &dst);
    void forget_pos(const level_pos &pos);

    void display();

    void refresh();

    bool empty() const { return !list || list->empty(); };
    int size() const;

    static bool items_are_same(const item_def& item_a,
                               const item_def& item_b);

private:
    CrawlVector* list;

    int min_unbuyable_cost;
    int min_unbuyable_idx;
    int max_buyable_cost;
    int max_buyable_idx;

private:
    int find_thing(const item_def &item, const level_pos &pos) const;
    int find_thing(const string &desc, const level_pos &pos) const;
    void del_thing_at_index(int idx);

    void fill_out_menu(Menu& shopmenu);

    static       bool        thing_is_item(const CrawlHashTable& thing);
    static const item_def&   get_thing_item(const CrawlHashTable& thing);
    static       string get_thing_desc(const CrawlHashTable& thing);

    static int       thing_cost(const CrawlHashTable& thing);
    static level_pos thing_pos(const CrawlHashTable& thing);

    static string name_thing(const CrawlHashTable& thing,
                             description_level_type descrip = DESC_PLAIN);
    static string describe_thing(const CrawlHashTable& thing,
                                 description_level_type descrip = DESC_PLAIN);
    static string item_name_simple(const item_def& item, bool ident = false);
};

extern ShoppingList shopping_list;

#endif
