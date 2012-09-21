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

#include "mpr.h"

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

ProceduralSample
DiamondLayout::operator()(const coord_def &p, const uint32_t offset) const
{             
    
    uint8_t halfCell = w + s;
    uint8_t cellSize = halfCell * 2;
    uint8_t x = abs(abs(p.x) % cellSize - halfCell);
    uint8_t y = abs(abs(p.y) % cellSize - halfCell);
    if (x+y < w)
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
    worley::noise_datum n = worley::noise(x, y, z + seed);

    const uint32_t changepoint = offset + _get_changepoint(n, scale);
    ProceduralSample sample = (*layouts[n.id[0] % layouts.size()])(p, offset);

    return ProceduralSample(p, sample.feat(), 
                min(changepoint, sample.changepoint()));
}

ProceduralSample
ChaosLayout::operator()(const coord_def &p, const uint32_t offset) const
{
   uint64_t base = hash3(p.x, p.y, seed);
   uint32_t density = baseDensity + seed % 50 + (seed >> 16) % 60;
    if ((base % 1000) > density)
        return ProceduralSample(p, _pick_pseudorandom_feature(base/3), offset + 4096);
    return ProceduralSample(p, DNGN_FLOOR, offset + 4096);
}

ProceduralSample
RoilingChaosLayout::operator()(const coord_def &p, const uint32_t offset) const
{   
    const double scale = (density - 350) + 800;
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
    const double scale = 1000;
    const double scalar = 90.0;
    double x = (p.x + perlin::fBM(p.x/4.0, p.y/4.0, seed, 5) * 3) / scalar;
    double y = (p.y + perlin::fBM(p.x/4.0 + 3.7, p.y/4.0 + 1.9, seed + 4, 5) * 3) / scalar;
    worley::noise_datum n = worley::noise(x, y, offset / scale + seed);
    const uint32_t changepoint = offset + _get_changepoint(n, scale);
    if ((n.id[0] ^ n.id[1] ^ seed) % 4) {
        return layout(p, offset);
    }
    double delta = n.distance[1] - n.distance[0];
    if (delta < 1.25/scalar)
    {
        dungeon_feature_type feat = DNGN_SHALLOW_WATER;
        uint64_t hash = hash3(p.x, p.y, n.id[0] + seed);
        if (!(hash % 5))
            feat = DNGN_DEEP_WATER;
        if (!(hash % 23))
            feat = DNGN_MANGROVE;
        return ProceduralSample(p, feat, changepoint);
    }
    return layout(p, offset);
}
