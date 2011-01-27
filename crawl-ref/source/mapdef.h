/**
 * @defgroup mapdef Map Parsing
 * @brief Parse .des files into map objects.
 * @section DESCRIPTION
 *
 * mapdef.h:
 * Header for map structures used by the level compiler.
 *
 * NOTE: When we refer to map, this could be a full map, filling an entire
 * level or a minivault that occupies just a portion of the level.
 */

#ifndef __MAPDEF_H__
#define __MAPDEF_H__

#include <string>
#include <vector>
#include <cstdio>
#include <memory>

#include "dlua.h"
#include "enum.h"
#include "externs.h"
#include "matrix.h"
#include "fprop.h"
#include "makeitem.h"
#include "travel_defs.h"

// Invalid heightmap height.
static const int INVALID_HEIGHT = -31999;

// Exception thrown when a map cannot be loaded from its .dsc file
// because the .dsc file has changed under it.
class map_load_exception : public std::exception
{
public:
    map_load_exception(const std::string &_mapname) : mapname(_mapname) { }
    ~map_load_exception() throw () { }
    const char *what() const throw()
    {
        return mapname.c_str();
    }
private:
    std::string mapname;
};

// [dshaligram] Maps can be mirrored; for every orientation, there must be
// a suitable mirror.
enum map_section_type                  // see maps.cc and dungeon.cc {dlb}
{
    MAP_NONE  = -1,
    MAP_NORTH = 1,                     //    1
    MAP_SOUTH,
    MAP_EAST,
    MAP_WEST,
    MAP_NORTHWEST,                     //    5
    MAP_NORTHEAST,
    MAP_SOUTHWEST,
    MAP_SOUTHEAST,
    MAP_ENCOMPASS,

    // A "floating" vault is placed somewhat like a minivault, away from
    // the edges, although in other respects it behaves like a regular vault.
    MAP_FLOAT,

    MAP_NUM_SECTION_TYPES
};

struct raw_range
{
    branch_type branch;
    int shallowest, deepest;
    bool deny;
};

struct level_range
{
public:
    level_area_type level_type;
    branch_type branch;
    int shallowest, deepest;
    bool deny;

public:
    level_range(const raw_range &range);
    level_range(branch_type br = BRANCH_MAIN_DUNGEON, int s = -1, int d = -1);

    void set(int s, int d = -1);
    void set(const std::string &branch, int s, int d) throw (std::string);

    void reset();
    bool matches(const level_id &) const;
    bool matches(int depth) const;

    void write(writer&) const;
    void read(reader&);

    bool valid() const;
    int span() const;

    static level_range parse(std::string lr) throw (std::string);

    std::string describe() const;
    std::string str_depth_range() const;

    bool operator == (const level_range &lr) const;

    operator raw_range () const;
    operator std::string () const
    {
        return describe();
    }

private:
    static void parse_partial(level_range &lr, const std::string &s)
        throw (std::string);
    static void parse_depth_range(const std::string &s, int *low, int *high)
        throw (std::string);
};

typedef std::pair<int,int> glyph_weighted_replacement_t;
typedef std::vector<glyph_weighted_replacement_t> glyph_replacements_t;

class map_lines;

class subst_spec
{
public:
    subst_spec(std::string torepl, bool fix, const glyph_replacements_t &repls);
    subst_spec(int count, bool fix, const glyph_replacements_t &repls);
    subst_spec() : key(""), count(-1), fix(false), frozen_value(0), repl() { }

    int value();

public:
    std::string key;
    // If this is part of an nsubst spec, how many to replace.
    // -1 corresponds to all (i.e. '*')
    int count;

    bool fix;
    int frozen_value;

    glyph_replacements_t repl;
};

class nsubst_spec
{
public:
    nsubst_spec(std::string key, const std::vector<subst_spec> &specs);
public:
    std::string key;
    std::vector<subst_spec> specs;
};

typedef std::pair<int, int> map_weighted_colour;
class map_colour_list : public std::vector<map_weighted_colour>
{
public:
    bool parse(const std::string &s, int weight);
};
class colour_spec
{
public:
    colour_spec(std::string _key, bool _fix, const map_colour_list &clist)
        : key(_key), fix(_fix), fixed_colour(BLACK), colours(clist)
    {
    }

    int get_colour();

public:
    std::string key;
    bool fix;
    int fixed_colour;
    map_colour_list colours;
};

typedef std::pair<unsigned long, int> map_weighted_fprop;
class map_fprop_list : public std::vector<map_weighted_fprop>
{
public:
    bool parse(const std::string &fp, int weight);
};

typedef std::pair<int, int> map_weighted_fheight;
class map_featheight_list : public std::vector<map_weighted_fheight>
{
public:
    bool parse(const std::string &fp, int weight);
};

class fprop_spec
{
public:
    fprop_spec(std::string _key, bool _fix, const map_fprop_list &flist)
        : key(_key), fix(_fix), fixed_prop(FPROP_NONE), fprops(flist)
    {
    }

    unsigned long get_property();

public:
    std::string key;
    bool fix;
    unsigned long fixed_prop;
    map_fprop_list fprops;
};

class fheight_spec
{
public:
    fheight_spec(std::string _key, bool _fix,
                 const map_featheight_list &_fheights)
        : key(_key), fix(_fix), fixed_height(INVALID_HEIGHT),
          fheights(_fheights)
    {
    }
    int get_height();
public:
    std::string key;
    bool fix;
    int fixed_height;
    map_featheight_list fheights;
};

#ifdef USE_TILE
typedef std::pair<std::string, int> map_weighted_tile;
class map_tile_list : public std::vector<map_weighted_tile>
{
public:
    bool parse(const std::string &s, int weight);
};

class tile_spec
{
public:
    tile_spec(const std::string &_key, bool _fix, bool _rand, bool _last, bool _floor, bool _feat, const map_tile_list &_tiles)
        : key(_key), fix(_fix), chose_fixed(false), no_random(_rand), last_tile(_last), floor(_floor), feat(_feat),
          fixed_tile(""), tiles(_tiles)
    {
    }

    std::string get_tile();

public:
    std::string key;
    bool fix;
    bool chose_fixed;
    bool no_random;
    bool last_tile;
    bool floor;
    bool feat;
    std::string fixed_tile;
    map_tile_list tiles;
};
#endif

class map_marker_spec
{
public:
    std::string key;
    std::string marker;

    // Special handling for Lua markers:
    std::auto_ptr<lua_datum> lua_fn;

    map_marker_spec(std::string _key, const std::string &mark)
        : key(_key), marker(mark), lua_fn() { }

    map_marker_spec(std::string _key, const lua_datum &fn)
        : key(_key), marker(), lua_fn(new lua_datum(fn)) { }

    std::string apply_transform(map_lines &map);

private:
    map_marker *create_marker();
};

typedef std::pair<std::string, int> map_weighted_string;
class map_string_list : public std::vector<map_weighted_string>
{
public:
    bool parse(const std::string &fp, int weight);
};
class string_spec
{
public:
    string_spec(std::string _key, bool _fix, const map_string_list &slist)
        : key(_key), fix(_fix), fixed_str(""), strlist(slist)
    {
    }

    std::string get_property();

public:
    std::string key;
    bool fix;
    std::string fixed_str;
    map_string_list strlist;
};

template<class T>
std::string parse_weighted_str(const std::string &cspec, T &list);

class map_def;
class rectangle_iterator;
struct keyed_mapspec;
class map_lines
{
public:
    class iterator {
    public:
        iterator(map_lines &ml, const std::string &key);
        operator bool () const;
        coord_def operator ++ ();
        coord_def operator ++ (int);
        coord_def operator * () const;
    private:
        void advance();
    private:
        map_lines &maplines;
        std::string key;
        coord_def p;
    };

public:
    map_lines();
    map_lines(const map_lines &);
    ~map_lines();

    map_lines &operator = (const map_lines &);

    bool in_map(const coord_def &pos) const;

    void add_line(const std::string &s);
    std::string add_nsubst(const std::string &st);
    std::string add_subst(const std::string &st);
    std::string add_shuffle(const std::string &s);
    std::string add_colour(const std::string &col);
    std::string add_fproperty(const std::string &sub);
    std::string add_fheight(const std::string &arg);
    void clear_markers();

    void write_maplines(writer &) const;
    void read_maplines(reader&);

#ifdef USE_TILE
    std::string add_floortile(const std::string &s);
    std::string add_rocktile(const std::string &s);
    std::string add_spec_tile(const std::string &s);
#endif

    std::vector<coord_def> find_glyph(const std::string &glyphs) const;
    std::vector<coord_def> find_glyph(int glyph) const;
    coord_def find_first_glyph(int glyph) const;
    coord_def find_first_glyph(const std::string &glyphs) const;

    // Find rectangular bounds (inclusive) for uses of the glyph in the map.
    // Returns false if glyph could not be found.
    bool find_bounds(int glyph, coord_def &tl, coord_def &br) const;
    // Same as above, but for any of the glyphs in glyph_str.
    bool find_bounds(const char *glyph_str, coord_def &tl, coord_def &br) const;

    void set_orientation(const std::string &s);

    int width() const;
    int height() const;
    coord_def size() const;

    int glyph(int x, int y) const;
    int glyph(const coord_def &) const;
    bool is_solid(int gly) const;

    bool solid_borders(map_section_type border);

    // Make all lines the same length.
    void normalise(char fillc = ' ');

    // Rotate 90 degrees either clockwise or anticlockwise
    void rotate(bool clockwise);
    void hmirror();
    void vmirror();

    void clear();

    void add_marker(map_marker *marker);
    std::string add_feature_marker(const std::string &desc);
    std::string add_lua_marker(const std::string &key,
                               const lua_datum &fn);

    void apply_markers(const coord_def &pos);
    void apply_grid_overlay(const coord_def &pos);
    void apply_overlays(const coord_def &pos);

    const std::vector<std::string> &get_lines() const;
    std::vector<std::string> &get_lines();

    rectangle_iterator get_iter() const;
    char operator () (const coord_def &c) const;
    char& operator () (const coord_def &c);
    char operator () (int x, int y) const;
    char& operator () (int x, int y);

    const keyed_mapspec *mapspec_at(const coord_def &c) const;
    keyed_mapspec *mapspec_at(const coord_def &c);

    std::string add_key_item(const std::string &s);
    std::string add_key_mons(const std::string &s);
    std::string add_key_feat(const std::string &s);
    std::string add_key_mask(const std::string &s);

    bool in_bounds(const coord_def &c) const;

    // Extend map dimensions with glyph 'fill' to minimum width and height.
    void extend(int min_width, int min_height, char fill);

    bool fill_zone(travel_distance_grid_t &tpd, const coord_def &start,
                   const coord_def &tl, const coord_def &br, int zone,
                   const char *wanted, const char *passable) const;

    int count_feature_in_box(const coord_def &tl, const coord_def &br,
                             const char *feat) const;

    void fill_mask_matrix(const std::string &glyphs, const coord_def &tl,
                          const coord_def &br, Matrix<bool> &flags);

    // Merge vault onto the tl/br subregion, where mask is true.
    void merge_subvault(const coord_def &tl, const coord_def &br,
                        const Matrix<bool> &mask, const map_def &vault);
private:
    void init_from(const map_lines &map);
    template <typename V> void clear_vector(V &vect);
    void vmirror_markers();
    void hmirror_markers();
    void rotate_markers(bool clock);
    void vmirror_marker(map_marker *, int par);
    void hmirror_marker(map_marker *, int par);
    void rotate_marker(map_marker *, int par);
    void translate_marker(void (map_lines::*xform)(map_marker *, int par),
                          int par = 0);

    void resolve_shuffle(const std::string &shuffle);
    void subst(std::string &s, subst_spec &spec);
    void subst(subst_spec &);
    void nsubst(nsubst_spec &);
    void bind_overlay();
    void overlay_colours(colour_spec &);
    void overlay_fprops(fprop_spec &);
    void overlay_fheights(fheight_spec &);

    // Merge cell (vx, vy) from vault onto this map's (x, y) cell.
    typedef FixedVector<int, 256> keyspec_map;
    void merge_cell(int x, int y, const map_def &vault, int vx, int vy,
                    int keyspec_idx);

#ifdef USE_TILE
    void overlay_tiles(tile_spec &);
#endif
    void check_borders();
    std::string shuffle(std::string s);
    std::string block_shuffle(const std::string &s);
    std::string check_shuffle(std::string &s);
    std::string check_block_shuffle(const std::string &s);
    std::string clean_shuffle(std::string s);
    std::string parse_nsubst_spec(const std::string &s,
                                  subst_spec &spec);
    int apply_nsubst(std::vector<coord_def> &pos,
                     int start, int nsub,
                     subst_spec &spec);
    std::string parse_glyph_replacements(std::string s,
                                         glyph_replacements_t &gly);

#ifdef USE_TILE
    std::string add_tile(const std::string &sub, bool is_floor, bool is_feat);
#endif

    std::string add_key_field(
        const std::string &s,
        std::string (keyed_mapspec::*set_field)(
            const std::string &s, bool fixed),
        void (keyed_mapspec::*copy_field)(const keyed_mapspec &spec));

    const keyed_mapspec *mapspec_for_key(int key) const;
    keyed_mapspec *mapspec_for_key(int key);

    friend class subst_spec;
    friend class nsubst_spec;
    friend class shuffle_spec;
    friend class map_marker_spec;
    friend class colour_spec;
    friend class tile_spec;

private:
    std::vector<map_marker *> markers;
    std::vector<std::string> lines;

    struct overlay_def
    {
        overlay_def() :
            colour(0), rocktile(""), floortile(""), tile(""),
            no_random(false), last_tile(false), property(0), height(INVALID_HEIGHT),
            keyspec_idx(0)
        {}
        int colour;
        std::string rocktile;
        std::string floortile;
        std::string tile;
        bool no_random;
        bool last_tile;
        int property;
        int height;      // heightmap height
        int keyspec_idx;
    };
    typedef Matrix<overlay_def> overlay_matrix;
    std::auto_ptr<overlay_matrix> overlay;

    typedef std::map<int, keyed_mapspec> keyed_specs;
    keyed_specs keyspecs;
    int next_keyspec_idx;

    enum
    {
        SUBVAULT_GLYPH = 1,
    };

    int map_width;
    bool solid_north, solid_east, solid_south, solid_west;
    bool solid_checked;
};

enum item_spec_type
{
    ISPEC_GOOD    = -2,
    ISPEC_SUPERB  = -3,
    ISPEC_DAMAGED = -4,
    ISPEC_BAD     = -5,
    ISPEC_RANDART = -6,
    ISPEC_MUNDANE = -7,
    ISPEC_ACQUIREMENT = -9,
};

class mons_spec;
class item_spec
{
public:
    int genweight;

    object_class_type base_type;
    int sub_type;
    int plus, plus2;
    int ego;
    int allow_uniques;
    int level;
    int race;
    int item_special;
    int qty;
    int acquirement_source;
    level_id place;

    // Specifically for storing information about randart spell books.
    CrawlHashTable props;

    item_spec() : genweight(10), base_type(OBJ_RANDOM), sub_type(OBJ_RANDOM),
        plus(-1), plus2(-1), ego(0), allow_uniques(1), level(-1),
        race(MAKE_ITEM_RANDOM_RACE), item_special(0), qty(0),
        acquirement_source(0), place(), props(),
        _corpse_monster_spec(NULL)
    {
    }

    item_spec(const item_spec &other);
    item_spec &operator = (const item_spec &other);
    ~item_spec();

    bool corpselike() const;
    const mons_spec &corpse_monster_spec() const;
    void set_corpse_monster_spec(const mons_spec &spec);

private:
    mons_spec *_corpse_monster_spec;

private:
    void release_corpse_monster_spec();
};
typedef std::vector<item_spec> item_spec_list;

class item_list
{
public:
    item_list() : items() { }

    void clear();

    item_spec get_item(int index);
    item_spec random_item ();
    item_spec random_item_weighted ();
    size_t size() const { return items.size(); }
    bool empty() const { return items.empty(); }

    std::string add_item(const std::string &spec, bool fix = false);
    std::string set_item(int index, const std::string &spec);

    // Set this list to be a copy of the item_spec_slot in list.
    void set_from_slot(const item_list &list, int slot_index);

private:
    struct item_spec_slot
    {
        item_spec_list ilist;
        bool fix_slot;

        item_spec_slot() : ilist(), fix_slot(false)
        {
        }
    };

private:
    item_spec item_by_specifier(const std::string &spec);
    item_spec_slot parse_item_spec(std::string spec);
    void build_deck_spec(std::string s, item_spec* spec);
    item_spec parse_single_spec(std::string s);
    int parse_acquirement_source(const std::string &source);
    void parse_raw_name(std::string name, item_spec &spec);
    void parse_random_by_class(std::string c, item_spec &spec);
    item_spec pick_item(item_spec_slot &slot);
    item_spec parse_corpse_spec(item_spec &result, std::string s);
    bool monster_corpse_is_valid(monster_type *, const std::string &name,
                                 bool corpse, bool skeleton, bool chunk);

private:
    std::vector<item_spec_slot> items;
    std::string error;
};

class mons_spec
{
 public:
    int mid;
    level_id place;
    monster_type monbase;     // Base monster for zombies and dracs.
    mon_attitude_type attitude;
    int number;               // Head count for hydras, etc.
    int quantity;             // Number of monsters (usually 1).
    int genweight, mlevel;
    bool fix_mons;
    bool generate_awake;
    bool patrolling;
    bool band;
    int colour;

    god_type god;
    bool god_gift;

    int hd;
    int hp;
    int abjuration_duration;
    int summon_type;

    item_list items;
    std::string monname;
    std::string non_actor_summoner;

    bool explicit_spells;
    std::vector<monster_spells> spells;
    unsigned long extra_monster_flags;

    CrawlHashTable props;

    mons_spec(int id = RANDOM_MONSTER,
              monster_type base = MONS_NO_MONSTER,
              int num = 0,
              int gw = 10, int ml = 0,
              bool _fixmons = false, bool awaken = false, bool patrol = false)
        : mid(id), place(), monbase(base), attitude(ATT_HOSTILE), number(num),
          quantity(1), genweight(gw), mlevel(ml), fix_mons(_fixmons),
          generate_awake(awaken), patrolling(false), band(false),
          colour(BLACK), god(GOD_NO_GOD), god_gift(false), hd(0), hp(0),
          abjuration_duration(0), summon_type(0), items(), monname(""),
          non_actor_summoner(""), explicit_spells(false), spells(),
          extra_monster_flags(0L), props()
    {
    }
};

class mons_list
{
public:
    mons_list();

    void clear();

    // Set this list to be a copy of the mons_spec_slot in list.
    void set_from_slot(const mons_list &list, int slot_index);

    mons_spec get_monster(int index);
    mons_spec get_monster(int slot_index, int list_index) const;

    // Returns an error string if the monster is unrecognised.
    std::string add_mons(const std::string &s, bool fix_slot = false);
    std::string set_mons(int slot, const std::string &s);

    size_t size()              const { return mons.size(); }
    size_t slot_size(int slot) const { return mons[slot].mlist.size(); }

private:
    typedef std::vector<mons_spec> mons_spec_list;

    struct mons_spec_slot
    {
        mons_spec_list mlist;
        bool fix_slot;

        mons_spec_slot(const mons_spec_list &list, bool fix = false)
            : mlist(list), fix_slot(fix)
        {
        }

        mons_spec_slot()
            : mlist(), fix_slot(false)
        {
        }
    };

private:
    mons_spec mons_by_name(std::string name) const;
    mons_spec drac_monspec(std::string name) const;
    void get_zombie_type(std::string s, mons_spec &spec) const;
    mons_spec get_hydra_spec(const std::string &name) const;
    mons_spec get_slime_spec(const std::string &name) const;
    mons_spec get_zombified_monster(const std::string &name,
                                    monster_type zomb) const;
    mons_spec_slot parse_mons_spec(std::string spec);
    void parse_mons_spells(mons_spec &slot, std::vector<std::string> &spells);
    mons_spec pick_monster(mons_spec_slot &slot);
    int fix_demon(int id) const;
    bool check_mimic(const std::string &s, int *mid, bool *fix) const;

private:
    std::vector< mons_spec_slot > mons;
    std::string error;
};

/**
 * @class shop_spec
 * @ingroup mapdef
 * @brief Specify how to create a shop.
 *
 * This specification struct is used when converting a vault-specified shop from
 * a string into something the builder can use to actually create and place a
 * shop.
**/
struct shop_spec
{
    shop_type sh_type;  /**< One of the shop_type enum values. */

    std::string name;   /**< If provided, this is apostrophised and used as the
                          *  shop keeper's name, ie, Plog as a name becomes
                          *  Plog's. */

    std::string type;   /**< If provided, this is used as the shop type name,
                          *  ie, Hide, Antique, Wand, etc. */

    std::string suffix; /**< If provided, this is used as the shop suffix,
                          *  ie, Shop, Boutique, Parlour, etc. */

    int greed;          /**< If provided, this value is used for price
                          *  calculation. The higher the greed, the more
                          *  inflation applied to prices. */

    int num_items;      /**< Cap the number of items in a shop at this. */

    item_list items;    /**< If provided, and `use_all` is false, items will be
                          *  selected at random from the list and used to
                          *  populate the shop. If `use_all` is true, the items
                          *  contained will be placed precisely and in order. If
                          *  the number of items contained is less than
                          *  specified, random items according to the shop's
                          *  type will be used to fill in the rest of the stock.
                          *  */

    bool use_all;       /**< True if all items in `items` should be used. */

    shop_spec (shop_type sh, std::string n="", std::string t="",
               std::string s="", int g=-1, int ni=-1, bool u=false)
        : sh_type(sh), name(n), type(t), suffix(s),
          greed(g), num_items(ni), items(), use_all(u) { }
};

/**
 * @class trap_spec
 * @ingroup mapdef
 * @brief Specify how to create a trap.
 *
 * This specification struct is used when converting a vault-specified trap
 * string into something that the builder can use to place a trap.
**/
struct trap_spec
{
    trap_type tr_type; /*> One of the trap_type enum values. */
    trap_spec (trap_type tr)
        : tr_type(static_cast<trap_type>(tr)) { }
};


/**
 * @class feature_spec
 * @ingroup mapdef
 * @brief Specify how to create a feature.
 *
 * This specification struct is used firstly when a feature is specified in
 * vault code (any feature), and secondly, if that feature is either a trap or a
 * shop, as a container for a std::auto_ptr to that shop_spec or trap_spec.
**/
struct feature_spec
{
    int genweight;                 /**> The weight of this specific feature. */
    int feat;                      /**> The specific feature being placed. */
    std::auto_ptr<shop_spec> shop; /**> A pointer to a shop_spec. */
    std::auto_ptr<trap_spec> trap; /**> A pointer to a trap_spec. */
    int glyph;                     /**> What glyph to use instead. */

    feature_spec();
    feature_spec(int f, int wt = 10);
    feature_spec(const feature_spec& other);
    feature_spec& operator = (const feature_spec& other);
    void init_with (const feature_spec& other);
};

typedef std::vector<feature_spec> feature_spec_list;
struct feature_slot
{
    feature_spec_list feats;
    bool fix_slot;

    feature_slot();
    feature_spec get_feat(int default_glyph);
};

struct map_flags
{
    unsigned long flags_set, flags_unset;

    map_flags();
    void clear();

    static map_flags parse(const std::string flag_list[],
                           const std::string &s) throw(std::string);
};


struct keyed_mapspec
{
public:
    int          key_glyph;

    feature_slot feat;
    item_list    item;
    mons_list    mons;
    map_flags    map_mask;

public:
    keyed_mapspec();

    // Parse the string and set the given entry.  If fix is true,
    // then whatever is selected for the first feature will be
    // permanently fixed.
    std::string set_feat(const std::string &s, bool fix);
    std::string set_mons(const std::string &s, bool fix);
    std::string set_item(const std::string &s, bool fix);
    std::string set_mask(const std::string &s, bool garbage);
    std::string set_height(const std::string &s, bool garbage);

    // Copy from the given mapspec.  If that entry is fixed,
    // it should be pre-selected prior to the copy.
    void copy_feat(const keyed_mapspec &spec);
    void copy_mons(const keyed_mapspec &spec);
    void copy_item(const keyed_mapspec &spec);
    void copy_mask(const keyed_mapspec &spec);
    void copy_height(const keyed_mapspec &spec);

    feature_spec get_feat();
    mons_list   &get_monsters();
    item_list   &get_items();
    map_flags   &get_mask();

private:
    std::string err;

private:
    void parse_features(const std::string &);
    feature_spec_list parse_feature(const std::string &s);
    feature_spec parse_shop(std::string s, int weight);
    feature_spec parse_trap(std::string s, int weight);
};

class map_def;
class dlua_set_map
{
public:
    dlua_set_map(map_def *map);
    ~dlua_set_map();
private:
    std::auto_ptr<lua_datum> old_map;
};

class map_def;
dungeon_feature_type map_feature_at(map_def *map,
                                    const coord_def &c,
                                    int rawfeat);

struct map_file_place
{
    std::string filename;
    int lineno;

    map_file_place(const std::string &s = "", int line = 0)
        : filename(s), lineno(line)
    {
    }

    void clear()
    {
        filename.clear();
        lineno = 0;
    }
};

const int DEFAULT_CHANCE_PRIORITY = 100;
struct map_chance
{
    int chance_priority;
    int chance;
    map_chance() : chance_priority(-1), chance(-1) { }
    map_chance(int _priority, int _chance)
        : chance_priority(_priority), chance(_chance) { }
    map_chance(int _chance)
        : chance_priority(DEFAULT_CHANCE_PRIORITY), chance(_chance) { }
    bool valid() const { return chance_priority >= 0 && chance >= 0; }
    bool dummy_chance() const { return chance_priority == 0 && chance >= 0; }
    std::string describe() const;
    // Returns true if the vault makes the random CHANCE_ROLL.
    bool roll() const;
    void write(writer &) const;
    void read(reader &);
};

// For the bison parser's token union:
struct map_chance_pair
{
   int priority;
   int chance;
};

typedef std::vector<level_range> depth_ranges_v;
class depth_ranges
{
private:
    depth_ranges_v depths;
public:
    static depth_ranges parse_depth_ranges(
        const std::string &depth_ranges_string);
    void read(reader &);
    void write(writer &) const;
    void clear() { depths.clear(); }
    bool empty() const { return depths.empty(); }
    bool is_usable_in(const level_id &lid) const;
    void add_depth(const level_range &range) { depths.push_back(range); }
    void add_depths(const depth_ranges &other_ranges);
    std::string describe() const;
};

template <typename X>
struct depth_range_X
{
    depth_ranges depths;
    X depth_thing;
    depth_range_X() : depths(), depth_thing() { }
    depth_range_X(const std::string &depth_range_string, const X &thing)
        : depths(depth_ranges::parse_depth_ranges(depth_range_string)),
          depth_thing(thing)
    {
    }
    bool is_usable_in(const level_id &lid) const
    {
        return depths.is_usable_in(lid);
    }
    template <typename reader_fn_type>
    static depth_range_X read(reader &inf, reader_fn_type reader_fn)
    {
        depth_range_X range_x;
        range_x.depths.read(inf);
        range_x.depth_thing = reader_fn(inf);
        return range_x;
    }
    template <typename writer_fn_type>
    void write(writer &outf, writer_fn_type writer_fn) const
    {
        depths.write(outf);
        writer_fn(outf, depth_thing);
    }
};

template <typename X>
class depth_ranges_X
{
private:
    typedef std::vector<depth_range_X<X> > depth_range_X_v;

    X default_thing;
    depth_range_X_v depth_range_Xs;
public:
    depth_ranges_X() : default_thing(), depth_range_Xs() { }
    depth_ranges_X(const X &_default_thing)
        : default_thing(_default_thing), depth_range_Xs() { }
    void clear(const X &_default_X = X())
    {
        depth_range_Xs.clear();
        set_default(_default_X);
    }
    void set_default(const X &_default_X)
    {
        default_thing = _default_X;
    }
    X get_default() const { return default_thing; }
    void add_range(const std::string &depth_range_string,
                   const X &thing)
    {
        depth_range_Xs.push_back(depth_range_X<X>(depth_range_string, thing));
    }
    X depth_value(const level_id &lid) const
    {
        typename depth_range_X_v::const_iterator i = depth_range_Xs.begin();
        for ( ; i != depth_range_Xs.end(); ++i)
            if (i->is_usable_in(lid))
                return i->depth_thing;
        return default_thing;
    }
    template <typename reader_fn_type>
    static depth_ranges_X read(reader &inf, reader_fn_type reader_fn)
    {
        depth_ranges_X ranges;
        ranges.clear(reader_fn(inf));
        const int count = unmarshallShort(inf);
        for (int i = 0; i < count; ++i)
            ranges.depth_range_Xs.push_back(
                depth_range_X<X>::read(inf, reader_fn));
        return ranges;
    }
    template <typename writer_fn_type>
    void write(writer &outf, writer_fn_type writer_fn) const
    {
        writer_fn(outf, default_thing);
        marshallShort(outf, depth_range_Xs.size());
        for (int i = 0, size = depth_range_Xs.size(); i < size; ++i)
            depth_range_Xs[i].write(outf, writer_fn);
    }
};

/////////////////////////////////////////////////////////////////////////////
// map_def: map definitions for maps loaded from .des files.
//
// Please read this before changing map_def.
//
// When adding Lua-visible fields to map_def, note that there are two
// kinds of fields:
//
// * Fields that determine placement of the map, or are unchanging,
//   such as "place", "depths" (determine placement), or "name" (does
//   not change between different evaluations of the map). Such fields
//   must be reset to their default values in map_def::init() if they
//   determine placement, or just initialised in the constructor if
//   they will not change.
//
// * Fields that do not determine placement and may change between
//   different uses of the map (such as "mons", "items",
//   "level_flags", etc.). Such fields must be reset to their default
//   values in map_def::reinit(), which is called before the map is
//   used.
//
// If you do not do this, maps will not work correctly, and will break
// in obscure, hard-to-find ways. The level-compiler will not (cannot)
// warn you.
//
class map_def
{
public:
    std::string     name;
    // Description for the map that can be shown to players.
    std::string     description;
    std::string     tags;
    level_id        place;

    depth_ranges     depths;
    map_section_type orient;

    typedef depth_ranges_X<map_chance> range_chance_t;
    typedef depth_ranges_X<int> range_weight_t;

    range_chance_t   _chance;
    range_weight_t   _weight;

    int              weight_depth_mult;
    int              weight_depth_div;

    std::vector<std::string> welcome_messages;

    map_lines       map;
    mons_list       mons;
    item_list       items;

    static bool valid_item_array_glyph(int gly);
    static int item_array_glyph_to_slot(int gly);
    static bool valid_monster_array_glyph(int gly);
    static bool valid_monster_glyph(int gly);
    static int monster_array_glyph_to_slot(int gly);

    std::vector<mons_spec> random_mons;

    map_flags       level_flags, branch_flags;

    dlua_chunk      prelude, mapchunk, main, validate, veto, epilogue;

    map_file_place  place_loaded_from;

    map_def         *original;

    uint8_t         rock_colour, floor_colour;
    std::string     rock_tile, floor_tile;

    dungeon_feature_type border_fill_type;
private:
    // This map has been loaded from an index, and not fully realised.
    bool            index_only;
    mutable long    cache_offset;
    std::string     file;

    typedef Matrix<bool> subvault_mask;
    subvault_mask *svmask;

    // True if this map is in the process of being validated.
    bool validating_map_flag;

public:
    map_def();

    std::string desc_or_name() const;

    std::string describe() const;
    void init();
    void reinit();

    void load();
    void strip();

    int weight(const level_id &lid) const;
    map_chance chance(const level_id &lid) const;

    bool in_map(const coord_def &p) const;
    bool map_already_used() const;

    coord_def size() const { return coord_def(map.width(), map.height()); }

    std::vector<coord_def> find_glyph(int glyph) const;
    coord_def find_first_glyph(int glyph) const;
    coord_def find_first_glyph(const std::string &glyphs) const;

    void write_index(writer&) const;
    void write_full(writer&) const;
    void write_maplines(writer &) const;

    void read_index(reader&);
    void read_full(reader&, bool check_cache_version);
    void read_maplines(reader&);

    void set_file(const std::string &s);
    std::string run_lua(bool skip_main);
    bool run_hook(const std::string &hook_name, bool die_on_lua_error = false);
    bool run_postplace_hook(bool die_on_lua_error = false);
    void copy_hooks_from(const map_def &other_map,
                         const std::string &hook_name);


    // Returns true if the validation passed.
    bool test_lua_validate(bool croak = false);

    // Returns true if *not* vetoed, i.e., the map is good to go.
    bool test_lua_veto();

    // Executes post-generation lua code.
    bool run_lua_epilogue(bool croak = false);

    std::string validate_map_def(const depth_ranges &);
    std::string validate_temple_map();
    // Returns true if this map is in the middle of validation.
    bool is_validating() const { return (validating_map_flag); }

    void add_prelude_line(int line,  const std::string &s);
    void add_main_line(int line, const std::string &s);

    void hmirror();
    void vmirror();
    void rotate(bool clockwise);
    void normalise();
    std::string resolve();
    void fixup();

    bool is_usable_in(const level_id &lid) const;

    const keyed_mapspec *mapspec_at(const coord_def &c) const;
    keyed_mapspec *mapspec_at(const coord_def &c);

    bool has_depth() const;
    void add_depth(const level_range &depth);
    void add_depths(const depth_ranges &depth);

    bool can_dock(map_section_type) const;
    coord_def dock_pos(map_section_type) const;
    coord_def float_dock();
    coord_def float_place();
    coord_def float_aligned_place() const;
    coord_def float_random_place() const;

    std::vector<coord_def> anchor_points() const;

    bool is_minivault() const;
    bool is_overwritable_layout() const;
    bool has_tag(const std::string &tag) const;
    bool has_tag_prefix(const std::string &tag) const;
    bool has_tag_suffix(const std::string &suffix) const;

    template <typename TagIterator>
    bool has_any_tag(TagIterator begin, TagIterator end) const
    {
        for ( ; begin != end; ++begin)
            if (has_tag(*begin))
                return true;
        return false;
    }

    std::vector<std::string> get_tags() const;

    std::vector<std::string> get_shuffle_strings() const;
    std::vector<std::string> get_subst_strings() const;

    int glyph_at(const coord_def &c) const;

    // Subvault functions.
    std::string subvault_from_tagstring(const std::string &s);
    bool is_subvault() const;
    void apply_subvault_mask();
    bool subvault_cell_valid(const coord_def &c) const;
    int subvault_width() const;
    int subvault_height() const;
    // Returns the number of cells in the subvault map that
    // will not get copied to parent vault at a given placement.
    int subvault_mismatch_count(const coord_def &place) const;

public:
    struct map_feature_finder
    {
        map_def &map;
        map_feature_finder(map_def &map_) : map(map_) { }
        // This may actually modify the underlying map by fixing KFEAT:
        // feature slots, but that's fine by us.
        dungeon_feature_type operator () (const coord_def &c) const
        {
            return (map_feature_at(&map, c, -1));
        }
    };

    struct map_bounds_check
    {
        map_def &map;
        map_bounds_check(map_def &map_) : map(map_) { }
        bool operator () (const coord_def &c) const
        {
            return (c.x >= 0 && c.x < map.map.width()
                    && c.y >= 0 && c.y < map.map.height());
        }
    };

private:
    void write_depth_ranges(writer&) const;
    void read_depth_ranges(reader&);
    bool test_lua_boolchunk(dlua_chunk &, bool def = false, bool croak = false);
    std::string rewrite_chunk_errors(const std::string &s) const;
    std::string apply_subvault(string_spec &);
    std::string validate_map_placeable();
};

const int CHANCE_ROLL = 10000;

void map_register_flag(const std::string &flag);

std::string escape_string(std::string in, const std::string &toesc,
                          const std::string &escapewith);

std::string mapdef_split_key_item(const std::string &s,
                                  std::string *key,
                                  int *separator,
                                  std::string *arg,
                                  int key_max_len = 1);

const char *map_section_name(int msect);

int store_tilename_get_index(const std::string tilename);
#endif
