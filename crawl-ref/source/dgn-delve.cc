#include "AppHdr.h"
#include <deque>

#include "env.h"
#include "coord.h"
#include "coordit.h"
#include "directn.h"
#include "dungeon.h"
#include "mapdef.h"
#include "random.h"
#include "dgn-delve.h"

/* Original algorithm by Kusigrosz.
 * Kusigrosz' description:
 *
 * Arguments:
 *     ngb_min, ngb_max: the minimum and maximum number of neighbouring
 *         floor cells that a wall cell must have to become a floor cell.
 *         1 <= ngb_min <= 3; ngb_min <= ngb_max <= 8;
 *     connchance: the chance (in percent) that a new connection is
 *         allowed; for ngb_max == 1 this has no effect as any
 *         connecting cell must have 2 neighbours anyway.
 *     cellnum: the maximum number of floor cells that will be generated.
 * The default values of the arguments are defined below.
 *
 * Algorithm description:
 * The algorithm operates on a rectangular grid. Each cell can be 'wall'
 * or 'floor'. A (non-border) cell has 8 neighbours - diagonals count.
 * There is also a cell store with two operations: store a given cell on
 * top, and pull a cell from the store. The cell to be pulled is selected
 * randomly from the store if N_cells_in_store < 125, and from the top
 * 25 * cube_root(N_cells_in_store) otherwise. There is no check for
 * repetitions, so a given cell can be stored multiple times.
 *
 * The algorithm starts with most of the map filled with 'wall', with a
 * "seed" of some floor cells; their neighbouring wall cells are in store.
 * The main loop in delveon() is repeated until the desired number of
 * floor cells is achieved, or there is nothing in store:
 *     1) Get a cell from the store;
 *     Check the conditions:
 *     a) the cell has between ngb_min and ngb_max floor neighbours,
 *     b) making it a floor cell won't open new connections,
 *         or the RNG allows it with connchance/100 chance.
 *     if a) and b) are met, the cell becomes floor, and its wall
 *     neighbours are put in store in random order.
 * There are many variants possible, for example:
 * 1) picking the cell in rndpull() always from the whole store makes
 *     compact patterns;
 * 2) storing the neighbours in digcell() clockwise starting from
 *     a random one, and picking the bottom cell in rndpull() creates
 *     meandering or spiral patterns.
 */

/*
  My changes (1KB):

  * the cell pulled is always taken from the top 125 entries rather than
    from cube_root(store size).  This doesn't seem to make a large
    difference compared to just bumping the number, but I might have not
    looked well enough.  This makes it simple to customize this number
    ("top"), but that can be easily done other ways.

  * order of adding neighbours to the store is always randomized, otherwise
    we had a bias towards one of the corners, pronounced for small values
    of "top" (hardly noticeable for 125).

  * when pulling a cell from the store, the rest of the store is moved
    instead of shuffling the top cell into the removed one's place. This
    makes theoretical analysis of the algorithm simpler and makes
    meandering slightly more pronounced, at the cost of performance and
    some randomness.  I guess that's a bad idea after all...
*/

typedef deque<coord_def> store_type;

static coord_def _rndpull(store_type& store, int top)
{
    if (store.empty())
        return coord_def();

    ASSERT(top > 0);
    top = min<int>(top, store.size());
    ASSERT(top > 0);

    // TODO: that ³√size thingy?

    top = store.size() - 1 - random2(top);
    coord_def c = store[top];
    store.erase(store.begin() + top);
    return c;
}

static inline bool _in_map(map_lines *map, coord_def c)
{
    return map ? map->in_map(c) : in_bounds(c);
}

static bool _diggable(map_lines *map, coord_def c)
{
    if (map)
        return (*map)(c) == 'x';
    return grd(c) == DNGN_ROCK_WALL && !map_masked(c, MMT_VAULT);
}

static bool _dug(map_lines *map, coord_def c)
{
    if (map)
        return strchr(traversable_glyphs, (*map)(c));
    return grd(c) == DNGN_FLOOR;
}

static void _digcell(map_lines *map, store_type& store, coord_def c)
{
    ASSERT(_in_map(map, c));
    if (!_diggable(map, c))
        return;
    if (map)
        (*map)(c) = '.';
    else
        grd(c) = DNGN_FLOOR;

    int order[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (unsigned int d = 8; d > 0; d--)
    {
        int ornd = random2(d);
        coord_def neigh = c + Compass[order[ornd]];
        order[ornd] = order[d - 1];

        if (!_in_map(map, neigh) || !_diggable(map, neigh))
            continue;

        store.push_back(neigh);
    }
}

// Count dug out neighbours.
static int ngb_count(map_lines *map, coord_def c)
{
    ASSERT(_in_map(map, c));

    int cnt = 0;
    for (unsigned int d = 0; d < 8; d++)
    {
        coord_def neigh = c + Compass[d];
        if (_dug(map, neigh))
            cnt++;
    }
    return cnt;
}

// Count disjoint groups of dug out neighbours.
static int ngb_groups(map_lines *map, coord_def c)
{
    ASSERT(_in_map(map, c));

    bool prev2 = 0, prev = _dug(map, c + Compass[0]);
    int cnt = 0;
    for (int d = 7; d >= 0; d--)
    {
        bool cur = _dug(map, c + Compass[d]);
        // Diagonal connectivity counts, too -- but only cardinal directions
        // (even Compass indices) can reach their predecessors.
        if (cur && !prev && (d&1 || !prev2))
            cnt++;
        prev2 = prev;
        prev = cur;
    }

    // special case: everything around is dug out
    if (prev && !cnt)
        return 1;

    return cnt;
}

// A reasonable default for the given map size and arguments.
static int cellnum_est(int world, int ngb_min, int ngb_max)
{
    static int denom[12] = {0, 0, 8, 7, 6, 5, 5, 4, 4, 4, 3, 3};
    ASSERT(world > 0);
    ASSERT_RANGE(ngb_min + ngb_max, 2, 12);

    return world / denom[ngb_min + ngb_max];
}

static bool _is_seed(map_lines *map, coord_def c)
{
    for (adjacent_iterator ai(c); ai; ++ai)
        if (_dug(map, *ai))
            return true;
    return false;
}

// Ensure there's something in the store.
static int _make_seed(map_lines *map, store_type& store)
{
    rectangle_iterator rect = map ? map->get_iter() : rectangle_iterator(1);

    int x = 0, y = 0, cnt = 0;
    for (rectangle_iterator ri = rect; ri; ++ri)
        if (_diggable(map, *ri))
        {
            if (_is_seed(map, *ri))
                store.push_back(*ri);
            x += ri->x;
            y += ri->y;
            cnt++;
        }

    // Note: every seed must have enough meat on it to allow depositing more
    // around it; this is not checked for here.

    if (!store.empty())
        return cnt;

    if (!cnt)
        return 0;

    // No existing seed, pick a point nearest to the center.

    coord_def center(x / cnt, y / cnt);
    coord_def best;
    int bdist = INT_MAX;
    for (rectangle_iterator ri = rect; ri; ++ri)
        if (_diggable(map, *ri))
        {
            int dist = (*ri - center).abs();
            if (dist < bdist)
                best = *ri, bdist = dist;
        }
    ASSERT(bdist != INT_MAX);
    store.push_back(best);

    return cnt;
}

void delve(map_lines *map, int ngb_min, int ngb_max, int connchance, int cellnum, int top)
{
    ASSERT_RANGE(ngb_min, 1, 4);
    ASSERT(ngb_min <= ngb_max);
    ASSERT(ngb_max <= 8);
    ASSERT_RANGE(connchance, 0, 101);

    store_type store;
    int world = _make_seed(map, store);

    if (cellnum < 0)
        cellnum = cellnum_est(world, ngb_min, ngb_max);

    ASSERT(cellnum <= world);

    int delved = 0;
    int retries = 0;

retry:
    int minseed = delved + 2 * ngb_min;
    coord_def center = _rndpull(store, 999999);
    if (center.origin()) // can't do anything
        return;
    store.push_back(center);
    while (delved < minseed && delved < cellnum)
    {
        coord_def c = _rndpull(store, top);
        if (c.origin())
            break;
        if (!_diggable(map, c))
            continue;

        if ((c - center).abs() > 2
            || ngb_count(map, c) > ngb_max
            || (ngb_groups(map, c) > 1 && !x_chance_in_y(connchance, 100)))
        {
            // Original algorithm:
            // * ignore ngb_min
            // * restricting the initial seed to a 3x3 square
            // This works only on an empty grid, fixme.
            //
            // What we really need is to grow every seed enough to make
            // further generation possible with the ngb_min requirement.
            continue;
        }

        _digcell(map, store, c);
        delved++;
    }

    while (delved < cellnum)
    {
        coord_def c = _rndpull(store, top);
        if (c.origin())
            break;
        if (!_diggable(map, c))
            continue;

        int ngbcount = ngb_count(map, c);

        if (ngbcount < ngb_min || ngbcount > ngb_max
            || (ngb_groups(map, c) > 1 && !x_chance_in_y(connchance, 100)))
        {
            continue;
        }

        _digcell(map, store, c);
        delved++;
    }

    // Certain parameters (2,2 and sometimes 3,3) tend to get stuck.
    if (delved < cellnum && retries++ < 50)
    {
        dprf("delve() try %d: only %d/%d done", retries, delved, cellnum);
        _make_seed(map, store);
        goto retry;
    }
}
