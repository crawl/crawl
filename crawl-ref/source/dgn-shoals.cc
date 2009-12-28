#include "AppHdr.h"

#include "branch.h"
#include "coord.h"
#include "coordit.h"
#include "dungeon.h"
#include "dgn-shoals.h"
#include "env.h"
#include "flood_find.h"
#include "items.h"
#include "maps.h"
#include "mon-place.h"
#include "mon-util.h"
#include "random.h"
#include "terrain.h"

#include <algorithm>
#include <vector>
#include <cmath>

const char *ENVP_SHOALS_TIDE_KEY = "shoals-tide-height";
const char *ENVP_SHOALS_TIDE_VEL = "shoals-tide-velocity";

inline short &shoals_heights(const coord_def &c)
{
    return (*env.heightmap)(c);
}

static std::vector<coord_def> _shoals_islands;

const int ISLAND_COLLIDE_DIST2 = 5 * 5;
const int N_PERTURB_ISLAND_CENTER = 50;
const int ISLAND_CENTER_RADIUS_LOW = 3;
const int ISLAND_CENTER_RADIUS_HIGH = 10;

const int N_PERTURB_OFFSET_LOW  = 25;
const int N_PERTURB_OFFSET_HIGH = 45;
const int PERTURB_OFFSET_RADIUS_LOW = 2;
const int PERTURB_OFFSET_RADIUS_HIGH = 7;

const int _shoals_margin = 6;

enum shoals_height_thresholds
{
    SHT_UNDEFINED = -10000,
    SHT_STONE = 230,
    SHT_ROCK  = 170,
    SHT_FLOOR = 0,
    SHT_SHALLOW_WATER = -14
};

enum tide_direction
{
    TIDE_RISING,
    TIDE_FALLING
};

static tide_direction _shoals_tide_direction;

static double _to_radians(int degrees)
{
    return degrees * M_PI / 180;
}

static dungeon_feature_type _shoals_feature_by_height(int height)
{
    return height >= SHT_STONE ? DNGN_STONE_WALL :
        height >= SHT_ROCK? DNGN_ROCK_WALL :
        height >= SHT_FLOOR? DNGN_FLOOR :
        height >= SHT_SHALLOW_WATER? DNGN_SHALLOW_WATER
        : DNGN_DEEP_WATER;
}

static dungeon_feature_type _shoals_feature_at(const coord_def &c)
{
    const int height = shoals_heights(c);
    return _shoals_feature_by_height(height);
}

static int _shoals_feature_height(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_FLOOR:
        return SHT_FLOOR;
    case DNGN_SHALLOW_WATER:
        return SHT_SHALLOW_WATER;
    case DNGN_DEEP_WATER:
        return SHT_SHALLOW_WATER - 1;
    default:
        return 0;
    }
}

// Returns true if the given feature can be affected by Shoals tides.
static bool _shoals_tide_susceptible_feat(dungeon_feature_type feat)
{
    return (feat_is_water(feat) || feat == DNGN_FLOOR);
}

// Return true if tide effects can propagate through this square.
// NOTE: uses RNG!
static bool _shoals_tide_passable_feat(dungeon_feature_type feat)
{
    return (feat_is_watery(feat)
            // The Shoals tide can sometimes lap past the doorways of rooms
            // near the water. Note that the actual probability of the tide
            // getting through a doorway is this probability * 0.5 --
            // see _shoals_apply_tide.
            || (feat == DNGN_OPEN_DOOR && !one_chance_in(3))
            || (feat == DNGN_CLOSED_DOOR && one_chance_in(3)));
}

static void _shoals_init_heights()
{
    env.heightmap.reset(new grid_heightmap);
    for (rectangle_iterator ri(0); ri; ++ri)
        shoals_heights(*ri) = SHT_SHALLOW_WATER - 3;
}

static double _angle_fuzz()
{
    double fuzz = _to_radians(random2(15));
    return coinflip()? fuzz : -fuzz;
}

static coord_def _random_point_from(const coord_def &c, int radius,
                                    int directed_angle = -1)
{
    const double directed_radians(
        directed_angle == -1? 0.0 : _to_radians(directed_angle));
    int attempts = 70;
    while (attempts-- > 0)
    {
        const double angle =
            directed_angle == -1? _to_radians(random2(360))
            : ((coinflip()? directed_radians : -directed_radians)
               + _angle_fuzz());
        coord_def res = c + coord_def(radius * cos(angle),
                                      radius * sin(angle));
        if (res.x >= _shoals_margin && res.x < GXM - _shoals_margin
            && res.y >= _shoals_margin && res.y < GYM - _shoals_margin)
        {
            return res;
        }
    }
    return coord_def();
}

static coord_def _random_point(int offset = 0)
{
    return coord_def(random_range(offset, GXM - offset - 1),
                     random_range(offset, GYM - offset - 1));
}

static void _shoals_island_center(const coord_def &c, int n_perturb, int radius,
                                  int bounce_low, int bounce_high)
{
    for (int i = 0; i < n_perturb; ++i) {
        coord_def p = _random_point_from(c, random2(1 + radius));
        if (!p.origin())
            shoals_heights(p) += random_range(bounce_low, bounce_high);
    }
}

static coord_def _shoals_pick_island_spot()
{
    coord_def c;
    for (int i = 0; i < 15; ++i)
    {
        c = _random_point(_shoals_margin * 2);

        bool collides = false;
        for (int j = 0, size = _shoals_islands.size(); j < size; ++j)
        {
            const coord_def island = _shoals_islands[j];
            const coord_def dist = island - c;
            if (dist.abs() < ISLAND_COLLIDE_DIST2)
            {
                collides = true;
                break;
            }
        }
        if (!collides)
            break;
    }
    _shoals_islands.push_back(c);
    return c;
}

static void _shoals_build_island()
{
    coord_def c = _shoals_pick_island_spot();
    _shoals_island_center(c, N_PERTURB_ISLAND_CENTER,
                          random_range(ISLAND_CENTER_RADIUS_LOW,
                                       ISLAND_CENTER_RADIUS_HIGH),
                          40, 60);
    const int additional_heights = random2(4);
    for (int i = 0; i < additional_heights; ++i) {
        const int addition_offset = random_range(2, 10);

        coord_def offsetC = _random_point_from(c, addition_offset);
        if (!offsetC.origin())
            _shoals_island_center(offsetC, random_range(N_PERTURB_OFFSET_LOW,
                                                        N_PERTURB_OFFSET_HIGH),
                                  random_range(PERTURB_OFFSET_RADIUS_LOW,
                                               PERTURB_OFFSET_RADIUS_HIGH),
                                  25, 35);
    }
}

static void _shoals_init_islands(int depth)
{
    const int nislands = 20 - depth * 2;
    for (int i = 0; i < nislands; ++i)
        _shoals_build_island();
}

// Cliffs are usually constructed in shallow water adjacent to deep
// water (for effect).
static void _shoals_build_cliff()
{
    coord_def cliffc = _random_point(_shoals_margin * 2);
    if (in_bounds(cliffc))
    {
        const int length = random_range(6, 15);
        double angle = _to_radians(random2(360));
        for (int i = 0; i < length; i += 3)
        {
            int distance = i - length / 2;
            coord_def place = cliffc + coord_def(distance * cos(angle),
                                                 distance * sin(angle));
            coord_def fuzz = coord_def(random_range(-2, 2),
                                       random_range(-2, 2));
            place += fuzz;
            _shoals_island_center(place, random_range(40, 60), 3,
                                  100, 130);
        }
    }
}

static void _shoals_cliffs()
{
    const int ncliffs = random_range(0, 6, 2);
    for (int i = 0; i < ncliffs; ++i)
        _shoals_build_cliff();
}

static void _shoals_smooth_at(const coord_def &c, int radius,
                              int max_height = SHT_UNDEFINED)
{
    const int height = shoals_heights(c);
    if (max_height != SHT_UNDEFINED && height > max_height)
        return;

    int max_delta = radius * radius * 2 + 2;
    int divisor = 0;
    int total = 0;
    for (int y = c.y - radius; y <= c.y + radius; ++y) {
        for (int x = c.x - radius; x <= c.x + radius; ++x) {
            const coord_def p(x, y);
            if (!in_bounds(p))
                continue;
            const int nheight = shoals_heights(p);
            if (max_height != SHT_UNDEFINED && nheight > max_height)
                continue;
            const coord_def off = c - p;
            int weight = max_delta - off.abs();
            divisor += weight;
            total += nheight * weight;
        }
    }
    shoals_heights(c) = total / divisor;
}

static void _shoals_smooth()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        _shoals_smooth_at(*ri, 1);
}

static void _shoals_smooth_water()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        _shoals_smooth_at(*ri, 1, SHT_SHALLOW_WATER - 1);
}

static void _shoals_apply_level()
{
    for (rectangle_iterator ri(1); ri; ++ri)
        grd(*ri) = _shoals_feature_at(*ri);
}

static bool _has_adjacent_feat(coord_def c, dungeon_feature_type feat)
{
    for (adjacent_iterator ai(c); ai; ++ai)
        if (grd(*ai) == feat)
            return true;
    return false;
}

// Returns all points in deep water with an adjacent square in shallow water.
static std::vector<coord_def> _shoals_water_depth_change_points()
{
    std::vector<coord_def> points;
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        coord_def c(*ri);
        if (grd(c) == DNGN_DEEP_WATER
            && _has_adjacent_feat(c, DNGN_SHALLOW_WATER))
            points.push_back(c);
    }
    return points;
}

static inline void _shoals_deepen_water_at(coord_def p, int distance)
{
    shoals_heights(p) -= distance * 7;
}

static void _shoals_deepen_water()
{
    std::vector<coord_def> pages[2];
    int current_page = 0;
    pages[current_page] = _shoals_water_depth_change_points();
    FixedArray<bool, GXM, GYM> seen_points(false);

    for (int i = 0, size = pages[current_page].size(); i < size; ++i)
        seen_points(pages[current_page][i]) = true;

    int distance = 0;
    while (!pages[current_page].empty())
    {
        const int next_page = !current_page;
        std::vector<coord_def> &cpage(pages[current_page]);
        std::vector<coord_def> &npage(pages[next_page]);
        for (int i = 0, size = cpage.size(); i < size; ++i)
        {
            coord_def c(cpage[i]);
            if (distance)
                _shoals_deepen_water_at(c, distance);

            for (adjacent_iterator ai(c); ai; ++ai)
            {
                coord_def adj(*ai);
                if (!seen_points(adj) && grd(adj) == DNGN_DEEP_WATER)
                {
                    npage.push_back(adj);
                    seen_points(adj) = true;
                }
            }
        }
        cpage.clear();
        current_page = next_page;
        distance++;
    }
}

static coord_def _pick_shoals_island()
{
    const int lucky_island = random2(_shoals_islands.size());
    const coord_def c = _shoals_islands[lucky_island];
    _shoals_islands.erase(_shoals_islands.begin() + lucky_island);
    return c;
}

struct point_sort_distance_from
{
    coord_def bad_place;
    point_sort_distance_from(coord_def c) : bad_place(c) { }
    bool operator () (coord_def a, coord_def b) const
    {
        const int dista = (a - bad_place).abs(), distb = (b - bad_place).abs();
        return dista >= distb;
    }
};

static coord_def _pick_shoals_island_distant_from(coord_def bad_place)
{
    ASSERT(!_shoals_islands.empty());

    std::sort(_shoals_islands.begin(), _shoals_islands.end(),
              point_sort_distance_from(bad_place));
    const int top_picks = std::min(4, int(_shoals_islands.size()));
    const int choice = random2(top_picks);
    coord_def chosen = _shoals_islands[choice];
    _shoals_islands.erase(_shoals_islands.begin() + choice);
    return chosen;
}

static void _shoals_furniture(int margin)
{
    if (at_branch_bottom())
    {
        const coord_def c = _pick_shoals_island();
        // Put all the stairs on one island.
        grd(c) = DNGN_STONE_STAIRS_UP_I;
        grd(c + coord_def(1, 0)) = DNGN_STONE_STAIRS_UP_II;
        grd(c - coord_def(1, 0)) = DNGN_STONE_STAIRS_UP_III;

        const coord_def p = _pick_shoals_island_distant_from(c);
        // Place the rune
        const map_def *vault = random_map_for_tag("shoal_rune");
        dgn_place_map(vault, false, true, p);

        const int nhuts = std::min(8, int(_shoals_islands.size()));
        for (int i = 2; i < nhuts; ++i)
        {
            // Place (non-rune) minivaults on the other islands
            do
                vault = random_map_for_tag("shoal");
            while (!vault);
            dgn_place_map(vault, false, true, _pick_shoals_island());
        }
    }
    else
    {
        // Place stairs randomly. No elevators.
        for (int i = 0; i < 3; ++i)
        {
            int x, y;
            do
            {
                x = margin + random2(GXM - 2*margin);
                y = margin + random2(GYM - 2*margin);
            }
            while (grd[x][y] != DNGN_FLOOR);

            grd[x][y]
              = static_cast<dungeon_feature_type>(DNGN_STONE_STAIRS_DOWN_I + i);

            do
            {
                x = margin + random2(GXM - 2*margin);
                y = margin + random2(GYM - 2*margin);
            }
            while (grd[x][y] != DNGN_FLOOR);

            grd[x][y]
                = static_cast<dungeon_feature_type>(DNGN_STONE_STAIRS_UP_I + i);
        }
    }
}

static void _shoals_deepen_edges()
{
    const int edge = 2;
    // Water of the edge of the screen is too deep to be exposed by tides.
    for (int y = 1; y < GYM - 2; ++y)
    {
        for (int x = 1; x <= edge; ++x)
        {
            shoals_heights(coord_def(x, y)) -= 800;
            shoals_heights(coord_def(GXM - 1 - x, y)) -= 800;
        }
    }
    for (int x = 1; x < GXM - 2; ++x)
    {
        for (int y = 1; y <= edge; ++y)
        {
            shoals_heights(coord_def(x, y)) -= 800;
            shoals_heights(coord_def(x, GYM - 1 - y)) -= 800;
        }
    }
}

void prepare_shoals(int level_number)
{
    dgn_Build_Method += make_stringf(" shoals+ [%d]", level_number);
    dgn_Layout_Type   = "shoals";

    const int shoals_depth = level_id::current().depth - 1;
    dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_ROCK_WALL, DNGN_OPEN_SEA);
    _shoals_init_heights();
    _shoals_islands.clear();
    _shoals_init_islands(shoals_depth);
    _shoals_cliffs();
    _shoals_smooth();
    _shoals_apply_level();
    _shoals_deepen_water();
    _shoals_deepen_edges();
    _shoals_smooth_water();
    _shoals_furniture(_shoals_margin);
}

// Search the map for vaults and set the terrain heights for features
// in the vault to reasonable levels.
void shoals_postprocess_level()
{
    if (!player_in_branch(BRANCH_SHOALS) || !env.heightmap.get())
        return;

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const coord_def c(*ri);
        if (!(dgn_Map_Mask(c) & MMT_VAULT))
            continue;

        const dungeon_feature_type feat(grd(c));
        if (!_shoals_tide_susceptible_feat(feat))
            continue;

        const dungeon_feature_type expected_feat(_shoals_feature_at(c));
        // It would be nice to do actual height contours within
        // vaults, but for now, keep it simple.
        if (feat != expected_feat)
            shoals_heights(c) = _shoals_feature_height(feat);
    }
}

// The raw tide height / TIDE_MULTIPLIER is the actual tide height. The higher
// the tide multiplier, the slower the tide advances and recedes. A multiplier
// of X implies that the tide will advance visibly about once in X turns.
const int TIDE_MULTIPLIER = 30;

const int LOW_TIDE = -18 * TIDE_MULTIPLIER;
const int HIGH_TIDE = 25 * TIDE_MULTIPLIER;
const int TIDE_DECEL_MARGIN = 8;
const int START_TIDE_RISE = 2;

static void _shoals_run_tide(int &tide, int &acc)
{
    tide += acc;
    tide = std::max(std::min(tide, HIGH_TIDE), LOW_TIDE);
    if ((tide == HIGH_TIDE && acc > 0)
        || (tide == LOW_TIDE && acc < 0))
        acc = -acc;
    bool in_decel_margin =
        (abs(tide - HIGH_TIDE) < TIDE_DECEL_MARGIN)
        || (abs(tide - LOW_TIDE) < TIDE_DECEL_MARGIN);
    if ((abs(acc) == 2) == in_decel_margin)
        acc = in_decel_margin? acc / 2 : acc * 2;
}

static coord_def _shoals_escape_place_from(coord_def bad_place,
                                           bool monster_free)
{
    int best_height = -1000;
    coord_def chosen;
    for (adjacent_iterator ai(bad_place); ai; ++ai)
    {
        coord_def p(*ai);
        const dungeon_feature_type feat(grd(p));
        if (!feat_is_solid(feat) && !feat_destroys_items(feat)
            && (!monster_free || !actor_at(p)))
        {
            if (best_height == -1000 || shoals_heights(p) > best_height)
            {
                best_height = shoals_heights(p);
                chosen = p;
            }
        }
    }
    return chosen;
}

static bool _shoals_tide_sweep_items_clear(coord_def c)
{
    int link = igrd(c);
    if (link == NON_ITEM)
        return true;

    const coord_def target(_shoals_escape_place_from(c, false));
    // Don't abort tide entry because of items. If we can't sweep the
    // item clear here, let dungeon_terrain_changed teleport the item
    // to the nearest safe square.
    if (!target.origin())
        move_item_stack_to_grid(c, target);
    return true;
}

static bool _shoals_tide_sweep_actors_clear(coord_def c)
{
    actor *victim = actor_at(c);
    if (!victim || victim->airborne() || victim->swimming())
        return true;

    if (victim->atype() == ACT_MONSTER)
    {
        monsters *mvictim = dynamic_cast<monsters *>(victim);
        // Plants and statues cannot be moved away; the tide cannot
        // drown them.
        if (mons_class_is_stationary(mvictim->type))
            return false;

        // If the monster doesn't need help, move along.
        if (monster_habitable_grid(mvictim, DNGN_DEEP_WATER))
            return true;
    }
    coord_def evacuation_point(_shoals_escape_place_from(c, true));
    // The tide moves on even if we cannot evacuate the tile!
    if (!evacuation_point.origin())
        victim->move_to_pos(evacuation_point);
    return true;
}

// The tide will attempt to push items and non-water-capable monsters to
// adjacent squares.
static bool _shoals_tide_sweep_clear(coord_def c)
{
    return _shoals_tide_sweep_items_clear(c)
        && _shoals_tide_sweep_actors_clear(c);
}

static void _shoals_apply_tide_feature_at(coord_def c,
                                          dungeon_feature_type feat)
{
    if (feat == DNGN_DEEP_WATER && !_shoals_tide_sweep_clear(c))
        feat = DNGN_SHALLOW_WATER;

    const dungeon_feature_type current_feat = grd(c);
    if (feat == current_feat)
        return;

    dungeon_terrain_changed(c, feat, true, false, true);
}

// Determines if the tide is rising or falling based on before and
// after features at the same square.
static tide_direction _shoals_feature_tide_height_change(
    dungeon_feature_type oldfeat,
    dungeon_feature_type newfeat)
{
    const int height_delta =
        _shoals_feature_height(newfeat) - _shoals_feature_height(oldfeat);
    // If the apparent height of the new feature is greater (floor vs water),
    // the tide is receding.
    return height_delta < 0? TIDE_RISING : TIDE_FALLING;
}

static void _shoals_apply_tide_at(coord_def c, int tide)
{
    const int effective_height = shoals_heights(c) - tide;
    dungeon_feature_type newfeat =
        _shoals_feature_by_height(effective_height);
    // Make sure we're not sprouting new walls.
    if (feat_is_wall(newfeat))
        newfeat = DNGN_FLOOR;
    const dungeon_feature_type oldfeat = grd(c);

    if (oldfeat == newfeat
        || (_shoals_feature_tide_height_change(oldfeat, newfeat) !=
            _shoals_tide_direction))
    {
        return;
    }

    _shoals_apply_tide_feature_at(c, newfeat);
}

static void _shoals_apply_tide(int tide)
{
    std::vector<coord_def> pages[2];
    int current_page = 0;

    // Start from corners of the map.
    pages[current_page].push_back(coord_def(1,1));
    pages[current_page].push_back(coord_def(GXM - 2, 1));
    pages[current_page].push_back(coord_def(1, GYM - 2));
    pages[current_page].push_back(coord_def(GXM - 2, GYM - 2));

    FixedArray<bool, GXM, GYM> seen_points(false);

    while (!pages[current_page].empty())
    {
        int next_page = !current_page;
        std::vector<coord_def> &cpage(pages[current_page]);
        std::vector<coord_def> &npage(pages[next_page]);

        for (int i = 0, size = cpage.size(); i < size; ++i)
        {
            coord_def c(cpage[i]);
            const bool was_wet(_shoals_tide_passable_feat(grd(c)));
            seen_points(c) = true;
            _shoals_apply_tide_at(c, tide);

            const bool is_wet(feat_is_water(grd(c)));
            // Only squares that were wet (before applying tide
            // effects!) can propagate the tide onwards. If the tide is
            // receding and just left the square dry, there's only a chance of
            // it continuing past and draining other squares through this one.
            if (was_wet && (is_wet || coinflip()))
            {
                for (adjacent_iterator ai(c); ai; ++ai)
                {
                    coord_def adj(*ai);
                    if (!in_bounds(adj))
                        continue;
                    if (!seen_points(adj))
                    {
                        const dungeon_feature_type feat = grd(adj);
                        if (_shoals_tide_susceptible_feat(feat))
                        {
                            npage.push_back(adj);
                            seen_points(adj) = true;
                        }
                    }
                }
            }
        }

        cpage.clear();
        current_page = next_page;
    }
}

static void _shoals_init_tide()
{
    if (!env.properties.exists(ENVP_SHOALS_TIDE_KEY))
    {
        env.properties[ENVP_SHOALS_TIDE_KEY] = short(0);
        env.properties[ENVP_SHOALS_TIDE_VEL] = short(2);
    }
}

void shoals_apply_tides(int turns_elapsed)
{
    if (!player_in_branch(BRANCH_SHOALS) || !turns_elapsed
        || !env.heightmap.get())
    {
        return;
    }

    const int TIDE_UNIT = HIGH_TIDE - LOW_TIDE;
    // If we've been gone a long time, eliminate some unnecessary math.
    if (turns_elapsed > TIDE_UNIT * 2)
        turns_elapsed = turns_elapsed % TIDE_UNIT + TIDE_UNIT;

    _shoals_init_tide();
    int tide = env.properties[ENVP_SHOALS_TIDE_KEY].get_short();
    int acc = env.properties[ENVP_SHOALS_TIDE_VEL].get_short();
    const int old_tide = tide;
    while (turns_elapsed-- > 0)
        _shoals_run_tide(tide, acc);
    env.properties[ENVP_SHOALS_TIDE_KEY] = short(tide);
    env.properties[ENVP_SHOALS_TIDE_VEL] = short(acc);
    if (old_tide / TIDE_MULTIPLIER != tide / TIDE_MULTIPLIER)
    {
        _shoals_tide_direction =
            tide > old_tide ? TIDE_RISING : TIDE_FALLING;
        _shoals_apply_tide(tide / TIDE_MULTIPLIER);
    }
}
