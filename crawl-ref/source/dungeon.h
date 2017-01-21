/**
 * @file
 * @brief Functions used when building new levels.
**/

#ifndef DUNGEON_H
#define DUNGEON_H

#include <algorithm>
#include <set>
#include <stdexcept>
#include <vector>

#include "env.h"
#include "mapdef.h"

#define BUILD_METHOD_KEY "build_method_key"
#define LAYOUT_TYPE_KEY  "layout_type_key"

// See _build_overflow_temples() in dungeon.cc for details on overflow
// temples.
#define TEMPLE_GODS_KEY      "temple_gods_key"
#define OVERFLOW_TEMPLES_KEY "overflow_temples_key"
#define TEMPLE_MAP_KEY       "temple_map_key"
#define TEMPLE_SIZE_KEY      "temple_size_key"

const unsigned short INVALID_MAP_INDEX = 10000;

// Should be the larger of GXM/GYM
#define MAP_SIDE ((GXM) > (GYM) ? (GXM) : (GYM))

// Map mask constants.

// Be sure to change flag_list in keyed_mapspec::set_mask to match!
enum map_mask_type
{
    MMT_NONE            = 0x0,
    MMT_VAULT           = 0x01,  // This is a square in a vault.
    MMT_NO_ITEM         = 0x02,  // Random items should not be placed here.
    MMT_NO_MONS         = 0x04,  // Random monsters should not be placed here.
    MMT_NO_POOL         = 0x08,  // Pool fixup should not be applied here.
                       // 0x10,  // Unused
    MMT_NO_WALL         = 0x20,  // Wall fixup should not be applied here.
    MMT_OPAQUE          = 0x40,  // Vault may impede connectivity.
    MMT_NO_TRAP         = 0x80,  // No trap generation
    MMT_MIMIC           = 0x100, // Feature mimics
    MMT_NO_MIMIC        = 0x200, // Feature shouldn't be turned into a mimic.
#if TAG_MAJOR_VERSION == 34
    MMT_WAS_DOOR_MIMIC  = 0x400, // There was a door mimic there.
#endif
    MMT_TURNED_TO_FLOOR = 0x800, // This feature was dug, deconstructed or such.
};

class dgn_region;
typedef vector<dgn_region> dgn_region_list;

struct dgn_veto_exception : public runtime_error
{
    dgn_veto_exception(const string& msg) : runtime_error(msg) {}
    dgn_veto_exception(const char *msg) : runtime_error(msg) {}
};

class dgn_region
{
public:
    // pos is top-left corner.
    coord_def pos, size;

    dgn_region(int left, int top, int width, int height)
        : pos(left, top), size(width, height)
    {
    }

    dgn_region(const coord_def &_pos, const coord_def &_size)
        : pos(_pos), size(_size)
    {
    }

    dgn_region() : pos(-1, -1), size()
    {
    }

    coord_def end() const
    {
        return pos + size - coord_def(1, 1);
    }

    coord_def random_edge_point() const;
    coord_def random_point() const;

    static dgn_region absolute(int left, int top, int right, int bottom)
    {
        return dgn_region(left, top, right - left + 1, bottom - top + 1);
    }

    static dgn_region absolute(const coord_def &c1, const coord_def &c2)
    {
        return dgn_region(c1.x, c1.y, c2.x, c2.y);
    }

    static bool between(int val, int low, int high)
    {
        return val >= low && val <= high;
    }

    bool contains(const coord_def &p) const
    {
        return contains(p.x, p.y);
    }

    bool contains(int xp, int yp) const
    {
        return xp >= pos.x && xp < pos.x + size.x
               && yp >= pos.y && yp < pos.y + size.y;
    }

    bool fully_contains(const coord_def &p) const
    {
        return p.x > pos.x && p.x < pos.x + size.x - 1
               && p.y > pos.y && p.y < pos.y + size.y - 1;
    }

    bool overlaps(const dgn_region &other) const;
    bool overlaps_any(const dgn_region_list &others) const;
    bool overlaps(const dgn_region_list &others,
                  const map_mask &dgn_map_mask) const;
    bool overlaps(const map_mask &dgn_map_mask) const;
};

struct vault_placement
{
public:
    coord_def pos;
    coord_def size;

    map_section_type orient;
    map_def map;
    vector<coord_def> exits;

    // The PC has seen at least one square of this vault.
    bool seen;

public:
    vault_placement();
    void reset();
    void apply_grid();
    void draw_at(const coord_def &c);
    int connect(bool spotty = false) const;
    string map_name_at(const coord_def &c) const;
    dungeon_feature_type feature_at(const coord_def &c);
    bool is_exit(const coord_def &c);
    bool is_space(const coord_def &c);
};

class vault_place_iterator
{
public:
    vault_place_iterator(const vault_placement &vp);
    operator bool () const;
    coord_def operator * () const;
    const coord_def *operator -> () const;
    vault_place_iterator &operator ++ ();
    vault_place_iterator operator ++ (int);
private:
    const vault_placement &vault_place;
    coord_def pos;
    coord_def tl, br;
};

class unwind_vault_placement_mask
{
public:
    unwind_vault_placement_mask(const map_bitmask *mask);
    ~unwind_vault_placement_mask();
private:
    const map_bitmask *oldmask;
};

extern vector<vault_placement> Temp_Vaults;

extern const map_bitmask *Vault_Placement_Mask;

void init_level_connectivity();
void read_level_connectivity(reader &th);
void write_level_connectivity(writer &th);

bool builder(bool enable_random_maps = true,
             dungeon_feature_type dest_stairs_type = NUM_FEATURES);

void dgn_clear_vault_placements();
void dgn_erase_unused_vault_placements();
void dgn_flush_map_memory();

double dgn_degrees_to_radians(int degrees);
bool dgn_has_adjacent_feat(coord_def c, dungeon_feature_type feat);
coord_def dgn_random_point_in_margin(int margin);
coord_def dgn_random_point_from(const coord_def &c, int radius, int margin = 1);
coord_def dgn_find_feature_marker(dungeon_feature_type feat);

// Generate 3 stone stairs in both directions.
void dgn_place_stone_stairs(bool maybe_place_hatches = false);

void dgn_set_grid_colour_at(const coord_def &c, int colour);

const vault_placement *dgn_place_map(const map_def *map,
                                     bool check_collision,
                                     bool make_no_exits,
                                     const coord_def &pos = INVALID_COORD);

const vault_placement *dgn_safe_place_map(const map_def *map,
                                          bool check_collision,
                                          bool make_no_exits,
                                          const coord_def &pos = INVALID_COORD);

void level_clear_vault_memory();
void run_map_epilogues();

void place_specific_trap(const coord_def& where, trap_type trap_spec, int charges = 0);

struct shop_spec;
void place_spec_shop(const coord_def& where, shop_type force_type);
void place_spec_shop(const coord_def& where, shop_spec &spec, int shop_level = 0);
int greed_for_shop_type(shop_type shop, int level_number);
object_class_type item_in_shop(shop_type shop_type);
bool seen_destroy_feat(dungeon_feature_type old_feat);
bool map_masked(const coord_def &c, unsigned mask);
coord_def dgn_find_nearby_stair(dungeon_feature_type stair_to_find,
                                coord_def base_pos, bool find_closest);

class mons_spec;
monster *dgn_place_monster(mons_spec &mspec, coord_def where,
                           bool force_pos = false, bool generate_awake = false,
                           bool patrolling = false);
int dgn_place_item(const item_spec &spec,
                   const coord_def &where,
                   int level = INVALID_ABSDEPTH);

class item_list;
void dgn_place_multiple_items(item_list &list,
                              const coord_def& where);

void dgn_set_branch_epilogue(branch_type br, string callback_name);

void dgn_reset_level(bool enable_random_maps = true);

const vault_placement *dgn_register_place(const vault_placement &place,
                                          bool register_vault);

// Count number of mutually isolated zones. If choose_stairless, only count
// zones with no stairs in them. If fill is set to anything other than
// DNGN_UNSEEN, chosen zones will be filled with the provided feature.
int dgn_count_disconnected_zones(
    bool choose_stairless,
    dungeon_feature_type fill = DNGN_UNSEEN);

void dgn_replace_area(const coord_def& p1, const coord_def& p2,
                      dungeon_feature_type replace,
                      dungeon_feature_type feature,
                      unsigned mmask = 0, bool needs_update = false);
void dgn_replace_area(int sx, int sy, int ex, int ey,
                      dungeon_feature_type replace,
                      dungeon_feature_type feature,
                      unsigned mmask = 0, bool needs_update = false);

vault_placement *dgn_vault_at(coord_def gp);
void dgn_seen_vault_at(coord_def gp);

int count_neighbours(int x, int y, dungeon_feature_type feat);
static inline int count_neighbours(const coord_def& p, dungeon_feature_type feat)
{
    return count_neighbours(p.x, p.y, feat);
}

string dump_vault_maps();

bool dgn_square_travel_ok(const coord_def &c);

// Resets travel_point_distance!
vector<coord_def> dgn_join_the_dots_pathfind(const coord_def &from,
                                             const coord_def &to,
                                             uint32_t mapmask);

bool join_the_dots(const coord_def &from, const coord_def &to,
                   unsigned mmask,
                   bool (*overwriteable)(dungeon_feature_type) = nullptr);
int count_feature_in_box(int x0, int y0, int x1, int y1,
                         dungeon_feature_type feat);
bool door_vetoed(const coord_def pos);

void fixup_misplaced_items();
#endif
