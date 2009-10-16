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

opacity_type opacity_default::operator()(const coord_def& p) const
{
    int m;
    dungeon_feature_type f = env.grid(p);
    if (grid_is_opaque(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(env.cgrid(p)))
        return OPC_HALF;
    else if (f == DNGN_TREES)
        return OPC_HALF;
    else if ((m = mgrd(p)) != NON_MONSTER && menv[m].type == MONS_BUSH)
        return OPC_HALF;
    else
        return OPC_CLEAR;
}

// Make anything solid block in addition to normal LOS.
// XXX: Are trees, bushes solid?
opacity_type opacity_solid::operator()(const coord_def& p) const
{
    int m;
    dungeon_feature_type f = env.grid(p);
    if (grid_is_solid(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(env.cgrid(p)))
        return OPC_HALF;
    else if (f == DNGN_TREES)
        return OPC_HALF;
    else if ((m = mgrd(p)) != NON_MONSTER && menv[m].type == MONS_BUSH)
        return OPC_HALF;
    else
        return OPC_CLEAR;
}

// LOS bounded by fixed presquared radius.
bool bounds_radius_sq::operator()(const coord_def& p) const
{
    return (p.abs() <= radius_sq);
}

// LOS bounded by current global LOS radius.
bool bounds_los_radius::operator()(const coord_def& p) const
{
    return (p.abs() <= get_los_radius_squared());
}
