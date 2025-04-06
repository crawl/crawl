/**
 * @file
 * @brief Shop keeper functions.
**/

#pragma once

#include <unordered_set>
#include <vector>

#include "tag-version.h"

using std::vector;

struct shop_struct;

int artefact_value(const item_def &item);

// ident == true overrides the item ident level and gives the price
// as if the item was fully id'd
unsigned int item_value(item_def item, bool ident = false);
// price of an item if it were being sold in a given shop
int item_price(const item_def& item, const shop_struct& shop);
// Return true if an item is classified as a worthless consumable.
// Note that this does not take into account the player's condition:
// curse scrolls are worthless for everyone, most potions aren't worthless
// for mummies, etcetera.
bool is_worthless_consumable(const item_def &item);

void shop();
void shop(shop_struct& shop, const level_pos& pos);

shop_struct *shop_at(const coord_def& where);

void destroy_shop_at(coord_def p);

string shop_name(const shop_struct& shop);
string shop_type_name(shop_type type);

bool shoptype_identifies_stock(shop_type type);

bool is_shop_item(const item_def &item);
bool shop_item_unknown(const item_def &item);
bool have_voucher();

shop_type str_to_shoptype(const string &s);
const char *shoptype_to_str(shop_type type);

/////////////////////////////////////////////////////////////////////

struct level_pos;
class  Menu;

struct shop_struct
{
    coord_def           pos;
    uint8_t             greed;
    shop_type           type;
    uint8_t             level;
    string              shop_name;
    string              shop_type_name;
    string              shop_suffix_name;

    FixedVector<uint8_t, 3> keeper_name;

    vector<item_def> stock;
#if TAG_MAJOR_VERSION == 34
    uint8_t num; // used in a save compat hack
#endif

    shop_struct () : pos(), greed(0), type(SHOP_UNASSIGNED), level(0),
                     shop_name(""), shop_type_name(""), shop_suffix_name("") { }

    bool defined() const { return type != SHOP_UNASSIGNED; }
};

typedef pair<string, int> shoplist_entry;
class ShoppingList
{
public:
    ShoppingList();

    bool add_thing(const item_def &item, int cost,
                   const level_pos* pos = nullptr);

    bool is_on_list(const item_def &item, const level_pos* pos = nullptr) const;
    bool is_on_list(string desc, const level_pos* pos = nullptr) const;

    bool del_thing(const item_def &item, const level_pos* pos = nullptr);
    bool del_thing(string desc, const level_pos* pos = nullptr);

    void del_things_from(const level_id &lid);

    void item_type_identified(object_class_type base_type, int sub_type);
    void spells_added_to_library(const vector<spell_type>& spells, bool quiet);
    bool cull_identical_items(const item_def& item, int cost = -1);
    void remove_dead_shops();

    void gold_changed(int old_amount, int new_amount);

    void move_things(const coord_def &src, const coord_def &dst);
    void forget_pos(const level_pos &pos);

    void display(bool view_only = false);

    void refresh();

    bool empty() const { return !list || list->empty(); };
    int size() const;

    vector<shoplist_entry> entries();

    static bool items_are_same(const item_def& item_a,
                               const item_def& item_b);

    void do_excursion_work();

private:
    // An alias for you.props[SHOPPING_LIST_KEY], kept in sync by refresh()
    CrawlVector* list;

    int min_unbuyable_cost;
    int min_unbuyable_idx;
    int max_buyable_cost;
    int max_buyable_idx;

    vector<pair<object_class_type, int>> need_excursions;

private:
    unordered_set<int> find_thing(const item_def &item, const level_pos &pos) const;
    unordered_set<int> find_thing(const string &desc, const level_pos &pos) const;
    void del_thing_at_index(int idx);
    template <typename C> void del_thing_at_indices(C const &idxs);


    void fill_out_menu(Menu& shopmenu);

public:
    // what is this nonsense
    static       bool        thing_is_item(const CrawlHashTable& thing);
    static const item_def&   get_thing_item(const CrawlHashTable& thing);
    static       string get_thing_desc(const CrawlHashTable& thing);

    static int       thing_cost(const CrawlHashTable& thing);
    static level_pos thing_pos(const CrawlHashTable& thing);
    static string    describe_thing_pos(const CrawlHashTable& thing);

    static string name_thing(const CrawlHashTable& thing,
                             description_level_type descrip = DESC_PLAIN);
    static string describe_thing(const CrawlHashTable& thing,
                                 description_level_type descrip = DESC_PLAIN);
    static string item_name_simple(const item_def& item, bool ident = false);
};

extern ShoppingList shopping_list;

#if TAG_MAJOR_VERSION == 34
#define REMOVED_DEAD_SHOPS_KEY "removed_dead_shops"
#endif
