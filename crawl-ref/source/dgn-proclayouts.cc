/**
 * @file
 * @brief Procedurally generated dungeon layouts.
 **/
#include "AppHdr.h"

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
    static dungeon_feature_type features[] =
    {
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
            solid_count += cell_is_solid(*ai);
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
    dungeon_feature_type feat = DNGN_FLOOR;

    const static WorleyFunction base(0.32,0.4,0.5,0,0,0);
    const static WorleyFunction offx(0.6,0.6,0.2,854.3,123.4,0.0);
    const static WorleyFunction offy(0.6,0.6,0.2,123.2,3623.51,0.0);
    const static WorleyDistortFunction tfunc(base,offx,2.0,offy,1.5);

    worley::noise_datum fn = tfunc.datum(p.x,p.y,offset);

    // Split the id into some 8-bit numbers to use for randomness
    uint8_t rand[2] = { (uint8_t)(fn.id[0] >> 24), (uint8_t)(fn.id[0] >> 16) };
        // , fn.id[0] >> 8 & 0x000000ff, fn.id[0] & 0x000000ff };

    double diff = fn.distance[1]-fn.distance[0];
    float size_factor = (float)rand[1]/(255.0 * 2.0) + 0.15;
    // 75% chance of trees
    if (rand[0]<0xC0)
    {
        // Size of clump depends on factor
        if (diff > size_factor)
            feat = DNGN_TREE;
    }
    // Small chance of henge
    else if (rand[0]<0xC8)
    {
        if (diff > (size_factor+0.3) && diff < (size_factor+0.4))
            feat = DNGN_STONE_ARCH;
    }
    // Somewhat under 25% chance of pool
    else if (rand[0]<0xE0)
    {
        if (diff > size_factor*2.0)
            feat = DNGN_DEEP_WATER;
        else if (diff > size_factor)
            feat = DNGN_SHALLOW_WATER;
    }
    // 25% chance of empty cell
    return ProceduralSample(p, feat, offset + 1); // Delta is always 1 because the layout will be clamped
}

// An expansive underworld containing seas, rivers, lakes, forests, cities, mountains, and perhaps more...
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
UnderworldLayout::operator()(const coord_def &p, const uint32_t offset) const
{
    // Define various environmental functions based on noise. These factors
    // combine to determine what terrain gets drawn at a given coordinate.

    // Wetness gives us features like water, and optimal wetness will be required for plants
    const static SimplexFunction func_wet(0.5,0.5,3.0,3463.128,-3737.987,0,2);
    // Terrain height gives us mountains, rivers, ocean
    const static SimplexFunction func_height(0.3,0.3,1.0,7.543,2.123,0,4);
    // Temperament. Plants struggle to grow in extreme temperatures, and we only see lava in hot places.
    const static SimplexFunction func_hot(0.1,0.1,2.0,111.612,11.243,0,1);
    // Citification; areas with a high settle factor will tend  to feature "man"-made architecture
    // TODO: Citi/gentrification could use a worley layer instead (or a mix) to have better geometry
    // and stop things like wall types suddenly changing halfway through a city.
    const static SimplexFunction func_city(0.2,0.2,1.0,2.732,22.43,0,1);
    // Gentrification; some cities are richer than others, this affects wall types
    // but we can also choose what features to build inside the cities, influencing
    // features like statues, fountains, plants, regularness, and even monsters/loot
    const static SimplexFunction func_rich(0.05,0.05,1,9.543,5.543,0,1);

    // To create lots of lines everywhere that can often be perpendicular to other
    // features; for creating bridges, dividing walls
    const static WorleyFunction func_lateral(0.2,0.2,1,2000.543,1414.823,0);

    // Jitter needs to be completely random everywhere, so this can be used
    // instead of normal random methods:
    // if (jitter < (chance_in_1)) { ... }
    const static SimplexFunction func_jitter(10,10,0.5,1123.543,2451.143,0,5);

    // Compute all our environment factors at the current spot
    double wet = func_wet(p, offset);
    double height = func_height(p, offset);
    double hot = func_hot(p, offset);
    double city = func_city(p, offset);
    double rich = func_rich(p, offset);
    double lateral = func_lateral(p, offset);
    double jitter = func_jitter(p, offset);

    // TODO: The abyss doesn't support all of these yet but would be nice if:
    //  * Clusters of plants around water edge
    //  * Hot and wet areas are "tropical" with plants/trees/mangroves (and steam)
    //  * Extremely hot or cold areas should generate fire or ice clouds respectively.
    //    If an "ice" feature were ever created this would be a good place for it.
    //  * Wet cities have
    //  * City + water areas have lateral bridges
    //  * Borrow some easing functions from somewhere to better
    //    control how features vary across bounaries
    //  * Look at surrounding squares to determine gradients - will help
    //    with lateral features and also e.g. growing plants on sunlit mountainsides...
    //  * Use some lateral wetness to try and join mountain streams up to rivers...
    //  * Petrified trees and other fun stuff in extreme temperatures
    //  * Cities - Make the decor more interesting, and choose fountain types based on wetness
    //  * Cities - Pave floor within wall limit
    //  * Cities - might sometimes want to modify the terrain based on what was here before the city.
    //             e.g. if the city was built on water then there should be fountains,
    //             flooding, pools, aqueducts, with lava we get furnaces, etc.
    //  * Rather than basing all the factors purely on separate perlin layers,
    //    could combine some factors; e.g. cities thrive best at optimal combinations
    //    of wet, height and hot, so the city/rich factor could be based on how close to optimum those three are...

    // Factors controlling how the environment is mapped to terrain
    double water_depth = 0.2 * wet;
    double water_deep_depth = 0.07 * wet;
    double mountain_height = 0.8;
    double mountain_top_height = 0.95;

    // Default feature
    dungeon_feature_type feat = DNGN_FLOOR;

    // Lakes and rivers
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

    // Forest
    bool enable_forest = true;
    // Forests fill an important gap in the middling height gap between water and mountainous regions
    double forest_start_height = 0.43;
    double forest_end_height = 0.57;

    if (enable_forest)
    {
        // A narrow river running through the middle of forresty heights at good wetness levels
        bool is_river = (abs(height-0.5) < (wet/10.0));
        if (is_river)
            feat = DNGN_SHALLOW_WATER;

        // Forests are somewhat finnicky about their conditions now
        double forest =
            _optimum_range(height, forest_start_height, forest_end_height)
            * _optimum_range(wet, 0.5, 0.8)
            * _optimum_range(hot, 0.4, 0.6);

        // Forest should now be 1.0 in the center of the range, 0.0 at the end
        if (jitter < (forest * 0.5))
        {
            if (is_river)
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
    double city_wall_limit = 0.65;
    double city_wall_width = 0.05;
    double city_inner_wall_limit = 0.8;
    bool enable_city = true;

    // Cities become less likely at extreme heights and depths
    double extreme_proximity = max(0.0,abs(height-0.5)-0.3) * 5.0;
    city = city * (1.0 - extreme_proximity);

    if (enable_city && city >= city_outer_limit)
    {
        dungeon_feature_type city_wall = DNGN_ROCK_WALL;
        if (rich > 0.5) city_wall = DNGN_STONE_WALL;
        else if (rich > 0.75) city_wall = DNGN_METAL_WALL;
        else if (rich > 0.9) city_wall = DNGN_GREEN_CRYSTAL_WALL;

        // Doors and windows
        if (jitter>0.5 && jitter<0.6) city_wall = DNGN_CLOSED_DOOR;
        if (jitter>0.7 && jitter<0.75) city_wall = DNGN_CLEAR_STONE_WALL;

        // Outer cloisters
        if (city < city_wall_limit)
        {
            if ((lateral >= 0.3 && lateral < 0.4 || lateral >= 0.6 && lateral < 0.7)
                 || (lateral >= 0.4 && lateral < 0.6 && city < (city_outer_limit+city_wall_width)))
                feat = city_wall;
            else if (lateral >= 0.4 && lateral < 0.6)
                feat = DNGN_FLOOR;
        }

        // Main outer wall
        if (city >= city_wall_limit)
        {
            // Within outer wall reset all terrain to floor.
            feat = DNGN_FLOOR;

            if (city < (city_wall_limit + city_wall_width))
                feat = city_wall;
            // Wall of inner halls
            else if (city >= city_inner_wall_limit)
            {
                if (city < (city_inner_wall_limit + city_wall_width))
                    feat = city_wall;
                // Decide on what decor we want within the inner walls
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
            }
        }
    }

    int delta = 1;

    return ProceduralSample(p, feat, offset + delta);
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

double ProceduralFunction::operator()(const coord_def &p, const uint32_t offset) const
{
    return ProceduralFunction::operator()(p.x,p.y,offset);
}

double ProceduralFunction::operator()(double x, double y, double z) const
{
    return 0;
}

double SimplexFunction::operator()(const coord_def &p, const uint32_t offset) const
{
    return SimplexFunction::operator()(p.x,p.y,offset);
}

double SimplexFunction::operator()(double x, double y, double z) const
{
    double hx = (x / (double)10 + seed_x) * scale_x;
    double hy = (y / (double)10 + seed_y) * scale_y;
    double hz = (z / (double)10000 + seed_z) * scale_z;
    // Use octaval simplex and scale into a 0..1 range
    return perlin::fBM(hx, hy, hz, octaves) / 2.0 + 0.5;
}

double WorleyFunction::operator()(const coord_def &p, const uint32_t offset) const
{
    return WorleyFunction::operator()(p.x,p.y,offset);
}

double WorleyFunction::operator()(double x, double y, double z) const
{
    worley::noise_datum d = this->datum(x,y,z);
    return d.distance[1]-d.distance[0];
}

worley::noise_datum WorleyFunction::datum(double x, double y, double z) const
{
    double hx = (x * (double)0.8 + seed_x) * scale_x;
    double hy = (y * (double)0.8 + seed_y) * scale_y;
    double hz = (z * (double)0.0008 + seed_z) * scale_z;
    return worley::noise(hx, hy, hz);
}

double DistortFunction::operator()(double x, double y, double z) const
{
    double offx = off_x(x,y,z);
    double offy = off_y(x,y,z);
    return base(x+offx,y+offy,z);
}

worley::noise_datum WorleyDistortFunction::datum(double x, double y, double z) const
{
    double offx = off_x(x,y,z);
    double offy = off_y(x,y,z);
    return wbase.datum(x+offx,y+offy,z);
}
