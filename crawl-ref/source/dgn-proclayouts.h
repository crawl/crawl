/**
 * @file
 * @brief Procedurally generated dungeon layouts.
 **/

#ifndef PROC_LAYOUTS_H
#define PROC_LAYOUTS_H

#include "AppHdr.h"
#include "fixedvector.h"

#include "dungeon.h"
#include "enum.h"
#include "externs.h"
#include "worley.h"

dungeon_feature_type sanitize_feature(dungeon_feature_type feature,
        bool strict = false);

class ProceduralSample
{
    public:
        ProceduralSample(const coord_def _c,
            const dungeon_feature_type _ft,
            const uint32_t _cp, map_mask_type _m = MMT_NONE) :
            c(_c), ft(_ft), cp(_cp), m(_m) {}
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
        // bad behavior when a game is loaded. [bh]
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
            const uint32_t offset = 0) const;
    private:
        int _col_width, _col_space, _row_width, _row_space;
};

class DiamondLayout : public ProceduralLayout
{
    public:
        DiamondLayout(int _w, int _s) : w(_w) , s(_s) { }
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const;
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
            const uint32_t offset = 0) const;
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
            const uint32_t offset = 0) const;
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
            const uint32_t offset = 0) const;
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
            const uint32_t offset = 0) const;
};

class RiverLayout : public ProceduralLayout
{
    public:
        RiverLayout(uint32_t _seed, const ProceduralLayout &_layout) :
            seed(_seed), layout(_layout) {}
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const;
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
            const uint32_t offset = 0) const;
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
            const uint32_t offset = 0) const;
    private:
        feature_grid grid;
        uint32_t seed;
        const ProceduralLayout &layout;
};

class NoiseLayout : public ProceduralLayout
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
            const uint32_t offset = 0) const;
    private:
        const ProceduralLayout &layout;
        const int clamp;
        const bool bursty;
};

class CityLayout : public ProceduralLayout
{
    public:
        CityLayout() {}
        ProceduralSample operator()(const coord_def &p,
            const uint32_t offset = 0) const;
    private:
};


class PlainsLayout : public ProceduralLayout
{
    public:
        NoiseLayout() { };
        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const;

    protected:
        double _perlin(const coord_def &p, const uint32_t offset, const double xmul, const double xoff, const double ymul,const double yoff, const double zmul,const double zoff, const int oct) const;
        worley::noise_datum _worley(const coord_def &p, const uint32_t offset, const double xmul, const double xoff, const double ymul,const double yoff, const double zmul,const double zoff, const int oct) const;
        double _optimum_range(const double val, const double rstart, const double rend) const;
        double _optimum_range_mid(const double val, const double rstart, const double rmax1, const double rmax2, const double rend) const;
};

class ForestLayout : public NoiseLayout
{
    public:
        ForestLayout() { };
        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const;
};

class PlainsLayout : public NoiseLayout
{
    public:
        PlainsLayout() { };
        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const;
};

#endif /* PROC_LAYOUTS_H */
