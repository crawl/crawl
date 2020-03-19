/**
 * @file
 * @brief Procedurally generated dungeon layouts.
 **/

#pragma once

#include "dungeon.h"
#include "enum.h"
#include "fixedvector.h"
#include "worley.h"

dungeon_feature_type sanitize_feature(dungeon_feature_type feature,
        bool strict = false);

class ProceduralSample
{
    public:
        ProceduralSample(const coord_def _c, const dungeon_feature_type _ft,
                         const uint32_t _cp, map_mask_type _m = MMT_NONE)
            : c(_c), ft(_ft), cp(_cp), m(_m)
        {
            ASSERT_RANGE(ft, DNGN_UNSEEN + 1, NUM_FEATURES);
        }
        coord_def coord() const { return c; }
        dungeon_feature_type feat() const { return ft; }
        uint32_t changepoint() const { return cp; }
        map_mask_type mask() const { return m; }
    private:
        coord_def c;
        dungeon_feature_type ft;
        // A lower bound estimate of when this feat can change in terms of
        // absolute abyss depth. If you say that a feature might change by
        // depth 1000, it will get checked at depth 1000. Then it will get
        // pushed back into the terrain queue with the new depth estimate.
        // If you overestimate the time between shifts, this will introduce
        // bad behaviour when a game is loaded. [bh]
        uint32_t cp;
        map_mask_type m;
};

class ProceduralSamplePQCompare
{
    public:
        bool operator() (const ProceduralSample &lhs,
            const ProceduralSample &rhs)
        {
            return lhs.changepoint() > rhs.changepoint();
        }
};

class ProceduralLayout
{
    public:
        virtual ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const = 0;
        virtual ~ProceduralLayout() { }
};

// Geometric layout that generates columns with width cw, col spacing cs, row width rw, and row spacing rs.
// cw is the only required parameter and will generate uniform columns.
class ColumnLayout : public ProceduralLayout
{
    public:
        ColumnLayout(int cw, int cs = -1, int rw = -1, int rs = -1)
        {
            cs = (cs < 0 ? cw : cs);
            _col_width = cw;
            _col_space = cs;
            _row_width = (rw < 0 ? cw : rw);
            _row_space = (rs < 0 ? cs : rs);
        }

        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const override;
    private:
        int _col_width, _col_space, _row_width, _row_space;
};

class DiamondLayout : public ProceduralLayout
{
    public:
        DiamondLayout(int _w, int _s) : w(_w) , s(_s) { }
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const override;
    private:
        uint32_t w, s;
};

// A worley noise layout that selects between other layouts.
class WorleyLayout : public ProceduralLayout
{
    public:
        WorleyLayout(uint32_t _seed,
            vector<const ProceduralLayout*> _layouts,
            const float _scale = 2.0) :
            seed(_seed), layouts(_layouts), scale(_scale) {}
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const override;
    private:
        const uint32_t seed;
        const vector<const ProceduralLayout*> layouts;
        const float scale;
};

// A pseudo-random layout with variable density.
// By default, 45% of the area is wall. Densities in excess of 50% (500)
// are very likely to create isolated bubbles.
// See http://en.wikipedia.org/wiki/Percolation_theory for additional
// information.
// This layout is depth invariant.
class ChaosLayout : public ProceduralLayout
{
    public:
        ChaosLayout(uint32_t _seed, uint32_t _density = 450) :
            seed(_seed), baseDensity(_density) {}
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const override;
    private:
        const uint32_t seed;
        const uint32_t baseDensity;
};

// Similar to ChaosLayout, but changes relatively quickly with depth.
class RoilingChaosLayout : public ProceduralLayout
{
    public:
        RoilingChaosLayout(uint32_t _seed, uint32_t _density = 450) :
            seed(_seed), density(_density) {}
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const override;
    private:
        const uint32_t seed;
        const uint32_t density;
};

// A mostly empty layout.
class WastesLayout : public ProceduralLayout
{
    public:
        WastesLayout() { };
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const override;
};

class RiverLayout : public ProceduralLayout
{
    public:
        RiverLayout(uint32_t _seed, const ProceduralLayout &_layout) :
            seed(_seed), layout(_layout) {}
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const override;
    private:
        const uint32_t seed;
        const ProceduralLayout &layout;
};

// A reimagining of the beloved newabyss layout.
class NewAbyssLayout : public ProceduralLayout
{
    public:
        NewAbyssLayout(uint32_t _seed) : seed(_seed) {}
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const override;
    private:
        const uint32_t seed;
};

// This layout takes chunks of a single level the player has seen, corrupts them
// and uses them to build the level.
class LevelLayout : public ProceduralLayout
{
    public:
        LevelLayout(level_id id, uint32_t _seed,
            const ProceduralLayout &_layout);
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const override;
    private:
        feature_grid grid;
        uint32_t seed;
        const ProceduralLayout &layout;
};

// Clamps another layout so that it changes no more frequently than specified
// by the clamp parameter. This is useful if you can't find a good analytic
// bound on how rapidly your layout changes. If you provide a high valued clamp
// to a fast changing layout, aliasing will occur.
// If bursty is true, changes will be not be distributed uniformly over the
// clamp period.
class ClampLayout : public ProceduralLayout
{
    public:
        ClampLayout(const ProceduralLayout &_layout, int _clamp, bool _bursty) :
            layout(_layout), clamp(_clamp), bursty(_bursty) {}
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const override;
    private:
        const ProceduralLayout &layout;
        const int clamp;
        const bool bursty;
};

// Base class is only needed for a couple of support functions
// TODO: Refactor those functions into ProceduralFunctions

class NoiseLayout : public ProceduralLayout
{
    public:
        NoiseLayout() { };
        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const override;

    protected:
        double _optimum_range(const double val, const double rstart, const double rend) const;
        double _optimum_range_mid(const double val, const double rstart, const double rmax1, const double rmax2, const double rend) const;
};

class ForestLayout : public NoiseLayout
{
    public:
        ForestLayout() { };
        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const override;
};

class UnderworldLayout : public NoiseLayout
{
    public:
        UnderworldLayout() { };
        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const override;
};

// ProceduralFunctions abstract a noise calculation for x,y,z coordinates (which could
// include distortion by domain transformation)

class ProceduralFunction
{
    public:
        // Not virtual!
        double operator()(const coord_def &p, const uint32_t offset) const;
        double operator()(double x, double y, double z) const;
};

class SimplexFunction : public ProceduralFunction
{
    public:
        SimplexFunction(double _scale_x, double _scale_y, double _scale_z,
                        double _seed_x, double _seed_y, double _seed_z = 0,
                        int _octaves = 1)
            : scale_x(_scale_x), scale_y(_scale_y), scale_z(_scale_z),
              seed_x(_seed_x), seed_y(_seed_y), seed_z(_seed_z),
              octaves(_octaves) { };

        double operator()(const coord_def &p, const uint32_t offset) const;
        double operator()(double x, double y, double z) const;

    private:
        const double scale_x;
        const double scale_y;
        const double scale_z;
        const double seed_x;
        const double seed_y;
        const double seed_z;
        const int octaves;
};

class WorleyFunction : public ProceduralFunction
{
    public:
        WorleyFunction(double _scale_x, double _scale_y, double _scale_z,
                        double _seed_x, double _seed_y, double _seed_z = 0)
            : scale_x(_scale_x), scale_y(_scale_y), scale_z(_scale_z),
              seed_x(_seed_x), seed_y(_seed_y), seed_z(_seed_z) { };
        double operator()(const coord_def &p, const uint32_t offset) const;
        double operator()(double x, double y, double z) const;
        worley::noise_datum datum(double x, double y, double z) const;

    private:
        const double scale_x;
        const double scale_y;
        const double scale_z;
        const double seed_x;
        const double seed_y;
        const double seed_z;
};

class DistortFunction : public ProceduralFunction
{
    public:
        DistortFunction(const ProceduralFunction &_base,
                        const ProceduralFunction &_offx, double _scalex,
                        const ProceduralFunction &_offy, double _scaley)
            : base(_base), off_x(_offx), scale_x(_scalex),
                          off_y(_offy), scale_y(_scaley) { };
        double operator()(double x, double y, double z) const;

    protected:
        const ProceduralFunction &base;
        const ProceduralFunction &off_x;
        const double scale_x;
        const ProceduralFunction &off_y;
        const double scale_y;
};

class WorleyDistortFunction : public DistortFunction
{
    public:
        WorleyDistortFunction(const WorleyFunction &_base,
                              const ProceduralFunction &_offx, double _scalex,
                              const ProceduralFunction &_offy, double _scaley)
            : DistortFunction(_base,_offx,_scalex,_offy,_scaley), wbase(_base) { };
        worley::noise_datum datum(double x, double y, double z) const;

    private:
        const WorleyFunction &wbase;
};
