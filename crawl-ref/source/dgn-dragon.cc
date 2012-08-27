#include "AppHdr.h"
#include "env.h"
#include "coord.h"
#include "coordit.h"
#include "dgn-dragon.h"

void layout_dragon(int mask)
{
    ASSERT(mask > 0);
    mask <<= 1;

    coord_def c(40, 35);

    int d = 0;

    // For mask of 1 (ie, the actual dragon curve), it comes within Chebyshev
    // distance of 42 for the last time in 6553 iterations, 43 in 24030, 84 in
    // 26208, 85 in 96118.  We care about such distance of 69 at most (as the
    // map is rectangular) -- if the origin is in a corner.  The fractal itself
    // operates in Minkowski's metric (and produces Euclidean-like shapes for
    // most masks), yet we care about the region to fill.
    // Some masks may take longer to leave the visible area, but the shape
    // produced looks about as good with this cut-off.
    for (int n = 1; n <= 26208; n++)
    {
        int m = n;
        while (!(m & 1))
            m >>= 1;
        d += (m & mask) ? -1 : 1;
        c.step((d & 3) << 1);
        if (in_bounds(c))
            grd(c) = DNGN_FLOOR;
    }
}
