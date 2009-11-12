/*
 * File:     l_travel.cc
 * Summary:  Travel and exclusions.
 */

#include "AppHdr.h"

#include "l_libs.h"
#include "l_defs.h"

#include "cluautil.h"
#include "coord.h"
#include "exclude.h"
#include "player.h"

LUAFN(l_set_exclude)
{
    coord_def s;
    s.x = luaL_checkint(ls, 1);
    s.y = luaL_checkint(ls, 2);
    const coord_def p = player2grid(s);
    if (!in_bounds(p))
        return (0);
    int r = LOS_MAX_RADIUS;
    if (lua_gettop(ls) > 2)
        r = luaL_checkint(ls, 3);
    set_exclude(p, r);
    return (0);
}

LUAFN(l_del_exclude)
{
    coord_def s;
    s.x = luaL_checkint(ls, 1);
    s.y = luaL_checkint(ls, 2);
    const coord_def p = player2grid(s);
    if (!in_bounds(p))
        return (0);
    del_exclude(p);
    return (0);
}

static const struct luaL_reg travel_lib[] =
{
    { "set_exclude", l_set_exclude },
    { "del_exclude", l_del_exclude },

    { NULL, NULL }
};

void cluaopen_travel(lua_State *ls)
{
    luaL_openlib(ls, "travel", travel_lib, 0);
}
