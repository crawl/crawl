/*
 * File:      l_dgngrd.cc
 * Summary:   Grid and dungeon_feature_type-related bindings.
 */

#include "AppHdr.h"

#include "clua.h"
#include "cluautil.h"
#include "l_libs.h"

#include "coord.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "libutil.h"
#include "terrain.h"

static int dgn_feature_number(lua_State *ls)
{
    const std::string &name = luaL_checkstring(ls, 1);
    PLUARET(number, dungeon_feature_by_name(name));
}

static int dgn_feature_name(lua_State *ls)
{
    const unsigned feat = luaL_checkint(ls, 1);
    PLUARET(string,
            dungeon_feature_name(static_cast<dungeon_feature_type>(feat)));
}

static dungeon_feature_type _get_lua_feature(lua_State *ls, int idx)
{
    dungeon_feature_type feat = (dungeon_feature_type)0;
    if (lua_isnumber(ls, idx))
        feat = (dungeon_feature_type)luaL_checkint(ls, idx);
    else if (lua_isstring(ls, idx))
        feat = dungeon_feature_by_name(luaL_checkstring(ls, idx));
    else
        luaL_argerror(ls, idx, "Feature must be a string or a feature index.");

    return feat;
}

dungeon_feature_type check_lua_feature(lua_State *ls, int idx)
{
    const dungeon_feature_type f = _get_lua_feature(ls, idx);
    if (!f)
        luaL_argerror(ls, idx, "Invalid dungeon feature");
    return (f);
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
    const std::string s =
    feature_description(feat, NUM_TRAPS, "", dtype, need_stop);
    lua_pushstring(ls, s.c_str());
    return (1);
}

static int dgn_feature_desc_at(lua_State *ls)
{
    const description_level_type dtype =
    lua_isnumber(ls, 3)?
    static_cast<description_level_type>(luaL_checkint(ls, 3)) :
    description_type_by_name(lua_tostring(ls, 3));
    const bool need_stop = lua_isboolean(ls, 4)? lua_toboolean(ls, 4) : false;
    const std::string s =
    feature_description(coord_def(luaL_checkint(ls, 1),
                                  luaL_checkint(ls, 2)),
                        false, dtype, need_stop);
    lua_pushstring(ls, s.c_str());
    return (1);
}

static int dgn_set_feature_desc_short(lua_State *ls)
{
    const std::string base_name = luaL_checkstring(ls, 1);
    const std::string desc      = luaL_checkstring(ls, 2);

    if (base_name.empty())
    {
        luaL_argerror(ls, 1, "Base name can't be empty");
        return (0);
    }

    set_feature_desc_short(base_name, desc);

    return (0);
}

static int dgn_set_feature_desc_long(lua_State *ls)
{
    const std::string raw_name = luaL_checkstring(ls, 1);
    const std::string desc     = luaL_checkstring(ls, 2);

    if (raw_name.empty())
    {
        luaL_argerror(ls, 1, "Raw name can't be empty");
        return (0);
    }

    set_feature_desc_long(raw_name, desc);

    return (0);
}

static int dgn_set_feature_quote(lua_State *ls)
{
    const std::string raw_name = luaL_checkstring(ls, 1);
    const std::string quote     = luaL_checkstring(ls, 2);

    if (raw_name.empty())
    {
        luaL_argerror(ls, 1, "Raw name can't be empty");
        return (0);
    }

    set_feature_quote(raw_name, quote);

    return (0);
}

static int dgn_max_bounds(lua_State *ls)
{
    lua_pushnumber(ls, GXM);
    lua_pushnumber(ls, GYM);
    return (2);
}

static int dgn_in_bounds(lua_State *ls)
{
    int x = luaL_checkint(ls, 1);
    int y = luaL_checkint(ls, 2);

    lua_pushboolean(ls, in_bounds(x, y));
    return (1);
}

static int dgn_grid(lua_State *ls)
{
    GETCOORD(c, 1, 2, map_bounds);

    if (!lua_isnone(ls, 3))
    {
        const dungeon_feature_type feat = _get_lua_feature(ls, 3);
        if (feat)
            grd(c) = feat;
    }
    PLUARET(number, grd(c));
}

LUAFN(dgn_distance)
{
    COORDS(p1, 1, 2);
    COORDS(p2, 3, 4);
    lua_pushnumber(ls, distance(p1, p2));
    return (1);
}

LUAFN(dgn_seen_replace_feat)
{
    dungeon_feature_type f1 = _get_lua_feature(ls, 1);
    dungeon_feature_type f2 = _get_lua_feature(ls, 2);

    lua_pushboolean(ls, seen_replace_feat(f1, f2));
    return (1);
}

const struct luaL_reg dgn_grid_dlib[] =
{
{ "feature_number", dgn_feature_number },
{ "feature_name", dgn_feature_name },
{ "feature_desc", dgn_feature_desc },
{ "feature_desc_at", dgn_feature_desc_at },
{ "set_feature_desc_short", dgn_set_feature_desc_short },
{ "set_feature_desc_long", dgn_set_feature_desc_long },
{ "set_feature_quote", dgn_set_feature_quote },
{ "seen_replace_feat", dgn_seen_replace_feat },

{ "grid", dgn_grid },
{ "max_bounds", dgn_max_bounds },
{ "in_bounds", dgn_in_bounds },
{ "distance", dgn_distance },



{ NULL, NULL }
};
