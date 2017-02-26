/**
 * @file
 * @brief Definitions for common structs.
**/

#pragma once

#define __STDC_FORMAT_MACROS
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "bitary.h"
#include "deck-rarity-type.h"
#include "description-level-type.h"
#include "dungeon-feature-type.h"
#include "enum.h"
#include "spell-type.h"
#include "monster-type.h"
#include "branch-type.h"
#include "fixedarray.h"
#include "kill-category.h"
#include "killer-type.h"
#include "map-marker-type.h"
#include "menu-type.h"

#include "object-class-type.h"
#include "pattern.h"
#include "skill-type.h"
#include "shop-type.h"
#include "cloud-type.h"
#include "store.h"
#include "tiledef_defines.h"

struct tile_flavour
{
    unsigned short floor_idx;
    unsigned short wall_idx;
    unsigned short feat_idx;

    unsigned short floor;
    unsigned short wall;
    // Used (primarily) by the vault 'TILE' overlay.
    unsigned short feat;

    // Used as a random value or for special cases e.g. (bazaars, gates).
    unsigned short special;

    tile_flavour(): floor_idx(0), wall_idx(0), feat_idx(0),
                    floor(0), wall(0), feat(0), special(0) {}
};

// A glorified unsigned int that assists with ref-counting the mcache.
class tile_fg_store
{
public:
    tile_fg_store() : m_tile(0) {}
    tile_fg_store(tileidx_t tile) : m_tile(tile) {}
    operator tileidx_t() { return m_tile; }
    tileidx_t operator=(tileidx_t tile);
protected:
    tileidx_t m_tile;
};

#define MAX_NAME_LENGTH 30

typedef FixedArray<dungeon_feature_type, GXM, GYM> feature_grid;
typedef FixedArray<unsigned int, GXM, GYM> map_mask;
typedef FixedBitArray<GXM, GYM> map_bitmask;

struct item_def;
struct coord_def;
class level_id;
class map_marker;
class actor;
class player;
class monster;
class ghost_demon;

typedef pair<coord_def, int> coord_weight;

// Constexpr sign.
template <typename Z> static constexpr Z sgn(Z x)
{
    return x < 0 ? -1 : (x > 0 ? 1 : 0);
}

// Constexpr absolute value.
template <typename Z> static constexpr Z abs_ce(Z x)
{
    return x < 0 ? -x : x;
}

struct coord_def
{
    int         x;
    int         y;

    constexpr coord_def(int x_in, int y_in) : x(x_in), y(y_in) { }
    constexpr coord_def() : coord_def(0,0) { }

    void set(int xi, int yi)
    {
        x = xi;
        y = yi;
    }

    void reset()
    {
        set(0, 0);
    }

    constexpr int distance_from(const coord_def &b) const
    {
        return (b - *this).rdist();
    }

    constexpr bool operator == (const coord_def &other) const
    {
        return x == other.x && y == other.y;
    }

    constexpr bool operator != (const coord_def &other) const
    {
        return !operator == (other);
    }

    constexpr bool operator <  (const coord_def &other) const
    {
        return x < other.x || (x == other.x && y < other.y);
    }

    constexpr bool operator >  (const coord_def &other) const
    {
        return x > other.x || (x == other.x && y > other.y);
    }

    const coord_def &operator += (const coord_def &other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    const coord_def &operator += (int offset)
    {
        x += offset;
        y += offset;
        return *this;
    }

    const coord_def &operator -= (const coord_def &other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    const coord_def &operator -= (int offset)
    {
        x -= offset;
        y -= offset;
        return *this;
    }

    const coord_def &operator /= (int div)
    {
        x /= div;
        y /= div;
        return *this;
    }

    const coord_def &operator *= (int mul)
    {
        x *= mul;
        y *= mul;
        return *this;
    }

    constexpr coord_def operator + (const coord_def &other) const
    {
        return { x + other.x, y + other.y };
    }

    constexpr coord_def operator + (int other) const
    {
        return *this + coord_def(other, other);
    }

    constexpr coord_def operator - (const coord_def &other) const
    {
        return { x - other.x, y - other.y };
    }

    constexpr coord_def operator -() const
    {
        return { -x, -y };
    }

    constexpr coord_def operator - (int other) const
    {
        return *this - coord_def(other, other);
    }

    constexpr coord_def operator / (int div) const
    {
        return { x / div, y / div };
    }

    constexpr coord_def operator * (int mul) const
    {
        return { x * mul, y * mul };
    }

    constexpr coord_def sgn() const
    {
        return coord_def(::sgn(x), ::sgn(y));
    }

    constexpr int abs() const
    {
        return x * x + y * y;
    }

    constexpr int rdist() const
    {
        // Replace with max(abs_ce(x), abs_ce(y) when we require C++14.
        return abs_ce(x) > abs_ce(y) ? abs_ce(x) : abs_ce(y);
    }

    constexpr bool origin() const
    {
        return !x && !y;
    }

    constexpr bool zero() const
    {
        return origin();
    }

    constexpr bool equals(const int xi, const int yi) const
    {
        return *this == coord_def(xi, yi);
    }
};

namespace std {
    template <>
    struct hash<coord_def>
    {
        constexpr size_t operator()(const coord_def& c) const
        {
            // lazy assumption: no coordinate will ever be bigger than 2^16
            return (c.x << 16) + c.y;
        }
    };
}

constexpr coord_def INVALID_COORD {-1, -1};
constexpr coord_def NO_CURSOR { INVALID_COORD };

typedef bool (*coord_predicate)(const coord_def &c);

struct run_check_dir
{
    dungeon_feature_type grid;
    coord_def delta;
};

/**
 * Persistent unique identifier for an actor.
 *
 * An mid_t is a persistent (across levels and across save/restore)
 * and unique (within a given game) identifier for a monster, player,
 * or fake actor. The value 0 indicates "no actor", and any value
 * greater than or equal to MID_FIRST_NON_MONSTER indicates an actor
 * other than a monster.
 *
 * mid_t should be used for anything that needs to remember monster
 * identity from one turn to the next, as mindexes may be reused
 * if a monster dies, and are not unique across levels.
 */
typedef uint32_t mid_t;
#define PRImidt PRIu32
#define MID_NOBODY        ((mid_t)0x00000000)
#define MID_PLAYER        ((mid_t)0xffffffff)
// the numbers are meaningless, there's just plenty of space for gods, env,
// and whatever else we want to have, while keeping all monster ids smaller.
#define MID_ANON_FRIEND   ((mid_t)0xffff0000)
#define MID_YOU_FAULTLESS ((mid_t)0xffff0001)
/// Upper bound on the number of monsters that can ever exist in a game.
#define MID_FIRST_NON_MONSTER MID_ANON_FRIEND

/**
 * Define overloaded ++ and -- operators for the enum T.
 *
 * This macro produces several inline function definitions; use it only at
 * file/namespace scope. It requires a trailing semicolon.
 *
 * @param T A type expression naming the enum type to augument. Evaluated
 *          several times.
 */
#define DEF_ENUM_INC(T) \
    static inline T &operator++(T &x) { return x = static_cast<T>(x + 1); } \
    static inline T &operator--(T &x) { return x = static_cast<T>(x - 1); } \
    static inline T operator++(T &x, int) { T y = x; ++x; return y; } \
    static inline T operator--(T &x, int) { T y = x; --x; return y; } \
    COMPILE_CHECK(is_enum<T>::value)

DEF_ENUM_INC(monster_type);
DEF_ENUM_INC(spell_type);
DEF_ENUM_INC(skill_type);

struct cloud_struct
{
    coord_def     pos;
    cloud_type    type;
    int           decay;
    uint8_t       spread_rate;
    kill_category whose;
    killer_type   killer;
    mid_t         source;
    int           excl_rad;

    cloud_struct() : pos(), type(CLOUD_NONE), decay(0), spread_rate(0),
                     whose(KC_OTHER), killer(KILL_NONE), excl_rad(-1)
    {
    }
    cloud_struct(coord_def p, cloud_type c, int d, int spread, kill_category kc,
                 killer_type kt, mid_t src, int excl);

    bool defined() const { return type != CLOUD_NONE; }
    bool temporary() const { return excl_rad == -1; }
    int exclusion_radius() const { return excl_rad; }

    actor *agent() const;
    void set_whose(kill_category _whose);
    void set_killer(killer_type _killer);

    string cloud_name(bool terse = false) const;
    void announce_actor_engulfed(const actor *engulfee,
                                 bool beneficial = false) const;

    static kill_category killer_to_whose(killer_type killer);
    static killer_type   whose_to_killer(kill_category whose);
};

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

/// Exception indicating a bad level_id, level_range, or depth_range.
struct bad_level_id : public runtime_error
{
    explicit bad_level_id(const string &msg) : runtime_error(msg) {}
    explicit bad_level_id(const char *msg) : runtime_error(msg) {}
};

/**
 * Create a bad_level_id exception from a printf-like specification.
 * Users of this macro must #include "stringutil.h" themselves.
 */
#define bad_level_id_f(...) bad_level_id(make_stringf(__VA_ARGS__))

// Identifies a level. Should never include virtual methods or
// dynamically allocated memory (see code to push level_id onto Lua
// stack in l-dgn.cc)
class level_id
{
public:
    branch_type branch;     // The branch in which the level is.
    int depth;              // What depth (in this branch - starting from 1)

public:
    // Returns the level_id of the current level.
    static level_id current();

    // Returns the level_id of the level that the stair/portal/whatever at
    // 'pos' on the current level leads to.
    static level_id get_next_level_id(const coord_def &pos);

    // Important that if run after this, ::valid() is false.
    level_id()
        : branch(BRANCH_DUNGEON), depth(-1)
    {
    }
    level_id(branch_type br, int dep = 1)
        : branch(br), depth(dep)
    {
    }

    /// @throws bad_level_id if s could not be parsed.
    static level_id parse_level_id(const string &s);
#if TAG_MAJOR_VERSION == 34
    static level_id from_packed_place(const unsigned short place);
#endif

    string describe(bool long_name = false, bool with_number = true) const;

    void clear()
    {
        branch = BRANCH_DUNGEON;
        depth  = -1;
    }

    // Returns the absolute depth in the dungeon for the level_id;
    // non-dungeon branches (specifically Abyss and Pan) will return
    // depths suitable for use in monster and item generation.
    int absdepth() const;

    bool is_valid() const
    {
        return branch < NUM_BRANCHES && depth > 0;
    }

    bool operator == (const level_id &id) const
    {
        return branch == id.branch && depth == id.depth;
    }

    bool operator != (const level_id &id) const
    {
        return !operator == (id);
    }

    bool operator <(const level_id &id) const
    {
        return branch < id.branch || (branch==id.branch && depth < id.depth);
    }

    void save(writer&) const;
    void load(reader&);
};

// A position on a particular level.
struct level_pos
{
    level_id      id;
    coord_def     pos;      // The grid coordinates on this level.

    level_pos() : id(), pos()
    {
        pos.x = pos.y = -1;
    }

    level_pos(const level_id &lid, const coord_def &coord)
        : id(lid), pos(coord)
    {
    }

    level_pos(const level_id &lid)
        : id(lid), pos()
    {
        pos.x = pos.y = -1;
    }

    // Returns the level_pos of where the player is standing.
    static level_pos current();

    bool operator == (const level_pos &lp) const
    {
        return id == lp.id && pos == lp.pos;
    }

    bool operator != (const level_pos &lp) const
    {
        return id != lp.id || pos != lp.pos;
    }

    bool operator <  (const level_pos &lp) const
    {
        return id < lp.id || (id == lp.id && pos < lp.pos);
    }

    bool is_valid() const
    {
        return id.depth > -1 && pos.x != -1 && pos.y != -1;
    }

    bool is_on(const level_id _id)
    {
        return id == _id;
    }

    void clear()
    {
        id.clear();
        pos = coord_def(-1, -1);
    }

    void save(writer&) const;
    void load(reader&);
};

class monster;

// We are not 64 bits clean here yet since many places still pass (or store!)
// it as 32 bits or, worse, longs. I considered setting this as uint32_t,
// however, since free bits are exhausted, it's very likely we'll have to
// extend this in the future, so this should be easier than undoing the change.
typedef uint32_t iflags_t;

struct item_def
{
    object_class_type base_type; ///< basic class (eg OBJ_WEAPON)
    uint8_t        sub_type;       ///< type within that class (eg WPN_DAGGER)
#pragma pack(push,2)
    union
    {
        // These must all be the same size!
        short plus;                 ///< + to hit/dam (weapons)
        monster_type mon_type:16;   ///< corpse/chunk monster type
        skill_type skill:16;        ///< the skill provided by a manual
        short charges;              ///< # of charges held by a wand, etc
        short initial_cards;        ///< the # of cards a deck *started* with
        short net_durability;       ///< damage dealt to a net
    };
    union
    {
        // These must all be the same size!
        short plus2;        ///< legacy/generic name for this union
        short used_count;   ///< the # of known times it was used (decks, wands)
                            // for wands, may hold negative ZAPCOUNT knowledge
                            // info (e.g. "recharged", "empty", "unknown")
        short net_placed;   ///< is this throwing net trapping something?
        short skill_points; ///< # of skill points a manual gives
        short stash_freshness; ///< where stash.cc stores corpse freshness
    };
#pragma pack(pop)
    union
    {
        // These must all be the same size!
        int special;            ///< legacy/generic name
        int unrand_idx;         ///< unrandart index (for get_unrand_entry)
        deck_rarity_type deck_rarity;    ///< plain, ornate, legendary
        uint32_t subtype_rnd;   ///< appearance of un-ID'd items, by subtype.
                                /// jewellery, scroll, staff, wand, potions
                                // see comment in item_colour()
        int brand;              ///< weapon and armour brands
        int freshness;          ///< remaining time until a corpse rots
    };
    uint8_t        rnd;            ///< random number, used for tile choice,
                                   /// randart colours, and other per-item
                                   /// random cosmetics. 0 = uninitialized
    short          quantity;       ///< number of items
    iflags_t       flags;          ///< item status flags

    /// The location of the item. Items in player inventory are indicated by
    /// pos (-1, -1), items in monster inventory by (-2, -2), and items
    /// in shops by (0, y) for y >= 5.
    coord_def pos;
    /// For floor items, index in the mitm array of the next item in the
    /// pile. NON_ITEM for the last item in a pile. For items in player
    /// inventory, instead the index into you.inv. For items in monster
    /// inventory, equal to NON_ITEM + 1 + mindex. For items in shops,
    /// equal to ITEM_IN_SHOP.
    short  link;
    /// Inventory letter of the item. For items in player inventory, equal
    /// to index_to_letter(link). For other items, equal to the slot letter
    /// the item had when it was last in player inventory.
    short  slot;

    level_id orig_place;
    short    orig_monnum;

    string inscription;

    CrawlHashTable props;

public:
    item_def() : base_type(OBJ_UNASSIGNED), sub_type(0), plus(0), plus2(0),
                 special(0), rnd(0), quantity(0), flags(0),
                 pos(), link(NON_ITEM), slot(0), orig_place(),
                 orig_monnum(0), inscription()
    {
    }

    string name(description_level_type descrip, bool terse = false,
                bool ident = false, bool with_inscription = true,
                bool quantity_in_words = false,
                iflags_t ignore_flags = 0x0) const;
    bool has_spells() const;
    bool cursed() const;
    colour_t get_colour() const;

    bool is_type(int base, int sub) const
    {
        return base_type == base && sub_type == sub;
    }

    /**
     * Find the index of an item in the mitm array. Results are undefined
     * if this item is not in the array!
     *
     * @pre The item is actually in the mitm array.
     * @return  The index of this item in the mitm array, between
     *          0 and MAX_ITEMS-1.
     */
    int  index() const;

    int  armour_rating() const;

    bool launched_by(const item_def &launcher) const;

    void clear()
    {
        *this = item_def();
    }

    /**
     * Sets this item as being held by a given monster.
     *
     * @param mon The monster. Must be in menv!
     */
    void set_holding_monster(const monster& mon);

    /**
     * Get the monster holding this item.
     *
     * @return A pointer to the monster holding this item, null if none.
     */
    monster* holding_monster() const;

    /** Is this item being held by a monster? */
    bool held_by_monster() const;

    bool defined() const;
    bool appearance_initialized() const;
    bool is_valid(bool info = false) const;

    /** Should this item be preserved as far as possible? */
    bool is_critical() const;

    /** Is this item of a type that should not be generated enchanted? */
    bool is_mundane() const;

private:
    string name_aux(description_level_type desc, bool terse, bool ident,
                    bool with_inscription, iflags_t ignore_flags) const;

    colour_t randart_colour() const;

    colour_t ring_colour() const;
    colour_t amulet_colour() const;

    colour_t rune_colour() const;

    colour_t weapon_colour() const;
    colour_t missile_colour() const;
    colour_t armour_colour() const;
    colour_t wand_colour() const;
    colour_t food_colour() const;
    colour_t jewellery_colour() const;
    colour_t potion_colour() const;
    colour_t book_colour() const;
    colour_t miscellany_colour() const;
    colour_t corpse_colour() const;
};

typedef item_def item_info;
item_info get_item_info(const item_def& info);

class runrest
{
public:
    int runmode;
    int mp;
    int hp;
    bool notified_mp_full;
    bool notified_hp_full;
    coord_def pos;
    int travel_speed;

    FixedVector<run_check_dir,3> run_check; // array of grids to check

public:
    runrest();
    void initialise(int rdir, int mode);
    void init_travel_speed();

    // returns runmode
    operator int () const;

    // sets runmode
    const runrest &operator = (int newrunmode);

    // Returns true if we're currently resting.
    bool is_rest() const;
    bool is_explore() const;
    bool is_any_travel() const;

    string runmode_name() const;

    // Clears run state.
    void clear();

    // Stops running.
    void stop(bool clear_delays = true);

    // Take one off the rest counter.
    void rest();

    // Checks if shift-run should be aborted and aborts the run if necessary.
    // Returns true if you were running and are now no longer running.
    bool check_stop_running();

private:
    void set_run_check(int index, int compass_dir);
    bool run_should_stop() const;
};

enum mon_spell_slot_flag
{
    MON_SPELL_NO_FLAGS  = 0,
    MON_SPELL_EMERGENCY   = 1 <<  0, // only use this spell slot in emergencies
    MON_SPELL_NATURAL     = 1 <<  1, // physiological, not really a spell
    MON_SPELL_MAGICAL     = 1 <<  2, // a generic magical ability
#if TAG_MAJOR_VERSION == 34
    MON_SPELL_DEMONIC     = 1 <<  3, // merged with magical abilities
#endif
    MON_SPELL_WIZARD      = 1 <<  4, // a real spell, affected by AM and silence
    MON_SPELL_PRIEST      = 1 <<  5,

    MON_SPELL_FIRST_CATEGORY = MON_SPELL_NATURAL,
    MON_SPELL_LAST_CATEGORY  = MON_SPELL_PRIEST,

    MON_SPELL_BREATH      = 1 <<  6, // sets a breath timer, requires it to be 0
    MON_SPELL_NO_SILENT   = 1 <<  7, // can't be used while silenced/mute/etc.

    MON_SPELL_INSTANT     = 1 <<  8, // allows another action on the same turn
    MON_SPELL_NOISY       = 1 <<  9, // makes noise despite being innate

    MON_SPELL_SHORT_RANGE = 1 << 10, // only use at short distances
    MON_SPELL_LONG_RANGE  = 1 << 11, // only use at long distances

    MON_SPELL_LAST_FLAG = MON_SPELL_LONG_RANGE,
};
DEF_BITFIELD(mon_spell_slot_flags, mon_spell_slot_flag, 11);
const int MON_SPELL_LAST_EXPONENT = mon_spell_slot_flags::last_exponent;
COMPILE_CHECK(mon_spell_slot_flags::exponent(MON_SPELL_LAST_EXPONENT)
              == MON_SPELL_LAST_FLAG);

constexpr mon_spell_slot_flags MON_SPELL_TYPE_MASK
    = MON_SPELL_NATURAL | MON_SPELL_MAGICAL | MON_SPELL_WIZARD
    | MON_SPELL_PRIEST;

constexpr mon_spell_slot_flags MON_SPELL_INNATE_MASK
    = MON_SPELL_NATURAL | MON_SPELL_MAGICAL;

constexpr mon_spell_slot_flags MON_SPELL_ANTIMAGIC_MASK
    = MON_SPELL_MAGICAL | MON_SPELL_WIZARD;

constexpr mon_spell_slot_flags MON_SPELL_SILENCE_MASK
    = MON_SPELL_WIZARD  | MON_SPELL_PRIEST  | MON_SPELL_NO_SILENT;

struct mon_spell_slot
{
    // Allow implicit conversion (and thus copy-initialization) from a
    // three-element initializer list, but not from a smaller list or
    // from a plain spell_type.
    constexpr mon_spell_slot(spell_type spell_, uint8_t freq_,
                             mon_spell_slot_flags flags_)
        : spell(spell_), freq(freq_), flags(flags_)
    { }
    explicit constexpr mon_spell_slot(spell_type spell_ = SPELL_NO_SPELL,
                                      uint8_t freq_ = 0)
        : mon_spell_slot(spell_, freq_, MON_SPELL_NO_FLAGS)
    { }

    spell_type spell;
    uint8_t freq;
    mon_spell_slot_flags flags;
};

typedef vector<mon_spell_slot> monster_spells;

class reader;
class writer;
class map_markers
{
public:
    map_markers();
    map_markers(const map_markers &);
    map_markers &operator = (const map_markers &);
    ~map_markers();

    bool need_activate() const { return have_inactive_markers; }
    void clear_need_activate();
    void activate_all(bool verbose = true);
    void activate_markers_at(coord_def p);
    void add(map_marker *marker);
    void remove(map_marker *marker);
    void remove_markers_at(const coord_def &c, map_marker_type type = MAT_ANY);
    map_marker *find(const coord_def &c, map_marker_type type = MAT_ANY);
    map_marker *find(map_marker_type type);
    void move(const coord_def &from, const coord_def &to);
    void move_marker(map_marker *marker, const coord_def &to);
    vector<map_marker*> get_all(map_marker_type type = MAT_ANY);
    vector<map_marker*> get_all(const string &key, const string &val = "");
    vector<map_marker*> get_markers_at(const coord_def &c);
    string property_at(const coord_def &c, map_marker_type type,
                       const string &key);
    string property_at(const coord_def &c, map_marker_type type,
                       const char *key)
    { return property_at(c, type, string(key)); }
    void clear();

    void write(writer &) const;
    void read(reader &);

private:
    typedef multimap<coord_def, map_marker *> dgn_marker_map;
    typedef pair<coord_def, map_marker *> dgn_pos_marker;

    void init_from(const map_markers &);
    void unlink_marker(const map_marker *);
    void check_empty();

private:
    dgn_marker_map markers;
    bool have_inactive_markers;
};

class InvEntry;
typedef int (*item_sort_fn)(const InvEntry *a, const InvEntry *b);
struct item_comparator
{
    item_sort_fn cmpfn;
    bool negated;

    item_comparator(item_sort_fn cfn, bool neg = false)
        : cmpfn(cfn), negated(neg)
    {
    }
    int compare(const InvEntry *a, const InvEntry *b) const
    {
        return negated? -cmpfn(a, b) : cmpfn(a, b);
    }
};
typedef vector<item_comparator> item_sort_comparators;

struct menu_sort_condition
{
public:
    menu_type mtype;
    int       sort;
    item_sort_comparators cmp;

public:
    menu_sort_condition(menu_type mt = MT_INVLIST, int sort = 0);
    menu_sort_condition(const string &s);

    bool matches(menu_type mt) const;

private:
    void set_menu_type(string &s);
    void set_sort(string &s);
    void set_comparators(string &s);
};

struct cglyph_t
{
    char32_t ch;
    unsigned short col; // XXX: real or unreal depending on context...

    cglyph_t(char32_t _ch = 0, unsigned short _col = LIGHTGREY)
        : ch(_ch), col(_col)
    {
    }
};

typedef FixedArray<bool, NUM_OBJECT_CLASSES, MAX_SUBTYPES> id_arr;
