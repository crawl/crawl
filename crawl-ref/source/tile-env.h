#pragma once

#include <vector>
#include <string>

#include "defines.h"
#include "externs.h"
#include "fixedarray.h"
#include "rltiles/tiledef_defines.h"

struct flavour_knowledge
{
    flavour_knowledge()
    {
        reset();
    }

    void reset()
    {
        _feat_flavour.init(0);
        _feat_flavour_idx.init(0);
    }

    tileidx_t feat_flavour(coord_def pos) const
    {
        return _feat_flavour(pos);
    }

    unsigned short feat_flavour_idx(coord_def pos) const
    {
        return _feat_flavour_idx(pos);
    }

    void set_feat_flavour(coord_def pos, tileidx_t tile,
                          unsigned short tile_idx)
    {
        _feat_flavour(pos) = tile;
        _feat_flavour_idx(pos) = tile_idx;
    }

    void copy_at(coord_def pos, const flavour_knowledge& source)
    {
        _feat_flavour(pos) = source._feat_flavour(pos);
        _feat_flavour_idx(pos) = source._feat_flavour_idx(pos);
    }

    void clear_at(coord_def pos)
    {
        set_feat_flavour(pos, 0, 0);
    }

private:
    FixedArray<tileidx_t, GXM, GYM> _feat_flavour;
    FixedArray<unsigned short, GXM, GYM> _feat_flavour_idx;
};

struct crawl_tile_environment
{
    // indexed by grid coords
#ifdef USE_TILE // TODO: separate out this stuff from crawl_environment
    FixedArray<tile_fg_store, GXM, GYM> bk_fg;
    FixedArray<tile_with_flags_t, GXM, GYM> bk_bg;
    FixedArray<tileidx_t, GXM, GYM> bk_cloud;
    map<coord_def, set<tileidx_t>> icons;
#endif
    FixedArray<tile_flavour, GXM, GYM> flv;
    tile_flavour default_flavour;
    flavour_knowledge remembered_flavour;
    std::vector<std::string> names;
};

#ifdef DEBUG_GLOBALS
#define tile_env (*real_tile_env)
#endif
extern struct crawl_tile_environment tile_env;
