/*
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

#include "enum.h"
#include "externs.h"

enum map_flags
{
    MAPF_PANDEMONIUM_VAULT = 0x01,     // A pandemonium minivault.

    MAPF_MIRROR_VERTICAL   = 0x10,     // The map may be mirrored vertically
    MAPF_MIRROR_HORIZONTAL = 0x20,     // may be mirrored horizontally.
    MAPF_ROTATE            = 0x40      // may be rotated
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

    bool valid() const;
    int span() const;

    std::string describe() const;
    std::string str_depth_range() const;

    operator raw_range () const;
    operator std::string () const
    {
        return describe();
    }
};

typedef std::pair<int,int> glyph_weighted_replacement_t;
typedef std::vector<glyph_weighted_replacement_t> glyph_replacements_t;

class map_lines;
class map_transformer
{
public:
    virtual ~map_transformer() = 0;
    virtual void apply_transform(map_lines &map) = 0;
    virtual map_transformer *clone() const = 0;
};

class subst_spec : public map_transformer
{
public:
    subst_spec(int torepl, bool fix, const glyph_replacements_t &repls);

    int key() const
    {
        return (foo);
    }
    
    int value();
    
    void apply_transform(map_lines &map);
    map_transformer *clone() const;

private:
    int foo;        // The thing to replace.
    bool fix;       // If true, the first replacement fixes the value.
    int frozen_value;
    
    glyph_replacements_t repl;
};

struct shuffle_spec : public map_transformer
{
    std::string shuffle;

    shuffle_spec(const std::string &spec)
        : shuffle(spec)
    {
    }
    
    void apply_transform(map_lines &map);
    map_transformer *clone() const;
};

class map_lines
{
public:
    map_lines();
    map_lines(const map_lines &);
    ~map_lines();

    map_lines &operator = (const map_lines &);

    void add_line(const std::string &s);
    std::string add_subst(const std::string &st);
    std::string add_shuffle(const std::string &s);

    void set_orientation(const std::string &s);

    int width() const;
    int height() const;

    int glyph(int x, int y) const;
    bool is_solid(int gly) const;
    
    bool solid_borders(map_section_type border);

    void apply_transforms();

    // Make all lines the same length.
    void normalise(char fillc = 'x');

    // Rotate 90 degrees either clockwise or anticlockwise
    void rotate(bool clockwise);
    void hmirror();
    void vmirror();

    void clear();

    const std::vector<std::string> &get_lines() const;

private:
    void init_from(const map_lines &map);
    void release_transforms();
    
    void resolve_shuffle(const std::string &shuffle);
    void subst(std::string &s, subst_spec &spec);
    void subst(subst_spec &);
    void check_borders();
    std::string shuffle(std::string s);
    std::string block_shuffle(const std::string &s);
    std::string check_shuffle(std::string &s);
    std::string check_block_shuffle(const std::string &s);
    std::string clean_shuffle(std::string s);
    std::string parse_glyph_replacements(std::string s,
                                         glyph_replacements_t &gly);

    friend class subst_spec;
    friend class shuffle_spec;
    
private:
    std::vector<map_transformer *> transforms;
    std::vector<std::string> lines;
    int map_width;
    bool solid_north, solid_east, solid_south, solid_west;
    bool solid_checked;
};

struct mons_spec
{
    int mid;
    int  genweight, mlevel;
    bool fix_mons;
    bool generate_awake;

    mons_spec(int id = RANDOM_MONSTER, int gw = 10, int ml = 0,
              bool _fixmons = false, bool awaken = false)
        : mid(id), genweight(gw), mlevel(ml), fix_mons(_fixmons),
          generate_awake(awaken)
    {
    }
};

class mons_list
{
public:
    mons_list();

    void clear();

    mons_spec get_monster(int index);

    // Returns an error string if the monster is unrecognised.
    std::string add_mons(const std::string &s, bool fix_slot = false);

    size_t size() const { return mons.size(); }

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
    int mons_by_name(std::string name) const;
    mons_spec_slot parse_mons_spec(std::string spec);
    mons_spec pick_monster(mons_spec_slot &slot);
    int fix_demon(int id) const;
    bool check_mimic(const std::string &s, int *mid, bool *fix) const;

private:
    std::vector< mons_spec_slot > mons;
    std::string error;
};

enum item_spec_type
{
    ISPEC_GOOD   = -2,
    ISPEC_SUPERB = -3
};

struct item_spec
{
    int genweight;
    
    object_class_type base_type;
    int sub_type;
    int allow_uniques;
    int level;
    int race;

    item_spec() : genweight(10), base_type(OBJ_RANDOM), sub_type(OBJ_RANDOM),
        allow_uniques(1), level(-1), race(MAKE_ITEM_RANDOM_RACE)
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

struct keyed_mapspec
{
public:
    int          key_glyph;
    
    feature_slot feat;
    item_list    item;
    mons_list    mons;

public:
    keyed_mapspec();

    std::string set_feat(const std::string &s, bool fix);
    std::string set_mons(const std::string &s, bool fix);
    std::string set_item(const std::string &s, bool fix);

    feature_spec get_feat();
    mons_list &get_monsters();
    item_list &get_items();

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

// Not providing a constructor to make life easy for C-style initialisation.
class map_def
{
public:
    std::string     name;
    std::string     tags;
    std::string     place;

    depth_ranges     depths;
    map_section_type orient;
    int             chance;
    long            flags;

    map_lines       map;
    mons_list       mons;
    item_list       items;

    keyed_specs     keyspecs;

public:
    void init();
    void hmirror();
    void vmirror();
    void rotate(bool clockwise);
    void normalise();
    void resolve();
    void fixup();

    bool is_usable_in(const level_id &lid) const;
    
    keyed_mapspec *mapspec_for_key(int key);

    bool has_depth() const;
    void add_depth(const level_range &depth);
    void add_depths(depth_ranges::const_iterator s,
                    depth_ranges::const_iterator e);
    
    std::string add_key_item(const std::string &s);
    std::string add_key_mons(const std::string &s);
    std::string add_key_feat(const std::string &s);
    
    bool can_dock(map_section_type) const;
    coord_def dock_pos(map_section_type) const;
    coord_def float_dock();
    coord_def float_place();
    coord_def float_random_place() const;

    bool is_minivault() const;
    bool has_tag(const std::string &tag) const;
    bool has_tag_prefix(const std::string &tag) const;

private:
    std::string add_key_field(
        const std::string &s,
        std::string (keyed_mapspec::*set_field)(
            const std::string &s, bool fixed));
};

std::string escape_string(std::string in, const std::string &toesc,
                          const std::string &escapewith);

#endif
