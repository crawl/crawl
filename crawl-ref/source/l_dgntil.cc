/**
 * @file
 * @brief Tiles-specific dungeon builder functions.
**/

#include "AppHdr.h"

#include "l_libs.h"

#include "cluautil.h"
#include "coord.h"
#include "env.h"
#include "mapdef.h"
#include "stringutil.h"
#include "tiledef-dngn.h"

tileidx_t get_tile_idx(lua_State *ls, int arg)
{
    if (!lua_isstring(ls, arg))
    {
        luaL_argerror(ls, arg, "Expected string for tile name");
        return 0;
    }

    const char *tile_name = luaL_checkstring(ls, arg);

    tileidx_t idx;
    if (!tile_dngn_index(tile_name, &idx))
    {
        string error = "Couldn't find tile '";
        error += tile_name;
        error += "'";
        luaL_argerror(ls, arg, error.c_str());
        return 0;
    }

    return idx;
}

LUAFN(dgn_lrocktile)
{
    MAP(ls, 1, map);

    map->rock_tile = luaL_checkstring(ls, 2);
    PLUARET(string, map->rock_tile.c_str());
}

LUAFN(dgn_lfloortile)
{
    MAP(ls, 1, map);

    map->floor_tile = luaL_checkstring(ls, 2);
    PLUARET(string, map->floor_tile.c_str());
}

LUAFN(dgn_change_rock_tile)
{
    string tilename = luaL_checkstring(ls, 1);

    tileidx_t rock;
    if (!tile_dngn_index(tilename.c_str(), &rock))
    {
        string error = "Couldn't find tile '";
        error += tilename;
        error += "'";
        luaL_argerror(ls, 1, error.c_str());
        return 0;
    }
    env.tile_default.wall     = rock;
    env.tile_default.wall_idx =
        store_tilename_get_index(tilename);

    PLUARET(string, tilename.c_str());
}

LUAFN(dgn_change_floor_tile)
{
    string tilename = luaL_checkstring(ls, 1);

    tileidx_t floor;
    if (!tile_dngn_index(tilename.c_str(), &floor))
    {
        string error = "Couldn't find tile '";
        error += tilename;
        error += "'";
        luaL_argerror(ls, 1, error.c_str());
        return 0;
    }
    env.tile_default.floor     = floor;
    env.tile_default.floor_idx =
        store_tilename_get_index(tilename);

    PLUARET(string, tilename.c_str());
}

LUAFN(dgn_ftile)
{
    return dgn_map_add_transform(ls, &map_lines::add_floortile);
}

LUAFN(dgn_rtile)
{
    return dgn_map_add_transform(ls, &map_lines::add_rocktile);
}

LUAFN(dgn_tile)
{
    return dgn_map_add_transform(ls, &map_lines::add_spec_tile);
}

LUAFN(dgn_tile_feat_changed)
{
    COORDS(c, 1, 2);

    if (lua_isnil(ls, 3))
    {
        env.tile_flv(c).feat     = 0;
        env.tile_flv(c).feat_idx = 0;
        return 0;
    }

    string tilename = luaL_checkstring(ls, 3);

    tileidx_t feat;
    if (!tile_dngn_index(tilename.c_str(), &feat))
    {
        string error = "Couldn't find tile '";
        error += tilename;
        error += "'";
        luaL_argerror(ls, 1, error.c_str());
        return 0;
    }
    env.tile_flv(c).feat     = feat;
    env.tile_flv(c).feat_idx =
        store_tilename_get_index(tilename);

    return 0;
}

LUAFN(dgn_tile_floor_changed)
{
    COORDS(c, 1, 2);

    if (lua_isnil(ls, 3))
    {
        env.tile_flv(c).floor     = 0;
        env.tile_flv(c).floor_idx = 0;
        return 0;
    }

    string tilename = luaL_checkstring(ls, 3);

    tileidx_t floor;
    if (!tile_dngn_index(tilename.c_str(), &floor))
    {
        string error = "Couldn't find tile '";
        error += tilename;
        error += "'";
        luaL_argerror(ls, 1, error.c_str());
        return 0;
    }
    env.tile_flv(c).floor     = floor;
    env.tile_flv(c).floor_idx =
        store_tilename_get_index(tilename);

    return 0;
}

const struct luaL_reg dgn_tile_dlib[] =
{
{ "lrocktile", dgn_lrocktile },
{ "lfloortile", dgn_lfloortile },
{ "rtile", dgn_rtile },
{ "ftile", dgn_ftile },
{ "tile", dgn_tile },
{ "change_rock_tile", dgn_change_rock_tile },
{ "change_floor_tile", dgn_change_floor_tile },
{ "tile_feat_changed", dgn_tile_feat_changed },
{ "tile_floor_changed", dgn_tile_floor_changed },

{ nullptr, nullptr }
};
