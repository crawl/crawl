/*
 *  File:       losparam.h
 *  Summary:    Parameters for the LOS algorithm 
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "losparam.h"

#include "cloud.h"
#include "externs.h"
#include "los.h"
#include "stuff.h"
#include "terrain.h"


/* los_param_trans */

los_param_trans::los_param_trans(const coord_def& c)
    : center(c)
{
}

coord_def los_param_trans::trans(const coord_def& p) const
{
    return (p + center);
}

bool los_param_trans::los_bounds(const coord_def& p) const
{
    return (map_bounds(trans(p)) && p.abs() <= get_los_radius_squared());
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


/* los_param_nocloud */

los_param_nocloud::los_param_nocloud(const coord_def& c)
    : los_param_trans(c)
{
}

dungeon_feature_type los_param_nocloud::feature(const coord_def& p) const
{
    return env.grid(trans(p));
}

unsigned los_param_nocloud::appearance(const coord_def& p) const
{
    return grid_appearance(trans(p));
}

opacity_type los_param_nocloud::opacity(const coord_def& p) const
{
    dungeon_feature_type f = feature(p);
    if (grid_is_opaque(f))
        return OPC_OPAQUE;
    else
        return OPC_CLEAR;
}


/* los_param_base */

los_param_base::los_param_base(const coord_def& c)
    : los_param_nocloud(c)
{
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
    else if (f == DNGN_TREES)
        return OPC_HALF;
    else
        return OPC_CLEAR;
}


/* los_param_solid */

los_param_solid::los_param_solid(const coord_def& c)
    : los_param_base(c)
{
}

opacity_type los_param_solid::opacity(const coord_def& p) const
{
    dungeon_feature_type f = feature(p);
    if (grid_is_solid(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(cloud_idx(p)))
        return OPC_HALF;
    else
        return OPC_CLEAR;
}
