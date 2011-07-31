#include "AppHdr.h"
#include "dgn-forest.h"

#include <math.h>
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "random.h"

static void render_fuzzy_line(coord_def a, coord_def b)
{
    ASSERT(in_bounds(a));
    ASSERT(in_bounds(b));
    grd(a) = DNGN_FLOOR;
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    int sx = sgn(dx);
    int sy = sgn(dy);
    dx = abs(dx);
    dy = abs(dy);
    while (dx || dy)
    {
        if (x_chance_in_y(std::min(dx, dy), (dx + dy) * 2 / 3))
        {
            a.x += sx, dx--;
            a.y += sy, dy--;
        }
        else if (x_chance_in_y(dx, dx + dy))
            a.x += sx, dx--;
        else
            a.y += sy, dy--;
        grd(a) = DNGN_FLOOR;
    }
}

void layout_forest1()
{
    grd.init(DNGN_ROCK_WALL);
    for (rectangle_iterator ri(1); ri; ++ri)
        grd(*ri) = DNGN_TREE;
    coord_def a = random_in_bounds();
    for (int i = 0; i < 20; i++)
    {
        coord_def b = random_in_bounds();
        render_fuzzy_line(a, b);
        a = b;
    }
}

double random_real()
{
    return random_int() / 4294967296.0;
}

#define INITIAL_SKEW 0.4
#define MEANDER 0.8
#define RETURN  0.1

static void forest_path(std::vector<coord_def> &pool, coord_def a,
                        double major_dir, double minor_dir)
{
    double x = a.x;
    double y = a.y;
    do
    {
        if (grd(a) != DNGN_FLOOR)
        {
            grd(a) = DNGN_FLOOR;
            pool.push_back(a);
        }
        coord_def b;
        do
        {
            if (minor_dir >= 2 * PI)
                minor_dir -= 2 * PI;
            else if (minor_dir < 0)
                minor_dir += 2 * PI;
            double skew = major_dir - minor_dir;
            if (skew > PI)
                skew -= 2 * PI;
            else if (skew < -PI)
                skew += 2 * PI;
            minor_dir += random_real() * MEANDER * 2 - MEANDER
                       + skew * random_real() * RETURN;
            x += cos(minor_dir);
            y += sin(minor_dir);
            b.x = round(x);
            b.y = round(y);
        } while (b == a);
        a = b;
    } while (grd(a) == DNGN_TREE);
}

void layout_forest()
{
    grd.init(DNGN_ROCK_WALL);
    for (rectangle_iterator ri(1); ri; ++ri)
        grd(*ri) = DNGN_TREE;

    std::vector<coord_def> pool;
    coord_def a = random_in_bounds();
    grd(a) = DNGN_FLOOR;
    pool.push_back(a);
    while (pool.size() < 666)
    {
        a = pool[random2(pool.size())];
        // a = random_in_bounds();
        double major_dir = random_real() * 2 * PI;
        double minor_dir = major_dir + random_real() * INITIAL_SKEW * 2 - INITIAL_SKEW;
        forest_path(pool, a, major_dir, minor_dir);
    }
}
