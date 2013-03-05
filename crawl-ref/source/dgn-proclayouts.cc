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

LevelLayout::LevelLayout(level_id id, uint32_t _seed, const ProceduralLayout &_layout) : seed(_seed), layout(_layout)
{
    if (!is_existing_level(id))
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

ProceduralSample
NoiseLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    return ProceduralSample(p, DNGN_FLOOR, offset + 4096);
}

ProceduralSample
ForestLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    dungeon_feature_type feat = DNGN_TREE;
    /* Extremely dense forest:
    worley::noise_datum fn = _worley(p, offset, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1);
    double height = fn.distance[0];
    double other = fn.distance[1];
    
    if (0.8 < height && height < 1.2) {
        feat = DNGN_FLOOR;
    }
    */
    // A maze of twisty passages, all different:
    /*
    worley::noise_datum fn = _worley(p, offset, 0.5, 0.0, 0.5, 0.0, 0.5, 0.0, 1);
    double height = fn.distance[0];
    double other = fn.distance[1];
    
    if (0.8 < height && height < 1.2) {
        feat = DNGN_FLOOR;
    }
    */
    /* Big swirly paths
    worley::noise_datum fn = _worley(p, offset, 0.2, 0.0, 0.2, 0.0, 2, 0.0, 1);
    double height = fn.distance[0];
    double other = fn.distance[1];
    
    if (0.8 < height && height < 1.2) {
        feat = DNGN_FLOOR;
    }
    */
    /* Craziness:
    worley::noise_datum fn = _worley(p, offset, 0.2, 0.0, 0.2, 0.0, 2, 0.0, 1);
    double height = fn.distance[0];
    double other = fn.distance[1];
    
    if (0.8 < height && height < 1.2 || 1.4 < other && other < 1.6) {
        feat = DNGN_FLOOR;
    }*/
    /* Nice big variably shaped regions: 
    worley::noise_datum fn = _worley(p, offset, 0.2, 0.0, 0.2, 0.0, 1, 0.0, 1);
    double height = fn.distance[0];
    double other = fn.distance[1];
    double diff = other-height;
    if (diff < 0.3) {
        feat = DNGN_FLOOR;
    }
    */
    // Enchanted Forest
    // TODO: Need to add some perlin distortion and/or jitter at the edges of range to make the shapes less orderly
    /*
    worley::noise_datum fn = _worley(p, offset, 0.16, 0.0, 0.16, 0.0, 1, 0.0, 1);
    double height = fn.distance[0];
    double other = fn.distance[1];
    double diff = other-height;
    worley::noise_datum fn2 = _worley(p, offset, 0.3, 100.0, 0.3, 123.0, 1, 321.0, 1);
    double diff2 = fn2.distance[1] - fn2.distance[0];
    if (diff < 0.1 || diff2 < 0.15) {
        feat = DNGN_FLOOR;
    }
    */

    worley::noise_datum fndx = _worley(p, offset, 0.2, 854.3, 0.2, 123.4, 1, 0.0, 1);
    worley::noise_datum fndy = _worley(p, offset, 0.2, 123.2, 0.2, 3623.51, 1, 0.0, 1);

    double adjustedx = (fndx.distance[0]-fndx.distance[1]-0.5) * 4.0; //* 2.0;
    double adjustedy = (fndy.distance[0]-fndy.distance[1]-0.5) * 4.0; // * 2.0;

    worley::noise_datum fn = _worley(p, offset, 0.16, adjustedx, 0.16, adjustedy, 1, 0.0, 1);
    // worley::noise_datum fn = _worley(p, offset, 0.16, 0, 0.16, 0, 1, 0.0, 1);
    double height = fn.distance[0];
    double other = fn.distance[1];
    double diff = other-height;
    //worley::noise_datum fn2 = _worley(p, offset, 0.3, 100.0, 0.3, 123.0, 1, 321.0, 1);
    //double diff2 = fn2.distance[1] - fn2.distance[0];
    if /*(height>0.9) { */(diff < 0.3) { // || diff2 < 0.15) {
        feat = DNGN_FLOOR;
    }
    //printf("1: %f, 2: %f\n, 3: %f, dx: %f, dy: %f", height,other,diff, adjustedx,adjustedy);
    int delta = 1;
    return ProceduralSample(p, feat, offset + delta);
}

ProceduralSample
ClampLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    uint32_t cycle = offset / clamp;
    uint32_t order = hash3(p.x, p.y, 0xDEADBEEF + cycle);
    if (bursty)
        order &= hash3(p.x + 31, p.y - 37, 0x0DEFACED + cycle);
    order %= clamp;
    uint32_t clamp_offset = (offset + order) / clamp * clamp;
    ProceduralSample sample = layout(p, clamp_offset);
    uint32_t cp = max(sample.changepoint(), offset + order);
    return ProceduralSample(p, sample.feat(), cp);
}

ProceduralSample
CityLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    const double scale = 9.0;
    double x = p.x / scale;
    double y = p.y / scale;
    double z = 12.0;
    worley::noise_datum n = worley::noise(x, y, z + 0xF00);
    int size = 3 + (n.id[0] % 5) & (n.id[0] / 5) % 5;
    int x_off = ceil(n.pos[0][0] * scale);
    int y_off = ceil(n.pos[0][1] * scale);
    int dist = coord_def(x_off, y_off).rdist();
    if (dist == size)
        return ProceduralSample(p, DNGN_ROCK_WALL, offset + 4096);
    return ProceduralSample(p, DNGN_FLOOR, offset + 4096);

}

ProceduralSample
PlainsLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    // Wetness gives us features like water, and optimal wetness is required for plants.
    double wet_xoff = 3463.128;
    double wet_yoff = -3737.987;
    double wet_xmul = 0.5;
    double wet_ymul = 0.5;
    double wet_dmul = 3;
    double wet_doff = 0;
    double wet_oct = 2;

    // Height
    double height_xoff = 7.543;
    double height_yoff = 2.123;
    double height_xmul = 0.3; // 0.2;    // Setting muls to 10 produces very dense terrain variation and looks cool
    double height_ymul = 0.3;// 0.2;
    double height_dmul = 1;
    double height_doff = 0;
    double height_oct = 4; // 4;

    // Temperament. Plants struggle to grow in extreme temperatures, and we only see lava in hot places.
    double hot_xoff = 111.612;
    double hot_yoff = 11.243;
    double hot_xmul = 0.1;
    double hot_ymul = 0.1;
    double hot_dmul = 2;
    double hot_doff = 0;
    double hot_oct = 1;

    // Citification; areas with a high settle factor will tend to feature "man"-made architecture
    double city_xoff = 2.732;
    double city_yoff = 22.43;
    double city_xmul = 0.2;  // 0.4 , 0.4
    double city_ymul = 0.2;
    double city_dmul = 1;
    double city_doff = 0;
    double city_oct = 1;

    // Gentrification; some cities are richer than others, for now this will just affect wall types
    // but it could be interesting to make architecture more complex and regular in rich areas too,
    // as well as influencing features like statues, fountains, plants, even monsters/loot
    double rich_xoff = 9.543;
    double rich_yoff = 5.543;
    double rich_xmul = 0.05;
    double rich_ymul = 0.05;
    double rich_dmul = 1;
    double rich_doff = 0;
    double rich_oct = 1;

    // to create lots of lines everywhere that can often be perpendicular to other features; for creating bridges, dividing walls
    double lateral_xoff = 2000.543;
    double lateral_yoff = 1414.823;
    double lateral_xmul = 4;
    double lateral_ymul = 4;
    double lateral_dmul = 1;
    double lateral_doff = 0;
    double lateral_oct = 2;

    // jitter needs to be completely random everywhere, so this can be used instead of normal random methods:
    // if (jitter < (chance_in_1)) { ... }
    double jitter_xoff = 1123.543;
    double jitter_yoff = 2451.143;
    double jitter_xmul = 10;
    double jitter_ymul = 10;
    double jitter_dmul = 0.5;
    double jitter_doff = 0;
    double jitter_oct = 5;

    // Based on the various perlin layers, determine environmental parameters at the current spot
    // printf ("Abyss: %d %d", p.x, p.y);
    double wet = _perlin(p, offset, wet_xmul, wet_xoff, wet_ymul, wet_yoff, wet_dmul, wet_doff, wet_oct);
    double height = _perlin(p, offset, height_xmul, height_xoff, height_ymul, height_yoff, height_dmul, height_doff, height_oct);

    // Note: worley noise with x,y multiplied by something below 10 (maybe about 8) produces similar density to Perlin.
    /*
    worley::noise_datum n = worley::noise(hx, hy, hz);
    double height = n.distance[0] - 1.0;
    double other = n.distance[1] - 1.0;
    */
    //printf ("X: %d, Y: %d, HX: %f, HY: %f, HZ: %f, Height: %f, Other: %f\n", p.x, p.y, hx, hy, hz, height, other);
    double hot = _perlin(p, offset, hot_xmul, hot_xoff, hot_ymul, hot_yoff, hot_dmul, hot_doff, hot_oct);
    double city = _perlin(p, offset, city_xmul, city_xoff, city_ymul, city_yoff, city_dmul, city_doff, city_oct);
    double rich = _perlin(p, offset, rich_xmul, rich_xoff, rich_ymul, rich_yoff, rich_dmul, rich_doff, rich_oct);
    double lateral = _perlin(p, offset, lateral_xmul, lateral_xoff, lateral_ymul, lateral_yoff, lateral_dmul, lateral_doff, lateral_oct);
    double jitter = _perlin(p, offset, jitter_xmul, jitter_xoff, jitter_ymul, jitter_yoff, jitter_dmul, jitter_doff, jitter_oct);

    // TODO: The abyss doesn't support all of these yet but would be nice if:
    //  * Clusters of plants around water edge
    //  * Hot and wet areas are "tropical" with plants/trees/mangroves (and steam)
    //  * Extremely hot or cold areas should generate fire or ice clouds respectively. If an "ice" feature were ever created this would be a good place for it.
    //  * Wet cities have flooding, pools, fountains, aqueducts
    //  * City + water areas have lateral bridges
    //  * Pave areas within city
    //  * Borrow some easing functions from somewhere to better control how features vary across bounaries
    //  * Look at surrounding squares to determine gradients - will help with lateral features and also e.g. growing plants on sunlit mountainsides...

    // TODO: Rather than basing all the factors purely on the perlin layers, we should combine some factors; e.g. cities thrive best at optimal combinations
    // of wet, height and hot, so the city factor could be based on how close to optimum those three are...

    // Factors controlling how the environment is mapped to terrain
    /*
    double dryness = ((1.0-wet)*0.5+0.5);
    double water_depth = -0.8 * dryness;
    double water_deep_depth = -0.9 * dryness;
    */
    double water_depth = 0.2 * wet; // 0.4,0.2
    double water_deep_depth = 0.07 * wet;
    // Good values for mountains: 0.75/0.95
    double mountain_height = 0.8;
    double mountain_top_height = 0.95;
    // Default feature
    dungeon_feature_type feat = DNGN_FLOOR;
    // printf ("Height: %f, Wet: %f", height,wet);

    // Lakes and rivers
    /* height with a mul of 10  looked really nice with:
    if (height<0.5)
        feat = DNGN_SHALLOW_WATER;
    if (height<0.3)
        feat = DNGN_DEEP_WATER;
    if (height>0.8)
        feat = DNGN_ROCK_WALL; */
    if (height < water_depth)
        feat = DNGN_SHALLOW_WATER;
    if (height < water_deep_depth)
        feat = DNGN_DEEP_WATER;

    if (height > mountain_height)
    {
        double dist_to_top = (height - mountain_height) / (mountain_top_height - mountain_height);
        if (height > mountain_top_height
            || lateral < dist_to_top)
        {
            if (hot > 0.7 && (dist_to_top>=1.0 || lateral/dist_to_top < 0.2))
                feat = DNGN_LAVA;
            else
                feat = DNGN_ROCK_WALL;
        }
    }
    /*
    if (wet > 0.9)
    {
        feat = DNGN_DEEP_WATER;
        if (hot > 0.7)
            feat = DNGN_LAVA;
    }
    else if (wet > 0.88)
    {
        feat = DNGN_SHALLOW_WATER;
        if (hot > 0.7)
            feat = DNGN_DEEP_WATER;
    }
    */

    // Forest
    bool enable_forest = true;
    // Forests fill an important gap in the middling height gap between water and mountainous regions
    double forest_start_height = 0.4; // 0.3,0.7
    double forest_end_height = 0.6;

    if (enable_forest)
    {
        // A narrow river running through the middle of forresty heights at good wetness levels
        // TODO: Use some lateral wetness to try and join mountain streams up to rivers...
        bool is_river = (abs(height-0.5) < (wet/10.0));
        if (is_river)
            feat = DNGN_SHALLOW_WATER;
        // if (  0.4 < height < 0.6 && wet > 0.5)
        /*
        double forest = (height - forest_start_height) / (forest_end_height - forest_start_height);
        forest = max(0.0,min(1.0,forest));
        forest = 1.0 - 2.0 * abs(forest - 0.5);
        */
        // Forests are somewhat finnicky about their conditions now
        double forest = 
            _optimum_range(height, forest_start_height, forest_end_height)
            * _optimum_range(wet, 0.5, 0.8)
            * _optimum_range(hot, 0.4, 0.6);

        // TODO: Petrified trees and other fun stuff in extreme temperatures

        // Forest should now be 1.0 in the center of the range, 0.0 at the end
        if (jitter < (forest * 0.7))
        {
            if (is_river && jitter < forest / 2.0)
            {
                if (forest > 0.5 && wet > 0.5)
                    feat = DNGN_MANGROVE;
            }
            else
                feat = DNGN_TREE;
        }
    }

    // City
    double city_outer_limit = 0.4;
    double city_wall_limit = 0.65; // 0.6
    double city_wall_width = 0.05;
    double city_inner_wall_limit = 0.8; // 0.8
    bool enable_city = true;

    // Cities become less likely at extreme heights and depths
    // double extreme_proximity = max(0.0,abs(height-0.5)-0.25) * 4.0;
    double extreme_proximity = max(0.0,abs(height-0.5)-0.3) * 5.0;
    city = city * (1.0 - extreme_proximity);

    if (enable_city && city >= city_outer_limit) {
        // feat = DNGN_FLOOR;

        dungeon_feature_type city_wall = DNGN_ROCK_WALL;
        if (rich > 0.5) city_wall = DNGN_STONE_WALL;
        else if (rich > 0.75) city_wall = DNGN_METAL_WALL;
        else if (rich > 0.9) city_wall = DNGN_GREEN_CRYSTAL_WALL;

        // Doors and windows
        if (jitter>0.5 && jitter<0.6) city_wall = DNGN_CLOSED_DOOR;
        if (jitter>0.7 && jitter<0.75) city_wall = DNGN_CLEAR_STONE_WALL;

        // Outer cloisters
        /*
        if (city < city_wall_limit) {
            if ((lateral >= 0.3 && lateral < 0.4 || lateral >= 0.6 && lateral < 0.7)
                 || (lateral >= 0.4 && lateral < 0.6 && city < (city_outer_limit+city_wall_width)))
                feat = city_wall;
            else if (lateral >= 0.4 && lateral < 0.6)
                feat = DNGN_FLOOR;
        }*/
        // Main outer wall
        if (city >= city_wall_limit)
        {
            // Within outer wall reset all terrain to floor.
            // TODO: We might sometimes want to modify the terrain based on what was here before the city.
            // e.g. if the city was built on water then there should be fountains, with lava we get furnaces, etc.
            feat = DNGN_FLOOR;

            if (city < (city_wall_limit + city_wall_width))
                feat = city_wall;
            /*
            else if (city <= 0.7) {
                if (lateral > 0.0 && lateral < 0.1 || lateral > 0.5 || lateral < 0.6)
                    feat = city_wall;
            }*/
            // Wall of inner halls
            else if (city >= city_inner_wall_limit)
            {
                if (city < (city_inner_wall_limit + city_wall_width))
                    feat = city_wall;
                // Decide on what decor we want within the inner walls
                // TODO: Make the decor more interesting, and choose fountain types based on wetness
                else if (jitter > 0.9)
                {
                    if (rich>0.8)
                        feat = DNGN_FOUNTAIN_BLUE;
                    else if (rich>0.5)
                        feat = DNGN_GRANITE_STATUE;
                    else if (rich>0.2)
                        feat = DNGN_GRATE;
                    else
                        feat = DNGN_STONE_ARCH;
                }

                // TOOD: Pave coords within wall limit
            }
        }
    }

    int delta = 100;

    return ProceduralSample(p, feat, offset + delta);
}

worley::noise_datum NoiseLayout::_worley(const coord_def &p, const uint32_t offset, const double xmul, const double xoff, const double ymul,const double yoff, const double zmul,const double zoff, const int oct) const
{
    double hx = ((double)p.x * (double)0.8 + xoff) * xmul;
    double hy = ((double)p.y * (double)0.8 + yoff) * ymul;
    double hz = ((double)offset / (double)500 + zoff) * zmul;
    return worley::noise(hx, hy, hz);
}

double NoiseLayout::_perlin(const coord_def &p, const uint32_t offset, const double xmul, const double xoff, const double ymul,const double yoff, const double zmul,const double zoff, const int oct) const
{
    double hx = ((double)p.x / (double)10 + xoff) * xmul;
    double hy = ((double)p.y / (double)10 + yoff) * ymul;
    double hz = ((double)offset / (double)10000 + zoff) * zmul;
    return perlin::fBM(hx, hy, hz, oct) / 2.0 + 0.5;
}

double NoiseLayout::_optimum_range(const double val, const double rstart, const double rend) const
{
    double mid = (rstart + rend) / 2.0;
    return _optimum_range_mid(val, rstart, mid, mid, rend);
}
double NoiseLayout::_optimum_range_mid(const double val, const double rstart, const double rmax1, const double rmax2, const double rend) const
{
    if (rmax1 <= val <= rmax2) return 1.0;
    if (val <= rstart || val >= rend) return 0.0;
    if (val < rmax1)
        return (val - rstart) / (rmax1-rstart);
    return 1.0 - (val - rmax2)/(rend - rmax2);
}