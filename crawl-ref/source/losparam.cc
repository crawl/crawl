/**
 * @file
 * @brief Parameters for the LOS algorithm
**/

#include "AppHdr.h"

#include "losparam.h"

#include "cloud.h"
#include "env.h"
#include "externs.h"
#include "mon-util.h"
#include "state.h"
#include "terrain.h"

const opacity_default opc_default = opacity_default();
const opacity_fullyopaque opc_fullyopaque = opacity_fullyopaque();
const opacity_no_trans opc_no_trans = opacity_no_trans();
const opacity_immob opc_immob = opacity_immob();
const opacity_solid opc_solid = opacity_solid();
const opacity_solid_see opc_solid_see = opacity_solid_see();
const opacity_no_actor opc_no_actor = opacity_no_actor();

opacity_type opacity_default::operator()(const coord_def& p) const
{
    // Secret doors in translucent walls shouldn't block LOS,
    // hence grid_appearance.
    dungeon_feature_type f = grid_appearance(p);
    if (feat_is_opaque(f))
        return OPC_OPAQUE;
    else if (feat_is_tree(f))
        return OPC_HALF;
    else if (is_opaque_cloud(env.cgrid(p)))
        return OPC_HALF;
    if (const monster *mon = monster_at(p))
        return mons_opacity(mon, LOS_DEFAULT);
    return OPC_CLEAR;
}

opacity_type opacity_fullyopaque::operator()(const coord_def& p) const
{
    if (feat_is_opaque(env.grid(p)))
        return OPC_OPAQUE;
    else
        return OPC_CLEAR;
}

opacity_type opacity_no_trans::operator()(const coord_def& p) const
{
    dungeon_feature_type f = grid_appearance(p);
    if (feat_is_opaque(f) || feat_is_wall(f) || feat_is_tree(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(env.cgrid(p)))
        return OPC_HALF;
    if (const monster *mon = monster_at(p))
        return mons_opacity(mon, LOS_NO_TRANS);
    return OPC_CLEAR;
}

static bool mons_block_immob(const monster* mons)
{
    if (mons == NULL)
        return false;

    // In Zotdef, plants don't block movement as critters
    // will attack them
    if (crawl_state.game_is_zotdef())
        return false;

    return mons_is_stationary(mons);
}

opacity_type opacity_immob::operator()(const coord_def& p) const
{
    opacity_type base = opc_no_trans(p);

    if (mons_block_immob(monster_at(p)))
        return OPC_OPAQUE;
    else
        return base;
}

opacity_type opacity_solid::operator()(const coord_def& p) const
{
    dungeon_feature_type f = env.grid(p);
    if (feat_is_solid(f))
        return OPC_OPAQUE;

    return OPC_CLEAR;
}

// Make anything solid block in addition to normal LOS.
// That's just granite statues in addition to opacity_no_trans.
opacity_type opacity_solid_see::operator()(const coord_def& p) const
{
    dungeon_feature_type f = env.grid(p);
    if (feat_is_solid(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(env.cgrid(p)))
        return OPC_HALF;
    else if (const monster *mon = monster_at(p))
        return mons_opacity(mon, LOS_SOLID_SEE);

    return OPC_CLEAR;
}

opacity_type opacity_monmove::operator()(const coord_def& p) const
{
    dungeon_feature_type feat = env.grid(p);
    if (feat < DNGN_MINMOVE || !mon.can_pass_through_feat(feat))
        return OPC_OPAQUE;
    else
        return OPC_CLEAR;
}

opacity_type opacity_no_actor::operator()(const coord_def& p) const
{
    if (feat_is_solid(env.grid(p)) || actor_at(p))
        return OPC_OPAQUE;
    else
        return OPC_CLEAR;
}
