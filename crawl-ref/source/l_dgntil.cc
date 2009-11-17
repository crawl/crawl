/*
 * File:        l_dgntile.cc
 * Summary:     Tiles-specific dungeon builder functions.
 */

#include "AppHdr.h"

#include "cluautil.h"
#include "l_libs.h"

#include "branch.h"
#include "mapdef.h"

#ifdef USE_TILE

#include "env.h"

unsigned int get_tile_idx(lua_State *ls, int arg)
{
    if (!lua_isstring(ls, arg))
    {
        luaL_argerror(ls, arg, "Expected string for tile name");
        return 0;
    }

    const char *tile_name = luaL_checkstring(ls, arg);

    unsigned int idx;
    if (!tile_dngn_index(tile_name, idx))
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
    LEVEL(lev, br, 1);

    tile_flavour flv;
    tile_default_flv(lev, br, flv);

    const char *tile_name = tile_dngn_name(flv.floor);
    PLUARET(string, tile_name);
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_lev_rocktile)
{
#ifdef USE_TILE
    LEVEL(lev, br, 1);

    tile_flavour flv;
    tile_default_flv(lev, br, flv);

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
    unsigned short tile = get_tile_idx(ls, 2);
    map->rock_tile = tile;

    const char *tile_name = tile_dngn_name(tile);
    PLUARET(string, tile_name);
#else
    UNUSED(map);
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_lfloortile)
{
    MAP(ls, 1, map);

#ifdef USE_TILE
    unsigned short tile = get_tile_idx(ls, 2);
    map->floor_tile = tile;

    const char *tile_name = tile_dngn_name(tile);
    PLUARET(string, tile_name);
#else
    UNUSED(map);
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_change_rock_tile)
{
#ifdef USE_TILE
    unsigned short tile = get_tile_idx(ls, 1);
    if (tile)
        env.tile_default.wall = tile;

    const char *tile_name = tile_dngn_name(tile);
    PLUARET(string, tile_name);
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_change_floor_tile)
{
#ifdef USE_TILE
    unsigned short tile = get_tile_idx(ls, 1);
    if (tile)
        env.tile_default.floor = tile;

    const char *tile_name = tile_dngn_name(tile);
    PLUARET(string, tile_name);
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_ftile)
{
#ifdef USE_TILE
    return dgn_map_add_transform(ls, &map_lines::add_floortile);
#else
    return 0;
#endif
}

LUAFN(dgn_rtile)
{
#ifdef USE_TILE
    return dgn_map_add_transform(ls, &map_lines::add_rocktile);
#else
    return 0;
#endif
}

const struct luaL_reg dgn_tile_dlib[] =
{
{ "lrocktile", dgn_lrocktile },
{ "lfloortile", dgn_lfloortile },
{ "rtile", dgn_rtile },
{ "ftile", dgn_ftile },
{ "change_rock_tile", dgn_change_rock_tile },
{ "change_floor_tile", dgn_change_floor_tile },
{ "lev_floortile", dgn_lev_floortile },
{ "lev_rocktile", dgn_lev_rocktile },

{ NULL, NULL }
};
