/**
 * @file
 * @brief Procedurally generated dungeon layouts.
 **/

#ifndef PROC_LAYOUTS_H
#define PROC_LAYOUTS_H

#include "AppHdr.h"

#include <vector>

#include "dungeon.h"
#include "enum.h"
#include "externs.h"
#include "worley.h"

bool less_dense_than(const dungeon_feature_type &a, const dungeon_feature_type &b);

class ProceduralSample
{
    public:
        ProceduralSample(const coord_def _c, const dungeon_feature_type _ft, const uint32_t _cp, map_mask_type _m = MMT_NONE) : 
            c(_c), ft(_ft), cp(_cp), m(_m) {}
        coord_def coord() const { return c; }
        dungeon_feature_type feat() const { return ft; }
        uint32_t changepoint() const { return cp; }
        map_mask_type mask() const { return m; }
    private:
        coord_def c;
        dungeon_feature_type ft;
        uint32_t cp;
        map_mask_type m;
};

class ProceduralSamplePQCompare
{
    public:
        bool operator() (const ProceduralSample &lhs, const ProceduralSample &rhs)
        {
        return lhs.changepoint() > rhs.changepoint();
        }
};

class ProceduralLayout
{
    public:
        virtual ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const = 0;
};

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

        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const;
    private:
        int _col_width, _col_space, _row_width, _row_space;
};

class WorleyLayout : public ProceduralLayout
{
    public:
        WorleyLayout(uint32_t _seed,
                const ProceduralLayout &_a,
                const ProceduralLayout &_b) : seed(_seed), a(_a), b(_b) {}
        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const;
    private:
        const uint32_t seed;
        const ProceduralLayout &a, &b;
};

class ChaosLayout : public ProceduralLayout
{
    public:
        ChaosLayout(uint32_t _seed) : seed(_seed) {}
        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const;
    private:
        const uint32_t seed;
};

class RoilingChaosLayout : public ProceduralLayout
{
    public:
        RoilingChaosLayout(uint32_t _seed) : seed(_seed) {}
        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const;
    private:
        const uint32_t seed; 
        
};

class TheRiver : public ProceduralLayout
{
    public:
        TheRiver(uint32_t _seed, const ProceduralLayout &_layout) : seed(_seed), layout(_layout) {}
        ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) const;
    private:
        const uint32_t seed; 
        const ProceduralLayout &layout;
};

#endif /* PROC_LAYOUTS_H */
