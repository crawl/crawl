/*
 *  File:       dungeon.cc
 *  Summary:    Functions used when building new levels.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author: haranp $ on $Date: 2007-11-05 11:29:43 +0100 (Mon, 05 Nov 2007) $
 *
 *  Modified for Hexcrawl by Martin Bays, 2007
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef DUNGEON_H
#define DUNGEON_H

#include "FixVec.h"
#include "FixAry.h"
#include "externs.h"
#include "terrain.h"
#include "travel.h"
#include "stuff.h"
#include <vector>
#include <algorithm>

enum portal_type
{
    PORTAL_NONE = 0,
    PORTAL_LABYRINTH,
    PORTAL_HELL,
    PORTAL_ABYSS,
    PORTAL_PANDEMONIUM,
    NUM_PORTALS
};

enum special_room_type
{
    SROOM_LAIR_ORC,                    //    0
    SROOM_LAIR_KOBOLD,
    SROOM_TREASURY,
    SROOM_BEEHIVE,
    SROOM_JELLY_PIT,
    SROOM_MORGUE,
    NUM_SPECIAL_ROOMS                  //    5 - must remain final member {dlb}
};

const int MAKE_GOOD_ITEM = 351;

// Should be the larger of GXM/GYM
#define MAP_SIDE ( (GXM) > (GYM) ? (GXM) : (GYM) )

// This may sometimes be used as map_type[x][y] (for minivaults) and as
// map_type[y][x] for large-scale vaults. Keep an eye out for the associated
// brain-damage. [dshaligram]
typedef char map_type[MAP_SIDE + 1][MAP_SIDE + 1];
typedef FixedArray<unsigned short, GXM, GYM> map_mask;

extern map_mask dgn_map_mask;

// Map mask constants.

enum map_mask_type
{
    MMT_NONE       = 0x0,
    MMT_VAULT      = 0x01,    // This is a square in a vault.
    MMT_NO_ITEM    = 0x02,    // Random items should not be placed here.
    MMT_NO_MONS    = 0x04,    // Random monsters should not be placed here.
    MMT_NO_POOL    = 0x08,    // Pool fixup should not be applied here.
    MMT_NO_DOOR    = 0x10,    // No secret-doorisation.
    MMT_OPAQUE     = 0x20     // Vault may impede connectivity.
};

class dgn_region;
typedef std::vector<dgn_region> dgn_region_list;

struct dgn_region
{
    // pos is top-left corner.
    coord_def pos, size;
    int diamond_rot; // [hex] XXX: -1 means square
    int sZ, eZ; // [hex] XXX: diamond_rot == -1 && sZ == eZ == 0 means diamond

    // [hex] XXX: Some hackery here, I'm afraid. We use this same structure
    // for 3 different shapes:
    //
    // Rectangles in the square-coordinate sense (diamond_rot == -1); 
    // Diamonds (diamond_rot != -1; sZ == 0 == eZ);
    // Arbitrary hexes (diamond_rot != -1; sZ < 0)
    //
    // In all cases, we work with two relative co-ordinates x and y. For the
    // square case, these are the obvious things. For the diamond case, x is
    // dX and y is dY, but we then rotate by diamond_rot. The hex case is
    // considered as a diamond with extra bounds sZ <= -x-y <= eZ.
    //
    // TODO: eliminate remaining uses of square dgn_regions. Having hex-only
    // dgn_regions will allow much neater and more efficient algorithms. Then
    // we should probably also use a nicer representation for the regions, as
    // intersections of half-planes.

    dgn_region(int left, int top, int width, int height, int rot = -1, int in_sZ = 0, int in_eZ = 0)
        : pos(left, top), size(width, height), diamond_rot(rot), sZ(in_sZ), eZ(in_eZ)
    {
    }

    dgn_region(const hexcoord &p, int width, int height, int rot = -1,
	    int in_sZ = 0, int in_eZ = 0)
        : pos(p), size(width, height), diamond_rot(rot), sZ(in_sZ), eZ(in_eZ)
    {
    }

    dgn_region(const hexcoord &_pos, const coord_def &_size, int rot = -1, int
	    in_sZ = 0, int in_eZ = 0)
        : pos(_pos), size(_size), diamond_rot(rot), sZ(in_sZ), eZ(in_eZ)
    {
    }


    dgn_region() : pos(-1, -1), size(), diamond_rot(-1), sZ(0), eZ(0)
    {
    }

    hexcoord pos_in(int x, int y) const
    {
	if (diamond_rot == -1)
	    return pos + coord_def(x,y);
	else
	    return pos.tohex() + ( hexdir::u*x +
		    (-hexdir::w)*y ).rotated(diamond_rot);
    }

    hexcoord pos_in(const coord_def &p) const
    {
	return pos_in(p.x, p.y);
    }

    // [hex]: place.pos_in(place.abs_to_rel(foo)) == foo
    coord_def abs_to_rel(const hexcoord &p) const
    {
	if (diamond_rot == -1)
	    return (coord_def)p - pos;
	else
	{
	    hexdir d = (p - pos.tohex()).rotated(-diamond_rot);
	    return coord_def(d.X, d.Y);
	}
    }

    hexcoord end() const
    {
	return pos_in(size.x-1, size.y-1);
    }

    hexcoord random_edge_point() const;
    hexcoord random_point() const;

    static dgn_region absolute(int left, int top, int right, int bottom)
    {
        return dgn_region(left, top, right - left + 1, bottom - top + 1);
    }

    static dgn_region absolute(const coord_def &c1, const coord_def &c2)
    {
        return dgn_region(c1.x, c1.y, c2.x, c2.y);
    }

    static dgn_region perfect_hex(const hexcoord &c, int radius)
    {
	return dgn_region(c - hexdir::u*radius - (-hexdir::w*radius),
		2*radius+1, 2*radius+1, 0, -3*radius, -radius);
    }

    static dgn_region equilateral_triangle(const hexcoord &c, int side, int r)
    {
	return dgn_region(c, side+1, side+1, r, -side);
    }

    static bool between(int val, int low, int high)
    {
        return (val >= low && val <= high);
    }

    bool contains(const hexcoord &p) const
    {
	if (diamond_rot == -1)
	    return (p.x >= pos.x && p.x < pos.x + size.x
		    && p.y >= pos.y && p.y < pos.y + size.y);
	else
	{
	    const hexdir d = (p-pos).rotated(-diamond_rot);
	    return (d.X >= 0 && d.X < size.x
		    && d.Y >= 0 && d.Y < size.y
		    && (sZ == 0 || ( d.Z >= sZ && d.Z <= eZ )));
	}
    }

    bool contains(int xp, int yp) const
    {
	return contains(hexcoord(xp,yp));
    }

    bool contains_rel(const coord_def &p) const
    {
	    return (p.x >= 0 && p.x < size.x
		    && p.y >= 0 && p.y < size.y
		    && (sZ == 0 || (-p.x-p.y >= sZ && -p.x-p.y <= eZ)));
    }

    const std::vector<coord_def> corners() const;
    
    bool fully_contains(const hexcoord &p) const
    {
	if (diamond_rot == -1)
	    return (p.x > pos.x && p.x < pos.x + size.x - 1
		    && p.y > pos.y && p.y < pos.y + size.y - 1);
	else
	{
	    const hexdir d = (p-pos).rotated(-diamond_rot);
	    return (d.X > 0 && d.X < size.x - 1
		    && d.Y > 0 && d.Y < size.y - 1
		    && (sZ == 0 || ( d.Z > sZ && d.Z < eZ )));
	}
    }

    bool fully_contains_rel(const coord_def &p) const
    {
	    return (p.x > 0 && p.x < size.x -1
		    && p.y > 0 && p.y < size.y -1
		    && (sZ == 0 || (-p.x-p.y > sZ && -p.x-p.y < eZ)));
    }

    bool overlaps(const dgn_region &other) const;
    bool overlaps_any(const dgn_region_list &others) const;
    bool overlaps(const dgn_region_list &others,
                  const map_mask &dgn_map_mask) const;
    bool overlaps(const map_mask &dgn_map_mask) const;

    bool contains(const dgn_region &other) const;

    dgn_region inner_region(int d = 1) const
    {
	if (diamond_rot == -1)
	    return dgn_region(pos + coord_def(d,d), size.x-d, size.y-d);
	else
	    return dgn_region(pos.tohex() +
		    ((hexdir::u + (-hexdir::w)).rotated(diamond_rot))*d,
		    std::max(0,size.x-2*d), std::max(0,size.y-2*d),
		    diamond_rot, sZ == 0 ? 0 : sZ+2*d, eZ == 0 ? 0 : eZ);
    }


    class perimeter_iterator :
	public std::iterator<std::forward_iterator_tag, coord_def>
    {
	// marches anticlockwise around the perimeter
	private:
	    coord_def p;
	    const dgn_region *regp;
	    coord_def dir;
	    bool unmoved;

	    static coord_def dirs[];
	public:
	    explicit perimeter_iterator( const coord_def &in_p,
		    const dgn_region *in_regp,
		    const coord_def &in_dir = coord_def(1,0),
		    bool in_unmoved = true ) :
		p(in_p), regp(in_regp), dir(in_dir), unmoved(in_unmoved) {}

	    perimeter_iterator operator=(const perimeter_iterator& other)
	    {
		p = other.p;
		regp = other.regp;
		dir = other.dir;
		unmoved = other.unmoved;
		return (*this);
	    }

	    bool operator==(const perimeter_iterator& other)
	    {
		return (p == other.p && unmoved == other.unmoved);
	    }  
	    bool operator!=(const perimeter_iterator& other)
	    {
		return !operator==(other);
	    }

	    const coord_def& operator*() const
	    {
		return p;
	    }

	    perimeter_iterator operator++()
	    {
		int safetynet = 6; // in case of empty regions
		while (!regp->contains_rel(p+dir) && safetynet-- > 0)
		{
		    if (regp->diamond_rot == -1)
		    {
			int t = dir.x;
			dir.x = dir.y;
			dir.y = -t;
		    }
		    else
		    {
			for ( int i = 0; i < 6; i++ )
			{
			    if (dir == dirs[i])
			    {
				dir = dirs[ (i+1)%6 ];
				break;
			    }
			}
		    }
		}
		if (safetynet > 0)
		    p += dir;
		unmoved = false;
		return *this;
	    }

	    perimeter_iterator operator++(int)
	    {
		perimeter_iterator tmp(*this);
		++(*this);
		return(tmp);
	    }

	    const hexdir abs_hexdir() const
	    {
		// direction in which we are currently going
		ASSERT(regp->diamond_rot != -1);
		for ( int i = 0; i < 6; i++ )
		    if (dir == dirs[i])
			return hexdir::u.rotated(i+regp->diamond_rot);
		ASSERT(false);
		return hexdir::zero;
	    }
    };
    perimeter_iterator begin_perimeter() const
    {
	coord_def p;
	if (eZ >= 0)
	    p.set(0,0);
	else
	    if (-eZ <= size.x-1)
		p.set(-eZ, 0);
	    else
		p.set(size.x-1, -eZ - (size.x-1));
	return perimeter_iterator( p, this);
    }
    perimeter_iterator end_perimeter() const
    {
	coord_def p;
	if (eZ >= 0)
	    p.set(0,0);
	else
	    if (-eZ <= size.x-1)
		p.set(-eZ, 0);
	    else
		p.set(size.x-1, -eZ - (size.x-1));
	return perimeter_iterator( p, this, coord_def(1,0), false);
    }

    class contents_iterator :
	public std::iterator<std::forward_iterator_tag, coord_def>
    {
	private:
	    coord_def p;
	    const dgn_region *regp;

	    static coord_def dirs[];
	public:
	    explicit contents_iterator( const coord_def &in_p,
		    const dgn_region *in_regp ) :
		p(in_p), regp(in_regp) {}

	    contents_iterator operator=(const contents_iterator& other)
	    {
		p = other.p;
		regp = other.regp;
		return (*this);
	    }

	    bool operator==(const contents_iterator& other)
	    {
		return (p == other.p);
	    }  
	    bool operator!=(const contents_iterator& other)
	    {
		return !operator==(other);
	    }

	    const coord_def& operator*() const
	    {
		return p;
	    }

	    contents_iterator operator++()
	    {
		p.y++;
		if (!regp->contains_rel(p))
		{
		    p.x++;
		    if (regp->diamond_rot == -1)
			p.y = 0;
		    else
			p.y = std::max(0, -(regp->eZ)-p.x);
		}
		return *this;
	    }

	    contents_iterator operator++(int)
	    {
		contents_iterator tmp(*this);
		++(*this);
		return(tmp);
	    }
    };
    contents_iterator begin_contents() const
    {
	const int first_x = std::max(0, -eZ-(size.y-1));
	return contents_iterator( eZ == 0 ?
		coord_def(0,0) :
		coord_def(first_x, std::max(0,-eZ-first_x)),
		this);
    }
    contents_iterator end_contents() const
    {
	const int one_after_x = std::min(size.x, -sZ+1);
	return contents_iterator( sZ == 0 ? coord_def(size.x,0) :
		coord_def(one_after_x, std::max(0, -eZ-one_after_x)),
		this);
    }
};

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


bool builder(int level_number, int level_type);

void replace_region(const dgn_region &region,
                         dungeon_feature_type replace,
                         dungeon_feature_type feature,
                         unsigned mmask = 0);

int bazaar_floor_colour(int curr_level);
// Set floor/wall colour based on the mons_alloc array. Used for
// Abyss and Pan.
void dgn_set_colours_from_monsters();
void dgn_set_floor_colours();
void dgn_set_grid_colour_at(const coord_def &c, int colour);

bool dgn_place_map(int map, bool generating_level, bool clobber,
                   bool make_no_exits,
                   const coord_def &pos = coord_def(-1, -1));
void level_clear_vault_memory();
void level_welcome_messages();
void define_zombie(int mid, int ztype, int cs, int power);
bool is_wall(int feature);
bool place_specific_trap(int spec_x, int spec_y,  trap_type spec_type);
void place_spec_shop(int level_number, int shop_x, int shop_y,
                     int force_s_type, bool representative = false);
bool unforbidden(const coord_def &c, unsigned mask);
coord_def dgn_find_nearby_stair(dungeon_feature_type stair_to_find,
                                bool find_closest);

class mons_spec;
bool dgn_place_monster(const mons_spec &mspec,
                       int monster_level, int vx, int vy,
                       bool generate_awake);

#endif
