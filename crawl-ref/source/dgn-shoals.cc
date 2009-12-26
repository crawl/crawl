#include "AppHdr.h"

#include "branch.h"
#include "coord.h"
#include "dungeon.h"
#include "dgn-shoals.h"
#include "env.h"
#include "maps.h"
#include "random.h"

#include <algorithm>
#include <vector>
#include <cmath>

static int _shoals_heights[GYM][GXM];
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

static void _shoals_init_heights()
{
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
            _shoals_heights[y][x] = -17;
}

static coord_def _random_point_from(const coord_def &c, int radius)
{
    while (true) {
        const double angle = random2(360) * M_PI / 180;
        coord_def res = c + coord_def(radius * cos(angle), radius * sin(angle));
        if (res.x >= _shoals_margin && res.x < GXM - _shoals_margin
            && res.y >= _shoals_margin && res.y < GYM - _shoals_margin)
        {
            return res;
        }
    }
}

static coord_def _random_point(int offset = 0) {
    return coord_def(random_range(offset, GXM - offset - 1),
                     random_range(offset, GYM - offset - 1));
}

static void _shoals_island_center(const coord_def &c, int n_perturb, int radius,
                                  int bounce_low, int bounce_high)
{
    for (int i = 0; i < n_perturb; ++i) {
        coord_def p = _random_point_from(c, random2(1 + radius));
        _shoals_heights[p.y][p.x] += random_range(bounce_low, bounce_high);
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

static void _shoals_smooth_at(const coord_def &c, int radius)
{
    int max_delta = radius * radius * 2 + 2;
    int divisor = 0;
    int total = 0;
    for (int y = c.y - radius; y <= c.y + radius; ++y) {
        for (int x = c.x - radius; x <= c.x + radius; ++x) {
            const coord_def p(x, y);
            if (!in_bounds(p))
                continue;
            const coord_def off = c - p;
            int weight = max_delta - off.abs();
            divisor += weight;
            total += _shoals_heights[p.y][p.x] * weight;
        }
    }
    _shoals_heights[c.y][c.x] = total / divisor;
}

static void _shoals_smooth()
{
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
            _shoals_smooth_at(coord_def(x, y), 1);
}

static dungeon_feature_type _shoals_feature_at(int x, int y)
{
    const int height = _shoals_heights[y][x];
    return height >= 230 ? DNGN_STONE_WALL :
        height >= 170? DNGN_ROCK_WALL :
        height >= 0? DNGN_FLOOR :
        height >= -14? DNGN_SHALLOW_WATER : DNGN_DEEP_WATER;
}

static void _shoals_apply_level()
{
    for (int y = 1; y < GYM - 1; ++y)
        for (int x = 1; x < GXM - 1; ++x)
            grd[x][y] = _shoals_feature_at(x, y);
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

void prepare_shoals(int level_number)
{
    dgn_Build_Method += make_stringf(" shoals+ [%d]", level_number);
    dgn_Layout_Type   = "shoals";

    const int shoals_depth = level_id::current().depth - 1;
    dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_ROCK_WALL, DNGN_OPEN_SEA);
    _shoals_init_heights();
    _shoals_islands.clear();
    _shoals_init_islands(shoals_depth);
    _shoals_smooth();
    _shoals_apply_level();
    _shoals_furniture(6);
}
