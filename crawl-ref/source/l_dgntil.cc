/**
 * @file
 * @brief Tiles-specific dungeon builder functions.
**/

#include "AppHdr.h"

#include "cluautil.h"
#include "libutil.h"
#include "l_libs.h"

#include "branch.h"
#include "coord.h"
#include "mapdef.h"

#ifdef USE_TILE

#include "env.h"
#include "tiledef-dngn.h"
#include "tileview.h"


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
        std::string error = "Couldn't find tile '";
        error += tile_name;
        error += "'";
        luaL_argerror(ls, arg, error.c_str());
        return 0;
    }

    return idx;
}
#endif

LUAFN(dgn_lev_floortile)
{
#ifdef USE_TILE
    LEVEL(br, 1);

    tile_flavour flv;
    tile_default_flv(br, flv);

    const char *tile_name = tile_dngn_name(flv.floor);
    PLUARET(string, tile_name);
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_lev_rocktile)
{
#ifdef USE_TILE
    LEVEL(br, 1);

    tile_flavour flv;
    tile_default_flv(br, flv);

    const char *tile_name = tile_dngn_name(flv.wall);
    PLUARET(string, tile_name);
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_lrocktile)
{
    MAP(ls, 1, map);

#ifdef USE_TILE
    map->rock_tile = luaL_checkstring(ls, 2);
    PLUARET(string, map->rock_tile.c_str());
#else
    UNUSED(map);
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_lfloortile)
{
    MAP(ls, 1, map);

#ifdef USE_TILE
    map->floor_tile = luaL_checkstring(ls, 2);
    PLUARET(string, map->floor_tile.c_str());
#else
    UNUSED(map);
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_change_rock_tile)
{
#ifdef USE_TILE
    std::string tilename = luaL_checkstring(ls, 1);

    tileidx_t rock;
    if (!tile_dngn_index(tilename.c_str(), &rock))
    {
        std::string error = "Couldn't find tile '";
        error += tilename;
        error += "'";
        luaL_argerror(ls, 1, error.c_str());
        return 0;
    }
    env.tile_default.wall     = rock;
    env.tile_default.wall_idx =
        store_tilename_get_index(tilename);

    PLUARET(string, tilename.c_str());
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_change_floor_tile)
{
#ifdef USE_TILE
    std::string tilename = luaL_checkstring(ls, 1);

    tileidx_t floor;
    if (!tile_dngn_index(tilename.c_str(), &floor))
    {
        std::string error = "Couldn't find tile '";
        error += tilename;
        error += "'";
        luaL_argerror(ls, 1, error.c_str());
        return 0;
    }
    env.tile_default.floor     = floor;
    env.tile_default.floor_idx =
        store_tilename_get_index(tilename);

    PLUARET(string, tilename.c_str());
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_ftile)
{
#ifdef USE_TILE
    return dgn_map_add_transform(ls, &map_lines::add_floortile);
#else
    return (0);
#endif
}

LUAFN(dgn_rtile)
{
#ifdef USE_TILE
    return dgn_map_add_transform(ls, &map_lines::add_rocktile);
#else
    return (0);
#endif
}

LUAFN(dgn_tile)
{
#ifdef USE_TILE
    return dgn_map_add_transform(ls, &map_lines::add_spec_tile);
#else
    return (0);
#endif
}

LUAFN(dgn_tile_feat_changed)
{
#ifdef USE_TILE
    COORDS(c, 1, 2);

    if (lua_isnil(ls, 3))
    {
        env.tile_flv(c).feat     = 0;
        env.tile_flv(c).feat_idx = 0;
        return (0);
    }

    std::string tilename = luaL_checkstring(ls, 3);

    tileidx_t feat;
    if (!tile_dngn_index(tilename.c_str(), &feat))
    {
        std::string error = "Couldn't find tile '";
        error += tilename;
        error += "'";
        luaL_argerror(ls, 1, error.c_str());
        return 0;
    }
    env.tile_flv(c).feat     = feat;
    env.tile_flv(c).feat_idx =
        store_tilename_get_index(tilename);
#endif

    return (0);
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
{ "lev_floortile", dgn_lev_floortile },
{ "lev_rocktile", dgn_lev_rocktile },
{ "tile_feat_changed", dgn_tile_feat_changed },

{ NULL, NULL }
};
