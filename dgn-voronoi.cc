#include "AppHdr.h"
#include "env.h"
#include "coord.h"
#include "coordit.h"
#include "dgn-voronoi.h"
#include "random.h"
#include <stack>

void layout_voronoi(int nbeacons, bool uncompact)
{
    ASSERT(nbeacons >= 2);
    std::vector<coord_def> beacons;
    std::vector<int> powers;
    FixedArray<int, GXM, GYM> rmap;
    std::vector<std::map<int, bool> > adj;
    std::deque<coord_def> q;

    // Part 1: split the map into Voronoi regions.

    beacons.resize(nbeacons + 1);
    powers.resize(nbeacons + 1);
    adj.resize(nbeacons + 1);

    // a fake beacon far far away
    beacons[nbeacons] = coord_def(1000, 1000);
    powers[nbeacons] = 1000;
    rmap.init(nbeacons);

    for (int i=0; i < nbeacons; i++)
    {
        do
            beacons[i] = random_in_bounds();
        while (rmap(beacons[i]) != nbeacons);
        powers[i] = random2(10);
        rmap(beacons[i]) = i;
        q.push_back(beacons[i]);
    }

    while (!q.empty())
    {
        const coord_def s = q.front();
        q.pop_front();

        int bs = rmap(s);

        for (adjacent_iterator ai(s); ai; ai++)
        {
            const coord_def t = *ai;
            int bt = rmap(t);

            if ((t - beacons[bt]).abs() + powers[bt]
              > (t - beacons[bs]).abs() + powers[bs])
            {
                rmap(t) = bs;
                q.push_back(t);
            }
            else
            {
                // mark the regions as adjacent
                adj[bs][bt] = true;
                adj[bt][bs] = true;
            }
        }
    }


    // Part 2: randomly walk, marking regions as floor.

    std::vector<int> mark;
    mark.resize(nbeacons, 0);
    mark[0] = true;
    int marked = 1;

    while (marked < nbeacons / 2)
    {
        int bs = random2(nbeacons);
        if (mark[bs])
            continue;

        std::stack<int> todo, path;
        while(1)
        {
            path.push(bs);
            todo.push(bs);
            mark[bs] = 2;

        retry:
            int bt = -1;
            int count = 0;
            int cpath = 0;

            for (std::map<int,bool>::const_iterator neigh = adj[bs].begin();
                 neigh != adj[bs].end(); neigh++)
            {
                int bn = neigh->first;
                if (mark[bn] == 1)
                {
                    goto unwind;
                    break;
                }
                if (mark[bn] == 2)
                {
                    cpath++;
                    continue;
                }
                if (one_chance_in(++count))
                    bt = bn;
            }

            if (bt == -1 || (uncompact && cpath > 1))
            {
                path.pop();
                if (uncompact)
                {
                    todo.pop();
                    if (todo.empty())
                        goto unwind;
                }
                ASSERT(!path.empty());
                bs = path.top();
                goto retry;
            }

            bs = bt;
        }

    unwind:
        while (!todo.empty())
        {
            if (mark[todo.top()] != 1)
                mark[todo.top()] = 1, marked++;
            todo.pop();
        }

        for (int i = 0; i < nbeacons; i++)
            if (mark[i] == 2)
            {
                ASSERT(uncompact);
                mark[i] = 0;
            }
    }

    for (rectangle_iterator ri(1); ri; ri++)
        grd(*ri) = mark[rmap(*ri)] ? DNGN_FLOOR : DNGN_ROCK_WALL;

//    for (int i=1; i < nbeacons; i++)
//        grd(beacons[i]) = mark[i] ? DNGN_STONE_ARCH : DNGN_STONE_WALL;
}

void smooth_map()
{
    FixedArray<bool, GXM, GYM> todo;
    todo.init(false);

    for (rectangle_iterator ri(1); ri; ri++)
    {
        int ngb = 0;

        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
                if (grd[ri->x + dx][ri->y + dy] == DNGN_FLOOR)
                    ngb++;
        if (ngb > 4)
            todo(*ri) = true;
    }

    for (rectangle_iterator ri(1); ri; ri++)
        if (todo(*ri))
            grd(*ri) = DNGN_FLOOR;
}
