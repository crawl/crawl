/*
 * mapdef.h:
 * Header for map structures used by the level compiler.
 * Created by: dshaligram on Wed Nov 22 08:41:20 2006 UTC
 *
 * Modified for Crawl Reference by $Author$ on $Date$
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

#include "luadgn.h"
#include "enum.h"
#include "externs.h"
#include "makeitem.h"
#include "travel.h"
#include "FixAry.h"

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
class map_transformer
{
public:
    enum transform_type
    {
        TT_SHUFFLE,
        TT_SUBST,
        TT_NSUBST,
        TT_MARKER,
        TT_COLOUR,
        TT_ROCKTILE,
        TT_FLOORTILE
    };

public:
    virtual ~map_transformer() = 0;
    virtual std::string apply_transform(map_lines &map) = 0;
    virtual transform_type type() const = 0;
    virtual std::string describe() const = 0;
};

class subst_spec : public map_transformer
{
public:
    subst_spec(int torepl, bool fix, const glyph_replacements_t &repls);
    subst_spec() : foo(0), fix(false), frozen_value(0), repl() { }

    int key() const
    {
        return (foo);
    }

    int value();

    std::string apply_transform(map_lines &map);
    transform_type type() const;
    std::string describe() const;

    bool operator == (const subst_spec &other) const;

private:
    int foo;        // The thing to replace.
    bool fix;       // If true, the first replacement fixes the value.
    int frozen_value;

    glyph_replacements_t repl;
};

class nsubst_spec : public map_transformer
{
public:
    nsubst_spec(int key, const std::vector<subst_spec> &specs);
    std::string apply_transform(map_lines &map);
    transform_type type() const { return TT_NSUBST; }
    std::string describe() const;

public:
    int key;
    std::vector<subst_spec> specs;
};

typedef std::pair<int, int> map_weighted_colour;
class map_colour_list : public std::vector<map_weighted_colour>
{
public:
    bool parse(const std::string &s, int weight);
};
class colour_spec : public map_transformer
{
public:
    colour_spec(int _key, bool _fix, const map_colour_list &clist)
        : key(_key), fix(_fix), fixed_colour(BLACK), colours(clist)
    {
    }
    std::string apply_transform(map_lines &map);
    transform_type type() const { return TT_COLOUR; }
    std::string describe() const;

    int get_colour();

public:
    int key;
    bool fix;
    int fixed_colour;
    map_colour_list colours;
};

#ifdef USE_TILE
typedef std::pair<int, int> map_weighted_tile;
class map_tile_list : public std::vector<map_weighted_colour>
{
public:
    bool parse(const std::string &s, int weight);
};
class tile_spec : public map_transformer
{
public:
    tile_spec(int _key, bool _fix, bool _floor, const map_tile_list &_tiles)
        : key(_key), fix(_fix), chose_fixed(false), floor(_floor),
          fixed_tile(0), tiles(_tiles)
    {
    }

    std::string apply_transform(map_lines &map);
    transform_type type() const { return (floor ? TT_FLOORTILE : TT_ROCKTILE); }
    std::string describe() const;

    int get_tile();

public:
    int key;
    bool fix;
    bool chose_fixed;
    bool floor;
    int fixed_tile;
    map_tile_list tiles;
};
#endif

class shuffle_spec : public map_transformer
{
 public:
    std::string shuffle;

    shuffle_spec(const std::string &spec)
        : shuffle(spec)
    {
    }

    std::string apply_transform(map_lines &map);
    transform_type type() const;
    std::string describe() const;
    bool operator == (const shuffle_spec &other) const
    {
        return (shuffle == other.shuffle);
    }
};

class map_marker_spec : public map_transformer
{
public:
    int key;
    std::string marker;

    // Special handling for Lua markers:
    std::auto_ptr<lua_datum> lua_fn;

    map_marker_spec(int _key, const std::string &mark)
        : key(_key), marker(mark), lua_fn() { }

    map_marker_spec(int _key, const lua_datum &fn)
        : key(_key), marker(), lua_fn(new lua_datum(fn)) { }

    std::string apply_transform(map_lines &map);
    transform_type type() const;
    std::string describe() const;

private:
    map_marker *create_marker();
};

class map_def;
class map_lines
{
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
    void remove_shuffle(const std::string &s);
    void remove_subst(const std::string &s);
    void clear_shuffles();
    void clear_substs();
    void clear_nsubsts();
    void clear_markers();
    void clear_colours();

#ifdef USE_TILE
    std::string add_floortile(const std::string &s);
    std::string add_rocktile(const std::string &s);
    void clear_rocktiles();
    void clear_floortiles();
#endif

    std::vector<coord_def> find_glyph(int glyph) const;
    coord_def find_first_glyph(int glyph) const;
    coord_def find_first_glyph(const std::string &glyphs) const;

    void set_orientation(const std::string &s);

    int width() const;
    int height() const;
    coord_def size() const;

    int glyph(int x, int y) const;
    int glyph(const coord_def &) const;
    bool is_solid(int gly) const;

    bool solid_borders(map_section_type border);

    std::string apply_transforms();

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
    std::vector<std::string> get_shuffle_strings() const;
    std::vector<std::string> get_subst_strings() const;

    int operator () (const coord_def &c) const;

private:
    void init_from(const map_lines &map);
    void clear_transforms();
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
    void overlay_colours(colour_spec &);
#ifdef USE_TILE
    void overlay_tiles(tile_spec &);
#endif
    void check_borders();
    void clear_transforms(map_transformer::transform_type);
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
    template<class T>
    std::string parse_weighted_str(const std::string &cspec, T &list);
#ifdef USE_TILE
    std::string add_tile(const std::string &sub, bool is_floor);
#endif

    friend class subst_spec;
    friend class nsubst_spec;
    friend class shuffle_spec;
    friend class map_marker_spec;
    friend class colour_spec;
    friend class tile_spec;

private:
    std::vector<map_transformer *> transforms;
    std::vector<map_marker *> markers;
    std::vector<std::string> lines;
    struct overlay_def
    {
        overlay_def() : colour(0), rocktile(0), floortile(0) {}
        int colour;
        int rocktile;
        int floortile;
    };
    typedef Matrix<overlay_def> overlay_matrix;
    std::auto_ptr<overlay_matrix> overlay;

    int map_width;
    bool solid_north, solid_east, solid_south, solid_west;
    bool solid_checked;
};

enum item_spec_type
{
    ISPEC_GOOD    = -2,
    ISPEC_SUPERB  = -3,
    ISPEC_DAMAGED = -4,
    ISPEC_BAD     = -5
};

struct item_spec
{
    int genweight;

    object_class_type base_type;
    int sub_type;
    int plus, plus2;
    int ego;
    int allow_uniques;
    int level;
    int race;
    int qty;
    level_id place;

    item_spec() : genweight(10), base_type(OBJ_RANDOM), sub_type(OBJ_RANDOM),
        plus(-1), plus2(-1), ego(0), allow_uniques(1), level(-1),
        race(MAKE_ITEM_RANDOM_RACE), qty(0)
    {
    }
};
typedef std::vector<item_spec> item_spec_list;

class item_list
{
public:
    item_list() : items() { }

    void clear();

    item_spec get_item(int index);
    size_t size() const { return items.size(); }

    std::string add_item(const std::string &spec, bool fix = false);
    std::string set_item(int index, const std::string &spec);

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
    item_spec parse_single_spec(std::string s);
    void parse_raw_name(std::string name, item_spec &spec);
    void parse_random_by_class(std::string c, item_spec &spec);
    item_spec pick_item(item_spec_slot &slot);

private:
    std::vector<item_spec_slot> items;
    std::string error;
};

class mons_spec
{
 public:
    int  mid;
    level_id place;
    monster_type monbase;     // Base monster for zombies and dracs.
    mon_attitude_type attitude;
    int  number;              // Head count for hydras
    int  quantity;            // Number of monsters (usually 1).
    int  genweight, mlevel;
    bool fix_mons;
    bool generate_awake;
    bool patrolling;
    bool band;
    int  colour;

    item_list items;

    mons_spec(int id = RANDOM_MONSTER,
              monster_type base = MONS_PROGRAM_BUG,
              int num = 0,
              int gw = 10, int ml = 0,
              bool _fixmons = false, bool awaken = false, bool patrol = false)
        : mid(id), place(), monbase(base), attitude(ATT_HOSTILE), number(num),
          quantity(1), genweight(gw), mlevel(ml), fix_mons(_fixmons),
          generate_awake(awaken), patrolling(false), band(false),
          colour(BLACK), items()
    {
    }
};

class mons_list
{
public:
    mons_list();

    void clear();

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
    mons_spec get_zombified_monster(const std::string &name,
                                    monster_type zomb) const;
    mons_spec_slot parse_mons_spec(std::string spec);
    mons_spec pick_monster(mons_spec_slot &slot);
    int fix_demon(int id) const;
    bool check_mimic(const std::string &s, int *mid, bool *fix) const;

private:
    std::vector< mons_spec_slot > mons;
    std::string error;
};

struct feature_spec
{
    int genweight;
    int feat;
    int shop;
    int trap;
    int glyph;

    feature_spec(int f, int wt = 10)
        : genweight(wt), feat(f), shop(-1),
          trap(-1), glyph(-1)
    { }
    feature_spec() : genweight(0), feat(0), shop(-1), trap(-1), glyph(-1) { }
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

    std::string set_feat(const std::string &s, bool fix);
    std::string set_mons(const std::string &s, bool fix);
    std::string set_item(const std::string &s, bool fix);
    std::string set_mask(const std::string &s, bool garbage);

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

typedef std::map<int, keyed_mapspec> keyed_specs;

typedef std::vector<level_range> depth_ranges;

class map_def;
struct dlua_set_map
{
    dlua_set_map(map_def *map);
    ~dlua_set_map();
};

class map_def;
dungeon_feature_type map_feature(map_def *map, const coord_def &c, int rawfeat);

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
    std::string     tags;
    level_id        place;

    depth_ranges     depths;
    map_section_type orient;

    int              chance_priority;
    int              chance;
    int              weight;
    int              weight_depth_mult;
    int              weight_depth_div;

    std::vector<std::string> welcome_messages;

    map_lines       map;
    mons_list       mons;
    item_list       items;

    std::vector<mons_spec> random_mons;

    map_flags       level_flags, branch_flags;

    keyed_specs     keyspecs;

    dlua_chunk      prelude, main, validate, veto;

    map_file_place  place_loaded_from;

    map_def         *original;

    unsigned char   rock_colour, floor_colour;
    int             rock_tile, floor_tile;

private:
    // This map has been loaded from an index, and not fully realised.
    bool            index_only;
    long            cache_offset;
    std::string     file;

public:
    map_def();
    void init();
    void reinit();

    void load();

    bool in_map(const coord_def &p) const;

    coord_def size() const { return coord_def(map.width(), map.height()); }

    std::vector<coord_def> find_glyph(int glyph) const;
    coord_def find_first_glyph(int glyph) const;
    coord_def find_first_glyph(const std::string &glyphs) const;

    void write_index(writer&) const;
    void write_full(writer&);

    void read_index(reader&);
    void read_full(reader&);

    void set_file(const std::string &s);
    std::string run_lua(bool skip_main);

    // Returns true if the validation passed.
    bool test_lua_validate(bool croak = false);

    // Returns true if *not* vetoed, i.e., the map is good to go.
    bool test_lua_veto();

    std::string validate_map_def();

    void add_prelude_line(int line,  const std::string &s);
    void add_main_line(int line, const std::string &s);

    void hmirror();
    void vmirror();
    void rotate(bool clockwise);
    void normalise();
    std::string resolve();
    void fixup();

    bool is_usable_in(const level_id &lid) const;

    keyed_mapspec *mapspec_for_key(int key);
    const keyed_mapspec *mapspec_for_key(int key) const;

    bool has_depth() const;
    void add_depth(const level_range &depth);
    void add_depths(depth_ranges::const_iterator s,
                    depth_ranges::const_iterator e);

    std::string add_key_item(const std::string &s);
    std::string add_key_mons(const std::string &s);
    std::string add_key_feat(const std::string &s);
    std::string add_key_mask(const std::string &s);

    bool can_dock(map_section_type) const;
    coord_def dock_pos(map_section_type) const;
    coord_def float_dock();
    coord_def float_place();
    coord_def float_aligned_place() const;
    coord_def float_random_place() const;

    std::vector<coord_def> anchor_points() const;

    bool is_minivault() const;
    bool has_tag(const std::string &tag) const;
    bool has_tag_prefix(const std::string &tag) const;
    bool has_tag_suffix(const std::string &suffix) const;
    std::vector<std::string> get_tags() const;

    std::vector<std::string> get_shuffle_strings() const;
    std::vector<std::string> get_subst_strings() const;

    int glyph_at(const coord_def &c) const;

public:
    struct map_feature_finder
    {
        map_def &map;
        map_feature_finder(map_def &map_) : map(map_) { }
        // This may actually modify the underlying map by fixing KFEAT:
        // feature slots, but that's fine by us.
        dungeon_feature_type operator () (const coord_def &c) const
        {
            return (map_feature(&map, c, -1));
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

    std::string add_key_field(
        const std::string &s,
        std::string (keyed_mapspec::*set_field)(
            const std::string &s, bool fixed));
};

const int CHANCE_ROLL = 10000;

std::string escape_string(std::string in, const std::string &toesc,
                          const std::string &escapewith);

std::string mapdef_split_key_item(const std::string &s,
                                  std::string *key,
                                  int *separator,
                                  std::string *arg,
                                  int key_max_len = 1);

const char *map_section_name(int msect);

#endif
