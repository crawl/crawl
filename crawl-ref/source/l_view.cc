/**
 * @file
 * @brief User interaction with env.show.
**/

#include "AppHdr.h"

#include "l_libs.h"
#include "l_defs.h"

#include "cluautil.h"
#include "coord.h"
#include "env.h"
#include "libutil.h"
#include "player.h"
#include "terrain.h"
#include "cloud.h"
#include "travel.h"


coord_def player2show(const coord_def &s)
{
    return (s + coord_def(ENV_SHOW_OFFSET, ENV_SHOW_OFFSET));
}

LUAFN(view_feature_at)
{
    COORDSHOW(s, 1, 2)
    const coord_def p = player2grid(s);
    if (!map_bounds(p))
        return (1);
    dungeon_feature_type f = env.map_knowledge(p).feat();
    if (f != DNGN_UNSEEN)
        lua_pushstring(ls, dungeon_feature_name(f));
    return (1);
}

LUAFN(view_is_safe_square)
{
    COORDSHOW(s, 1, 2)
    const coord_def p = player2grid(s);
    if (!map_bounds(p))
        return (1);
    cloud_type c = env.map_knowledge(p).cloud();
    if (c != CLOUD_NONE && is_damaging_cloud(c, true))
    {
        PLUARET(boolean, false);
        return (1);
    }
    trap_type t = env.map_knowledge(p).trap();
    if (t != TRAP_UNASSIGNED)
    {
        trap_def trap;
        trap.type = t;
        trap.ammo_qty = 1;
        PLUARET(boolean, trap.is_safe());
        return (1);
    }
    dungeon_feature_type f = env.map_knowledge(p).feat();
    if (f != DNGN_UNSEEN && !feat_is_traversable_now(f))
    {
        PLUARET(boolean, false);
        return (1);
    }
    PLUARET(boolean, true);
    return (1);
}

static const struct luaL_reg view_lib[] =
{
    { "feature_at", view_feature_at },
    { "is_safe_square", view_is_safe_square },

    { NULL, NULL }
};

void cluaopen_view(lua_State *ls)
{
    luaL_openlib(ls, "view", view_lib, 0);
}
