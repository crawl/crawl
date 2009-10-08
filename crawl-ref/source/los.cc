/*
 *  File:       los.cc
 *  Summary:    Line-of-sight algorithm.
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "los.h"

#include <cmath>

#include "cloud.h"
#include "debug.h"
#include "directn.h"
#include "externs.h"
#include "ray.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"

// The LOS code now uses raycasting -- haranp

#define LONGSIZE (sizeof(unsigned long)*8)
#define LOS_MAX_RANGE_X 9
#define LOS_MAX_RANGE_Y 9
#define LOS_MAX_RANGE 9

// the following two constants represent the 'middle' of the sh array.
// since the current shown area is 19x19, centering the view at (9,9)
// means it will be exactly centered.
// This is done to accomodate possible future changes in viewable screen
// area - simply change sh_xo and sh_yo to the new view center.

const int sh_xo = 9;            // X and Y origins for the sh array
const int sh_yo = 9;

unsigned long* los_blockrays = NULL;
unsigned long* dead_rays     = NULL;
unsigned long* smoke_rays    = NULL;
std::vector<short> ray_coord_x;
std::vector<short> ray_coord_y;
std::vector<short> compressed_ray_x;
std::vector<short> compressed_ray_y;
std::vector<int> raylengths;
std::vector<ray_def> fullrays;

void clear_rays_on_exit()
{
    delete[] dead_rays;
    delete[] smoke_rays;
    delete[] los_blockrays;
}

int _los_radius_squared = LOS_RADIUS * LOS_RADIUS + 1;

void setLOSRadius(int newLR)
{
    _los_radius_squared = newLR * newLR + 1*1;
}

// XXX: just for monster_los
int get_los_radius_squared()
{
    return _los_radius_squared;
}

bool _get_bit_in_long_array( const unsigned long* data, int where )
{
    int wordloc = where / LONGSIZE;
    int bitloc = where % LONGSIZE;
    return ((data[wordloc] & (1UL << bitloc)) != 0);
}

static void _set_bit_in_long_array( unsigned long* data, int where )
{
    int wordloc = where / LONGSIZE;
    int bitloc = where % LONGSIZE;
    data[wordloc] |= (1UL << bitloc);
}

bool double_is_zero( const double x )
{
    return (x > -EPSILON_VALUE) && (x < EPSILON_VALUE);
}

// Check if the passed ray has already been created.
static bool _is_duplicate_ray( int len, int xpos[], int ypos[] )
{
    int cur_offset = 0;
    for (unsigned int i = 0; i < raylengths.size(); ++i)
    {
        // Only compare equal-length rays.
        if (raylengths[i] != len)
        {
            cur_offset += raylengths[i];
            continue;
        }

        int j;
        for (j = 0; j < len; ++j)
        {
            if (ray_coord_x[j + cur_offset] != xpos[j]
                || ray_coord_y[j + cur_offset] != ypos[j])
            {
                break;
            }
        }

        // Exact duplicate?
        if (j == len)
            return (true);

        // Move to beginning of next ray.
        cur_offset += raylengths[i];
    }
    return (false);
}

// Is starta...lengtha a subset of startb...lengthb?
static bool _is_subset( int starta, int startb, int lengtha, int lengthb )
{
    int cura = starta, curb = startb;
    int enda = starta + lengtha, endb = startb + lengthb;

    while (cura < enda && curb < endb)
    {
        if (ray_coord_x[curb] > ray_coord_x[cura])
            return (false);
        if (ray_coord_y[curb] > ray_coord_y[cura])
            return (false);

        if (ray_coord_x[cura] == ray_coord_x[curb]
            && ray_coord_y[cura] == ray_coord_y[curb])
        {
            ++cura;
        }

        ++curb;
    }

    return (cura == enda);
}

// Returns a vector which lists all the nonduped cellrays (by index).
static std::vector<int> _find_nonduped_cellrays()
{
    // A cellray c in a fullray f is duped if there is a fullray g
    // such that g contains c and g[:c] is a subset of f[:c].
    int raynum, cellnum, curidx, testidx, testray, testcell;
    bool is_duplicate;

    std::vector<int> result;
    for (curidx = 0, raynum = 0;
         raynum < static_cast<int>(raylengths.size());
         curidx += raylengths[raynum++])
    {
        for (cellnum = 0; cellnum < raylengths[raynum]; ++cellnum)
        {
            // Is the cellray raynum[cellnum] duplicated?
            is_duplicate = false;
            // XXX: We should really check everything up to now
            // completely, and all further rays to see if they're
            // proper subsets.
            const int curx = ray_coord_x[curidx + cellnum];
            const int cury = ray_coord_y[curidx + cellnum];
            for (testidx = 0, testray = 0; testray < raynum;
                 testidx += raylengths[testray++])
            {
                // Scan ahead to see if there's an intersect.
                for (testcell = 0; testcell < raylengths[raynum]; ++testcell)
                {
                    const int testx = ray_coord_x[testidx + testcell];
                    const int testy = ray_coord_y[testidx + testcell];
                    // We can short-circuit sometimes.
                    if (testx > curx || testy > cury)
                        break;

                    // Bingo!
                    if (testx == curx && testy == cury)
                    {
                        is_duplicate = _is_subset(testidx, curidx,
                                                  testcell, cellnum);
                        break;
                    }
                }
                if (is_duplicate)
                    break;      // No point in checking further rays.
            }
            if (!is_duplicate)
                result.push_back(curidx + cellnum);
        }
    }
    return result;
}

// Create and register the ray defined by the arguments.
// Return true if the ray was actually registered (i.e., not a duplicate.)
static bool _register_ray( double accx, double accy, double slope )
{
    int xpos[LOS_MAX_RANGE * 2 + 1], ypos[LOS_MAX_RANGE * 2 + 1];
    int raylen = shoot_ray( accx, accy, slope, LOS_MAX_RANGE, xpos, ypos );

    // Early out if ray already exists.
    if (_is_duplicate_ray(raylen, xpos, ypos))
        return (false);

    // Not duplicate, register.
    for (int i = 0; i < raylen; ++i)
    {
        // Create the cellrays.
        ray_coord_x.push_back(xpos[i]);
        ray_coord_y.push_back(ypos[i]);
    }

    // Register the fullray.
    raylengths.push_back(raylen);
    ray_def ray;
    ray.accx = accx;
    ray.accy = accy;
    ray.slope = slope;
    ray.quadrant = 0;
    fullrays.push_back(ray);

    return (true);
}

static void _create_blockrays()
{
    // determine nonduplicated rays
    std::vector<int> nondupe_cellrays    = _find_nonduped_cellrays();
    const unsigned int num_nondupe_rays  = nondupe_cellrays.size();
    const unsigned int num_nondupe_words =
        (num_nondupe_rays + LONGSIZE - 1) / LONGSIZE;
    const unsigned int num_cellrays = ray_coord_x.size();
    const unsigned int num_words = (num_cellrays + LONGSIZE - 1) / LONGSIZE;

    // first build all the rays: easier to do blocking calculations there
    unsigned long* full_los_blockrays;
    full_los_blockrays = new unsigned long[num_words * (LOS_MAX_RANGE_X+1) *
                                           (LOS_MAX_RANGE_Y+1)];
    memset((void*)full_los_blockrays, 0, sizeof(unsigned long) * num_words *
           (LOS_MAX_RANGE_X+1) * (LOS_MAX_RANGE_Y+1));

    int cur_offset = 0;

    for (unsigned int ray = 0; ray < raylengths.size(); ++ray)
    {
        for (int i = 0; i < raylengths[ray]; ++i)
        {
            // every cell blocks...
            unsigned long* const inptr = full_los_blockrays +
                (ray_coord_x[i + cur_offset] * (LOS_MAX_RANGE_Y + 1) +
                 ray_coord_y[i + cur_offset]) * num_words;

            // ...all following cellrays
            for (int j = i+1; j < raylengths[ray]; ++j)
                _set_bit_in_long_array( inptr, j + cur_offset );

        }
        cur_offset += raylengths[ray];
    }

    // we've built the basic blockray array; now compress it, keeping
    // only the nonduplicated cellrays.

    // allocate and clear memory
    los_blockrays = new unsigned long[num_nondupe_words * (LOS_MAX_RANGE_X+1) * (LOS_MAX_RANGE_Y + 1)];
    memset((void*)los_blockrays, 0, sizeof(unsigned long) * num_nondupe_words *
           (LOS_MAX_RANGE_X+1) * (LOS_MAX_RANGE_Y+1));

    // we want to only keep the cellrays from nondupe_cellrays.
    compressed_ray_x.resize(num_nondupe_rays);
    compressed_ray_y.resize(num_nondupe_rays);
    for (unsigned int i = 0; i < num_nondupe_rays; ++i)
    {
        compressed_ray_x[i] = ray_coord_x[nondupe_cellrays[i]];
        compressed_ray_y[i] = ray_coord_y[nondupe_cellrays[i]];
    }
    unsigned long* oldptr = full_los_blockrays;
    unsigned long* newptr = los_blockrays;
    for (int x = 0; x <= LOS_MAX_RANGE_X; ++x)
        for (int y = 0; y <= LOS_MAX_RANGE_Y; ++y)
        {
            for (unsigned int i = 0; i < num_nondupe_rays; ++i)
                if (_get_bit_in_long_array(oldptr, nondupe_cellrays[i]))
                    _set_bit_in_long_array(newptr, i);

            oldptr += num_words;
            newptr += num_nondupe_words;
        }

    // we can throw away full_los_blockrays now
    delete[] full_los_blockrays;

    dead_rays  = new unsigned long[num_nondupe_words];
    smoke_rays = new unsigned long[num_nondupe_words];

#ifdef DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS, "Cellrays: %d Fullrays: %u Compressed: %u",
          num_cellrays, raylengths.size(), num_nondupe_rays );
#endif
}

static int _gcd( int x, int y )
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

bool complexity_lt( const std::pair<int,int>& lhs,
                    const std::pair<int,int>& rhs )
{
    return lhs.first * lhs.second < rhs.first * rhs.second;
}

// Cast all rays
void raycast()
{
    static bool done_raycast = false;
    if (done_raycast)
        return;

    // Creating all rays for first quadrant
    // We have a considerable amount of overkill.
    done_raycast = true;

    // register perpendiculars FIRST, to make them top choice
    // when selecting beams
    _register_ray( 0.5, 0.5, 1000.0 );
    _register_ray( 0.5, 0.5, 0.0 );

    // For a slope of M = y/x, every x we move on the X axis means
    // that we move y on the y axis. We want to look at the resolution
    // of x/y: in that case, every step on the X axis means an increase
    // of 1 in the Y axis at the intercept point. We can assume gcd(x,y)=1,
    // so we look at steps of 1/y.

    // Changing the order a bit. We want to order by the complexity
    // of the beam, which is log(x) + log(y) ~ xy.
    std::vector<std::pair<int,int> > xyangles;
    for ( int xangle = 1; xangle <= 2*LOS_MAX_RANGE; ++xangle )
        for ( int yangle = 1; yangle <= 2*LOS_MAX_RANGE; ++yangle )
        {
            if ( _gcd(xangle, yangle) == 1 )
                xyangles.push_back(std::pair<int,int>(xangle, yangle));
        }

    std::sort( xyangles.begin(), xyangles.end(), complexity_lt );
    for ( unsigned int i = 0; i < xyangles.size(); ++i )
    {
        const int xangle = xyangles[i].first;
        const int yangle = xyangles[i].second;

        const double slope = ((double)(yangle)) / xangle;
        const double rslope = ((double)(xangle)) / yangle;
        for ( int intercept = 1; intercept <= 2*yangle; ++intercept )
        {
            double xstart = ((double)(intercept)) / (2*yangle);
            double ystart = 1;

            // now move back just inside the cell
            // y should be "about to change"
            xstart -= EPSILON_VALUE * xangle;
            ystart -= EPSILON_VALUE * yangle;

            _register_ray( xstart, ystart, slope );
            // also draw the identical ray in octant 2
            _register_ray( ystart, xstart, rslope );
        }
    }

    // Now create the appropriate blockrays array
    _create_blockrays();
}

static void _set_ray_quadrant( ray_def& ray, int sx, int sy, int tx, int ty )
{
    if ( tx >= sx && ty >= sy )
        ray.quadrant = 0;
    else if ( tx < sx && ty >= sy )
        ray.quadrant = 1;
    else if ( tx < sx && ty < sy )
        ray.quadrant = 2;
    else if ( tx >= sx && ty < sy )
        ray.quadrant = 3;
    else
        mpr("Bad ray quadrant!", MSGCH_DIAGNOSTICS);
}

static int _cyclic_offset( unsigned int ui, int cycle_dir, int startpoint,
                           int maxvalue )
{
    const int i = (int)ui;
    if ( startpoint < 0 )
        return i;
    switch ( cycle_dir )
    {
    case 1:
        return (i + startpoint + 1) % maxvalue;
    case -1:
        return (i - 1 - startpoint + maxvalue) % maxvalue;
    case 0:
    default:
        return i;
    }
}

static const double VERTICAL_SLOPE = 10000.0;
static double _calc_slope(double x, double y)
{
    if (double_is_zero(x))
        return (VERTICAL_SLOPE);

    const double slope = y / x;
    return (slope > VERTICAL_SLOPE? VERTICAL_SLOPE : slope);
}

static double _slope_factor(const ray_def &ray)
{
    double xdiff = fabs(ray.accx - 0.5), ydiff = fabs(ray.accy - 0.5);

    if (double_is_zero(xdiff) && double_is_zero(ydiff))
        return ray.slope;

    const double slope = _calc_slope(ydiff, xdiff);
    return (slope + ray.slope) / 2.0;
}

static bool _superior_ray(int shortest, int imbalance,
                          int raylen, int rayimbalance,
                          double slope_diff, double ray_slope_diff)
{
    if (shortest != raylen)
        return (shortest > raylen);

    if (imbalance != rayimbalance)
        return (imbalance > rayimbalance);

    return (slope_diff > ray_slope_diff);
}

// Find a nonblocked ray from source to target. Return false if no
// such ray could be found, otherwise return true and fill ray
// appropriately.
// If allow_fallback is true, fall back to a center-to-center ray
// if range is too great or all rays are blocked.
// If cycle_dir is 0, find the first fitting ray. If it is 1 or -1,
// assume that ray is appropriately filled in, and look for the next
// ray in that cycle direction.
// If find_shortest is true, examine all rays that hit the target and
// take the shortest (starting at ray.fullray_idx).

bool find_ray( const coord_def& source, const coord_def& target,
               bool allow_fallback, ray_def& ray, int cycle_dir,
               bool find_shortest, bool ignore_solid )
{
    int cellray, inray;

    const int sourcex = source.x;
    const int sourcey = source.y;
    const int targetx = target.x;
    const int targety = target.y;

    const int signx = ((targetx - sourcex >= 0) ? 1 : -1);
    const int signy = ((targety - sourcey >= 0) ? 1 : -1);
    const int absx  = signx * (targetx - sourcex);
    const int absy  = signy * (targety - sourcey);

    int cur_offset  = 0;
    int shortest    = INFINITE_DISTANCE;
    int imbalance   = INFINITE_DISTANCE;
    const double want_slope = _calc_slope(absx, absy);
    double slope_diff       = VERTICAL_SLOPE * 10.0;
    std::vector<coord_def> unaliased_ray;

    for ( unsigned int fray = 0; fray < fullrays.size(); ++fray )
    {
        const int fullray = _cyclic_offset( fray, cycle_dir, ray.fullray_idx,
                                            fullrays.size() );
        // Yeah, yeah, this is O(n^2). I know.
        cur_offset = 0;
        for (int i = 0; i < fullray; ++i)
            cur_offset += raylengths[i];

        for (cellray = 0; cellray < raylengths[fullray]; ++cellray)
        {
            if (ray_coord_x[cellray + cur_offset] == absx
                && ray_coord_y[cellray + cur_offset] == absy)
            {
                if (find_shortest)
                {
                    unaliased_ray.clear();
                    unaliased_ray.push_back(coord_def(0, 0));
                }

                // Check if we're blocked so far.
                bool blocked = false;
                coord_def c1, c3;
                int real_length = 0;
                for (inray = 0; inray <= cellray; ++inray)
                {
                    const int xi = signx * ray_coord_x[inray + cur_offset];
                    const int yi = signy * ray_coord_y[inray + cur_offset];
                    if (inray < cellray && !ignore_solid
                        && grid_is_solid(grd[sourcex + xi][sourcey + yi]))
                    {
                        blocked = true;
                        break;
                    }

                    if (find_shortest)
                    {
                        c3 = coord_def(xi, yi);

                        // We've moved at least two steps if inray > 0.
                        if (inray)
                        {
                            // Check for a perpendicular corner on the ray and
                            // pretend that it's a diagonal.
                            if ((c3 - c1).abs() != 2)
                                ++real_length;
                            else
                            {
                                // c2 was a dud move, pop it off
                                unaliased_ray.pop_back();
                            }
                        }
                        else
                            ++real_length;

                        unaliased_ray.push_back(c3);
                        c1 = unaliased_ray[real_length - 1];
                    }
                }

                int cimbalance = 0;
                // If this ray is a candidate for shortest, calculate
                // the imbalance. I'm defining 'imbalance' as the
                // number of consecutive diagonal or orthogonal moves
                // in the ray. This is a reasonable measure of deviation from
                // the Bresenham line between our selected source and
                // destination.
                if (!blocked && find_shortest && shortest >= real_length)
                {
                    int diags = 0, straights = 0;
                    for (int i = 1, size = unaliased_ray.size(); i < size; ++i)
                    {
                        const int dist =
                            (unaliased_ray[i] - unaliased_ray[i - 1]).abs();

                        if (dist == 2)
                        {
                            straights = 0;
                            if (++diags > cimbalance)
                                cimbalance = diags;
                        }
                        else
                        {
                            diags = 0;
                            if (++straights > cimbalance)
                                cimbalance = straights;
                        }
                    }
                }

                const double ray_slope_diff = find_shortest ?
                    fabs(_slope_factor(fullrays[fullray]) - want_slope) : 0.0;

                if (!blocked
                    &&  (!find_shortest
                         || _superior_ray(shortest, imbalance,
                                          real_length, cimbalance,
                                          slope_diff, ray_slope_diff)))
                {
                    // Success!
                    ray             = fullrays[fullray];
                    ray.fullray_idx = fullray;

                    shortest   = real_length;
                    imbalance  = cimbalance;
                    slope_diff = ray_slope_diff;

                    if (sourcex > targetx)
                        ray.accx = 1.0 - ray.accx;
                    if (sourcey > targety)
                        ray.accy = 1.0 - ray.accy;

                    ray.accx += sourcex;
                    ray.accy += sourcey;

                    _set_ray_quadrant(ray, sourcex, sourcey, targetx, targety);
                    if (!find_shortest)
                        return (true);
                }
            }
        }
    }

    if (find_shortest && shortest != INFINITE_DISTANCE)
        return (true);

    if (allow_fallback)
    {
        ray.accx = sourcex + 0.5;
        ray.accy = sourcey + 0.5;
        if (targetx == sourcex)
            ray.slope = VERTICAL_SLOPE;
        else
        {
            ray.slope  = targety - sourcey;
            ray.slope /= targetx - sourcex;
            if (ray.slope < 0)
                ray.slope = -ray.slope;
        }
        _set_ray_quadrant(ray, sourcex, sourcey, targetx, targety);
        ray.fullray_idx = -1;
        return (true);
    }
    return (false);
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

    ray.fullray_idx = -1; // to quiet valgrind

    // We don't need to find the shortest beam, any beam will suffice.
    find_ray( source, target, true, ray, 0, false, true );

    if (exclude_endpoints && ray.pos() == source)
    {
        ray.advance(true);
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
                return (count);
        }

        if (reached_target)
            break;

        ray.advance(true);
    }

    return (count);
}

// Usually calculates whether from one grid someone could see the other.
// Depending on the viewer's habitat, 'allowed' can be set to DNGN_FLOOR,
// DNGN_SHALLOW_WATER or DNGN_DEEP_WATER.
// Yes, this ignores lava-loving monsters.
// XXX: It turns out the beams are not symmetrical, i.e. switching
// pos1 and pos2 may result in small variations.
bool grid_see_grid(const coord_def& p1, const coord_def& p2,
                   dungeon_feature_type allowed)
{
    if (distance(p1, p2) > _los_radius_squared)
        return (false);

    dungeon_feature_type max_disallowed = DNGN_MAXOPAQUE;
    if (allowed != DNGN_UNSEEN)
        max_disallowed = static_cast<dungeon_feature_type>(allowed - 1);

    // XXX: Ignoring clouds for now.
    return (!num_feats_between(p1, p2, DNGN_UNSEEN, max_disallowed,
                               true, true));
}

// The rule behind LOS is:
// Two cells can see each other if there is any line from some point
// of the first to some point of the second ("generous" LOS.)
//
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
// first quadrant - and just mirror everything after that.  We can
// likely get away with a single octant, but we don't do that. (To
// do...)
// Rays are actually split by each cell they pass. So each "ray" only
// identifies a single cell, and we can do logical ORs.  Once a cell
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
void losight(env_show_grid &sh,
             feature_grid &gr, const coord_def& center,
             bool clear_walls_block, bool ignore_clouds)
{
    raycast();
    const int x_p = center.x;
    const int y_p = center.y;
    // go quadrant by quadrant
    const int quadrant_x[4] = {  1, -1, -1,  1 };
    const int quadrant_y[4] = {  1,  1, -1, -1 };

    // clear out sh
    sh.init(0);

    if (crawl_state.arena || crawl_state.arena_suspended)
    {
        for (int y = -ENV_SHOW_OFFSET; y <= ENV_SHOW_OFFSET; ++y)
            for (int x = -ENV_SHOW_OFFSET; x <= ENV_SHOW_OFFSET; ++x)
            {
                const coord_def pos = center + coord_def(x, y);
                if (map_bounds(pos))
                    sh[x + sh_xo][y + sh_yo] = gr(pos);
            }
        return;
    }

    const unsigned int num_cellrays = compressed_ray_x.size();
    const unsigned int num_words = (num_cellrays + LONGSIZE - 1) / LONGSIZE;

    for (int quadrant = 0; quadrant < 4; ++quadrant)
    {
        const int xmult = quadrant_x[quadrant];
        const int ymult = quadrant_y[quadrant];

        // clear out the dead rays array
        memset( (void*)dead_rays,  0, sizeof(unsigned long) * num_words);
        memset( (void*)smoke_rays, 0, sizeof(unsigned long) * num_words);

        // kill all blocked rays
        const unsigned long* inptr = los_blockrays;

        for (int xdiff = 0; xdiff <= LOS_MAX_RANGE_X; ++xdiff)
            for (int ydiff = 0; ydiff <= LOS_MAX_RANGE_Y;
                 ++ydiff, inptr += num_words)
            {

                const int realx = x_p + xdiff * xmult;
                const int realy = y_p + ydiff * ymult;

                if (!map_bounds(realx, realy))
                    continue;

                coord_def real(realx, realy);
                dungeon_feature_type dfeat = grid_appearance(gr, real);

                // if this cell is opaque...
                // ... or something you can see but not walk through ...
                if (grid_is_opaque(dfeat)
                    || clear_walls_block && dfeat < DNGN_MINMOVE)
                {
                    // then block the appropriate rays
                    for (unsigned int i = 0; i < num_words; ++i)
                        dead_rays[i] |= inptr[i];
                }
                else if (!ignore_clouds
                         && is_opaque_cloud(env.cgrid[realx][realy]))
                {
                    // block rays which have already seen a cloud
                    for (unsigned int i = 0; i < num_words; ++i)
                    {
                        dead_rays[i]  |= (smoke_rays[i] & inptr[i]);
                        smoke_rays[i] |= inptr[i];
                    }
                }
            }

        // ray calculation done, now work out which cells in this
        // quadrant are visible
        unsigned int rayidx = 0;
        for (unsigned int wordloc = 0; wordloc < num_words; ++wordloc)
        {
            const unsigned long curword = dead_rays[wordloc];
            // Note: the last word may be incomplete
            for (unsigned int bitloc = 0; bitloc < LONGSIZE; ++bitloc)
            {
                // make the cells seen by this ray at this point visible
                if ( ((curword >> bitloc) & 1UL) == 0 )
                {
                    // this ray is alive!
                    const int realx = xmult * compressed_ray_x[rayidx];
                    const int realy = ymult * compressed_ray_y[rayidx];
                    // update shadow map
                    if (x_p + realx >= 0 && x_p + realx < GXM
                        && y_p + realy >= 0 && y_p + realy < GYM
                        && realx * realx + realy * realy <= _los_radius_squared)
                    {
                        sh[sh_xo+realx][sh_yo+realy] = gr[x_p+realx][y_p+realy];
                    }
                }
                ++rayidx;
                if (rayidx == num_cellrays)
                    break;
            }
        }
    }

    // [dshaligram] The player's current position is always visible.
    sh[sh_xo][sh_yo] = gr[x_p][y_p];

    *dead_rays     = NULL;
    *smoke_rays    = NULL;
    *los_blockrays = NULL;
}

void calc_show_los()
{
    if (!crawl_state.arena && !crawl_state.arena_suspended)
    {
        // Must be done first.
        losight(env.show, grd, you.pos());

        // What would be visible, if all of the translucent walls were
        // made opaque.
        losight(env.no_trans_show, grd, you.pos(), true);
    }
    else
    {
        losight(env.show, grd, crawl_view.glosc());
    }
}

bool see_grid( const env_show_grid &show,
               const coord_def &c,
               const coord_def &pos )
{
    if (c == pos)
        return (true);

    const coord_def ip = pos - c;
    if (ip.rdist() < ENV_SHOW_OFFSET)
    {
        const coord_def sp(ip + coord_def(ENV_SHOW_OFFSET, ENV_SHOW_OFFSET));
        if (show(sp))
            return (true);
    }
    return (false);
}

// Answers the question: "Is a grid within character's line of sight?"
bool see_grid( const coord_def &p )
{
    return ((crawl_state.arena || crawl_state.arena_suspended)
                && crawl_view.in_grid_los(p))
            || see_grid(env.show, you.pos(), p);
}

// Answers the question: "Would a grid be within character's line of sight,
// even if all translucent/clear walls were made opaque?"
bool see_grid_no_trans( const coord_def &p )
{
    return see_grid(env.no_trans_show, you.pos(), p);
}

// Is the grid visible, but a translucent wall is in the way?
bool trans_wall_blocking( const coord_def &p )
{
    return see_grid(p) && !see_grid_no_trans(p);
}

