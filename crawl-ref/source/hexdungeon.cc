/*
 *  File:       hexdungeon.cc
 *  Summary:    Hex-specific functions used when building new levels.
 *  Written by: Martin Bays
 *
 *  For hexcrawl: adaptations of pre-existing dungeon-building functions to
 *  hex, and a few entirely new algorithms.
 */

#include <queue>

#include "AppHdr.h"
#include "abyss.h"
#include "branch.h"
#include "defines.h"
#include "dungeon.h"
#include "enum.h"
#include "externs.h"
#include "player.h"
#include "stuff.h"
#include "terrain.h"

static void make_trail_hex(const hexcoord &start, int rand_radius,
	int corrlength, int intersect_chance, int no_corr, int roundedness,
	dungeon_feature_type begin, dungeon_feature_type end=DNGN_UNSEEN);
static bool make_hex_room( const dgn_region& reg, int max_doors,
	int doorlevel);

void builder_basic_hex_trails(int level_number)
{
    const int corrlength = 2 + random2(14);
    const int no_corr = (one_chance_in(100) ? 500 + random2(500) : 20 +
	    random2(150));
    const int intersect_chance = (one_chance_in(20) ? 400 : random2(15));
    const int roundedness = (one_chance_in(20) ? 100 : 1 + random2(20));

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "builder_basic_hex_trails: "
	    "corrlength=%d;no_corr=%d;intersect_chance=%d;roundedness=%d",
	    corrlength, no_corr, intersect_chance, roundedness);
#endif

    make_trail_hex( hexcoord::centre + hexdir::u*30, 25, corrlength,
	    intersect_chance, no_corr, roundedness, DNGN_STONE_STAIRS_DOWN_I,
	    DNGN_STONE_STAIRS_UP_I );

    make_trail_hex( hexcoord::centre + hexdir::v*30, 25, corrlength,
	    intersect_chance, no_corr, roundedness, DNGN_STONE_STAIRS_DOWN_II,
	    DNGN_STONE_STAIRS_UP_II );

    make_trail_hex( hexcoord::centre + hexdir::w*30, 25, corrlength,
	    intersect_chance,no_corr, roundedness, DNGN_STONE_STAIRS_DOWN_III,
	    DNGN_STONE_STAIRS_UP_III);

    if (one_chance_in(4))
    {
        make_trail_hex( hexcoord::centre, 40, corrlength, intersect_chance, no_corr, roundedness,
                    DNGN_ROCK_STAIRS_DOWN );
    }

    if (one_chance_in(4))
    {
        make_trail_hex( hexcoord::centre, 40, corrlength, intersect_chance, no_corr, roundedness,
                    DNGN_ROCK_STAIRS_UP );
    }
}

void builder_basic_hex_rooms(int level_number)
{
    const int doorlevel = random2(11);
    const int roomsize = 5 + random2(6) + random2(7);
    int roomwidth = (one_chance_in(20) ? 40 : 3 + random2(20));
    int hexosity = (one_chance_in(40) ? 20 : random2(4));

    if (one_chance_in(50))
    {
	// make all rooms be near-perfect hexagons
	roomwidth = 10;
	hexosity = 50;
    }

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "builder_basic_hex_rooms: "
	    "doorlevel=%d;roomsize=%d;roomwidth=%d;hexosity=%d",
	    doorlevel, roomsize, roomwidth, hexosity);
#endif

    // make some rooms:
    int i, no_rooms, max_doors;
    int time_run;
    int temp_rand;

    temp_rand = random2(750);
    time_run = 0;

    no_rooms = ((temp_rand > 63) ? (5 + random2avg(29, 2)) : // 91.47% {dlb}
                (temp_rand > 14) ? 100                       //  6.53% {dlb}
                                 : 1);                       //  2.00% {dlb}

    max_doors = 2 + random2(8);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
	    "builder_basic_hex_main: no_rooms=%d; max_doors=%d", no_rooms,
	    max_doors);
#endif

    hexcoord c;
    int dX,dY,dZ,Zp;
    bool placed;

    const dgn_region mapgen_region =
	dgn_region::perfect_hex(hexcoord::centre, GRAD-MAPGEN_BORDER);

    dgn_region room_region;

    for (i = 0; i < no_rooms; i++)
    {

	// A diagram to meditate on:
	//
        //   eZ         sZ    sX          eX                |
        //     \          \  /           /                  |
	//       \         /\          /,(eX,eY,-dX,-dY)    |
	//        -\-----/----\------/---eY                 |
	//           \ /        \  /                        |
	//           / \         /\                         |
	//        -/-----\-----/----\----sY                 |
	//       /  `      \ /        \                     |
	//     /   (0,0,0) / \          \                   |
	//
	// (note we rotate later, there isn't actually any preferred
	// orientation)
	//
	// High roomwidth should tend to give diamonds
	// Low hexosity makes triangles more likely

	dX = 2 + random2(roomsize);

	dY = 2 + random2(roomsize);
	dY = (dY + dX*hexosity) / (1+hexosity);

	dZ = (random2( (dX+dY)*roomwidth/10 ) / 2);
	if (dZ < 2)
	    dZ = 2;

	Zp = -random2avg(dX+dY, 1+hexosity) - dZ/2;

	placed = false;
	for (int inner_tries = 0; !placed && inner_tries < 6; inner_tries++)
	{
	    do
	    {
		room_region = dgn_region(
			hexcoord::centre + random_hex(GRAD),
			dX+1, dY+1,
			random2(6),
			Zp, Zp+dZ);
	    } while (!mapgen_region.contains(room_region));

	    if (make_hex_room(room_region,
			max_doors, doorlevel))
		placed = true;
	}
	if (!placed)
	{
	    time_run++;
	    i--;
	}

        if (time_run > 5)
        {
            time_run = 0;
            i++;
        }
    }

    // make some more rooms:
    no_rooms = 1 + random2(3);
    max_doors = 1;

    for (i = 0; i < no_rooms; i++)
    {
	room_region = dgn_region(
		hexcoord::centre + random_hex(GRAD),
		3 + random2(5), 3 + random2(5),
		random2(6), -6, std::min(0, -6 + 3 + (random2(5))));

	if (!(mapgen_region.contains(room_region) && make_hex_room(
			room_region, max_doors, doorlevel)))
	{
	    time_run++;
	    i--;
	}

        if (time_run > 30)
        {
            time_run = 0;
            i++;
        }
    }
}

static void make_trail_hex(const hexcoord &start, int rand_radius,
	int corrlength, int intersect_chance, int no_corr, int roundedness,
	dungeon_feature_type begin, 
	dungeon_feature_type end)
{
    hexcoord real_start, pos;
    hexdir disp, dir, dir2, tdir;
    int finish = 0;
    int length = 0;

    do
    {
	real_start = start + random_hex(rand_radius);
    }
    while (!(in_hex_G_bounds(real_start, MAPGEN_BORDER) &&
		(grd(real_start) == DNGN_ROCK_WALL ||
		 grd(real_start) == DNGN_FLOOR)));

    // assign begin feature
    if (begin != DNGN_UNSEEN)
        grd(real_start) = begin;

    pos = real_start;
    disp = real_start - hexcoord::centre;

    dir = random_direction();

    // wander
    do                          // (while finish < no_corr)
    {
	// [hex] pick a new corridor direction. This vaguely preserves the
	// spirit of the square code, but only vaguely.
	if (disp.rdist() >= GRAD - 20 &&
		hex_dir_towards(disp) == dir)
	{
	    // fairly near the boundary and travelling straight outwards:
	    // strongly consider turning to one side
	    if (!one_chance_in(3))
		dir.rotate( (one_chance_in(roundedness)?2:1) * (coinflip()?1:-1) );
	}
	else if (disp.rdist() >= GRAD - 12 &&
		disp * dir > 20)
	{
	    // rather near the boundary, and travelling kind of towards it:
	    // maybe turn away
	    if (coinflip())
		if ( disp * dir.rotated(1) <= 20 )
		    dir.rotate(1 + one_chance_in(roundedness));
		else
		    dir.rotate(-1 - one_chance_in(roundedness));
	}
	else if (coinflip())
	    dir.rotate( (one_chance_in(roundedness)?2:1) * (coinflip()?1:-1) );

        // corridor length
	length = random2(corrlength) + 2;

        int bi = 0;

        for (bi = 0; bi < length; bi++)
        {
	    if ( disp.rdist() >= GRAD - 9 )
	    {
		// too close to the edge. Run away!
		dir = (coinflip() ? hexdir::w : -hexdir::u).rotated(disp.hextant());
	    }

            // don't interfere with special rooms
            if (grd(pos + dir) == DNGN_BUILDER_SPECIAL_WALL)
		break;

            // see if we stop due to intersection with another corridor/room
            if (grd(pos + dir*2) == DNGN_FLOOR
                && !one_chance_in(intersect_chance))
		break;

	    disp += dir;
	    pos += dir;

            if (grd(pos) == DNGN_ROCK_WALL)
                grd(pos) = DNGN_FLOOR;
        }

        if (finish == no_corr - 1 && grd(pos) != DNGN_FLOOR)
            finish -= 2;

        finish++;
    }
    while (finish < no_corr);

    // assign end feature
    if (end != DNGN_UNSEEN)
        grd(pos) = end;
}

bool join_the_dots_hex(
    const hexcoord &from,
    const hexcoord &to,
    unsigned mapmask,
    bool early_exit)
{
    if (from == to)
        return (true);

    int join_count = 0;

    hexcoord at = from;
    hexdir dir = hex_dir_towards(to - at);
    do
    {
        join_count++;

        const dungeon_feature_type feat = grd(at);
        if (early_exit && at != from && is_traversable(feat))
            return (true);

        if (unforbidden(at, MMT_VAULT) && !is_traversable(feat))
            grd(at) = DNGN_FLOOR;

        if (join_count > 10000) // just insurance
            return (false);

	if ( hexdir_is_straight(to - at) )
	    dir = hex_dir_towards(to - at);

	if (in_bounds(at+dir) && unforbidden(at + dir, mapmask))
	{
	    at += dir;
	    continue;
	}
	else
	{
	    dir = hex_dir_towards(to - at);
	    if (in_bounds(at+dir) && unforbidden(at + dir, mapmask))
	    {
		at += dir;
		continue;
	    }
	    if (in_bounds(at+dir.rotated(-1))
		    && unforbidden(at + dir.rotated(-1), mapmask))
	    {
		dir.rotate(-1);
		at += dir;
		continue;
	    }
	    if (in_bounds(at+dir.rotated(1))
		    && unforbidden(at + dir.rotated(1), mapmask))
	    {
		dir.rotate(1);
		at += dir.rotated(1);
		continue;
	    }
	}

        // If we get here, no progress can be made anyway. Why use the
        // join_count insurance?
        break;
    }
    while (at != to && in_bounds(at));

    if (in_bounds(at) && unforbidden(at, mapmask))
        grd(at) = DNGN_FLOOR;

    return (at == to);
}                               // end join_the_dots_hex()

// To give non-ugly dungeons, we want to find the best path between the
// two points, where we prefer straight lines and to go along pre-existing paths.
// We could consider this as path-finding in a weighted graph, where we have 6
// nodes for each hex of the map (one for each entry direction), and the edges
// are weighted to make digging through a wall quite heavy, and turning around
// a little heavy.
// We could then find the best path using A*, with heuristical lower bound the
// distance to the destination assuming a clear path.
//
// Sadly, this algorithm is too slow to be usable. Instead, we use a rough
// approximation to the above idea. We consider a graph whose nodes consist
// points in the dungeon, and whose edges correspond to straight line paths
// between those points. The cost of an edge is its length, weighted to
// consider tunneling through rock more costly than travelling along
// pre-existing paths. We then apply A* to the resulting graph.
//
// By default the nodes are on the points of a sublattice of the dungeon hex,
// but we also introduce extra nodes when we run into pre-existing traversable
// hexes. See the code for details.
//
// To make it fast enough, we also slightly relax the heuristic to one which
// isn't actually a lower bound.

struct jtd_node
{
    hexcoord pos;
    int f;
    int g;
    int h;
    jtd_node* parent;

    jtd_node(const hexcoord& in_pos, int in_g,
	    const hexcoord& target, jtd_node* in_parent) :
	pos(in_pos), g(in_g), parent(in_parent)
    {
	h = 2*pos.distance_from(target);
	f = g+h;
    }

    void update_g(int new_g)
    {
	g = new_g;
	f = g+h;
    }

    bool node_eq(jtd_node* other_p) const
    {
	return (pos == other_p->pos);
    }

    bool operator== (const jtd_node& other) const
    {
	return (pos == other.pos);
    }
};

struct jtd_node_p_comparator
{
    bool operator() (const jtd_node* a, const jtd_node* b)
    {
	return (a->f > b->f);
    }
};

struct jtd_node_p_eq_to
{
    const jtd_node* n;
    jtd_node_p_eq_to(jtd_node* n_in) : n(n_in) {}

    bool operator() (const jtd_node* other)
    {
	return (n->pos == other->pos);
    }
};

bool join_the_dots_hex_pathfind(
    const hexcoord &from,
    const hexcoord &to,
    unsigned mapmask,
    bool early_exit)
{
    // We use the standard a* algorithm.
    //
    // Closed nodes are those for which we know f <= d, where d is the actual
    // best-path distance, if the heuristic h is a lower bound for d. Open
    // nodes are those one step away from closed nodes. We keep these sorted
    // (with the std heap functions) in ascending order of f cost.
    std::vector<jtd_node*> closed;
    std::vector<jtd_node*> open;

    jtd_node* first_node = new jtd_node(from, 0, to, NULL);
    open.push_back(first_node);
    const jtd_node_p_comparator comp = jtd_node_p_comparator();

    std::make_heap(open.begin(), open.end(), comp);

    jtd_node* goal = NULL;
    int count = 0;
    while (!open.empty())
    {
	std::pop_heap(open.begin(), open.end(), comp);
	jtd_node* expand = open.back();
	open.pop_back();

	closed.push_back(expand);

	if (expand->pos == to)
	{
	    // found shortest path
	    goal = expand;
	    break;
	}

	// to expand the node, we pick nodes at appropriate points along
	// straight-line paths away from the current node position. We stop
	// when we break through into open space, are about to start
	// tunneling, are about to step into forbidden space, or if we reach
	// one of the lines X=g.X+k*8 for k\in\Integers, or sim for Y or Z.
	hexdir::circle c(1);
	for (hexdir::circle::iterator it = c.begin(); it != c.end(); it++)
	{
	    const hexcoord start_pos = expand->pos;
	    hexcoord new_pos = start_pos;
	    const dungeon_feature_type old_grid = grd(start_pos);
	    int cost = 0;
	    while (true)
	    {
		if (!(in_bounds(new_pos + *it) &&
			    unforbidden(new_pos + *it, mapmask)))
		    break;

		if (is_traversable(old_grid) && new_pos != start_pos &&
			!is_traversable(grd(new_pos + *it)))
		    break;

		new_pos += *it;

		cost += is_traversable(grd(new_pos)) ? 1 : 6;
		if (!in_hex_G_bounds(new_pos, MAPGEN_BORDER))
		    cost+=2;

		if ((!is_traversable(old_grid) &&
			    is_traversable(grd(new_pos))) ||
			((*it).X != 0 && (new_pos.X - to.X) % 8 == 0) ||
			((*it).Y != 0 && (new_pos.Y - to.Y) % 8 == 0) ||
			((*it).Z != 0 && (new_pos.Z - to.Z) % 8 == 0))
		    break;
	    }

	    if (new_pos == expand->pos)
		continue;

	    jtd_node* node = new jtd_node(
		    new_pos,
		    expand->g + cost,
		    to,
		    expand);
	    std::vector<jtd_node*>::iterator found_open =
		std::find_if(open.begin(), open.end(), jtd_node_p_eq_to(node));
	    if (found_open != open.end())
	    {
		// already in open: update if new cost is better
		if ((*found_open)->g > expand->g + cost)
		{
		    (*found_open)->update_g(expand->g + cost);
		    (*found_open)->parent = expand;
		    std::make_heap(open.begin(), open.end(), comp);
		}
		delete node;
	    }
	    else
	    {
		// not in open: if also not in closed, add to open
		std::vector<jtd_node*>::iterator found_closed =
		    std::find_if(closed.begin(), closed.end(),
			    jtd_node_p_eq_to(node));
		if (found_closed == closed.end())
		{
		    open.push_back(node);
		    std::push_heap(open.begin(), open.end(), comp);
		}
		else
		    delete node;
	    }
	}
	count++;
	if (count == 5000)
	{
	    // safety net
	    break;
	}
    }

    bool ret = true;

    if (open.empty() || count == 5000)
    {
	mprf(MSGCH_WARN, "join_the_dots_hex_pathfind failed to find path");
	ret = false;
    }
    else
    {
	jtd_node* node = goal;
	jtd_node* parent = goal->parent;
	while (parent != NULL)
	{
	    const hexdir dir = hex_dir_towards(parent->pos - node->pos);
	    hexcoord p = node->pos;
	    while (p != parent->pos)
	    {
		if (!is_traversable(grd(p)))
		    grd(p) = DNGN_FLOOR;
		p += dir;
	    }
	    node = parent;
	    parent = node->parent;
	}
	if (!is_traversable(grd(node->pos)))
	    grd(node->pos) = DNGN_FLOOR;
    }

    for (std::vector<jtd_node*>::iterator it = open.begin(); it != open.end();
	    it++)
    {
	delete *it;
    }
    for (std::vector<jtd_node*>::iterator it = closed.begin(); it !=
	    closed.end(); it++)
    {
	delete *it;
    }

    return ret;
}

static bool find_in_hex(const dgn_region& reg,
	dungeon_feature_type feature)
{
    if (feature != 0)
    {
	for ( dgn_region::contents_iterator it = reg.begin_contents();
		it != reg.end_contents();
		it++ )
	{
	    if (grd(reg.pos_in(*it)) == feature)
		return (true);
	}
    }
    return (false);
}

static int good_door_spot(const hexcoord &pos)
{
    if ((!grid_is_solid(grd(pos)) && grd(pos) < DNGN_ENTER_PANDEMONIUM)
        || grd(pos) == DNGN_CLOSED_DOOR)
    {
        return 1;
    }

    return 0;
}

static bool make_hex_room( const dgn_region& reg, int max_doors,
	int doorlevel)
{
    int find_door = 0;
    int find_door_outer = 0;

    for ( dgn_region::perimeter_iterator it = reg.begin_perimeter();
	    it != reg.end_perimeter();
	    it++ )
    {
	find_door += good_door_spot(reg.pos_in(*it));
    }

    const dgn_region outer = reg.inner_region(-1);
    for ( dgn_region::perimeter_iterator it = outer.begin_perimeter();
	    it != outer.end_perimeter();
	    it++ )
    {
	find_door_outer += good_door_spot(outer.pos_in(*it));
    }

    if (find_door == 0 || find_door > max_doors || find_door_outer > find_door)
	return false;

    // look for 'special' rock walls - don't interrupt them
    if (find_in_hex(reg, DNGN_BUILDER_SPECIAL_WALL))
	return false;

    // convert the area to floor
    for ( dgn_region::contents_iterator it = reg.begin_contents();
	    it != reg.end_contents();
	    it++ )
    {
	const hexcoord t = reg.pos_in(*it);
	if (grd(t) <= DNGN_FLOOR)
	    grd(t) = DNGN_FLOOR;
    }

    // install doors, where they will look pretty

    for ( dgn_region::perimeter_iterator it = outer.begin_perimeter();
	    it != outer.end_perimeter();
	    it++ )
    {
	const hexcoord p = outer.pos_in(*it);
	const hexdir d = it.abs_hexdir();
	if (grd(p) == DNGN_FLOOR 
		&& (grid_is_solid(grd(p+d)) ||
		    grid_is_solid(grd(p+d.rotated(1))))
		&& (grid_is_solid(grd(p-d)) ||
		    grid_is_solid(grd(p+d.rotated(2))))
		&& random2(10) < doorlevel)
	{
	    grd(p) = DNGN_CLOSED_DOOR;
	}
    }
    return true;
}                               //end make_hex_room()

// dissolved_cavern:
//
// something entirely new: "natural" caverns.
// Start with solid rock. Randomly set "hardness" of each hex, with wide
// variation in hardness. Pick three points to be the up stairs and the
// entry-points for our (acidic) water. 
//
// Every few ticks, introduce a drip of water to each of our entry-points.
// Each tick, process each drip: 
//    * try to move in the direction of its current velocity, if non-zero.
//	* if there's a wall there: reduce its hardness by 1;
//	  * if hardness is then 0: replace wall with floor, and set its height
//	      to the height of the current hex
//	  * else: subtract the direction from the drip's velocity
//	* else: move drip in that direction.
//	  * if this involves going downhill: add the direction to velocity.
//    * add a random dir to the velocity, prefering to go downhill
//    * one time in n: reduce height of current hex by 1
//    * if height of current hex is 0 and hex is open space: replace it with
//        down stairs and stop the dripper.
// Stop when all drippers have.

struct drip
{
    static bool is_dead(const drip& drip)
    { return drip.dead; }

    hexcoord pos;
    hexdir vel;
    bool dead;

    drip( hexcoord pos_in, hexdir vel_in = hexdir::zero )
	: pos(pos_in), vel(vel_in), dead(false) {}

    void accelerate(int gravity, int max_vel,
	    const FixedArray<unsigned short, GXM, GYM>& height,
	    const FixedArray<unsigned short, GXM, GYM>& numdrips);
};

struct dripper
{
    hexcoord source;
    dungeon_feature_type begin;
    dungeon_feature_type end;
    int trailers;
    dungeon_feature_type trailtype;
    int traillength;
    unsigned drip_life;
    unsigned max_drips;
    bool dried;

    std::vector<drip> drips;

    dripper( const hexcoord source_in, const int sourcerand, const
	    dungeon_feature_type begin_in, const dungeon_feature_type end_in =
	    DNGN_FLOOR, const int trailers_in = 0, const dungeon_feature_type
	    trailtype_in = DNGN_SHALLOW_WATER, int traillength_in = 400 ) :
	begin(begin_in), end(end_in),
	trailers(trailers_in), trailtype(trailtype_in),
	traillength(traillength_in),
	dried(false)
    {
	do
	{
	    source = source_in + random_hex(sourcerand);
	} while (!(in_hex_G_bounds(source, MAPGEN_BORDER)));

	drip_life = one_chance_in(5) ? (one_chance_in(5) ?
		6 + random2(20) : 51 + random2(50)) : 200;
	max_drips = 6 + random2(30);
    }

    void make_drip() {
	drips.push_back(drip(source));
    }
};

struct dissolving_cavern
{
    int drip_rate;
    unsigned short init_height;
    int init_hardness;
    int init_hardness_variation;
    int dissolve_vert;
    int gravity;
    int hard_areas;
    int encase;

    static const int lat_down = 1;
    static const int max_vel = 4;

    FixedArray<unsigned short, GXM, GYM> height;
    FixedArray<unsigned short, GXM, GYM> numdrips;
    FixedArray<unsigned short, GXM, GYM> hardness;

    std::vector<dripper>& drippers;

    dissolving_cavern(std::vector<dripper>& drippers_in) :
	drippers(drippers_in)
    {
	drip_rate = 20;
	init_height = 80;
	init_hardness = 2+random2(4)+random2(5);
	init_hardness_variation = (init_hardness*(1+random2(9)))/10;
	dissolve_vert = one_chance_in(5) ? 20 + random2(40) : 100;
	gravity = one_chance_in(10)? 1 : 3+random2(8);
	hard_areas = one_chance_in(20)? 0 : 6+random2(35);
	encase = coinflip()? 0 : 1+random2(4);

#if DEBUG_DIAGNOSTICS
	mprf(MSGCH_DIAGNOSTICS, "dissolving_cavern: "
		"dis_vert:%d; hardness:%d+-%d; "
		"gravity:%d; areas:%d; encase:%d",
		dissolve_vert, init_hardness, init_hardness_variation,
		gravity, hard_areas, encase);
#endif

	height.init(init_height);
	numdrips.init(0);
    }

    void make_rock();
    void dissolve();
    void spring(int down_height,
	    dungeon_feature_type shallow_feature=DNGN_SHALLOW_WATER,
	    dungeon_feature_type deep_feature=DNGN_DEEP_WATER,
	    int deep_cutoff=10);
};

void dissolved_cavern(int level_number, bool dry)
{
    std::vector<dripper> drippers;

    drippers.push_back(dripper( hexcoord::centre + hexdir::u*30, 25,
		DNGN_STONE_STAIRS_UP_I, DNGN_STONE_STAIRS_DOWN_I));
    drippers.push_back(dripper( hexcoord::centre + hexdir::v*30, 25,
		DNGN_STONE_STAIRS_UP_II, DNGN_STONE_STAIRS_DOWN_II));
    drippers.push_back(dripper( hexcoord::centre + hexdir::w*30, 25,
		DNGN_STONE_STAIRS_UP_III, DNGN_STONE_STAIRS_DOWN_III));

    if (one_chance_in(4))
	drippers.push_back(dripper( hexcoord::centre, 40,
		    DNGN_ROCK_STAIRS_DOWN));
    if (one_chance_in(4))
	drippers.push_back(dripper( hexcoord::centre, 40,
		    DNGN_ROCK_STAIRS_UP ));

    dissolving_cavern cavern(drippers);

    cavern.make_rock();

    cavern.dissolve();

    if (!dry &&
	    level_number > 7 && one_chance_in(3))
    {
	const int down_height = 10 + random2(60);

	if (level_number > 11 
		&& (one_chance_in(5)
		    || (level_number > 15 && !one_chance_in(5))))
	    cavern.spring(down_height, DNGN_LAVA, DNGN_LAVA);
	else
	    cavern.spring(down_height, DNGN_SHALLOW_WATER,
		    DNGN_DEEP_WATER);
    }
}

#if DEBUG_DISSOLVED
static int rainbow_colour(int i)
{
    switch (i)
    {
	case 0: return DARKGREY;
	case 1: return LIGHTMAGENTA;
	case 2: return MAGENTA;
	case 3: return LIGHTBLUE;
	case 4: return BLUE;
	case 5: return LIGHTCYAN;
	case 6: return CYAN;
	case 7: return LIGHTGREEN;
	case 8: return GREEN;
	case 9: return YELLOW;
	case 10: return BROWN;
	case 11: return LIGHTRED;
	case 12: return RED;
	case 13: return WHITE;
	case 14: return LIGHTGREY;
	default: return LIGHTGREY;
    }
}
#endif

void drip::accelerate(int gravity, int max_vel,
	const FixedArray<unsigned short, GXM, GYM>& height,
	const FixedArray<unsigned short, GXM, GYM>& numdrips)
{
    std::vector<hexdir> down_list;
    hexdir::circle c1(1);
    for (hexdir::circle::iterator it = c1.begin(); it != c1.end();
	    it++)
    {
	if (height(pos) + numdrips(pos)-1
		> height(pos + *it) +
		numdrips(pos + *it))
	    down_list.push_back(*it);
    }
    hexdir rand;
    if (down_list.size() == 0
	    || (gravity && one_chance_in(gravity+1)))
    {
	if (gravity)
	    rand = random_direction();
	else
	    rand = hexdir::zero;
    }
    else
    {
	short unsigned r = down_list.size();
	for (std::vector<hexdir>::iterator it = down_list.begin();
		it != down_list.end(); it++)
	    if (one_chance_in(r--))
	    {
		rand = *it;
		break;
	    }
    }

    if ((vel + rand).rdist() <= max_vel)
	vel += rand;
}

void dissolving_cavern::make_rock()
{
    hexdir::disc d(GRAD-MAPGEN_BORDER);
    for (hexdir::disc::iterator it = d.begin(); it != d.end(); it++)
    {
	const hexcoord h = hexcoord::centre + *it;
	hardness(h) = init_hardness - init_hardness_variation +
	    random2(1+2*init_hardness_variation);
    }

    // place some random areas of randomly altered hardness
    const dgn_region dgn_bounds_hex =
	dgn_region::perfect_hex(hexcoord::centre, GRAD-(MAPGEN_BORDER+1));
    for (int i = 0; i < hard_areas; i++)
    {
	// random hex
	// see builder_basic_hex() for reasoning
	int dX,dY,dZ,Zp;
	dgn_region myroom;

	dX = 2 + random2(20);

	dY = 2 + random2(20);

	dZ = (random2( dX+dY ) / 2);
	if (dZ < 2)
	    dZ = 2;

	Zp = -random2(dX+dY) - dZ/2;
	do
	{
	    myroom = dgn_region( dgn_bounds_hex.random_point(), dX+1, dY+1,
		    random2(6), Zp, Zp+dZ );
	} while (!dgn_bounds_hex.contains(myroom));

	const int region_hardness_factor = 5+random2(16);
	short unsigned region_hardness =
	    one_chance_in(3) ?
	    init_hardness/region_hardness_factor :
	    init_hardness*region_hardness_factor ;
	if (region_hardness == 0)
	    region_hardness = 1;

	for (dgn_region::contents_iterator it = myroom.begin_contents(); it !=
		myroom.end_contents(); it++)
	    hardness(myroom.pos_in(*it)) = region_hardness;
    }

    if (encase)
	for (std::vector<dripper>::iterator dripper = drippers.begin();
		dripper != drippers.end(); dripper++)
	{
	    hexdir::disc d2(encase);
	    hexdir outdir = random_direction();
	    for (hexdir::disc::iterator it = d2.begin(); it != d2.end();
		    it++)
	    {
		if (*it != outdir*((*it).rdist()))
		{
		    const hexcoord h = dripper->source + *it;
		    hardness(h) *= 10;
		}
	    }
	}

    // gently increase hardness towards the map boundaries
    for (int r=0; r<7; r++)
    {
	hexdir::circle c(GRAD-MAPGEN_BORDER-r);
	for (hexdir::circle::iterator it = c.begin(); it != c.end(); it++)
	{
	    const hexcoord h = hexcoord::centre + *it;
	    hardness(h) += 3*(7-r);
	}
    }
}

void dissolving_cavern::dissolve()
{
    // assign begin features
    for (std::vector<dripper>::iterator dripper = drippers.begin(); dripper !=
	    drippers.end(); dripper++)
    {
	grd(dripper->source) = dripper->begin;
	height(dripper->source) = init_height;
    }

    // dissolve me a dungeon
    int drip_ticker = 0;
    int time_ticker = 0; // for debugging
    unsigned int drycount = 0;
    bool dead_drips = false;
    bool diminishdrip = false;

#if DEBUG_DIAGNOSTICS
    for (std::vector<dripper>::iterator dripper_it = drippers.begin();
	    dripper_it != drippers.end(); dripper_it++)
    {
	mprf(MSGCH_DIAGNOSTICS,
		"dripper: source:(%d,%d,%d)=%d; drip_life:%d; max_drips:%d",
		dripper_it->source.X, dripper_it->source.Y,
		dripper_it->source.Z, dripper_it->begin,
		dripper_it->drip_life, dripper_it->max_drips);
    }
#endif

    const unsigned int numdrippers = drippers.size();
    while (drycount < numdrippers)
    {
	for (std::vector<dripper>::iterator dripper_it = drippers.begin();
		dripper_it != drippers.end(); dripper_it++)
	{
	    if (dripper_it->dried)
		continue;
	    if (drip_ticker == 0 &&
		dripper_it->drips.size() < dripper_it->max_drips)
		{
		    dripper_it->make_drip();
		    numdrips(dripper_it->source)++;
		}
	    for (std::vector<drip>::iterator drip_it =
		    dripper_it->drips.begin(); drip_it !=
		    dripper_it->drips.end(); drip_it++)
	    {
		if (drip_it->dead)
		    continue;
		if (drip_it->vel != hexdir::zero)
		{
		    hexdir towards = hex_dir_towards(drip_it->vel);
		    hexcoord to = drip_it->pos + towards;

		    if (in_hex_G_bounds(to, MAPGEN_BORDER))
		    {
			if (grd(to) == DNGN_ROCK_WALL)
			{
			    hardness(to)--;
			    if (hardness(to) == 0)
			    {
				grd(to) = DNGN_FLOOR;
				//time_cleared(to) = time_ticker;
				height(to) = height(drip_it->pos);
				if (height(to) < lat_down)
				    height(to) = 0;
				else
				    height(to) -= lat_down;
			    }
			    else
				drip_it->vel += -towards;
			    diminishdrip = true;
			}
			else // (grd(to) == DNGN_ROCK_WALL)
			{
			    if (height(drip_it->pos) + numdrips(drip_it->pos)-1
				    < height(to) + numdrips(to))
				drip_it->vel += -towards;

			    numdrips(drip_it->pos)--;
			    drip_it->pos = to;
			    numdrips(drip_it->pos)++;
			}
		    }
		    else // (in_G_bounds(to))
		    {
			drip_it->vel += -towards;
			diminishdrip = true;
		    }
		}

		drip_it->accelerate(gravity, max_vel, height, numdrips);

		if (height(drip_it->pos) == 0 &&
		    grd(drip_it->pos) == DNGN_FLOOR)
		    {
			dripper_it->dried = true;
			drycount += 1;
			grd(drip_it->pos) = dripper_it->end;
			for (std::vector<drip>::iterator drip_it2 =
				dripper_it->drips.begin(); drip_it2 !=
				dripper_it->drips.end(); drip_it2++)
			    if (!drip_it2->dead)
				numdrips(drip_it2->pos)--;
			break;
		    }
		else if (one_chance_in(dissolve_vert)
			&& grd(drip_it->pos) == DNGN_FLOOR)
		{
		    height(drip_it->pos) -= 1;
		    diminishdrip = true;
		}
		if (diminishdrip)
		{
		    if (dripper_it->drip_life &&
			    one_chance_in(dripper_it->drip_life))
		    {
			drip_it->dead = true;
			dead_drips = true;
			numdrips(drip_it->pos)--;
		    }
		    diminishdrip = false;
		}
	    }
	    if (dead_drips)
	    {
		dripper_it->drips.erase(remove_if(dripper_it->drips.begin(),
			    dripper_it->drips.end(), drip::is_dead),
			dripper_it->drips.end());
		dead_drips = false;
	    }
	}
	drip_ticker = (drip_ticker+1) % drip_rate;
	time_ticker++;
    }

#if DEBUG_DISSOLVED
    for (hexdir::disc::iterator it = d.begin(); it != d.end(); it++)
    {
	const hexcoord h = hexcoord::centre + *it;
	if (grd(h) != DNGN_ROCK_WALL)
	    env.grid_colours(h) = rainbow_colour(height(h)/(init_height/15));
	    //env.grid_colours(h) = rainbow_colour(time_cleared(h)/(time_ticker/15));
    }
#endif
}

void dissolving_cavern::spring(int down_height,
	dungeon_feature_type shallow_feature,
	dungeon_feature_type deep_feature,
	int deep_cutoff)
{

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "making spring: down_height=%d", down_height);
#endif

    hexcoord source;
    do
    {
	source = hexcoord::centre + random_hex(GRAD-MAPGEN_BORDER);
    } while (!( grd(source) == DNGN_FLOOR && height(source) >= down_height));

    drip drip(source);

    const int to_height = std::max(0, height(source) - down_height);
    while (height(drip.pos) > to_height)
    {
	drip.accelerate(0, max_vel, height, numdrips);

	if (drip.vel != hexdir::zero)
	{
	    hexdir towards = hex_dir_towards(drip.vel);
	    hexcoord to = drip.pos + towards;

	    if (in_hex_G_bounds(to, MAPGEN_BORDER))
		if (grd(to) == DNGN_ROCK_WALL)
		    drip.vel += -towards;
		else
		{
		    if (height(drip.pos) + numdrips(drip.pos)-1
			    < height(to) + numdrips(to))
			drip.vel += -towards;
		    drip.pos = to;
		}
	    else // (in_G_bounds(to))
		drip.vel += -towards;
	}
	else
	{
	    // pool - repeatedly flood fill up to the next height+num_drips
	    // level until find a way down.
	    std::queue<hexcoord> pool;
	    bool finished_pooling = false;
	    while (!finished_pooling)
	    {
		pool.push(drip.pos);
		while (!finished_pooling && !pool.empty())
		{
		    const hexcoord h = pool.front();
		    pool.pop();

		    numdrips(h)++;

		    hexdir::circle c1(1);
		    for (hexdir::circle::iterator it = c1.begin();
			    it != c1.end(); it++)
		    {
			const hexcoord h2 = h + *it;
			if (!grid_is_solid(h2))
			{
			    if (height(h2) + numdrips(h2) <
				    height(h) + numdrips(h) - 1
				    || height(h2) <= to_height)
			    {
				drip.pos = h2;
				drip.vel = *it;
				finished_pooling = true;
				break;
			    }
			    else if (height(h2) + numdrips(h2) ==
				    height(h) + numdrips(h) - 1)
				pool.push(h2);
			}
		    }
		}
	    }
	}

	numdrips(drip.pos)++;

    }

    hexdir::disc d(GRAD-MAPGEN_BORDER);
    for (hexdir::disc::iterator it = d.begin(); it != d.end(); it++)
    {
	const hexcoord h = hexcoord::centre + *it;
	if (numdrips(h) > 0)
	{
	    if (grd(h) == DNGN_FLOOR)
		if (numdrips(h) < deep_cutoff)
		    grd(h) = shallow_feature;
		else
		    grd(h) = deep_feature;
	    numdrips(h) = 0;
	}
    }
}

char stone_circle(int level_number)
{
    replace_region( dgn_region::perfect_hex(hexcoord::centre, 3*GRAD/4),
	    DNGN_ROCK_WALL, DNGN_FLOOR);

    const int inner_spiral = random2(6);
    const int outer_spiral = random2(GRAD/4);
    const hexdir inner_circle = (hexdir::u*6 + hexdir::v*inner_spiral);
    const hexdir outer_circle1 = hexdir::u*(2*GRAD/4) +
	hexdir::v*outer_spiral;
    const hexdir outer_circle2 = outer_circle1 + hexdir::v*GRAD/4;

    for (int hextant = 0; hextant < 6; hextant++)
    {
	replace_region(
		dgn_region::perfect_hex(hexcoord::centre +
		    outer_circle1.rotated(hextant), 2),
		DNGN_FLOOR, DNGN_ROCK_WALL);

	replace_region(
		dgn_region::perfect_hex(hexcoord::centre +
		    outer_circle2.rotated(hextant), 2),
		DNGN_FLOOR, DNGN_ROCK_WALL);

	grd(hexcoord::centre + inner_circle.rotated(hextant)) = DNGN_ROCK_WALL;
    }

    for (int i=-1; i<=1; i++)
    {
	grd( hexcoord::centre - hexdir::u*(3*GRAD/4) - hexdir::v*2) =
	    DNGN_STONE_STAIRS_UP_I;
	grd( hexcoord::centre - hexdir::u*(3*GRAD/4)) =
	    DNGN_STONE_STAIRS_UP_II;
	grd( hexcoord::centre - hexdir::u*(3*GRAD/4) - hexdir::w*2) =
	    DNGN_STONE_STAIRS_UP_III;

	grd( hexcoord::centre + hexdir::u*(3*GRAD/4) + hexdir::v*2) =
	    DNGN_STONE_STAIRS_DOWN_I;
	grd( hexcoord::centre + hexdir::u*(3*GRAD/4)) =
	    DNGN_STONE_STAIRS_DOWN_II;
	grd( hexcoord::centre + hexdir::u*(3*GRAD/4) + hexdir::w*2) =
	    DNGN_STONE_STAIRS_DOWN_III;
    }

    // This "back door" is often one of the easier ways to get out of 
    // pandemonium... the easiest is to use the banish spell.  
    //
    // Note, that although "level_number > 20" will work for most 
    // trips to pandemonium (through regular portals), it won't work 
    // for demonspawn who gate themselves there. -- bwr
    if (((player_in_branch( BRANCH_MAIN_DUNGEON ) && level_number > 20)
            || you.level_type == LEVEL_PANDEMONIUM)
        && (coinflip() || you.mutation[ MUT_PANDEMONIUM ]))
    {
        grd(hexcoord::centre) = DNGN_ENTER_ABYSS;
    }

    return 0;
}
