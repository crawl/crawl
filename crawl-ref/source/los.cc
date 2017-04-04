/**
 * @file
 * @brief Line-of-sight algorithm.
 *
 *
 *
 * == Definition of visibility ==
 *
 * Two cells are in view of each other if there is any straight
 * line that meets both cells and that doesn't meet any opaque
 * cell in between, and if the cells are in LOS range of each
 * other.
 *
 * Here, to "meet" a cell means to intersect the interior. In
 * particular, rays can pass between to diagonally adjacent
 * walls (as can the player).
 *
 * == Terminology ==
 *
 * A _ray_ is a line, specified by starting point (accx, accy)
 * and slope. A ray determines its _footprint_: the sequence of
 * cells whose interiour it meets.
 *
 * Any prefix of the footprint of a ray is called a _cellray_.
 *
 * For the purposes of LOS calculation, only the footprints
 * are relevant, but rays are also used for shooting beams,
 * which may travel beyond LOS and which can be reflected.
 * See ray.cc.
 *
 * == Overview ==
 *
 * At first use, the LOS code makes some precomputations,
 * filling a list of all relevant rays in one quadrant,
 * and filling data structures that allow calculating LOS
 * in a quadrant without checking each ray.
 *
 * The code provides functions for filling LOS information
 * around a given center efficiently, and for querying rays
 * between two given cells.
**/

#include "AppHdr.h"

#include "los.h"

#include <algorithm>
#include <cmath>

#include "areas.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "losglobal.h"

// These determine what rays are cast in the precomputation,
// and affect start-up time significantly.
// XXX: Argue that these values are sufficient.
#define LOS_MAX_ANGLE (2*LOS_MAX_RANGE-2)
#define LOS_INTERCEPT_MULT (2)

// These store all unique (in terms of footprint) full rays.
// The footprint of ray=fullray[i] consists of ray.length cells,
// stored in ray_coords[ray.start..ray.length-1].
// These are filled during precomputation (_register_ray).
// XXX: fullrays is not needed anymore after precomputation.
struct los_ray;
static vector<los_ray> fullrays;
static vector<coord_def> ray_coords;

// These store all unique minimal cellrays. For each i,
// cellray i ends in cellray_ends[i] and passes through
// thoses cells p that have blockrays(p)[i] set. In other
// words, blockrays(p)[i] is set iff an opaque cell p blocks
// the cellray with index i.
static vector<coord_def> cellray_ends;
typedef FixedArray<bit_vector*, LOS_MAX_RANGE+1, LOS_MAX_RANGE+1> blockrays_t;
static blockrays_t blockrays;

// We also store the minimal cellrays by target position
// for efficient retrieval by find_ray.
// XXX: Consider condensing this representation.
struct cellray;
static FixedArray<vector<cellray>, LOS_MAX_RANGE+1, LOS_MAX_RANGE+1> min_cellrays;

// Temporary arrays used in losight() to track which rays
// are blocked or have seen a smoke cloud.
// Allocated when doing the precomputations.
static bit_vector *dead_rays     = nullptr;
static bit_vector *smoke_rays    = nullptr;

class quadrant_iterator : public rectangle_iterator
{
public:
    quadrant_iterator()
        : rectangle_iterator(coord_def(0,0),
                             coord_def(LOS_MAX_RANGE, LOS_MAX_RANGE))
    {
    }
};

void clear_rays_on_exit()
{
    delete dead_rays;
    delete smoke_rays;
    for (quadrant_iterator qi; qi; ++qi)
        delete blockrays(*qi);
}

// LOS radius.
int los_radius = LOS_DEFAULT_RANGE;

static void _handle_los_change();

void set_los_radius(int r)
{
    ASSERT(r <= LOS_RADIUS);
    los_radius = r;
    invalidate_los();
    _handle_los_change();
}

int get_los_radius()
{
    return los_radius;
}

bool double_is_zero(const double x)
{
    return x > -EPSILON_VALUE && x < EPSILON_VALUE;
}

struct los_ray : public ray_def
{
    // The footprint of this ray is stored in
    // ray_coords[start..start+length-1].
    unsigned int start;
    unsigned int length;

    los_ray(geom::ray _r)
        : ray_def(_r), start(0), length(0)
    {
    }

    // Shoot a ray from the given start point (accx, accy) with the given
    // slope, bounded by the pre-calc bounds shape.
    // Returns the cells it travels through, excluding the origin.
    // Returns an empty vector if this was a bad ray.
    vector<coord_def> footprint()
    {
        vector<coord_def> cs;
        los_ray copy = *this;
        coord_def c;
        coord_def old;
        int cellnum;
        for (cellnum = 0; true; ++cellnum)
        {
            old = c;
            if (!copy.advance())
            {
//                dprf("discarding corner ray (%f,%f) + t*(%f,%f)",
//                     r.start.x, r.start.y, r.dir.x, r.dir.y);
                cs.clear();
                break;
            }
            c = copy.pos();
            if (c.rdist() > LOS_RADIUS)
                break;
            cs.push_back(c);
            ASSERT((c - old).rdist() == 1);
        }
        return cs;
    }

    coord_def operator[](unsigned int i)
    {
        ASSERT(i < length);
        return ray_coords[start+i];
    }
};

// Check if the passed rays have identical footprint.
static bool _is_same_ray(los_ray ray, vector<coord_def> newray)
{
    if (ray.length != newray.size())
        return false;
    for (unsigned int i = 0; i < ray.length; i++)
        if (ray[i] != newray[i])
            return false;
    return true;
}

// Check if the passed ray has already been created.
static bool _is_duplicate_ray(vector<coord_def> newray)
{
    for (los_ray lray : fullrays)
        if (_is_same_ray(lray, newray))
            return true;
    return false;
}

// A cellray given by fullray and index of end-point.
struct cellray
{
    // A cellray passes through cells ray_coords[ray.start..ray.start+end].
    los_ray ray;
    unsigned int end; // Relative index (inside ray) of end cell.

    cellray(const los_ray& r, unsigned int e)
        : ray(r), end(e), imbalance(-1), first_diag(false)
    {
    }

    // The end-point's index inside ray_coord.
    int index() const { return ray.start + end; }

    // The end-point.
    coord_def target() const { return ray_coords[index()]; }

    // XXX: Currently ray/cellray[0] is the first point outside the origin.
    coord_def operator[](unsigned int i)
    {
        ASSERT(i <= end);
        return ray_coords[ray.start+i];
    }

    // Parameters used in find_ray. These need to be calculated
    // only for the minimal cellrays.
    int imbalance;
    bool first_diag;

    void calc_params();
};

// Compare two cellrays to the same target.
// This determines which ray is considered better by find_ray,
// used with list::sort.
// Returns true if a is strictly better than b, false else.
static bool _is_better(const cellray& a, const cellray& b)
{
    // Only compare cellrays with equal target.
    ASSERT(a.target() == b.target());
    // calc_params() has been called.
    ASSERT(a.imbalance >= 0);
    ASSERT(b.imbalance >= 0);
    if (a.imbalance < b.imbalance)
        return true;
    else if (a.imbalance > b.imbalance)
        return false;
    else
        return a.first_diag && !b.first_diag;
}

enum class compare_type
{
    neither,
    subray,
    superray,
};

// Check whether one of the passed cellrays is a subray of the
// other in terms of footprint.
static compare_type _compare_cellrays(const cellray& a, const cellray& b)
{
    if (a.target() != b.target())
        return compare_type::neither;

    int cura = a.ray.start;
    int curb = b.ray.start;
    int enda = cura + a.end;
    int endb = curb + b.end;
    bool maybe_sub = true;
    bool maybe_super = true;

    while (cura < enda && curb < endb && (maybe_sub || maybe_super))
    {
        coord_def pa = ray_coords[cura];
        coord_def pb = ray_coords[curb];
        if (pa.x > pb.x || pa.y > pb.y)
        {
            maybe_super = false;
            curb++;
        }
        if (pa.x < pb.x || pa.y < pb.y)
        {
            maybe_sub = false;
            cura++;
        }
        if (pa == pb)
        {
            cura++;
            curb++;
        }
    }
    maybe_sub = maybe_sub && cura == enda;
    maybe_super = maybe_super && curb == endb;

    if (maybe_sub)
        return compare_type::subray;    // includes equality
    else if (maybe_super)
        return compare_type::superray;
    else
        return compare_type::neither;
}

// Determine all minimal cellrays.
// They're stored globally by target in min_cellrays,
// and returned as a list of indices into ray_coords.
static vector<int> _find_minimal_cellrays()
{
    FixedArray<list<cellray>, LOS_MAX_RANGE+1, LOS_MAX_RANGE+1> minima;
    list<cellray>::iterator min_it;

    for (los_ray ray : fullrays)
    {
        for (unsigned int i = 0; i < ray.length; ++i)
        {
            // Is the cellray ray[0..i] duplicated so far?
            bool dup = false;
            cellray c(ray, i);
            list<cellray>& min = minima(c.target());

            bool erased = false;
            for (min_it = min.begin();
                 min_it != min.end() && !dup;)
            {
                switch (_compare_cellrays(*min_it, c))
                {
                case compare_type::subray:
                    dup = true;
                    break;
                case compare_type::superray:
                    min_it = min.erase(min_it);
                    erased = true;
                    // clear this should be added, but might have
                    // to erase more
                    break;
                case compare_type::neither:
                default:
                    break;
                }
                if (!erased)
                    ++min_it;
                else
                    erased = false;
            }
            if (!dup)
                min.push_back(c);
        }
    }

    vector<int> result;
    for (quadrant_iterator qi; qi; ++qi)
    {
        list<cellray>& min = minima(*qi);
        for (min_it = min.begin(); min_it != min.end(); ++min_it)
        {
            // Calculate imbalance and slope difference for sorting.
            min_it->calc_params();
            result.push_back(min_it->index());
        }
        min.sort(_is_better);
        min_cellrays(*qi) = vector<cellray>(min.begin(), min.end());
    }
    return result;
}

// Create and register the ray defined by the arguments.
static void _register_ray(geom::ray r)
{
    los_ray ray = los_ray(r);
    vector<coord_def> coords = ray.footprint();

    if (coords.empty() || _is_duplicate_ray(coords))
        return;

    ray.start = ray_coords.size();
    ray.length = coords.size();
    for (coord_def c : coords)
        ray_coords.push_back(c);
    fullrays.push_back(ray);
}

static void _create_blockrays()
{
    // First, we calculate blocking information for all cell rays.
    // Cellrays are numbered according to the index of their end
    // cell in ray_coords.
    const int n_cellrays = ray_coords.size();
    blockrays_t all_blockrays;
    for (quadrant_iterator qi; qi; ++qi)
        all_blockrays(*qi) = new bit_vector(n_cellrays);

    for (los_ray ray : fullrays)
    {
        for (unsigned int i = 0; i < ray.length; ++i)
        {
            // Every cell is contained in (thus blocks)
            // all following cellrays.
            for (unsigned int j = i + 1; j < ray.length; ++j)
                all_blockrays(ray[i])->set(ray.start + j);
        }
    }

    // We've built the basic blockray array; now compress it, keeping
    // only the nonduplicated cellrays.

    // Determine minimal cellrays and store their indices in ray_coords.
    vector<int> min_indices = _find_minimal_cellrays();
    const int n_min_rays    = min_indices.size();
    cellray_ends.resize(n_min_rays);
    for (int i = 0; i < n_min_rays; ++i)
        cellray_ends[i] = ray_coords[min_indices[i]];

    // Compress blockrays accordingly.
    for (quadrant_iterator qi; qi; ++qi)
    {
        blockrays(*qi) = new bit_vector(n_min_rays);
        for (int i = 0; i < n_min_rays; ++i)
        {
            blockrays(*qi)->set(i, all_blockrays(*qi)
                                   ->get(min_indices[i]));
        }
    }

    // We can throw away all_blockrays now.
    for (quadrant_iterator qi; qi; ++qi)
        delete all_blockrays(*qi);

    dead_rays  = new bit_vector(n_min_rays);
    smoke_rays = new bit_vector(n_min_rays);

    dprf("Cellrays: %d Fullrays: %u Minimal cellrays: %u",
          n_cellrays, (unsigned int)fullrays.size(), n_min_rays);
}

static int _gcd(int x, int y)
{
    int tmp;
    while (y != 0)
    {
        x %= y;
        tmp = x;
        x = y;
        y = tmp;
    }
    return x;
}

static bool _complexity_lt(const pair<int,int>& lhs, const pair<int,int>& rhs)
{
    return lhs.first * lhs.second < rhs.first * rhs.second;
}

// Cast all rays
static void raycast()
{
    static bool done_raycast = false;
    if (done_raycast)
        return;

    // Creating all rays for first quadrant
    // We have a considerable amount of overkill.
    done_raycast = true;

    // register perpendiculars FIRST, to make them top choice
    // when selecting beams
    _register_ray(geom::ray(0.5, 0.5, 0.0, 1.0));
    _register_ray(geom::ray(0.5, 0.5, 1.0, 0.0));

    // For a slope of M = y/x, every x we move on the X axis means
    // that we move y on the y axis. We want to look at the resolution
    // of x/y: in that case, every step on the X axis means an increase
    // of 1 in the Y axis at the intercept point. We can assume gcd(x,y)=1,
    // so we look at steps of 1/y.

    // Changing the order a bit. We want to order by the complexity
    // of the beam, which is log(x) + log(y) ~ xy.
    vector<pair<int,int> > xyangles;
    for (int xangle = 1; xangle <= LOS_MAX_ANGLE; ++xangle)
        for (int yangle = 1; yangle <= LOS_MAX_ANGLE; ++yangle)
        {
            if (_gcd(xangle, yangle) == 1)
                xyangles.emplace_back(xangle, yangle);
        }

    sort(xyangles.begin(), xyangles.end(), _complexity_lt);
    for (auto xyangle : xyangles)
    {
        const int xangle = xyangle.first;
        const int yangle = xyangle.second;

        for (int intercept = 1; intercept < LOS_INTERCEPT_MULT*yangle; ++intercept)
        {
            double xstart = ((double)intercept) / (LOS_INTERCEPT_MULT*yangle);
            double ystart = 0.5;

            _register_ray(geom::ray(xstart, ystart, xangle, yangle));
            // also draw the identical ray in octant 2
            _register_ray(geom::ray(ystart, xstart, yangle, xangle));
        }
    }

    // Now create the appropriate blockrays array
    _create_blockrays();
}

static int _imbalance(ray_def ray, const coord_def& target)
{
    int imb = 0;
    int diags = 0, straights = 0;
    while (ray.pos() != target)
    {
        coord_def old = ray.pos();
        if (!ray.advance())
            die("can't advance ray");
        switch ((ray.pos() - old).abs())
        {
        case 1:
            diags = 0;
            if (++straights > imb)
                imb = straights;
            break;
        case 2:
            straights = 0;
            if (++diags > imb)
                imb = diags;
            break;
        default:
            die("ray imbalance out of range");
        }
    }
    return imb;
}

void cellray::calc_params()
{
    coord_def trg = target();
    imbalance = _imbalance(ray, trg);
    first_diag = ((*this)[0].abs() == 2);
}

// Find ray in positive quadrant.
// opc has been translated for this quadrant.
// XXX: Allow finding ray of minimum opacity.
static bool _find_ray_se(const coord_def& target, ray_def& ray,
                  const opacity_func& opc, int range, bool cycle)
{
    ASSERT(target.x >= 0);
    ASSERT(target.y >= 0);
    ASSERT(!target.origin());
    if (target.rdist() > range)
        return false;

    ASSERT(target.rdist() <= LOS_RADIUS);

    // Ensure the precalculations have been done.
    raycast();

    const vector<cellray> &min = min_cellrays(target);
    ASSERT(!min.empty());
    cellray c = min[0]; // XXX: const cellray &c ?
    unsigned int index = 0;

    if (cycle)
        dprf("cycling from %d (total %u)", ray.cycle_idx, (unsigned int)min.size());

    unsigned int start = cycle ? ray.cycle_idx + 1 : 0;
    ASSERT(start <= min.size());

    int blocked = OPC_OPAQUE;
    for (unsigned int i = start;
         (blocked >= OPC_OPAQUE) && (i < start + min.size()); i++)
    {
        index = i % min.size();
        c = min[index];
        blocked = OPC_CLEAR;
        // Check all inner points.
        for (unsigned int j = 0; j < c.end && blocked < OPC_OPAQUE; j++)
            blocked += opc(c[j]);
    }
    if (blocked >= OPC_OPAQUE)
        return false;

    ray = c.ray;
    ray.cycle_idx = index;

    return true;
}

// Coordinate transformation so we can find_ray quadrant-by-quadrant.
struct opacity_trans : public opacity_func
{
    const coord_def& source;
    int signx, signy;
    const opacity_func& orig;

    opacity_trans(const opacity_func& opc, const coord_def& s, int sx, int sy)
        : source(s), signx(sx), signy(sy), orig(opc)
    {
    }

    CLONE(opacity_trans)

    opacity_type operator()(const coord_def &l) const override
    {
        return orig(transform(l));
    }

    coord_def transform(const coord_def &l) const
    {
        return coord_def(source.x + signx*l.x, source.y + signy*l.y);
    }
};

// Find a nonblocked ray from source to target. Return false if no
// such ray could be found, otherwise return true and fill ray
// appropriately.
// if range is too great or all rays are blocked.
// If cycle is false, find the first fitting ray. If it is true,
// assume that ray is appropriately filled in, and look for the next
// ray. We only ever use ray.cycle_idx.
bool find_ray(const coord_def& source, const coord_def& target,
              ray_def& ray, const opacity_func& opc, int range,
              bool cycle)
{
    if (target == source || !map_bounds(source) || !map_bounds(target))
        return false;

    const int signx = ((target.x - source.x >= 0) ? 1 : -1);
    const int signy = ((target.y - source.y >= 0) ? 1 : -1);
    const int absx  = signx * (target.x - source.x);
    const int absy  = signy * (target.y - source.y);
    const coord_def abs = coord_def(absx, absy);
    opacity_trans opc_trans = opacity_trans(opc, source, signx, signy);

    if (!_find_ray_se(abs, ray, opc_trans, range, cycle))
        return false;

    if (signx < 0)
        ray.r.start.x = 1.0 - ray.r.start.x;
    if (signy < 0)
        ray.r.start.y = 1.0 - ray.r.start.y;
    ray.r.dir.x *= signx;
    ray.r.dir.y *= signy;

    ray.r.start.x += source.x;
    ray.r.start.y += source.y;

    return true;
}

bool exists_ray(const coord_def& source, const coord_def& target,
                const opacity_func& opc, int range)
{
    ray_def ray;
    return find_ray(source, target, ray, opc, range);
}

// Assuming that target is in view of source, but line of
// fire is blocked, what is it blocked by?
dungeon_feature_type ray_blocker(const coord_def& source,
                                 const coord_def& target)
{
    ray_def ray;
    if (!find_ray(source, target, ray, opc_default))
    {
        ASSERT(you.xray_vision);
        return NUM_FEATURES;
    }

    ray.advance();
    int blocked = 0;
    while (ray.pos() != target)
    {
        blocked += opc_solid_see(ray.pos());
        if (blocked >= OPC_OPAQUE)
            return env.grid(ray.pos());
        ray.advance();
    }
    ASSERT(false);
    return NUM_FEATURES;
}

// Returns a straight ray from source to target.
void fallback_ray(const coord_def& source, const coord_def& target,
                  ray_def& ray)
{
    ray.r.start.x = source.x + 0.5;
    ray.r.start.y = source.y + 0.5;
    coord_def diff = target - source;
    ray.r.dir.x = diff.x;
    ray.r.dir.y = diff.y;
    ray.on_corner = false;
}

// Count the number of matching features between two points along
// a beam-like path; the path will pass through solid features.
// By default, it excludes end points from the count.
// If just_check is true, the function will return early once one
// such feature is encountered.
int num_feats_between(const coord_def& source, const coord_def& target,
                      dungeon_feature_type min_feat,
                      dungeon_feature_type max_feat,
                      bool exclude_endpoints, bool just_check)
{
    ray_def ray;
    int     count    = 0;
    int     max_dist = grid_distance(source, target);

    ASSERT(map_bounds(source));
    ASSERT(map_bounds(target));

    if (source == target)
        return 0; // XXX: might want to count the cell.

    // We don't need to find the shortest beam, any beam will suffice.
    fallback_ray(source, target, ray);

    if (exclude_endpoints && ray.pos() == source)
    {
        ray.advance();
        max_dist--;
    }

    int dist = 0;
    bool reached_target = false;
    while (dist++ <= max_dist)
    {
        const dungeon_feature_type feat = grd(ray.pos());

        if (ray.pos() == target)
            reached_target = true;

        if (feat >= min_feat && feat <= max_feat
            && (!exclude_endpoints || !reached_target))
        {
            count++;

            if (just_check) // Only needs to be > 0.
                return count;
        }

        if (reached_target)
            break;

        ray.advance();
    }

    return count;
}

// Is p2 visible from p1, disregarding half-opaque objects?
bool cell_see_cell_nocache(const coord_def& p1, const coord_def& p2)
{
    return exists_ray(p1, p2, opc_fullyopaque);
}

// We use raycasting. The algorithm:
// PRECOMPUTATION:
// Create a large bundle of rays and cast them.
// Mark, for each one, which cells kill it (and where.)
// Also, for each one, note which cells it passes.
// ACTUAL LOS:
// Unite the ray-killers for the given map; this tells you which rays
// are dead.
// Look up which cells the surviving rays have, and that's your LOS!
// OPTIMIZATIONS:
// WLOG, we can assume that we're in a specific quadrant - say the
// first quadrant - and just mirror everything after that. We can
// likely get away with a single octant, but we don't do that. (To
// do...)
// Rays are actually split by each cell they pass. So each "ray" only
// identifies a single cell, and we can do logical ORs. Once a cell
// kills a cellray, it will kill all remaining cellrays of that ray.
// Also, rays are checked to see if they are duplicates of each
// other. If they are, they're eliminated.
// Some cellrays can also be eliminated. In general, a cellray is
// unnecessary if there is another cellray with the same coordinates,
// and whose path (up to those coordinates) is a subset, not necessarily
// proper, of the original path. We still store the original cellrays
// fully for beam detection and such.
// PERFORMANCE:
// With reasonable values we have around 6000 cellrays, meaning
// around 600Kb (75 KB) of data. This gets cut down to 700 cellrays
// after removing duplicates. That means that we need to do
// around 22*100*4 ~ 9,000 memory reads + writes per LOS call on a
// 32-bit system. Not too bad.
// IMPROVEMENTS:
// Smoke will now only block LOS after two cells of smoke. This is
// done by updating with a second array.

static void _losight_quadrant(los_grid& sh, const los_param& dat, int sx, int sy)
{
    const unsigned int num_cellrays = cellray_ends.size();

    dead_rays->reset();
    smoke_rays->reset();

    for (quadrant_iterator qi; qi; ++qi)
    {
        coord_def p = coord_def(sx*(qi->x), sy*(qi->y));
        if (!dat.los_bounds(p))
            continue;

        switch (dat.opacity(p))
        {
        case OPC_OPAQUE:
            // Block the appropriate rays.
            *dead_rays |= *blockrays(*qi);
            break;
        case OPC_HALF:
            // Block rays which have already seen a cloud.
            *dead_rays  |= (*smoke_rays & *blockrays(*qi));
            *smoke_rays |= *blockrays(*qi);
            break;
        default:
            break;
        }
    }

    // Ray calculation done. Now work out which cells in this
    // quadrant are visible.
    for (unsigned int rayidx = 0; rayidx < num_cellrays; ++rayidx)
    {
        // make the cells seen by this ray at this point visible
        if (!dead_rays->get(rayidx))
        {
            // This ray is alive, thus the end cell is visible.
            const coord_def p = coord_def(sx * cellray_ends[rayidx].x,
                                          sy * cellray_ends[rayidx].y);
            if (dat.los_bounds(p))
                sh(p) = true;
        }
    }
}

struct los_param_funcs : public los_param
{
    coord_def center;
    const opacity_func& opc;
    const circle_def& bounds;

    los_param_funcs(const coord_def& c,
                    const opacity_func& o, const circle_def& b)
        : center(c), opc(o), bounds(b)
    {
    }

    bool los_bounds(const coord_def& p) const override
    {
        return map_bounds(p + center) && bounds.contains(p);
    }

    opacity_type opacity(const coord_def& p) const override
    {
        return opc(p + center);
    }
};

void losight(los_grid& sh, const coord_def& center,
             const opacity_func& opc, const circle_def& bounds)
{
    const los_param& dat = los_param_funcs(center, opc, bounds);

    sh.init(false);

    // Do precomputations if necessary.
    raycast();

    const int quadrant_x[4] = {  1, -1, -1,  1 };
    const int quadrant_y[4] = {  1,  1, -1, -1 };
    for (int q = 0; q < 4; ++q)
        _losight_quadrant(sh, dat, quadrant_x[q], quadrant_y[q]);

    // Center is always visible.
    const coord_def o = coord_def(0,0);
    sh(o) = true;
}

opacity_type mons_opacity(const monster* mon, los_type how)
{
    // no regard for LOS_ARENA
    if (mons_species(mon->type) == MONS_BUSH
        && how != LOS_SOLID)
    {
        return OPC_HALF;
    }

    return OPC_CLEAR;
}

/////////////////////////////////////
// A start at tracking LOS changes.

// Something that affects LOS (with default parameters)
// has changed somewhere.
static void _handle_los_change()
{
    invalidate_agrid();
}

static bool _mons_block_sight(const monster* mons)
{
    // must be the least permissive one
    return mons_opacity(mons, LOS_SOLID_SEE) != OPC_CLEAR;
}

void los_actor_moved(const actor* act, const coord_def& oldpos)
{
    if (act->is_monster() && _mons_block_sight(act->as_monster()))
    {
        invalidate_los_around(oldpos);
        invalidate_los_around(act->pos());
        _handle_los_change();
    }
}

void los_monster_died(const monster* mon)
{
    if (_mons_block_sight(mon))
    {
        invalidate_los_around(mon->pos());
        _handle_los_change();
    }
}

// Might want to pass new/old terrain.
void los_terrain_changed(const coord_def& p)
{
    invalidate_los_around(p);
    _handle_los_change();
}

void los_changed()
{
    invalidate_los();
    _handle_los_change();
}
