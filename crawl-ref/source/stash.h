/*
 *  File:       stash.h
 *  Summary:    Classes tracking player stashes
 *  Written by: Darshan Shaligram
 */

#ifndef STASH_H
#define STASH_H

#include "shopping.h"
#include <string>

#include <iostream>
#include <map>
#include <vector>

#include "externs.h"
#include "player.h"

class input_history;
class reader;
class writer;
class StashMenu;

// Stash definitions
void stash_init_new_level();

enum STASH_TRACK_MODES
{
    STM_NONE,             // Stashes are not tracked
    STM_EXPLICIT,         // Only explicitly marked stashes are tracked
    STM_DROPPED,          // Dropped items and explicitly marked stashes are
                          // tracked
    STM_ALL,              // All seen items are tracked
};

struct stash_search_result;
class Stash
{
public:
    Stash(int xp = -1, int yp = -1);

    static bool is_boring_feature(dungeon_feature_type feat);
    static void filter(object_class_type base_type, uint8_t sub_type);
    static void filter(const std::string &filt);

    static std::string stash_item_name(const item_def &item);
    void update();
    void save(writer&) const;
    void load(reader&);

    std::string description() const;
    std::string feature_description() const;
    std::vector<item_def> get_items() const;

    bool show_menu(const level_pos &place, bool can_travel) const;

    // Returns true if this Stash contains items that are eligible for
    // autopickup.
    bool pickup_eligible() const;

    // Returns true if this Stash is unverified (a visit by the character will
    // verify the stash).
    bool unverified() const;

    bool matches_search(const std::string &prefix,
                        const base_pattern &search,
                        stash_search_result &res)
            const;

    void write(std::ostream &os, int refx = 0, int refy = 0,
                 std::string place = "",
                 bool identify = false) const;

    bool empty() const
    {
        return enabled && items.empty() && feat == DNGN_FLOOR;
    }

    bool isAt(const coord_def& c) const { return c.x == x && c.y == y; }
    int  abs_pos() const { return abspos; }
    int  getX() const { return x; }
    int  getY() const { return y; }

    bool is_verified() const {  return verified; }

    bool enabled;       // If false, this Stash will neither track
                        // items nor dump itself. Disabled stashes are
                        // also never removed from the level's map of
                        // stashes.

public:
    static bool is_filtered(const item_def &item);

private:
    void _update_corpses(int rot_time);
    void add_item(const item_def &item, bool add_to_front = false);

private:
    bool verified;      // Is this correct to the best of our knowledge?
    uint8_t x, y;
    int  abspos;
    dungeon_feature_type feat;
    trap_type trap;

    std::vector<item_def> items;

    /*
     * If true (the default), the stash-tracker is a lot more likely to consider
     * squares that the player is not standing on as correctly-verified. This
     * will lead to cleaner dumps, but the chance of stashes not accurately
     * reflecting what's in the dungeon also increases.
     */
    static bool aggressive_verify;
    static std::vector<item_def> filters;

    static bool are_items_same(const item_def &, const item_def &);

    friend class LevelStashes;
    friend class ST_ItemIterator;
};

class ShopInfo
{
public:
    ShopInfo(int xp, int yp);

    bool matches_search(const std::string &prefix,
                        const base_pattern &search,
                        stash_search_result &res)
            const;

    std::string description() const;

    void save(writer&) const;
    void load(reader&);

    bool show_menu(const level_pos &place, bool can_travel) const;
    bool is_visited() const { return items.size() || visited; }

    void write(std::ostream &os, bool identify = false) const;

    void reset() { items.clear(); visited = true; }
    void set_name(const std::string& s) { name = s; }

    void add_item(const item_def &item, unsigned price);

    bool isAt(const coord_def& c) const { return x == c.x && y == c.y; }

    // Messy!
    struct shop_item
    {
        item_def item;
        unsigned price;
    };

private:
    int x, y;
    std::string name;

    int shoptype;

    // Set true if the player has visited this shop
    bool visited;

    std::vector<shop_item> items;

    std::string shop_item_name(const shop_item &si) const;
    std::string shop_item_desc(const shop_item &si) const;
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

    // Number of items in the stash that match the search.
    int matches;

    // Count of items stacks in the stash that matched. This will be the same as
    // matches if each matching stack's quantity is 1.
    int count;

    // Text that describes this search result - usually the name of
    // the first matching item in the stash or the name of the shop.
    std::string match;

    // The stash or shop in question. Both can be null if this is a feature.
    const Stash    *stash;
    const ShopInfo *shop;

    stash_search_result() : pos(), player_distance(0), matches(0),
                            count(0), match(), stash(NULL), shop(NULL)
    {
    }

    bool operator < (const stash_search_result &ssr) const
    {
        return (player_distance < ssr.player_distance
                   || (player_distance == ssr.player_distance
                       && matches > ssr.matches));
    }
};

extern std::ostream &operator << (std::ostream &, const Stash &);

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
                              std::vector<stash_search_result> &results) const;

    // Update stash at (x,y) on current level, defaulting to player's current
    // location if no parameters are supplied.
    bool  update_stash(const coord_def& c);

    // Returns true if the square at c contains potentially interesting
    // swag that merits a personal visit (for EXPLORE_GREEDY).
    bool  needs_visit(const coord_def& c) const;
    bool  shop_needs_visit(const coord_def& c) const;

    // Returns true if the items at c are not fully known to the stash-tracker
    // and can be updated if the character steps on the square.
    bool  unverified_stash(const coord_def &c) const;

    // Add stash at (x,y), or player's current location if no parameters are
    // supplied
    void  add_stash(int x = -1, int y = -1);

    // Mark square (x,y) as not stashworthy. The player's current location is
    // used if no parameters are supplied.
    void  no_stash(int x = -1, int y = -1);

    void  kill_stash(const Stash &s);

    void  save(writer&) const;
    void  load(reader&);

    void  write(std::ostream &os, bool identify = false) const;
    std::string level_name() const;
    std::string short_level_name() const;

    int   stash_count() const { return m_stashes.size() + m_shops.size(); }
    int   visible_stash_count() const { return _num_enabled_stashes() + m_shops.size(); }

    bool  is_current() const;

 private:
    int _num_enabled_stashes() const;
    void _update_corpses(int rot_time);

 private:
    typedef std::map<int, Stash>  stashes_t;
    typedef std::vector<ShopInfo> shops_t;

    // which level
    level_id m_place;
    stashes_t m_stashes;
    shops_t m_shops;

    friend class StashTracker;
    friend class ST_ItemIterator;
};

extern std::ostream &operator << (std::ostream &, const LevelStashes &);

class StashTracker
{
public:
    static bool is_level_untrackable()
    {
        return you.level_type == LEVEL_LABYRINTH
            || you.level_type == LEVEL_ABYSS;
    }

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

    enum stash_update_mode
    {
        ST_PASSIVE,             // Maintain existing stashes only.
        ST_AGGRESSIVE,          // Create stashes for each square containing
                                // objects, even if those squares were not
                                // previously marked as stashes.
    };

    void update_corpses();

    void update_visible_stashes(StashTracker::stash_update_mode = ST_PASSIVE);

    // Update stash at (x,y) on current level, defaulting to player's current
    // location if no parameters are supplied, return true if a stash was
    // updated.
    bool update_stash(const coord_def& c);

    // Add stash at (x,y), or player's current location if no parameters are
    // supplied.
    void add_stash(int x = -1, int y = -1, bool verbose = false);

    // Mark square (x,y) as not stashworthy. The player's current location is
    // used if no parameters are supplied.
    void no_stash(int x = -1, int y = -1);

    void save(writer&) const;
    void load(reader&);

    void write(std::ostream &os, bool identify = false) const;

    void dump(const char *filename, bool identify = false) const;

private:
    void get_matching_stashes(const base_pattern &search,
                              std::vector<stash_search_result> &results) const;
    bool display_search_results(std::vector<stash_search_result> &results,
                                const char* sort_style);
    std::string stash_search_prompt();

private:
    typedef std::map<level_id, LevelStashes> stash_levels_t;
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
          level_id  m_place;
    const item_def* m_item;
          unsigned  m_price;
    const ShopInfo* m_shop;

    StashTracker::stash_levels_t::const_iterator m_stash_level_it;
    LevelStashes::stashes_t::const_iterator      m_stash_it;
    LevelStashes::shops_t::const_iterator        m_shop_it;
    std::vector<item_def>::const_iterator        m_stash_item_it;

    std::vector<ShopInfo::shop_item>::const_iterator m_shop_item_it;

private:
    void new_level();
};

extern StashTracker StashTrack;

void maybe_update_stashes();
bool is_stash(const coord_def& c);
std::string get_stash_desc(const coord_def& c);
void describe_stash(const coord_def& c);

std::vector<item_def> item_list_in_stash(const coord_def& pos);

std::string userdef_annotate_item(const char *s, const item_def *item,
                                  bool exclusive = false);

#define STASH_LUA_SEARCH_ANNOTATE "ch_stash_search_annotate_item"
#define STASH_LUA_DUMP_ANNOTATE   "ch_stash_dump_annotate_item"
#define STASH_LUA_VIEW_ANNOTATE   "ch_stash_view_annotate_item"

#endif
