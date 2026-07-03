/**
 * @file
 * @brief Parameters for the LOS algorithm
**/

#include "AppHdr.h"

#include "losparam.h"

#include "cloud.h"
#include "env.h"
#include "los.h"
#include "state.h"
#include "terrain.h"

const opacity_default opc_default = opacity_default();
const opacity_fullyopaque opc_fullyopaque = opacity_fullyopaque();
const opacity_no_trans opc_no_trans = opacity_no_trans();
const opacity_fully_no_trans opc_fully_no_trans = opacity_fully_no_trans();
const opacity_immob opc_immob = opacity_immob();
const opacity_solid opc_solid = opacity_solid();
const opacity_solid_see opc_solid_see = opacity_solid_see();
const opacity_no_actor opc_no_actor = opacity_no_actor();
const opacity_unblocked_shot opc_unblocked_shot = opacity_unblocked_shot();
const opacity_excl opc_excl = opacity_excl();
const opacity_map_default opc_map_default = opacity_map_default();
const opacity_map_no_trans opc_map_no_trans = opacity_map_no_trans();
const opacity_map_solid opc_map_solid = opacity_map_solid();
const opacity_map_solid_see opc_map_solid_see = opacity_map_solid_see();

opacity_type opacity_default::operator()(const coord_def& p) const
{
    dungeon_feature_type f = env.grid(p);
    if (feat_is_opaque(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(cloud_type_at(p)))
        return OPC_HALF;
    else if (const monster *mon = monster_at(p))
        return mons_opacity(mon->type, LOS_DEFAULT);
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
    dungeon_feature_type f = env.grid(p);
    if (feat_is_opaque(f) || feat_is_wall(f) || feat_is_closed_door(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(cloud_type_at(p)))
        return OPC_HALF;
    else if (const monster *mon = monster_at(p))
        return mons_opacity(mon->type, LOS_NO_TRANS);
    return OPC_CLEAR;
}

opacity_type opacity_fully_no_trans::operator()(const coord_def& p) const
{
    dungeon_feature_type f = env.grid(p);
    if (feat_is_opaque(f) || feat_is_wall(f) || feat_is_closed_door(f))
        return OPC_OPAQUE;
    return OPC_CLEAR;
}

opacity_type opacity_immob::operator()(const coord_def& p) const
{
    const monster* mons = monster_at(p);
    return (mons && mons->is_stationary()) ? OPC_OPAQUE : opc_no_trans(p);
}

opacity_type opacity_mons_immob::operator()(const coord_def& p) const
{
    const monster* other_mons = monster_at(p);
    const bool impassable = other_mons
                            && other_mons->is_stationary()
                            && mons_aligned(mon, other_mons);
    return impassable ? OPC_OPAQUE : opc_no_trans(p);
}

opacity_type opacity_solid::operator()(const coord_def& p) const
{
    dungeon_feature_type f = env.grid(p);
    if (feat_is_solid(f))
        return OPC_OPAQUE;

    return OPC_CLEAR;
}

// Make anything solid block in addition to normal LOS.
// That includes statues and grates in addition to opacity_no_trans.
opacity_type opacity_solid_see::operator()(const coord_def& p) const
{
    dungeon_feature_type f = env.grid(p);
    if (feat_is_solid(f))
        return OPC_OPAQUE;
    else if (is_opaque_cloud(cloud_type_at(p)))
        return OPC_HALF;
    else if (const monster *mon = monster_at(p))
        return mons_opacity(mon->type, LOS_SOLID_SEE);

    return OPC_CLEAR;
}

opacity_type opacity_monmove::operator()(const coord_def& p) const
{
    dungeon_feature_type feat = env.grid(p);
    if (feat_is_solid(feat) || !mon.can_pass_through_feat(feat))
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

opacity_type opacity_unblocked_shot::operator()(const coord_def& p) const
{
    if (feat_is_solid(env.grid(p)))
        return OPC_OPAQUE;

    if (actor* act = actor_at(p))
    {
        if (you.can_see(*act) && !shoot_through_actor(&you, act)
            && act->type != MONS_BUSH)
        {
            return OPC_OPAQUE;
        }
    }

    return OPC_CLEAR;
}

opacity_type opacity_excl::operator()(const coord_def& p) const
{
    map_cell& cell = env.map_knowledge(p);
    if (!cell.known())
        return OPC_CLEAR;
    else if (!cell.changed())
        return feat_is_opaque(env.grid(p)) ? OPC_OPAQUE : OPC_CLEAR;
    else if (cell.feat() != DNGN_UNSEEN)
        return feat_is_opaque(cell.feat()) ? OPC_OPAQUE : OPC_CLEAR;
    else
        return OPC_CLEAR;
}

opacity_type opacity_map_default::operator()(const coord_def& p) const
{
    map_cell& cell = env.map_knowledge(p);
    dungeon_feature_type f = cell.feat();
    if (!cell.known() || f == DNGN_UNSEEN)
        return OPC_CLEAR;

    if (feat_is_opaque(f))
       return OPC_OPAQUE;

    if (is_opaque_cloud(cell.cloud()))
        return OPC_HALF;

    if (cell.monsterinfo())
        return mons_opacity(cell.mon_type(), LOS_DEFAULT);

    return OPC_CLEAR;
}

opacity_type opacity_map_no_trans::operator()(const coord_def& p) const
{
    map_cell& cell = env.map_knowledge(p);
    dungeon_feature_type f = cell.feat();
    if (!cell.known() || f == DNGN_UNSEEN)
        return OPC_CLEAR;

    if (feat_is_opaque(f) || feat_is_wall(f) || feat_is_closed_door(f))
        return OPC_OPAQUE;

    if (is_opaque_cloud(cell.cloud()))
        return OPC_HALF;

    if (cell.monsterinfo())
        return mons_opacity(cell.mon_type(), LOS_NO_TRANS);

    return OPC_CLEAR;
}

opacity_type opacity_map_solid::operator()(const coord_def& p) const
{
    map_cell& cell = env.map_knowledge(p);
    dungeon_feature_type f = cell.feat();
    if (!cell.known() || f == DNGN_UNSEEN)
        return OPC_CLEAR;

    if (feat_is_solid(f))
        return OPC_OPAQUE;

    return OPC_CLEAR;
}

opacity_type opacity_map_solid_see::operator()(const coord_def& p) const
{
    map_cell& cell = env.map_knowledge(p);
    dungeon_feature_type f = cell.feat();
    if (!cell.known() || f == DNGN_UNSEEN)
        return OPC_CLEAR;

    if (feat_is_solid(f))
        return OPC_OPAQUE;

    if (is_opaque_cloud(cell.cloud()))
        return OPC_HALF;

    if (cell.monsterinfo())
        return mons_opacity(cell.mon_type(), LOS_SOLID_SEE);

    return OPC_CLEAR;
}
