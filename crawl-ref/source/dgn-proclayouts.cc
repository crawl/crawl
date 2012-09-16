/**
 * @file
 * @brief Procedurally generated dungeon layouts.
 **/  

#include <cmath>

#include "dgn-proclayouts.h"
#include "hash.h"
#include "perlin.h"
#include "terrain.h"
#include "worley.h"

#include "random.h"

bool less_dense_than(const dungeon_feature_type &a, const dungeon_feature_type &b)
{
    return true;
    // Is only one feature solid?
    if (feat_is_solid(a) ^ feat_is_solid(b))
        return feat_is_solid(b);
    if (feat_is_water(a) || feat_is_lava(a) || feat_is_statue_or_idol(a) && b == DNGN_FLOOR)
        return true;
    return false;
}

dungeon_feature_type _pick_pseudorandom_feature(uint64_t val)
{
    dungeon_feature_type features[] = {
        DNGN_STONE_WALL,
        DNGN_STONE_WALL,
        DNGN_STONE_WALL,
        DNGN_ROCK_WALL,
        DNGN_ROCK_WALL,
        DNGN_GREEN_CRYSTAL_WALL,
        DNGN_METAL_WALL
    };
    return features[val%7];
}

ProceduralSample
ColumnLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    int x = abs(p.x) % (_col_width + _col_space);
    int y = abs(p.y) % (_row_width + _row_space);
    if (x < _col_width && y < _row_width)
        return ProceduralSample(p, DNGN_ROCK_WALL, offset + 4096);
    return ProceduralSample(p, DNGN_FLOOR, offset + 4096); 
}
       
uint32_t _get_changepoint(const worley::noise_datum &n, const double scale)
{
    return max(1, (int) floor((n.distance[1] - n.distance[0]) * scale));
}

ProceduralSample _maybe_set_changepoint(const ProceduralSample &s,
    const uint32_t cp)
{
    return ProceduralSample(s.coord(), s.feat(), min(s.changepoint(), cp));
}

ProceduralSample
WorleyLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    const double scale = 500.0;
    double x = p.x / 5.0;
    double y = p.y / 5.0;
    double z = offset / scale;
    worley::noise_datum n = worley::noise(x, y, z);
    ProceduralSample sampleA = a(p, offset);
    ProceduralSample sampleB = b(p, offset);

    const uint32_t changepoint = offset + _get_changepoint(n, scale);

    if (n.id[0] % 2)
        return ProceduralSample(p, 
                sampleA.feat(), 
                min(changepoint, sampleA.changepoint()));
    return ProceduralSample(p,
            sampleB.feat(),
            min(changepoint, sampleB.changepoint()));
}

ProceduralSample
ChaosLayout::operator()(const coord_def &p, const uint32_t offset) const
{
   uint64_t base = hash3(p.x, p.y, seed);
   uint32_t density = 450 + seed % 50 + (seed >> 16) % 60;
    if ((base % 1000) > density)
        return ProceduralSample(p, _pick_pseudorandom_feature(base/3), offset + 4096);
    return ProceduralSample(p, DNGN_FLOOR, offset + 4096);

}

ProceduralSample
RoilingChaosLayout::operator()(const coord_def &p, const uint32_t offset) const
{   
    const double scale = 500.0;
    double x = p.x;
    double y = p.y;
    double z = offset / scale;
    worley::noise_datum n = worley::noise(x, y, z);
    const uint32_t changepoint = offset + _get_changepoint(n, scale);
    ProceduralSample sample = ChaosLayout(n.id[0] + seed)(p, offset);
    return ProceduralSample(p, sample.feat(), min(sample.changepoint(), changepoint));
}

ProceduralSample
RiverLayout::operator()(const coord_def &p, const uint32_t offset) const
{   
    const int periodicity = 100;
    const int baseWidth = 12;
    const double scale = 20000;
    worley::noise_datum n =
        worley::noise(p.x/100.0, p.y/1000.0, offset / scale + seed);
    if ((n.id[0] + n.id[1]) % 6 || p.x % periodicity > baseWidth * 2)
    {
        int cp = offset + _get_changepoint(n, scale);
        ProceduralSample sample = layout(p, offset);
        return _maybe_set_changepoint(sample, cp);
    }
    int xi = p.x + perlin::noise(p.x/4.0, p.y/4.0, offset / scale) * 6;
    int yi = p.y + perlin::noise(p.x/4.0 + 31., p.y/4.0 + 17., offset / scale) * 10;
    int x = xi + sin(yi / 6.0) * 7;
    int width = baseWidth + perlin::noise(p.x/5.0, p.y/5.0, seed) * 6;
    if (x % periodicity < width)
    {
        dungeon_feature_type feat = DNGN_SHALLOW_WATER;
        if (width > baseWidth && (x + baseWidth/2) % periodicity > width/2)
            feat = DNGN_DEEP_WATER;
        return ProceduralSample(p, feat, offset + 1);
    }
    if ((x + 4) % periodicity < width + 8)
    {
        dungeon_feature_type feat = DNGN_FLOOR;
        if (!(hash3(xi, yi, seed) % 23))
            feat = DNGN_MANGROVE;
        return ProceduralSample(p, feat, offset + 1);
    }
    return layout(p, offset);
}
