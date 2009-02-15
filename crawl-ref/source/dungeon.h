/*
 *  File:       dungeon.h
 *  Summary:    Functions used when building new levels.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef DUNGEON_H
#define DUNGEON_H

#include "FixVec.h"
#include "FixAry.h"
#include "externs.h"
#include "terrain.h"
#include "travel.h"
#include "stuff.h"
#include "mapdef.h"

#include <vector>
#include <set>
#include <algorithm>

#define BUILD_METHOD_KEY "build_method_key"
#define LAYOUT_TYPE_KEY  "layout_type_key"
#define LEVEL_VAULTS_KEY "level_vaults_key"
#define TEMP_VAULTS_KEY  "temp_vaults_key"
#define LEVEL_EXTRAS_KEY "level_extras_key"
#define LEVEL_ID_KEY     "level_id_key"

enum portal_type
{
    PORTAL_NONE = 0,
    PORTAL_LABYRINTH,
    PORTAL_HELL,
    PORTAL_ABYSS,
    PORTAL_PANDEMONIUM,
    NUM_PORTALS
};

const int MAKE_GOOD_ITEM = 351;

// Should be the larger of GXM/GYM
#define MAP_SIDE ( (GXM) > (GYM) ? (GXM) : (GYM) )

typedef FixedArray<unsigned short, GXM, GYM> map_mask;

// Map mask constants.

enum map_mask_type
{
    MMT_NONE       = 0x0,
    MMT_VAULT      = 0x01,    // This is a square in a vault.
    MMT_NO_ITEM    = 0x02,    // Random items should not be placed here.
    MMT_NO_MONS    = 0x04,    // Random monsters should not be placed here.
    MMT_NO_POOL    = 0x08,    // Pool fixup should not be applied here.
    MMT_NO_DOOR    = 0x10,    // No secret-doorisation.
    MMT_NO_WALL    = 0x20,    // Wall fixup should not be applied here.
    MMT_OPAQUE     = 0x40     // Vault may impede connectivity.
};

class dgn_region;
typedef std::vector<dgn_region> dgn_region_list;

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
        return (val >= low && val <= high);
    }

    bool contains(const coord_def &p) const
    {
        return contains(p.x, p.y);
    }

    bool contains(int xp, int yp) const
    {
        return (xp >= pos.x && xp < pos.x + size.x
                && yp >= pos.y && yp < pos.y + size.y);
    }

    bool fully_contains(const coord_def &p) const
    {
        return (p.x > pos.x && p.x < pos.x + size.x - 1
                && p.y > pos.y && p.y < pos.y + size.y - 1);
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

    int orient;
    map_def map;
    std::vector<coord_def> exits;

    int level_number, altar_count, num_runes;

    // If we're not placing runes, this is the substitute feature.
    int rune_subst;

public:
    vault_placement()
        : pos(-1, -1), size(0, 0), orient(0), map(),
          exits(), level_number(0), altar_count(0), num_runes(0),
          rune_subst(-1)
    {
    }

    void reset();
    void apply_grid();
    void draw_at(const coord_def &c);
};

extern map_mask dgn_Map_Mask;
extern bool Generating_Level;
extern std::string dgn_Layout_Type;

extern std::set<std::string> Level_Unique_Maps;
extern std::set<std::string> Level_Unique_Tags;

extern std::vector<vault_placement> Level_Vaults;
extern std::vector<vault_placement> Temp_Vaults;

//////////////////////////////////////////////////////////////////////////
template <typename fgrd, typename bound_check>
class flood_find : public travel_pathfind
{
public:
    flood_find(const fgrd &f, const bound_check &bc);

    void add_feat(int feat);
    void add_point(const coord_def &pos);
    coord_def find_first_from(const coord_def &c, const map_mask &vlts);
    bool points_connected_from(const coord_def &start);
    bool any_point_connected_from(const coord_def &start);
    bool has_exit_from(const coord_def &start);

    bool did_leave_vault() const { return left_vault; }

protected:
    bool path_flood(const coord_def &c, const coord_def &dc);
protected:
    bool point_hunt, want_exit;
    bool needed_features[NUM_FEATURES];
    std::vector<coord_def> needed_points;
    bool left_vault;
    const map_mask *vaults;

    const fgrd &fgrid;
    const bound_check &bcheck;
};

template <typename fgrd, typename bound_check>
flood_find<fgrd, bound_check>::flood_find(const fgrd &f, const bound_check &bc)
    : travel_pathfind(), point_hunt(false), want_exit(false),
      needed_features(), needed_points(), left_vault(true), vaults(NULL),
      fgrid(f), bcheck(bc)
{
    memset(needed_features, false, sizeof needed_features);
}

template <typename fgrd, typename bound_check>
void flood_find<fgrd, bound_check>::add_feat(int feat)
{
    if (feat >= 0 && feat < NUM_FEATURES)
        needed_features[feat] = true;
}

template <typename fgrd, typename bound_check>
coord_def
flood_find<fgrd, bound_check>::find_first_from(
    const coord_def &c,
    const map_mask &vlts)
{
    set_floodseed(c);
    vaults = &vlts;
    return pathfind(RMODE_EXPLORE);
}

template <typename fgrd, typename bound_check>
void flood_find<fgrd, bound_check>::add_point(const coord_def &c)
{
    needed_points.push_back(c);
}

template <typename fgrd, typename bound_check>
bool flood_find<fgrd, bound_check>::points_connected_from(
    const coord_def &sp)
{
    if (needed_points.empty())
        return (true);
    set_floodseed(sp);
    pathfind(RMODE_EXPLORE);
    return (needed_points.empty());
}

template <typename fgrd, typename bound_check>
bool flood_find<fgrd, bound_check>::any_point_connected_from(
    const coord_def &sp)
{
    if (needed_points.empty())
        return (true);
    set_floodseed(sp);
    const size_t sz = needed_points.size();
    pathfind(RMODE_EXPLORE);
    return (needed_points.size() < sz);
}

template <typename fgrd, typename bound_check>
bool flood_find<fgrd, bound_check>::has_exit_from(
    const coord_def &sp)
{
    want_exit = true;
    set_floodseed(sp);
    return (pathfind(RMODE_EXPLORE) == coord_def(-1, -1));
}

template <typename fgrd, typename bound_check>
bool flood_find<fgrd, bound_check>::path_flood(
    const coord_def &c,
    const coord_def &dc)
{
    if (!bcheck(dc))
    {
        if (want_exit)
        {
            greedy_dist = 100;
            greedy_place = coord_def(-1, -1);
            return (true);
        }
        return (false);
    }

    if (!needed_points.empty())
    {
        std::vector<coord_def>::iterator i =
            std::find(needed_points.begin(), needed_points.end(), dc);
        if (i != needed_points.end())
        {
            needed_points.erase(i);
            if (needed_points.empty())
                return (true);
        }
    }

    const dungeon_feature_type grid = fgrid(dc);

    if (grid == NUM_FEATURES)
    {
        if (want_exit)
        {
            greedy_dist = 100;
            greedy_place = coord_def(-1, -1);
            return (true);
        }
        return (false);
    }

    if (needed_features[ grid ])
    {
        unexplored_place = dc;
        unexplored_dist  = traveled_distance;
        return (true);
    }

    if (!is_traversable(grid)
        && grid != DNGN_SECRET_DOOR
        && !grid_is_trap(grid))
    {
        return (false);
    }

    if (!left_vault && vaults && !(*vaults)[dc.x][dc.y])
        left_vault = true;

    good_square(dc);

    return (false);
}
//////////////////////////////////////////////////////////////////////////

void init_level_connectivity();
void read_level_connectivity(reader &th);
void write_level_connectivity(writer &th);

bool builder(int level_number, int level_type);

coord_def dgn_find_feature_marker(dungeon_feature_type feat);

// Set floor/wall colour based on the mons_alloc array. Used for
// Abyss and Pan.
void dgn_set_colours_from_monsters();
void dgn_set_grid_colour_at(const coord_def &c, int colour);

bool dgn_place_map(const map_def *map, bool clobber, bool make_no_exits,
                   const coord_def &pos = coord_def(-1, -1));
void level_clear_vault_memory();
void level_welcome_messages();

bool place_specific_trap(const coord_def& where, trap_type spec_type);
void place_spec_shop(int level_number, const coord_def& where,
                     int force_s_type, bool representative = false);
void replace_area_wrapper(dungeon_feature_type old_feat,
                          dungeon_feature_type new_feat);
bool unforbidden(const coord_def &c, unsigned mask);
coord_def dgn_find_nearby_stair(dungeon_feature_type stair_to_find,
                                coord_def base_pos, bool find_closest);

class mons_spec;
int dgn_place_monster(mons_spec &mspec,
                      int monster_level, const coord_def& where,
                      bool force_pos = false, bool generate_awake = false,
                      bool patrolling = false);

class item_list;
void dgn_place_multiple_items(item_list &list,
                              const coord_def& where,
                              int level);

bool set_level_flags(unsigned long flags, bool silent = false);
bool unset_level_flags(unsigned long flags, bool silent = false);

void dgn_set_lt_callback(std::string level_type_name,
                         std::string callback_name);

void dgn_reset_level();

// Returns true if the given square is okay for use by any character,
// but always false for squares in non-transparent vaults. This
// function returns sane results only immediately after dungeon generation
// (specifically, saving and restoring a game discards information on the
// vaults used in the current level).
bool dgn_square_is_passable(const coord_def &c);

void dgn_register_place(const vault_placement &place, bool register_vault);

struct spec_room
{
    bool created;
    bool hooked_up;

    coord_def tl;
    coord_def br;

    spec_room() : created(false), hooked_up(false), tl(), br()
    {
    }

    coord_def random_spot() const;
};

bool join_the_dots(const coord_def &from, const coord_def &to,
                   unsigned mmask, bool early_exit = false);
void spotty_level(bool seeded, int iterations, bool boxy);
void smear_feature(int iterations, bool boxy, dungeon_feature_type feature,
                   int x1, int y1, int x2, int y2);
int process_disconnected_zones(int x1, int y1, int x2, int y2,
                               bool choose_stairless,
                               dungeon_feature_type fill);
bool octa_room(spec_room &sr, int oblique_max,
               dungeon_feature_type type_floor);

int count_feature_in_box(int x0, int y0, int x1, int y1,
                         dungeon_feature_type feat);
inline int count_feature_in_box( const coord_def& p1, const coord_def& p2,
                          dungeon_feature_type feat )
{
    return count_feature_in_box(p1.x, p1.y, p2.x, p2.y, feat);
}
int count_antifeature_in_box(int x0, int y0, int x1, int y1,
                             dungeon_feature_type feat);
int count_neighbours(int x, int y, dungeon_feature_type feat);
inline int count_neighbours(const coord_def& p, dungeon_feature_type feat)
{
  return count_neighbours(p.x, p.y, feat);
}

void remember_vault_placement(std::string key, vault_placement &place);
#endif
