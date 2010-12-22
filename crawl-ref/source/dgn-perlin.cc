#include "AppHdr.h"
#include "env.h"
#include "coord.h"
#include "coordit.h"
#include "perlin.h"
#include "random.h"
#include "dgn-perlin.h"

void layout_perlin()
{
    int z = random2(INT_MAX); // no real time coord for now
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        double h = perlin(ri->x, ri->y, z);
        if (h < -0.1)
            grd(*ri) = DNGN_DEEP_WATER;
        else if (h < 0.1)
            grd(*ri) = DNGN_SHALLOW_WATER;
        else
            grd(*ri) = DNGN_FLOOR;
    }
}
