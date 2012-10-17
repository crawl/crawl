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

dungeon_feature_type _pick_pseudorandom_wall(uint64_t val)
{
    static dungeon_feature_type features[] = {
        DNGN_STONE_WALL,
        DNGN_STONE_WALL,
        DNGN_STONE_WALL,
        DNGN_STONE_WALL,
        DNGN_ROCK_WALL,
        DNGN_ROCK_WALL,
        DNGN_ROCK_WALL,
        DNGN_GREEN_CRYSTAL_WALL,
        DNGN_METAL_WALL
    };
    return features[val%9];
}

dungeon_feature_type _pick_pseudorandom_feature(uint64_t val)
{
    if (!(val%5))
        return _pick_pseudorandom_wall(val/5);
    dungeon_feature_type features[] = {
        DNGN_STONE_WALL,
        DNGN_STONE_WALL,
        DNGN_ROCK_WALL,
        DNGN_GREEN_CRYSTAL_WALL,
        DNGN_METAL_WALL,
        DNGN_SHALLOW_WATER,
        DNGN_SHALLOW_WATER,
        DNGN_DEEP_WATER,
    };
    return features[(val/5)%9];
}

ProceduralSample
ColumnLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    int x = abs(p.x) % (_col_width + _col_space);
    int y = abs(p.y) % (_row_width + _row_space);
    if (x < _col_width && y < _row_width)
    {
        int w = _col_width + _col_space;
        dungeon_feature_type feat = _pick_pseudorandom_wall(hash3(p.x/w, p.y/w, 2));
        return ProceduralSample(p, feat, offset + 4096);
    }
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
    {
        dungeon_feature_type feat = _pick_pseudorandom_wall(hash3(p.x/w, p.y/w, 2));
        return ProceduralSample(p, feat, offset + 4096);
    }
    return ProceduralSample(p, DNGN_FLOOR, offset + 4096); 
}
 

uint32_t _get_changepoint(const worley::noise_datum &n, const double scale)
{
    return max(1, (int) floor((n.distance[1] - n.distance[0]) * scale) - 5);
}

ProceduralSample _maybe_set_changepoint(const ProceduralSample &s,
    const uint32_t cp)
{
    return ProceduralSample(s.coord(), s.feat(), min(s.changepoint(), cp));
}

ProceduralSample
WorleyLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    const double offset_scale = 5000.0;
    double x = p.x / scale;
    double y = p.y / scale;
    double z = offset / offset_scale;
    worley::noise_datum n = worley::noise(x, y, z + seed);

    const uint32_t changepoint = offset + _get_changepoint(n, offset_scale);
    const uint8_t size = layouts.size();
    bool parity = n.id[0] % 4;
    uint32_t id = n.id[0] / 4;
    const uint8_t choice = parity
        ? id % size
        : min(id % size, (id / size) % size);
    ProceduralSample sample = (*layouts[(choice + seed) % size])(p, offset);

    return ProceduralSample(p, sample.feat(), 
                min(changepoint, sample.changepoint()));
}

ProceduralSample
ChaosLayout::operator()(const coord_def &p, const uint32_t offset) const
{
   uint64_t base = hash3(p.x, p.y, seed);
   uint32_t density = baseDensity + seed % 50 + (seed >> 16) % 60;
    if ((base % 1000) < density)
        return ProceduralSample(p, _pick_pseudorandom_wall(base/3), offset + 4096);
    return ProceduralSample(p, DNGN_FLOOR, offset + 4096);
}

ProceduralSample
RoilingChaosLayout::operator()(const coord_def &p, const uint32_t offset) const
{   
    const double scale = (density - 350) + 4800;
    double x = p.x;
    double y = p.y;
    double z = offset / scale;
    worley::noise_datum n = worley::noise(x, y, z);
    const uint32_t changepoint = offset + _get_changepoint(n, scale);
    ProceduralSample sample = ChaosLayout(n.id[0] + seed, density)(p, offset);
    return ProceduralSample(p, sample.feat(), min(sample.changepoint(), changepoint));
}

ProceduralSample
RiverLayout::operator()(const coord_def &p, const uint32_t offset) const
{   
    const double scale = 10000;
    const double scalar = 90.0;
    double x = (p.x + perlin::fBM(p.x/4.0, p.y/4.0, seed, 5) * 3) / scalar;
    double y = (p.y + perlin::fBM(p.x/4.0 + 3.7, p.y/4.0 + 1.9, seed + 4, 5) * 3) / scalar;
    worley::noise_datum n = worley::noise(x, y, offset / scale + seed);
    const uint32_t changepoint = offset + _get_changepoint(n, scale);
    if ((n.id[0] ^ n.id[1] ^ seed) % 4) {
        return layout(p, offset);
    }
    double delta = n.distance[1] - n.distance[0];
    if (delta < 1.5/scalar)
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

ProceduralSample
NewAbyssLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    const double scale = 1.0 / 3.2;
    uint64_t base = hash3(p.x, p.y, seed);
    worley::noise_datum noise = worley::noise(
            p.x * scale,
            p.y * scale,
            offset / 1000.0);
    dungeon_feature_type feat = DNGN_FLOOR;

    int dist = noise.distance[0] * 100;
    bool isWall = (dist > 118 || dist < 30);
    int delta = min(abs(dist - 118), abs(30 - dist));

    if ((noise.id[0] + noise.id[1]) % 6 == 0)
        isWall = false;

    if (base % 3 == 0)
        isWall = !isWall;
  
    if (isWall)
    {
        int fuzz = (base / 3) % 3 ? 0 : (base / 9) % 3 - 1;
        feat = _pick_pseudorandom_wall(noise.id[0] + fuzz);
    }

    return ProceduralSample(p, feat, offset + delta);
}
