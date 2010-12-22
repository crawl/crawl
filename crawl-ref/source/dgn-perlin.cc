#include "AppHdr.h"
#include "env.h"
#include "coord.h"
#include "coordit.h"
#include "perlin.h"
#include "random.h"
#include "dgn-perlin.h"

#define MAX_HEIGHT 100

void layout_perlin()
{
    int height[GXM][GYM];
    int buckets[MAX_HEIGHT + 1];
    for (int i = 0; i <= MAX_HEIGHT; i++)
        buckets[i] = 0;
    int area = 0;

    int z = random2(INT_MAX); // no real time coord for now
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        double H = perlin(ri->x, ri->y, z);
        int h = (H + 1) * MAX_HEIGHT / 2; // from -1..1 to 0..100 ints
        ASSERT(h >= 0 && h <= MAX_HEIGHT);
        height[ri->x][ri->y] = h;
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
    printf("Water level %d, covered = %d/%d\n", h0, covered, area);

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        int h = height[ri->x][ri->y];
        if (h < h0 - 16)
            grd(*ri) = DNGN_DEEP_WATER;
        else if (h < h0)
            grd(*ri) = DNGN_SHALLOW_WATER;
        else
            grd(*ri) = DNGN_FLOOR;
    }
}
