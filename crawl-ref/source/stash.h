/**
 * @file
 * @brief Classes tracking player stashes
**/

#ifndef STASH_H
#define STASH_H

#include <map>
#include <string>
#include <vector>

#include "shopping.h"

class input_history;
class reader;
class writer;
class StashMenu;

struct stash_search_result;
class Stash
{
public:
    Stash(coord_def pos_ = coord_def());
    Stash(const Stash &other) { *this = other; };

    static bool is_boring_feature(dungeon_feature_type feat);

    static string stash_item_name(const item_def &item);
    void update();
    bool unmark_trapping_nets();
    void save(writer&) const;
    void load(reader&);

    string description() const;
    string feature_description() const;
    vector<item_def> get_items() const;

    // Returns true if this Stash contains items that are eligible for
    // autopickup.
    bool pickup_eligible() const;

    // Returns true if this Stash contains items that can be sacrificed
    bool sacrificeable() const;

    // Returns true if this Stash contain items not handled by autopickup and
    // auto_sacrifce
    bool needs_stop() const;

    // Returns true if this Stash is unverified (a visit by the character will
    // verify the stash).
    bool unverified() const;

    vector<stash_search_result> matches_search(
        const string &prefix, const base_pattern &search) const;

    void write(FILE *f, coord_def refpos, string place = "",
               bool identify = false) const;

    bool empty() const
    {
        return items.empty() && feat == DNGN_FLOOR;
    }

    bool is_verified() const {  return verified; }

private:
    void _update_corpses(int rot_time);
    void _update_identification();
    void add_item(const item_def &item, bool add_to_front = false);

private:
    bool verified;      // Is this correct to the best of our knowledge?
    coord_def pos;
    dungeon_feature_type feat;
    string feat_desc; // Only for interesting features.
    trap_type trap;

    vector<item_def> items;

    static bool are_items_same(const item_def &, const item_def &,
                               bool exact = false);

    friend class LevelStashes;
    friend class ST_ItemIterator;
};

class ShopInfo
{
public:
    ShopInfo(coord_def pos_);

    vector<stash_search_result> matches_search(
        const string &prefix, const base_pattern &search) const;

    string description() const;

    void save(writer&) const;
    void load(reader&);

    bool show_menu(const level_pos &place, bool can_travel) const;
    bool is_visited() const { return items.size() || visited; }

    void write(FILE *f, bool identify = false) const;

    void reset() { items.clear(); visited = true; }
    void set_name(const string& s) { name = s; }

    void add_item(const item_def &item, unsigned price);

    // Messy!
    struct shop_item
    {
        item_def item;
        unsigned price;
    };

    bool is_at(coord_def other) const { return pos == other; }

private:
    coord_def pos;
    string name;

    int shoptype;

    // Set true if the player has visited this shop
    bool visited;

    vector<shop_item> items;

    string shop_item_name(const shop_item &si) const;
    string shop_item_desc(const shop_item &si) const;
    void describe_shop_item(const shop_item &si) const;
    void fill_out_menu(StashMenu &menu, const level_pos &place) const;

    friend class ST_ItemIterator;
};

struct stash_search_result
{
    // Where's this thingummy of interest.
    level_pos pos;

    // Number of levels the player must cross to reach the level the stash/shop
    // is on.
    int player_distance;

    // Match for this search result - usually the first matching item in the
    // stash or the name of the shop.
    pattern_match match;

    // Item that was matched.
    item_def item;

    // The shop in question, if this is result is for a shop name.
    const ShopInfo *shop;

    // Whether the found items are in the player's inventory.
    bool in_inventory;

    stash_search_result() : pos(), player_distance(0),
                            match(pattern_match::failed()), item(),
                            shop(nullptr), in_inventory(false)
    {
    }

    stash_search_result(const stash_search_result &o)
        : pos(o.pos), player_distance(o.player_distance), match(o.match),
          item(o.item), shop(o.shop), in_inventory(o.in_inventory)
    {
    }

    stash_search_result &operator = (const stash_search_result &o)
    {
        pos = o.pos;
        player_distance = o.player_distance;
        match = o.match;
        item = o.item;
        shop = o.shop;
        in_inventory = o.in_inventory;
        return *this;
    }

    bool operator < (const stash_search_result &ssr) const
    {
        return player_distance < ssr.player_distance;
    }

    bool show_menu() const;
};

class LevelStashes
{
public:
    LevelStashes();

    const Stash *find_stash(coord_def c) const;
    Stash *find_stash(coord_def c);
    const ShopInfo *find_shop(const coord_def& c) const;
    ShopInfo &get_shop(const coord_def& c);

    level_id where() const;

    void get_matching_stashes(const base_pattern &search,
                              vector<stash_search_result> &results) const;

    // Update stash at (x,y).
    bool  update_stash(const coord_def& c);

    // Mark nets at (x,y) as no longer trapping an actor.
    bool unmark_trapping_nets(const coord_def &c);

    // Returns true if the square at c contains potentially interesting
    // swag that merits a personal visit (for EXPLORE_GREEDY).
    bool  needs_visit(const coord_def& c, bool autopickup, bool sacrifice) const;
    bool  shop_needs_visit(const coord_def& c) const;

    // Returns true if the items at c are not fully known to the stash-tracker
    // and the items are not all handled by autopickup/auto_sacrifice.
    bool  needs_stop(const coord_def &c) const;

    // Returns true if the items at c contains at least one that can be
    // sacrificed
    bool sacrificeable(const coord_def &c) const;

    void  add_stash(coord_def p);

    void  kill_stash(const Stash &s);
    void  move_stash(const coord_def& from, const coord_def& to);

    void  save(writer&) const;
    void  load(reader&);

    void  write(FILE *f, bool identify = false) const;
    string level_name() const;
    string short_level_name() const;

    bool  has_stashes() const { return m_stashes.size() + m_shops.size() > 0; }

    bool  is_current() const;

    void  remove_shop(const coord_def& c);
private:
    void _update_corpses(int rot_time);
    void _update_identification();
    void _waypoint_search(int n, vector<stash_search_result> &results) const;

    typedef map<coord_def, Stash> stashes_t;
    typedef vector<ShopInfo> shops_t;

    // which level
    level_id m_place;
    stashes_t m_stashes;
    shops_t m_shops;

    friend class StashTracker;
    friend class ST_ItemIterator;
};

class StashTracker
{
public:
    StashTracker() : levels(), last_corpse_update(0)
    {
    }

    void search_stashes();

    LevelStashes &get_current_level();
    LevelStashes *find_current_level();
    LevelStashes *find_level(const level_id &pos);

    ShopInfo &get_shop(const coord_def& c)
    {
        return get_current_level().get_shop(c);
    }

    void remove_level(const level_id &which = level_id::current());

    void update_corpses();
    void update_identification();

    void update_visible_stashes();

    // Update stash at (x,y) on current level, returning true if a stash was
    // updated.
    bool update_stash(const coord_def& c);
    void move_stash(const coord_def& from, const coord_def& to);

    // Mark nets at (x,y) on current level as no longer trapping an actor.
    bool unmark_trapping_nets(const coord_def &c);

    void  add_stash(coord_def p);

    void save(writer&) const;
    void load(reader&);

    void write(FILE *f, bool identify = false) const;

    void dump(const char *filename, bool identify = false) const;

    void remove_shop(const level_pos &pos);
private:
    void get_matching_stashes(const base_pattern &search,
                              vector<stash_search_result> &results,
                              bool curr_lev = false) const;
    bool display_search_results(vector<stash_search_result> &results,
                                bool& sort_by_dist,
                                bool& filter_useless,
                                bool& default_execute);
    string stash_search_prompt();

private:
    typedef map<level_id, LevelStashes> stash_levels_t;
    stash_levels_t levels;

    int last_corpse_update;

    friend class ST_ItemIterator;
};

class ST_ItemIterator
{
public:
    ST_ItemIterator();

    const ST_ItemIterator& operator ++ ();
    ST_ItemIterator operator ++ (int);

    operator bool() const;
    const item_def& operator *() const;
    const item_def* operator->() const;

    const level_id  &place();
    const ShopInfo*  shop();
    unsigned         price();

private:
    level_id        m_place;
    const item_def* m_item;
    unsigned        m_price;
    const ShopInfo* m_shop;

    StashTracker::stash_levels_t::const_iterator m_stash_level_it;
    LevelStashes::stashes_t::const_iterator      m_stash_it;
    LevelStashes::shops_t::const_iterator        m_shop_it;
    vector<item_def>::const_iterator             m_stash_item_it;

    vector<ShopInfo::shop_item>::const_iterator  m_shop_item_it;

private:
    void new_level();
};

extern StashTracker StashTrack;

void maybe_update_stashes();
bool is_stash(const coord_def& c);
string get_stash_desc(const coord_def& c);
void describe_stash(const coord_def& c);

vector<item_def> item_list_in_stash(const coord_def& pos);

string userdef_annotate_item(const char *s, const item_def *item,
                             bool exclusive = false);
string stash_annotate_item(const char *s, const item_def *item,
                           bool exclusive = false);

#define STASH_LUA_SEARCH_ANNOTATE "ch_stash_search_annotate_item"
#define STASH_LUA_DUMP_ANNOTATE   "ch_stash_dump_annotate_item"

#endif
