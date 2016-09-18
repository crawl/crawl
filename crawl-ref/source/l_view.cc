/**
 * @file
 * @brief User interaction with env.show.
**/

#include "AppHdr.h"

#include "l_libs.h"

#include "cloud.h"
#include "cluautil.h"
#include "coord.h"
#include "env.h"
#include "l_defs.h"
#include "mon-death.h"
#include "player.h"
#include "religion.h"
#include "stringutil.h"
#include "terrain.h"
#include "travel.h"
#include "view.h"

LUAFN(view_feature_at)
{
    COORDSHOW(s, 1, 2)
    const coord_def p = player2grid(s);
    if (!map_bounds(p))
    {
        lua_pushstring(ls, "unseen");
        return 1;
    }
    dungeon_feature_type f = env.map_knowledge(p).feat();
    lua_pushstring(ls, dungeon_feature_name(f));
    return 1;
}

LUAFN(view_cloud_at)
{
    COORDSHOW(s, 1, 2)
    const coord_def p = player2grid(s);
    if (!map_bounds(p))
    {
        lua_pushnil(ls);
        return 1;
    }
    cloud_type c = env.map_knowledge(p).cloud();
    if (c == CLOUD_NONE)
    {
        lua_pushnil(ls);
        return 1;
    }
    lua_pushstring(ls, cloud_type_name(c).c_str());
    return 1;
}

LUAFN(view_is_safe_square)
{
    COORDSHOW(s, 1, 2)
    const coord_def p = player2grid(s);
    if (!map_bounds(p))
    {
        PLUARET(boolean, false);
        return 1;
    }
    cloud_type c = env.map_knowledge(p).cloud();
    if (c != CLOUD_NONE
        && is_damaging_cloud(c, true, YOU_KILL(env.map_knowledge(p).cloudinfo()->killer)))
    {
        PLUARET(boolean, false);
        return 1;
    }
    trap_type t = env.map_knowledge(p).trap();
    if (t != TRAP_UNASSIGNED)
    {
        trap_def trap;
        trap.type = t;
        trap.ammo_qty = 1;
        PLUARET(boolean, trap.is_safe());
        return 1;
    }
    dungeon_feature_type f = env.map_knowledge(p).feat();
    if (f != DNGN_UNSEEN && !feat_is_traversable_now(f)
        || f == DNGN_RUNED_DOOR)
    {
        PLUARET(boolean, false);
        return 1;
    }
    PLUARET(boolean, true);
    return 1;
}

LUAFN(view_can_reach)
{
    COORDSHOW(s, 1, 2)
    const int x_distance  = abs(s.x);
    const int y_distance  = abs(s.y);
    if (x_distance > 2 || y_distance > 2)
    {
        PLUARET(boolean, false);
        return 1;
    }
    if (x_distance < 2 && y_distance < 2)
    {
        PLUARET(boolean, true);
        return 1;
    }
    const coord_def first_middle(s.x/2,s.y/2);
    const coord_def second_middle(s.x - s.x/2, s.y - s.y/2);
    if (!feat_is_reachable_past(grd(player2grid(first_middle)))
        && !feat_is_reachable_past(grd(player2grid(second_middle))))
    {
        PLUARET(boolean, false);
        return 1;
    }
    PLUARET(boolean, true);
    return 1;
}

LUAFN(view_withheld)
{
    COORDSHOW(s, 1, 2)
    const coord_def p = player2grid(s);
    if (!map_bounds(p))
    {
        PLUARET(boolean, false);
        return 1;
    }
    PLUARET(boolean, env.map_knowledge(p).flags & MAP_WITHHELD);
    return 1;
}

LUAFN(view_invisible_monster)
{
    COORDSHOW(s, 1, 2)
    const coord_def p = player2grid(s);
    if (!map_bounds(p))
    {
        PLUARET(boolean, false);
        return 1;
    }
    PLUARET(boolean, env.map_knowledge(p).flags & MAP_INVISIBLE_MONSTER);
    return 1;
}

LUAFN(view_cell_see_cell)
{
    COORDSHOW(s1, 1, 2)
    COORDSHOW(s2, 3, 4)
    const coord_def p1 = player2grid(s1);
    const coord_def p2 = player2grid(s2);
    if (!map_bounds(p1) || !map_bounds(p2))
    {
        PLUARET(boolean, false);
        return 1;
    }
    PLUARET(boolean, exists_ray(p1, p2, opc_excl));
    return 1;
}

LUAFN(view_update_monsters)
{
    ASSERT_DLUA;

    update_monsters_in_view();
    return 0;
}

static const struct luaL_reg view_lib[] =
{
    { "feature_at", view_feature_at },
    { "cloud_at", view_cloud_at },
    { "is_safe_square", view_is_safe_square },
    { "can_reach", view_can_reach },
    { "withheld", view_withheld },
    { "invisible_monster", view_invisible_monster },
    { "cell_see_cell", view_cell_see_cell },

    { "update_monsters", view_update_monsters },

    { nullptr, nullptr }
};

void cluaopen_view(lua_State *ls)
{
    luaL_openlib(ls, "view", view_lib, 0);
}
