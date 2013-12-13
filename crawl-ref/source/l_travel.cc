/**
 * @file
 * @brief Travel and exclusions.
**/

#include "AppHdr.h"

#include "l_libs.h"
#include "l_defs.h"

#include "branch.h"
#include "cluautil.h"
#include "coord.h"
#include "exclude.h"
#include "player.h"
#include "terrain.h"
#include "travel.h"

LUAFN(l_set_exclude)
{
    coord_def s;
    s.x = luaL_checkint(ls, 1);
    s.y = luaL_checkint(ls, 2);
    const coord_def p = player2grid(s);
    if (!in_bounds(p))
        return 0;
    int r = LOS_RADIUS;
    if (lua_gettop(ls) > 2)
        r = luaL_checkint(ls, 3);
    set_exclude(p, r);
    return 0;
}

LUAFN(l_del_exclude)
{
    coord_def s;
    s.x = luaL_checkint(ls, 1);
    s.y = luaL_checkint(ls, 2);
    const coord_def p = player2grid(s);
    if (!in_bounds(p))
        return 0;
    del_exclude(p);
    return 0;
}

LUAFN(l_feature_is_traversable)
{
    const string &name = luaL_checkstring(ls, 1);
    const dungeon_feature_type feat = dungeon_feature_by_name(name);
    PLUARET(boolean, feat_is_traversable_now(feat));
}

LUAFN(l_find_deepest_explored)
{
    const string &branch = luaL_checkstring(ls, 1);
    const level_id lid(str_to_branch(branch), 1);
    if (lid.branch == NUM_BRANCHES)
        luaL_error(ls, "Bad branch name: '%s'", branch.c_str());
    PLUARET(number, find_deepest_explored(lid).depth);
}

static const struct luaL_reg travel_lib[] =
{
    { "set_exclude", l_set_exclude },
    { "del_exclude", l_del_exclude },
    { "feature_traversable", l_feature_is_traversable },
    { "find_deepest_explored", l_find_deepest_explored },

    { NULL, NULL }
};

void cluaopen_travel(lua_State *ls)
{
    luaL_openlib(ls, "travel", travel_lib, 0);
}
