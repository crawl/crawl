#include "diffusion.h"

void DiffusionGrid::addPoint(int f, coord_def c)
{
    DiffusionElement de = score(f, c);
    diffeq.push(de);
    ASSERT(de.f != INT_MAX);
    arr(de.c) = de.f;
}

void DiffusionGrid::process()
{
    initialize();
    while (!diffeq.empty()) {
        DiffusionElement e = diffeq.top();
        diffeq.pop();
        for (adjacent_iterator ai(e.c); ai; ++ai)
        {
            if (arr(*ai) == INT_MAX) {
                addPoint(e.f, *ai);
            }
        }
    }
}

void DiffusionGrid::clear()
{
    arr = FixedArray< int, GXM, GYM >(INT_MAX);
}

coord_def DiffusionGrid::climb(coord_def c)
{
    if (!processed)
        process();
    int m = INT_MAX;
    coord_def t = c;
    for (adjacent_iterator ai(c); ai; ++ai)
    {
        if (arr(*ai) < m)
        {
            m = arr(*ai);
            c = *ai;
        }
    }
    return c;
}

void PlayerGrid::initialize()
{
    addPoint(0, you.pos());
}

DiffusionElement PlayerGrid::score(int f, coord_def c)
{
    if (feat_has_dry_floor(grd(c)))
    {
        return DiffusionElement(f + 1, c);
    }
    if (mgrd(c) != NON_MONSTER)
    {
        return DiffusionElement(f + 10, c);
    }
    return DiffusionElement(f + 1000, c);
}
