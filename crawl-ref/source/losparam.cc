/*
 *  File:       losparam.h
 *  Summary:    Parameters for the LOS algorithm
 */

#include "AppHdr.h"

#include "losparam.h"

#include "cloud.h"
#include "env.h"
#include "externs.h"
#include "mon-util.h"
#include "terrain.h"

opacity_type opacity_default::operator()(const coord_def& p) const
{
    // Secret doors in translucent walls shouldn't block LOS,
    // hence grid_appearance.
    dungeon_feature_type f = grid_appearance(p);
    if (feat_is_opaque(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(env.cgrid(p)))
        return OPC_HALF;
    else if (f == DNGN_TREES)
        return OPC_HALF;
    else if (monster_at(p) && monster_at(p)->type == MONS_BUSH)
        return OPC_HALF;
    else
        return OPC_CLEAR;
}

opacity_type opacity_fullyopaque::operator()(const coord_def& p) const
{
    if (feat_is_opaque(env.grid(p)))
        return OPC_OPAQUE;
    else
        return OPC_CLEAR;
}

static bool mons_block_no_trans(const monsters* mons)
{
    if (mons == NULL)
        return false;

    switch (mons->id())
    {
    case MONS_BUSH:
    case MONS_PLANT:
    case MONS_OKLOB_PLANT:
    case MONS_FUNGUS:
        return true;
    default:
        return false;
    }
}

// Make transparent obstacles block in addition to normal LOS.
// This is used for example for monster pathfinding, so things
// that should be walked around should be opaque.
opacity_type opacity_no_trans::operator()(const coord_def& p) const
{
    dungeon_feature_type f = env.grid(p);
    if (feat_is_opaque(f) || feat_is_wall(f) || f == DNGN_TREES)
        return OPC_OPAQUE;
    else if (mons_block_no_trans(monster_at(p)))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(env.cgrid(p)))
        return OPC_HALF;
    else
        return OPC_CLEAR;
}

// Make anything solid block in addition to normal LOS.
// That's just granite statues in addition to opacity_no_trans.
opacity_type opacity_solid::operator()(const coord_def& p) const
{
    dungeon_feature_type f = env.grid(p);
    if (feat_is_solid(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(env.cgrid(p)))
        return OPC_HALF;
    else if (f == DNGN_TREES)
        return OPC_HALF;
    else if (monster_at(p) && monster_at(p)->type == MONS_BUSH)
        return OPC_HALF;
    else
        return OPC_CLEAR;
}

opacity_type opacity_monmove::operator()(const coord_def& p) const
{
    dungeon_feature_type feat = env.grid(p);
    if (feat < DNGN_MINMOVE || !mon.can_pass_through_feat(feat))
        return (OPC_OPAQUE);
    else
        return (OPC_CLEAR);
}

opacity_type opacity_no_actor::operator()(const coord_def& p) const
{
    if (feat_is_solid(env.grid(p)) || actor_at(p))
        return (OPC_OPAQUE);
    else
        return (OPC_CLEAR);
}
