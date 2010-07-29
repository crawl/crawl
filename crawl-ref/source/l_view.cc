/*
 * File:     l_view.cc
 * Summary:  User interaction with env.show.
 */

#include "AppHdr.h"

#include "l_libs.h"
#include "l_defs.h"

#include "cluautil.h"
#include "env.h"
#include "libutil.h"
#include "player.h"
#include "terrain.h"

coord_def player2show(const coord_def &s)
{
    return (s + coord_def(ENV_SHOW_OFFSET, ENV_SHOW_OFFSET));
}

LUAFN(view_feature_at)
{
    COORDSHOW(s, 1, 2)
    const coord_def p = player2grid(s);
    dungeon_feature_type f = env.map_knowledge(p).feat();
    if (f != DNGN_UNSEEN)
        lua_pushstring(ls, dungeon_feature_name(f));
    return (1);
}

static const struct luaL_reg view_lib[] =
{
    { "feature_at", view_feature_at },

    { NULL, NULL }
};

void cluaopen_view(lua_State *ls)
{
    luaL_openlib(ls, "view", view_lib, 0);
}
