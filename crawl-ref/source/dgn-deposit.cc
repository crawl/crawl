#include "AppHdr.h"
#include "dgn-deposit.h"

#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "libutil.h"
#include "random.h"

#define GRAVITY_MULT (80 * 80 + 70 * 70)

#define dprintf(...)

static void _add_gravity(FixedArray<int, GXM, GYM> &gravity, coord_def c)
{
    for (int x = 1; x < GXM - 1; x++)
        for (int y = 1; y < GYM - 1; y++)
        {
            int dist = sqr(c.x - x) + sqr(c.y - y);
            // cumulative (normal):
            gravity[x][y] += GRAVITY_MULT / std::max(1, dist);
            // nearest:
            //gravity[x][y] = std::max(gravity[x][y], GRAVITY_MULT / (dist + 1));
        }
}

static bool avail(int x, int y)
{
    return grd[x][y] == DNGN_ROCK_WALL;
}

void layout_deposit(int deposit_chance)
{
    ASSERT(deposit_chance > 0);

    grd.init(DNGN_ROCK_WALL);

    FixedArray<int, GXM, GYM> gravity(0);

    coord_def seed(39, 34);
    grd(seed) = DNGN_STONE_ARCH;
    _add_gravity(gravity, seed);

    std::vector<coord_def> border;
    // TODO: other shapes
    for (int x = 1; x < GXM - 1; x++)
    {
        border.push_back(coord_def(x, 1));
        border.push_back(coord_def(x, GYM - 2));
    }
    for (int y = 2; y < GYM - 2; y++)
    {
        border.push_back(coord_def(1, y));
        border.push_back(coord_def(GXM - 2, y));
    }

    int part = 2000;
    while (part-- > 0)
    {
        coord_def c;

        while (1)
        {
            if (border.empty())
                return;
            int i = random2(border.size());
            c = border[i];
            if (!avail(c.x, c.y))
            {
                erase_any(border, i);
                continue;
            }
            break;
        }
        dprintf("Starting at (%d,%d)\n", c.x, c.y);

        while (1)
        {
            int neigh[8];
            int sum = 0;
            for (int n = 0; n < 8; n++)
            {
                int v = gravity[c.x + Compass[n].x][c.y + Compass[n].y];
                if (!(n & 1))
                    v *= 2;
                neigh[n] = v;
                sum += v;
            }
            ASSERT(sum > 0);

            coord_def n;
            while (1)
            {
                int r = random2(sum);
                int i;
                for (i = 0; i < 8; i++)
                    if ((r -= neigh[i]) < 0)
                        break;
                n = c + Compass[i];
                if (avail(n.x, n.y))
                    break;
                if (x_chance_in_y(deposit_chance, 100))
                {
                    grd(c) = DNGN_FLOOR;
                    goto end_part;
                }
                // otherwise, keep sliding along the surface
            }

            c = n;
            dprintf(" (%d,%d) g=%d\n", c.x, c.y, gravity(c));
        }
    end_part:
        dprintf("Deposited at (%d,%d)\n", c.x, c.y);
        ;
    }
}
