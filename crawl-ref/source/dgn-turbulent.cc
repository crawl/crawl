#include "AppHdr.h"
#include "env.h"
#include "coord.h"
#include "coordit.h"
#include "dgn-turbulent.h"
#include "perlin.h"

void layout_turbulent(int z)
{
    grd.init(DNGN_ROCK_WALL);
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        double x = ri->x / 5.0 - 7;
        double y = ri->y / 5.0 - 7;

        x += 1.7 * fBM(x, y, 5 * z, 5);
        y += 1.7 * fBM(x, y, 13 * z, 5);
        double r = x*x + y*y;
        if (r < 5 || r > 16)
            continue;
        grd(*ri) = DNGN_FLOOR;
    }
}
