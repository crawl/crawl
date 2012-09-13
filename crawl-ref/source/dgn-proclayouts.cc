/**
 * @file
 * @brief Procedurally generated dungeon layouts.
 **/  

#include <cmath>
#include "dgn-proclayouts.h"
#include "cellular.h"

ProceduralSample
ColumnLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    int x = abs(p.x) % (_col_width + _col_space);
    int y = abs(p.y) % (_row_width + _row_space);
    if (x < _col_width && y < _row_width)
        return ProceduralSample(p, DNGN_ROCK_WALL, -1);
    return ProceduralSample(p, DNGN_FLOOR, -1); 
}

ProceduralSample
WorleyLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    const double scale = 100.0;
    double x = p.x / 5.0;
    double y = p.y / 5.0;
    double z = offset / scale;
    worley::noise_datum n = worley::noise(x, y, z);
    ProceduralSample sampleA = a(p, offset);
    ProceduralSample sampleB = b(p, offset);

    const uint32_t changepoint = offset +
        max(1, (int) floor((n.distance[1] - n.distance[0]) * scale));
    if (n.id[0] % 2)
        return ProceduralSample(p, 
                sampleA.feat(), 
                min(changepoint, sampleA.changepoint()));
    return ProceduralSample(p,
            sampleB.feat(),
            min(changepoint, sampleB.changepoint()));
}
