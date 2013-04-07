#include "diffusion.h"

void DiffusionGrid::addPoint(float f, coord_def c)
{
    DiffusionElement de = score(f, c);
    diffeq.push(de);
    arr(c) = de.f;
}

FixedArray< float, GXM, GYM> DiffusionGrid::process()
{
    while (!diffeq.empty()) {
        DiffusionElement e = diffeq.top();
        for (adjacent_iterator ai(e.c); ai; ++ai)
        {
            if (arr(e.c) == DBL_MIN) {
                addPoint(e.f, e.c);
            }
        }
        diffeq.pop();
    }
    return arr;
}

void DiffusionGrid::clear()
{
    arr = FixedArray< float, GXM, GYM >(DBL_MIN);
}
