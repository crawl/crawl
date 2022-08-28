#pragma once

#include <vector>
#include <string>

#include "defines.h"
#include "externs.h"
#include "fixedarray.h"
#include "rltiles/tiledef_defines.h"

struct crawl_tile_environment
{
    // indexed by grid coords
#ifdef USE_TILE // TODO: separate out this stuff from crawl_environment
    FixedArray<tile_fg_store, GXM, GYM> bk_fg;
    FixedArray<tileidx_t, GXM, GYM> bk_bg;
    FixedArray<tileidx_t, GXM, GYM> bk_cloud;
#endif
    FixedArray<tile_flavour, GXM, GYM> flv;
    // indexed by (show-1) coords
#ifdef USE_TILE // TODO: separate out this stuff from crawl_environment
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> fg;
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> bg;
    FixedArray<tileidx_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> cloud;
    map<coord_def, set<tileidx_t>> icons;
#endif
    tile_flavour default_flavour;
    std::vector<std::string> names;
};

#ifdef DEBUG_GLOBALS
#define tile_env (*real_tile_env)
#endif
extern struct crawl_tile_environment tile_env;
