/**
 * @file
 * @brief Procedurally generated dungeon layouts.
 **/

#include <cmath>

#include "dgn-proclayouts.h"
#include "coord.h"
#include "coordit.h"
#include "files.h"
#include "hash.h"
#include "perlin.h"
#include "terrain.h"
#include "worley.h"

#include "mpr.h"

static dungeon_feature_type _pick_pseudorandom_wall(uint64_t val)
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


static uint32_t _get_changepoint(const worley::noise_datum &n, const double scale)
{
    return max(1, (int) floor((n.distance[1] - n.distance[0]) * scale) - 5);
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
    const coord_def pd = p + id;
    ProceduralSample sample = (*layouts[(choice + seed) % size])(pd, offset);

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
WastesLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    double x = p.x;
    double y = p.y;
    double z = offset / 3;
    worley::noise_datum n = worley::noise(x, y, z);
    const uint32_t changepoint = offset + _get_changepoint(n, 3);
    ProceduralSample sample = ChaosLayout(n.id[0], 10)(p, offset);
    dungeon_feature_type feat = feat_is_solid(sample.feat())
        ? DNGN_ROCK_WALL : DNGN_FLOOR;
    return ProceduralSample(p, feat, min(sample.changepoint(), changepoint));
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
    if ((n.id[0] ^ n.id[1] ^ seed) % 4)
        return layout(p, offset);

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

dungeon_feature_type sanitize_feature(dungeon_feature_type feature, bool strict)
{
    if (feat_is_gate(feature))
        feature = DNGN_STONE_ARCH;
    if (feat_is_stair(feature))
        feature = strict ? DNGN_FLOOR : DNGN_STONE_ARCH;
    if (feat_is_altar(feature))
        feature = DNGN_FLOOR;
    if (feature == DNGN_ENTER_SHOP)
        feature = DNGN_ABANDONED_SHOP;
    if (feat_is_trap(feature, true))
        feature = DNGN_FLOOR;
    switch (feature)
    {
        // demote permarock
        case DNGN_PERMAROCK_WALL:
            feature = DNGN_ROCK_WALL;
            break;
        case DNGN_CLEAR_PERMAROCK_WALL:
            feature = DNGN_CLEAR_ROCK_WALL;
            break;
        case DNGN_SLIMY_WALL:
            feature = DNGN_GREEN_CRYSTAL_WALL;
        case DNGN_UNSEEN:
            feature = DNGN_FLOOR;
        default:
            // handle more terrain types.
            break;
    }
    return feature;
}

LevelLayout::LevelLayout(level_id id, uint32_t _seed, const ProceduralLayout &_layout) : layout(_layout)
{
    if(!is_existing_level(id))
    {
        for (rectangle_iterator ri(0); ri; ++ri)
            grid(*ri) = DNGN_UNSEEN;
        return;
    }
    level_excursion le;
    le.go_to(id);
    grid = feature_grid(grd);
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        grid(*ri) = sanitize_feature(grid(*ri), true);
        if (!in_bounds(*ri))
        {
            grid(*ri) = DNGN_UNSEEN;
            continue;
        }

        uint32_t solid_count = 0;
        for (adjacent_iterator ai(*ri); ai; ++ai)
            solid_count += feat_is_solid(grd(*ai));
        coord_def p = *ri;
        uint64_t base = hash3(p.x, p.y, seed);
        int div = base % 2 ? 12 : 11;
        switch (solid_count)
        {
            case 8:
                grid(*ri) = DNGN_UNSEEN;
                break;
            case 7:
            case 6:
            case 5:
                if (!((base / 2) % div))
                    grid(*ri) = DNGN_UNSEEN;
                break;
            case 0:
            case 1:
                if (!(base % 14))
                    grid(*ri) = DNGN_UNSEEN;
                break;
        }
    }
    le.~level_excursion();
}

ProceduralSample
LevelLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    coord_def cp = clip(p);
    dungeon_feature_type feat = grid(cp);
    if (feat == DNGN_UNSEEN)
        return layout(p, offset);
    return ProceduralSample(p, feat, offset + 4096);
}
