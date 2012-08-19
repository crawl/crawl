#include "AppHdr.h"
#include "dgn-circles.h"

#include <math.h>
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "libutil.h"
#include "random.h"

void layout_circles(int nc)
{
    grd.init(DNGN_ROCK_WALL);

    while (nc > 0)
    {
        coord_def center = random_in_bounds();
        if (grd(center) != DNGN_ROCK_WALL)
            continue;

        int r = INT_MAX;
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            int dr = sqr(ri->x - center.x) + sqr(ri->y - center.y);
            if (r > dr && grd(*ri) != DNGN_ROCK_WALL)
                r = dr;
        }
        if (r == INT_MAX)
            r = 10 + random2(20);

        int walldist = std::min(std::min(center.x, GXM - 1 - center.x),
                                std::min(center.y, GYM - 1 - center.y));
        if (r > walldist * walldist)
            continue;

        if (r < 20 || r > 20 + random2(40))
            continue;

        for (rectangle_iterator ri(0); ri; ++ri)
        {
            if (sqr(ri->x - center.x) + sqr(ri->y - center.y) < r)
                grd(*ri) = DNGN_FLOOR;
        }

        nc--;
    }
}
