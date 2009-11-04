/*
 *  File:       externs.h
 *  Summary:    Fixed size 2D vector class that asserts if you do something bad.
 *  Written by: Linley Henzell
 */

#ifndef EXTERNS_H
#define EXTERNS_H

#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <cstdlib>
#include <deque>

#include <time.h>

#include "defines.h"
#include "enum.h"
#include "fixary.h"
#include "libutil.h"
#include "mpr.h"
#include "store.h"

#ifdef USE_TILE
struct tile_flavour
{
    // The floor tile to use.
    unsigned short floor;
    // The wall tile to use.
    unsigned short wall;
    // Used as a random value or for special cases e.g. (bazaars, gates).
    unsigned short special;
};

// A glorified unsigned int that assists with ref-counting the mcache.
class tile_fg_store
{
public:
    tile_fg_store() : m_tile(0) {}
    tile_fg_store(unsigned int tile) : m_tile(tile) {}
    operator unsigned int() { return m_tile; }
    unsigned int operator=(unsigned int tile);
protected:
    unsigned int m_tile;
};


#endif

#define INFO_SIZE       200          // size of message buffers
#define ITEMNAME_SIZE   200          // size of item names/shop names/etc
#define HIGHSCORE_SIZE  800          // <= 10 Lines for long format scores

#define MAX_NUM_GODS    21

#define BURDEN_TO_AUM 0.1f           // scale factor for converting burden to aum

extern char info[INFO_SIZE];         // defined in acr.cc {dlb}

const int kNameLen = 30;
#ifdef SHORT_FILE_NAMES
    const int kFileNameLen = 6;
#else
    const int kFileNameLen = 250;
#endif

// Used only to bound the init file name length
const int kPathLen = 256;

// This value is used to mark that the current berserk is free from
// penalty (Xom's granted or from a deck of cards).
#define NO_BERSERK_PENALTY    -1

typedef FixedArray<dungeon_feature_type, GXM, GYM> feature_grid;
typedef FixedArray<unsigned, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER>
    env_show_grid;

struct item_def;
class melee_attack;
struct coord_def;
class level_id;
class player_quiver;

struct coord_def
{
    int         x;
    int         y;

    explicit coord_def( int x_in = 0, int y_in = 0 ) : x(x_in), y(y_in) { }

    void set(int xi, int yi)
    {
        x = xi;
        y = yi;
    }

    void reset()
    {
        set(0, 0);
    }

    int distance_from(const coord_def &b) const;

    bool operator == (const coord_def &other) const
    {
        return x == other.x && y == other.y;
    }

    bool operator != (const coord_def &other) const
    {
        return !operator == (other);
    }

    bool operator <  (const coord_def &other) const
    {
        return (x < other.x) || (x == other.x && y < other.y);
    }

    bool operator >  (const coord_def &other) const
    {
        return (x > other.x) || (x == other.x && y > other.y);
    }

    const coord_def &operator += (const coord_def &other)
    {
        x += other.x;
        y += other.y;
        return (*this);
    }

    const coord_def &operator += (int offset)
    {
        x += offset;
        y += offset;
        return (*this);
    }

    const coord_def &operator -= (const coord_def &other)
    {
        x -= other.x;
        y -= other.y;
        return (*this);
    }

    const coord_def &operator -= (int offset)
    {
        x -= offset;
        y -= offset;
        return (*this);
    }

    const coord_def &operator /= (int div)
    {
        x /= div;
        y /= div;
        return (*this);
    }

    const coord_def &operator *= (int mul)
    {
        x *= mul;
        y *= mul;
        return (*this);
    }

    coord_def operator + (const coord_def &other) const
    {
        coord_def copy = *this;
        return (copy += other);
    }

    coord_def operator + (int other) const
    {
        coord_def copy = *this;
        return (copy += other);
    }

    coord_def operator - (const coord_def &other) const
    {
        coord_def copy = *this;
        return (copy -= other);
    }

    coord_def operator - (int other) const
    {
        coord_def copy = *this;
        return (copy -= other);
    }

    coord_def operator / (int div) const
    {
        coord_def copy = *this;
        return (copy /= div);
    }

    coord_def operator * (int mul) const
    {
        coord_def copy = *this;
        return (copy *= mul);
    }

    int abs() const
    {
        return (x * x + y * y);
    }

    int rdist() const
    {
        return (std::max(std::abs(x), std::abs(y)));
    }

    bool origin() const
    {
        return (!x && !y);
    }

    bool zero() const
    {
        return origin();
    }

    bool equals(const int xi, const int yi) const
    {
        return (xi == x && yi == y);
    }
};

typedef bool (*coord_predicate)(const coord_def &c);

struct run_check_dir
{
    dungeon_feature_type grid;
    coord_def delta;
};

struct cloud_struct
{
    coord_def     pos;
    cloud_type    type;
    int           decay;
    unsigned char spread_rate;
    kill_category whose;
    killer_type   killer;

    cloud_struct() : pos(), type(CLOUD_NONE), decay(0), spread_rate(0),
                     whose(KC_OTHER), killer(KILL_NONE)
    {
    }

    void set_whose(kill_category _whose);
    void set_killer(killer_type _killer);

    static kill_category killer_to_whose(killer_type killer);
    static killer_type   whose_to_killer(kill_category whose);

};

struct shop_struct
{
    coord_def           pos;
    unsigned char       greed;
    shop_type           type;
    unsigned char       level;

    FixedVector<unsigned char, 3> keeper_name;
};


struct delay_queue_item
{
    delay_type  type;
    int         duration;
    int         parm1;
    int         parm2;
    bool        started;
};


// Identifies a level. Should never include virtual methods or
// dynamically allocated memory (see code to push level_id onto Lua
// stack in l_dgn.cc)
class level_id
{
public:
    branch_type branch;     // The branch in which the level is.
    int depth;              // What depth (in this branch - starting from 1)
    level_area_type level_type;

public:
    // Returns the level_id of the current level.
    static level_id current();

    // Returns the level_id of the level that the stair/portal/whatever at
    // 'pos' on the current level leads to.
    static level_id get_next_level_id(const coord_def &pos);

    level_id()
        : branch(BRANCH_MAIN_DUNGEON), depth(-1),
          level_type(LEVEL_DUNGEON)
    {
    }
    level_id(branch_type br, int dep, level_area_type ltype = LEVEL_DUNGEON)
        : branch(br), depth(dep), level_type(ltype)
    {
    }
    level_id(const level_id &ot)
        : branch(ot.branch), depth(ot.depth), level_type(ot.level_type)
    {
    }
    level_id(level_area_type ltype)
        : branch(BRANCH_MAIN_DUNGEON), depth(-1), level_type(ltype)
    {
    }

    static level_id parse_level_id(const std::string &s) throw (std::string);
    static level_id from_packed_place(const unsigned short place);

    unsigned short packed_place() const;
    std::string describe(bool long_name = false, bool with_number = true) const;

    void clear()
    {
        branch = BRANCH_MAIN_DUNGEON;
        depth  = -1;
        level_type = LEVEL_DUNGEON;
    }

    // Returns the absolute depth in the dungeon for the level_id;
    // non-dungeon branches (specifically Abyss and Pan) will return
    // depths suitable for use in monster and item generation. If
    // you're looking for a depth to set you.your_level to, use
    // dungeon_absdepth().
    int absdepth() const;

    // Returns the absolute depth in the dungeon for the level_id, corresponding
    // to you.your_level.
    int dungeon_absdepth() const;

    bool is_valid() const
    {
        return (branch != NUM_BRANCHES && depth != -1)
            || level_type != LEVEL_DUNGEON;
    }

    const level_id &operator = (const level_id &id)
    {
        branch     = id.branch;
        depth      = id.depth;
        level_type = id.level_type;
        return (*this);
    }

    bool operator == ( const level_id &id ) const
    {
        return (level_type == id.level_type
                && (level_type != LEVEL_DUNGEON
                    || (branch == id.branch && depth == id.depth)));
    }

    bool operator != ( const level_id &id ) const
    {
        return !operator == (id);
    }

    bool operator <( const level_id &id ) const
    {
        if (level_type != id.level_type)
            return (level_type < id.level_type);

        if (level_type != LEVEL_DUNGEON)
            return (false);

        return (branch < id.branch) || (branch==id.branch && depth < id.depth);
    }

    bool operator == ( const branch_type _branch ) const
    {
        return (branch == _branch && level_type == LEVEL_DUNGEON);
    }

    bool operator != ( const branch_type _branch  ) const
    {
        return !operator == (_branch);
    }

    void save(writer&) const;
    void load(reader&);
};


struct item_def
{
    object_class_type  base_type;  // basic class (ie OBJ_WEAPON)
    unsigned char  sub_type;       // type within that class (ie WPN_DAGGER)
    short          plus;           // +to hit, charges, corpse mon id
    short          plus2;          // +to dam, sub-sub type for boots/helms
    long           special;        // special stuff
    unsigned char  colour;         // item colour
    unsigned long  flags;          // item status flags
    short          quantity;       // number of items

    coord_def pos;     // for inventory items == (-1, -1)
    short  link;       // link to next item;  for inventory items = slot
    short  slot;       // Inventory letter

    unsigned short orig_place;
    short          orig_monnum;

    std::string inscription;

    CrawlHashTable props;

public:
    item_def() : base_type(OBJ_UNASSIGNED), sub_type(0), plus(0), plus2(0),
                 special(0L), colour(0), flags(0L), quantity(0),
                 pos(), link(NON_ITEM), slot(0), orig_place(0),
                 orig_monnum(0), inscription()
    {
    }

    std::string name(description_level_type descrip,
                     bool terse = false, bool ident = false,
                     bool with_inscription = true,
                     bool quantity_in_words = false,
                     unsigned long ignore_flags = 0x0) const;
    bool has_spells() const;
    bool cursed() const;
    int book_number() const;
    zap_type zap() const; // what kind of beam it shoots (if wand).

    // Returns index in mitm array. Results are undefined if this item is
    // not in the array!
    int  index() const;

    int  armour_rating() const;

    bool launched_by(const item_def &launcher) const;

    void clear()
    {
        *this = item_def();
    }

    // Sets this item as being held by a given monster.
    void set_holding_monster(int midx);

    // Returns monster holding this item.  NULL if none.
    monsters* holding_monster() const;

    // Returns true if a monster is holding this item.
    bool held_by_monster() const;

private:
    std::string name_aux(description_level_type desc,
                         bool terse, bool ident,
                         unsigned long ignore_flags) const;
};

class runrest
{
public:
    int runmode;
    int mp;
    int hp;
    coord_def pos;

    FixedVector<run_check_dir,3> run_check; // array of grids to check

public:
    runrest();
    void initialise(int rdir, int mode);

    // returns runmode
    operator int () const;

    // sets runmode
    const runrest &operator = (int newrunmode);

    // Returns true if we're currently resting.
    bool is_rest() const;
    bool is_explore() const;
    bool is_any_travel() const;

    // Clears run state.
    void clear();

    // Stops running.
    void stop();

    // Take one off the rest counter.
    void rest();

    // Checks if shift-run should be aborted and aborts the run if necessary.
    // Returns true if you were running and are now no longer running.
    bool check_stop_running();

private:
    void set_run_check(int index, int compass_dir);
    bool run_grids_changed() const;
};

class PlaceInfo
{
public:
    int level_type;     // enum level_area_type
    int branch;         // enum branch_type if LEVEL_DUNGEON; otherwise -1

    unsigned long num_visits;
    unsigned long levels_seen;

    unsigned long mon_kill_exp;
    unsigned long mon_kill_exp_avail;
    unsigned long mon_kill_num[KC_NCATEGORIES];

    long turns_total;
    long turns_explore;
    long turns_travel;
    long turns_interlevel;
    long turns_resting;
    long turns_other;

    double elapsed_total;
    double elapsed_explore;
    double elapsed_travel;
    double elapsed_interlevel;
    double elapsed_resting;
    double elapsed_other;

public:
    PlaceInfo();

    bool is_global() const;
    void make_global();

    void assert_validity() const;

    const std::string short_name() const;

    const PlaceInfo &operator += (const PlaceInfo &other);
    const PlaceInfo &operator -= (const PlaceInfo &other);
    PlaceInfo operator + (const PlaceInfo &other) const;
    PlaceInfo operator - (const PlaceInfo &other) const;
};


typedef std::vector<delay_queue_item> delay_queue_type;

class KillMaster;




class monster_spells : public FixedVector<spell_type, NUM_MONSTER_SPELL_SLOTS>
{
public:
    monster_spells()
        : FixedVector<spell_type, NUM_MONSTER_SPELL_SLOTS>(SPELL_NO_SPELL)
    { }
    void clear() { init(SPELL_NO_SPELL); }
};

class ghost_demon;
class actor;
class monsters;

struct trap_def
{
    coord_def pos;
    trap_type type;
    int       ammo_qty;

    dungeon_feature_type category() const;
    std::string name(description_level_type desc = DESC_PLAIN) const;
    bool is_known(const actor* act = 0) const;
    void trigger(actor& triggerer, bool flat_footed = false);
    void disarm();
    void destroy();
    void hide();
    void reveal();
    void prepare_ammo();
    bool type_has_ammo() const;
    bool active() const;

private:
    void message_trap_entry();
    void shoot_ammo(actor& act, bool was_known);
    item_def generate_trap_item();
    int shot_damage(actor& act);
};

struct map_cell
{
    short object;           // The object: monster, item, feature, or cloud.
    unsigned short flags;   // Flags describing the mappedness of this square.
    unsigned short colour;
    unsigned long property; // Flags for blood, sanctuary, ...

    map_cell() : object(0), flags(0), colour(0), property(0) { }
    void clear() { flags = object = colour = 0; }

    unsigned glyph() const;
    bool known() const;
    bool seen() const;
};

class map_marker;
class reader;
class writer;
class map_markers
{
public:
    map_markers();
    map_markers(const map_markers &);
    map_markers &operator = (const map_markers &);
    ~map_markers();

    void activate_all(bool verbose = true);
    void add(map_marker *marker);
    void remove(map_marker *marker);
    void remove_markers_at(const coord_def &c, map_marker_type type = MAT_ANY);
    map_marker *find(const coord_def &c, map_marker_type type = MAT_ANY);
    map_marker *find(map_marker_type type);
    void move(const coord_def &from, const coord_def &to);
    void move_marker(map_marker *marker, const coord_def &to);
    std::vector<map_marker*> get_all(map_marker_type type = MAT_ANY);
    std::vector<map_marker*> get_all(const std::string &key,
                                     const std::string &val = "");
    std::vector<map_marker*> get_markers_at(const coord_def &c);
    std::string property_at(const coord_def &c, map_marker_type type,
                            const std::string &key);
    void clear();

    void write(writer &) const;
    void read(reader &, int minorVersion);

private:
    typedef std::multimap<coord_def, map_marker *> dgn_marker_map;
    typedef std::pair<coord_def, map_marker *> dgn_pos_marker;

    void init_from(const map_markers &);
    void unlink_marker(const map_marker *);

private:
    dgn_marker_map markers;
};

struct message_filter
{
    int             channel;        // Use -1 to match any channel.
    text_pattern    pattern;        // Empty pattern matches any message

    message_filter( int ch, const std::string &s )
        : channel(ch), pattern(s)
    {
    }

    message_filter( const std::string &s ) : channel(-1), pattern(s) { }

    bool is_filtered( int ch, const std::string &s ) const {
        bool channel_match = ch == channel || channel == -1;
        if (!channel_match || pattern.empty())
            return channel_match;
        return pattern.matches(s);
    }

};

struct sound_mapping
{
    text_pattern pattern;
    std::string  soundfile;
};

struct colour_mapping
{
    std::string tag;
    text_pattern pattern;
    int colour;
};

struct message_colour_mapping
{
    message_filter message;
    int colour;
};

struct feature_def
{
    dungeon_char_type   dchar;
    unsigned            symbol;          // symbol used for seen terrain
    unsigned            magic_symbol;    // symbol used for magic-mapped terrain
    unsigned short      colour;          // normal in LoS colour
    unsigned short      map_colour;      // colour when out of LoS on display
    unsigned short      seen_colour;     // map_colour when is_terrain_seen()
    unsigned short      em_colour;       // Emphasised colour when in LoS.
    unsigned short      seen_em_colour;  // Emphasised colour when out of LoS
    unsigned            flags;
    map_feature         minimap;         // mini-map categorization

    bool is_notable() const { return (flags & FFT_NOTABLE); }
};

struct feature_override
{
    dungeon_feature_type    feat;
    feature_def             override;
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
        return (negated? -cmpfn(a, b) : cmpfn(a, b));
    }
};
typedef std::vector<item_comparator> item_sort_comparators;

struct menu_sort_condition
{
public:
    menu_type mtype;
    int       sort;
    item_sort_comparators cmp;

public:
    menu_sort_condition(menu_type mt = MT_INVLIST, int sort = 0);
    menu_sort_condition(const std::string &s);

    bool matches(menu_type mt) const;

private:
    void set_menu_type(std::string &s);
    void set_sort(std::string &s);
    void set_comparators(std::string &s);
};

struct mon_display
{
    monster_type type;
    unsigned     glyph;
    unsigned     colour;

    mon_display(monster_type m = MONS_NO_MONSTER,
                unsigned gly = 0,
                unsigned col = 0) : type(m), glyph(gly), colour(col) { }
};

#include "msvc.h"

#endif // EXTERNS_H
