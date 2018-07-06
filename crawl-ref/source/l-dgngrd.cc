/***
 * @module dgn
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "coord.h"
#include "directn.h"
#include "dungeon.h"
#include "libutil.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"

static int dgn_feature_number(lua_State *ls)
{
    const string &name = luaL_checkstring(ls, 1);
    PLUARET(number, dungeon_feature_by_name(name));
}

static int dgn_feature_name(lua_State *ls)
{
    const unsigned feat = luaL_checkint(ls, 1);
    PLUARET(string,
            dungeon_feature_name(static_cast<dungeon_feature_type>(feat)));
}

static dungeon_feature_type _get_lua_feature(lua_State *ls, int idx,
                                             bool optional = false)
{
    dungeon_feature_type feat = (dungeon_feature_type)0;
    if (lua_isnumber(ls, idx))
        feat = (dungeon_feature_type)luaL_checkint(ls, idx);
    else if (lua_isstring(ls, idx))
        feat = dungeon_feature_by_name(luaL_checkstring(ls, idx));
    else if (!optional)
        luaL_argerror(ls, idx, "Feature must be a string or a feature index.");

    return feat;
}

dungeon_feature_type check_lua_feature(lua_State *ls, int idx, bool optional)
{
    const dungeon_feature_type f = _get_lua_feature(ls, idx, optional);
    if (!optional && !f)
        luaL_argerror(ls, idx, "Invalid dungeon feature");
    return f;
}

#define FEAT(f, pos) \
dungeon_feature_type f = check_lua_feature(ls, pos)

static int dgn_feature_desc(lua_State *ls)
{
    const dungeon_feature_type feat =
    static_cast<dungeon_feature_type>(luaL_checkint(ls, 1));
    const description_level_type dtype =
    lua_isnumber(ls, 2)?
    static_cast<description_level_type>(luaL_checkint(ls, 2)) :
    description_type_by_name(lua_tostring(ls, 2));
    const bool need_stop = lua_isboolean(ls, 3)? lua_toboolean(ls, 3) : false;
    const string s = feature_description(feat, NUM_TRAPS, "", dtype, need_stop);
    lua_pushstring(ls, s.c_str());
    return 1;
}

static int dgn_feature_desc_at(lua_State *ls)
{
    const description_level_type dtype =
    lua_isnumber(ls, 3)?
    static_cast<description_level_type>(luaL_checkint(ls, 3)) :
    description_type_by_name(lua_tostring(ls, 3));
    const bool need_stop = lua_isboolean(ls, 4)? lua_toboolean(ls, 4) : false;
    const string s = feature_description_at(coord_def(luaL_checkint(ls, 1),
                                                      luaL_checkint(ls, 2)),
                                            false, dtype, need_stop);
    lua_pushstring(ls, s.c_str());
    return 1;
}

static int dgn_max_bounds(lua_State *ls)
{
    lua_pushnumber(ls, GXM);
    lua_pushnumber(ls, GYM);
    return 2;
}

static int dgn_in_bounds(lua_State *ls)
{
    int x = luaL_checkint(ls, 1);
    int y = luaL_checkint(ls, 2);

    lua_pushboolean(ls, in_bounds(x, y));
    return 1;
}

static int dgn_grid(lua_State *ls)
{
    GETCOORD(c, 1, 2, map_bounds);

    if (!lua_isnone(ls, 3))
    {
        const dungeon_feature_type feat = _get_lua_feature(ls, 3);
        if (feat)
        {
            if (crawl_state.generating_level)
                grd(c) = feat;
            else
                dungeon_terrain_changed(c, feat);
        }
    }
    PLUARET(number, grd(c));
}

LUAFN(dgn_distance)
{
    COORDS(p1, 1, 2);
    COORDS(p2, 3, 4);
    lua_pushnumber(ls, distance2(p1, p2));
    return 1;
}

LUAFN(dgn_seen_destroy_feat)
{
    dungeon_feature_type feat = _get_lua_feature(ls, 1);

    lua_pushboolean(ls, seen_destroy_feat(feat));
    return 1;
}

const struct luaL_reg dgn_grid_dlib[] =
{
{ "feature_number", dgn_feature_number },
{ "feature_name", dgn_feature_name },
{ "feature_desc", dgn_feature_desc },
{ "feature_desc_at", dgn_feature_desc_at },
{ "seen_destroy_feat", dgn_seen_destroy_feat },

{ "grid", dgn_grid },
{ "max_bounds", dgn_max_bounds },
{ "in_bounds", dgn_in_bounds },
{ "distance", dgn_distance },

{ nullptr, nullptr }
};
