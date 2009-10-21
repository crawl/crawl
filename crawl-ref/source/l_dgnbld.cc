/*
 * File:       l_dgnbld.cc
 * Summary:    Building routines (library "dgn").
 */

#include "AppHdr.h"

#include <cmath>

#include "cluautil.h"
#include "l_libs.h"

#include "dungeon.h"

// Return a metatable for a point on the map_lines grid.
static int dgn_mapgrd_table(lua_State *ls)
{
    MAP(ls, 1, map);

    map_def **mapref = clua_new_userdata<map_def *>(ls, MAPGRD_METATABLE);
    *mapref = map;

    return (1);
}

static int dgn_width(lua_State *ls)
{
    MAP(ls, 1, map);

    lua_pushnumber(ls, map->map.width());
    return (1);
}

static int dgn_height(lua_State *ls)
{
    MAP(ls, 1, map);

    lua_pushnumber(ls, map->map.height());
    return (1);
}

static void _clamp_to_bounds(int &x, int &y, bool edge_ok = false)
{
    const int edge_offset = edge_ok ? 0 : 1;
    x = std::min(std::max(x, X_BOUND_1 + edge_offset), X_BOUND_2 - edge_offset);
    y = std::min(std::max(y, Y_BOUND_1 + edge_offset), Y_BOUND_2 - edge_offset);
}

static int dgn_fill_area(lua_State *ls)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    dungeon_feature_type feat = check_lua_feature(ls, 5);

    _clamp_to_bounds(x1, y1);
    _clamp_to_bounds(x2, y2);
    if (x2 < x1)
        std::swap(x1, x2);
    if (y2 < y1)
        std::swap(y1, y2);

    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            grd[x][y] = feat;

    return 0;
}

static int dgn_replace_area(lua_State *ls)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    dungeon_feature_type search = check_lua_feature(ls, 5);
    dungeon_feature_type replace = check_lua_feature(ls, 6);

    // gracefully handle out of bound areas by truncating them.
    _clamp_to_bounds(x1, y1);
    _clamp_to_bounds(x2, y2);
    if (x2 < x1)
        std::swap(x1, x2);
    if (y2 < y1)
        std::swap(y1, y2);

    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            if (grd[x][y] == search)
                grd[x][y] = replace;

    return 0;
}

static int dgn_octa_room(lua_State *ls)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    int oblique = luaL_checkint(ls, 5);
    dungeon_feature_type fill = check_lua_feature(ls, 6);

    spec_room sr;
    sr.tl.x = x1;
    sr.br.x = x2;
    sr.tl.y = y1;
    sr.br.y = y2;

    octa_room(sr, oblique, fill);

    return 0;
}

static int dgn_make_pillars(lua_State *ls)
{
    int center_x = luaL_checkint(ls, 1);
    int center_y = luaL_checkint(ls, 2);
    int num = luaL_checkint(ls, 3);
    int scale_x = luaL_checkint(ls, 4);
    int big_radius = luaL_checkint(ls, 5);
    int pillar_radius = luaL_checkint(ls, 6);
    dungeon_feature_type fill = check_lua_feature(ls, 8);

    // [enne] The underscore is for DJGPP's brain damage.
    const float _PI = 3.14159265f;
    for (int n = 0; n < num; n++)
    {
        float angle = n * 2 * _PI / (float)num;
        int x = (int)std::floor(std::cos(angle) * big_radius * scale_x + 0.5f);
        int y = (int)std::floor(std::sin(angle) * big_radius + 0.5f);

        lua_pushvalue(ls, 7);
        lua_pushnumber(ls, center_x + x);
        lua_pushnumber(ls, center_y + y);
        lua_pushnumber(ls, pillar_radius);
        lua_pushnumber(ls, fill);

        lua_call(ls, 4, 0);
    }

    return 0;
}

static int dgn_make_square(lua_State *ls)
{
    int center_x = luaL_checkint(ls, 1);
    int center_y = luaL_checkint(ls, 2);
    int radius = std::abs(luaL_checkint(ls, 3));
    dungeon_feature_type fill = check_lua_feature(ls, 4);

    for (int x = -radius; x <= radius; x++)
        for (int y = -radius; y <= radius; y++)
            grd[center_x + x][center_y + y] = fill;

    return 0;
}

static int dgn_make_rounded_square(lua_State *ls)
{
    int center_x = luaL_checkint(ls, 1);
    int center_y = luaL_checkint(ls, 2);
    int radius = std::abs(luaL_checkint(ls, 3));
    dungeon_feature_type fill = check_lua_feature(ls, 4);

    for (int x = -radius; x <= radius; x++)
        for (int y = -radius; y <= radius; y++)
            if (std::abs(x) != radius || std::abs(y) != radius)
                grd[center_x + x][center_y + y] = fill;

    return 0;
}

static int dgn_make_circle(lua_State *ls)
{
    int center_x = luaL_checkint(ls, 1);
    int center_y = luaL_checkint(ls, 2);
    int radius = std::abs(luaL_checkint(ls, 3));
    dungeon_feature_type fill = check_lua_feature(ls, 4);

    for (int x = -radius; x <= radius; x++)
        for (int y = -radius; y <= radius; y++)
            if (x * x + y * y < radius * radius)
                grd[center_x + x][center_y + y] = fill;

    return 0;
}

static int dgn_in_bounds(lua_State *ls)
{
    int x = luaL_checkint(ls, 1);
    int y = luaL_checkint(ls, 2);

    lua_pushboolean(ls, in_bounds(x, y));
    return 1;
}

static int dgn_replace_first(lua_State *ls)
{
    int x = luaL_checkint(ls, 1);
    int y = luaL_checkint(ls, 2);
    int dx = luaL_checkint(ls, 3);
    int dy = luaL_checkint(ls, 4);
    dungeon_feature_type search = check_lua_feature(ls, 5);
    dungeon_feature_type replace = check_lua_feature(ls, 6);

    _clamp_to_bounds(x, y);
    bool found = false;
    while (in_bounds(x, y))
    {
        if (grd[x][y] == search)
        {
            grd[x][y] = replace;
            found = true;
            break;
        }

        x += dx;
        y += dy;
    }

    lua_pushboolean(ls, found);
    return 1;
}

static int dgn_replace_random(lua_State *ls)
{
    dungeon_feature_type search = check_lua_feature(ls, 1);
    dungeon_feature_type replace = check_lua_feature(ls, 2);

    int x, y;
    do
    {
        x = random2(GXM);
        y = random2(GYM);
    }
    while (grd[x][y] != search);

    grd[x][y] = replace;

    return 0;
}

static int dgn_spotty_level(lua_State *ls)
{
    bool seeded = lua_toboolean(ls, 1);
    int iterations = luaL_checkint(ls, 2);
    bool boxy = lua_toboolean(ls, 3);

    spotty_level(seeded, iterations, boxy);
    return 0;
}

static int dgn_smear_feature(lua_State *ls)
{
    int iterations = luaL_checkint(ls, 1);
    bool boxy = lua_toboolean(ls, 2);
    dungeon_feature_type feat = check_lua_feature(ls, 3);

    int x1 = luaL_checkint(ls, 4);
    int y1 = luaL_checkint(ls, 5);
    int x2 = luaL_checkint(ls, 6);
    int y2 = luaL_checkint(ls, 7);

    _clamp_to_bounds(x1, y1, true);
    _clamp_to_bounds(x2, y2, true);

    smear_feature(iterations, boxy, feat, x1, y1, x2, y2);

    return 0;
}

static int dgn_count_feature_in_box(lua_State *ls)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    dungeon_feature_type feat = check_lua_feature(ls, 5);

    lua_pushnumber(ls, count_feature_in_box(x1, y1, x2, y2, feat));
    return 1;
}

static int dgn_count_antifeature_in_box(lua_State *ls)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    dungeon_feature_type feat = check_lua_feature(ls, 5);

    lua_pushnumber(ls, count_antifeature_in_box(x1, y1, x2, y2, feat));
    return 1;
}

static int dgn_count_neighbours(lua_State *ls)
{
    int x = luaL_checkint(ls, 1);
    int y = luaL_checkint(ls, 2);
    dungeon_feature_type feat = check_lua_feature(ls, 3);

    lua_pushnumber(ls, count_neighbours(x, y, feat));
    return 1;
}

static int dgn_join_the_dots(lua_State *ls)
{
    int from_x = luaL_checkint(ls, 1);
    int from_y = luaL_checkint(ls, 2);
    int to_x = luaL_checkint(ls, 3);
    int to_y = luaL_checkint(ls, 4);
    // TODO enne - push map masks to lua?
    unsigned map_mask = MMT_VAULT;
    bool early_exit = lua_toboolean(ls, 5);

    coord_def from(from_x, from_y);
    coord_def to(to_x, to_y);

    bool ret = join_the_dots(from, to, map_mask, early_exit);
    lua_pushboolean(ls, ret);

    return 1;
}

static int dgn_fill_disconnected_zones(lua_State *ls)
{
    int from_x = luaL_checkint(ls, 1);
    int from_y = luaL_checkint(ls, 2);
    int to_x = luaL_checkint(ls, 3);
    int to_y = luaL_checkint(ls, 4);

    dungeon_feature_type feat = check_lua_feature(ls, 5);

    process_disconnected_zones(from_x, from_y, to_x, to_y, true, feat);

    return 0;
}

const struct luaL_reg dgn_build_dlib[] =
{
{ "mapgrd_table", dgn_mapgrd_table },
{ "width", dgn_width },
{ "height", dgn_height },
{ "fill_area", dgn_fill_area },
{ "replace_area", dgn_replace_area },
{ "octa_room", dgn_octa_room },
{ "make_pillars", dgn_make_pillars },
{ "make_square", dgn_make_square },
{ "make_rounded_square", dgn_make_rounded_square },
{ "make_circle", dgn_make_circle },
{ "in_bounds", dgn_in_bounds },
{ "replace_first", dgn_replace_first },
{ "replace_random", dgn_replace_random },
{ "spotty_level", dgn_spotty_level },
{ "smear_feature", dgn_smear_feature },
{ "count_feature_in_box", dgn_count_feature_in_box },
{ "count_antifeature_in_box", dgn_count_antifeature_in_box },
{ "count_neighbours", dgn_count_neighbours },
{ "join_the_dots", dgn_join_the_dots },
{ "fill_disconnected_zones", dgn_fill_disconnected_zones },

{ NULL, NULL }
};
