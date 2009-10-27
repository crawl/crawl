/*
 * File:     l_view.cc
 * Summary:  User interaction with env.show.
 */

#include "AppHdr.h"

#include "l_libs.h"

#include "cluautil.h"
#include "env.h"
#include "l_defs.h"
#include "player.h"

coord_def player2show(const coord_def &s)
{
    return (s + coord_def(ENV_SHOW_OFFSET, ENV_SHOW_OFFSET));
}

LUAFN(view_feature_at)
{
    COORDSHOW(s, 1, 2)
    const coord_def p = player2show(s);
    if (env.show(p))
        lua_pushnumber(ls, env.grid(s + you.pos()));
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
