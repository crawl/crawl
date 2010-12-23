#include "AppHdr.h"
#include "env.h"
#include "coord.h"
#include "coordit.h"
#include "perlin.h"
#include "random.h"
#include "dgn-nolithius.h"

typedef FixedArray<int, GXM, GYM> heightmap_t;

static int _edge_dist(coord_def c)
{
    // Assumes that the map = screen.
    // If that changes, please update this function.
    int d = INT_MAX;
    d = std::min(d, c.x);
    d = std::min(d, c.y);
    d = std::min(d, GXM - 1 - c.x);
    d = std::min(d, GYM - 1 - c.y);
    return d;
}

static void _roiling_part(heightmap_t& rmap)
{
    rmap.init(0);

    for (int npart = 0; npart < 6000; npart++)
    {
        #define EDGE 12
        coord_def p(random_range(EDGE, GXM - 1 - EDGE),
                    random_range(EDGE, GYM - 1 - EDGE));

        for (int life = 50; life > 0; life--)
        {
            coord_def p1;
            rmap(p)++;

            uint8_t neighs = 0;
            while (neighs != 255)
            {
                p1 = p;
                int dir;
                do dir = random2(8); while (neighs & (1 << dir));
                neighs |= 1 << dir;
                p1.step(dir);

                // don't go out uphill
                if (!in_bounds(p1) || rmap(p1) > rmap(p))
                    continue;

                // we might stay put, but only if we're in a pit
                p = p1;
                break;
            }
        }
    }
    // note: max is almost always 85±1 for these parameters

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        switch (_edge_dist(*ri))
        {
        case 1:
            rmap(*ri) = rmap(*ri) * 3 / 4;
            break;
        case 2:
            rmap(*ri) = rmap(*ri) * 7 / 8;
            break;
        }
    }
}

#define MAX_HEIGHT 100

void layout_nolithius()
{
    heightmap_t height;
    _roiling_part(height);
    int buckets[MAX_HEIGHT + 1];
    for (int i = 0; i <= MAX_HEIGHT; i++)
        buckets[i] = 0;
    int area = 0;

    int z = random2(INT_MAX); // no real time coord for now
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        double H = (1 + perlin(ri->x, ri->y, z)) * height(*ri) / 85 / 2;
        int h = H * MAX_HEIGHT; // from -1..1 to 0..100 ints
        if (h < 0)
            h = 0;
        else if (h > MAX_HEIGHT)
            h = MAX_HEIGHT;
        height(*ri) = h;
        buckets[h]++;
        area++; // currently constant, might change later
    }

    int h0;
    int covered = 0;
    for (h0 = 0; h0 <= MAX_HEIGHT; h0++)
    {
        covered += buckets[h0];
        if (covered >= area * 6 / 10)
            break;
    }
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        int h = height(*ri);
        if (h < h0 - 6)
            grd(*ri) = DNGN_DEEP_WATER;
        else if (h < h0)
            grd(*ri) = DNGN_SHALLOW_WATER;
        else if (h > h0 + 6)
            grd(*ri) = DNGN_TREE;
        else
            grd(*ri) = DNGN_FLOOR;
    }
}
