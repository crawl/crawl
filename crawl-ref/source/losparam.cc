/*
 *  File:       losparam.h
 *  Summary:    Parameters for the LOS algorithm 
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "losparam.h"

#include "cloud.h"
#include "externs.h"
#include "stuff.h"
#include "terrain.h"


/* los_param_trans */

los_param_trans::los_param_trans(const coord_def& c)
    : center(c)
{
}

coord_def los_param_trans::trans(const coord_def& p) const
{
    return p + center;
}

bool los_param_trans::map_bounds(const coord_def& p) const
{
    return ::map_bounds(trans(p));
}


/* los_param_permissive */

los_param_permissive::los_param_permissive(const coord_def& c)
    : los_param_trans(c)
{
}

unsigned los_param_permissive::appearance(const coord_def& p) const
{
    return env.grid(trans(p));
}

opacity_type los_param_permissive::opacity(const coord_def& p) const
{
    return OPC_CLEAR;
}


/* los_param_base */

los_param_base::los_param_base(const coord_def& c)
    : los_param_trans(c)
{
}

dungeon_feature_type los_param_base::feature(const coord_def& p) const
{
    return env.grid(trans(p));
}

unsigned los_param_base::appearance(const coord_def& p) const
{
    return grid_appearance(trans(p));
}

unsigned short los_param_base::cloud_idx(const coord_def& p) const
{
    return env.cgrid(trans(p));
}

opacity_type los_param_base::opacity(const coord_def& p) const
{
    dungeon_feature_type f = feature(p);
    if (grid_is_opaque(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(cloud_idx(p)))
        return OPC_HALF;
    else
        return OPC_CLEAR;
}


/* los_param_compat */

los_param_compat::los_param_compat(const feature_grid& gr, const coord_def& c,
                                   bool sb, bool ic)
    : los_param_base(c), grid(gr), solid_blocks(sb), ignore_clouds(ic)
{
}

dungeon_feature_type los_param_compat::feature(const coord_def& p) const
{
    return grid(trans(p));
}

unsigned los_param_compat::appearance(const coord_def& p) const
{
    return grid_appearance(grid, trans(p));
}

opacity_type los_param_compat::opacity(const coord_def& p) const
{
    dungeon_feature_type f = feature(p);
    if (grid_is_opaque(f) || solid_blocks && grid_is_solid(f))
        return OPC_OPAQUE;
    else if (!ignore_clouds && is_opaque_cloud(cloud_idx(p)))
        return OPC_HALF;
    else
        return OPC_CLEAR;
}

