#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "mapdef.h"

/////////////////////////////////////////////////////////////////////
// mapgrd and mapgrd_col handling (i.e. map_lines in a metatable)

struct mapcolumn
{
    map_def* map;
    int col;
};

static int mapgrd_get(lua_State *ls)
{
    // Return a metatable for this column in the map grid.
    map_def *map = *(map_def **) luaL_checkudata(ls, 1, MAPGRD_METATABLE);

    int column = luaL_checkint(ls, 2);

    mapcolumn *mapref = clua_new_userdata<mapcolumn>(ls, MAPGRD_COL_METATABLE);
    mapref->map = map;
    mapref->col = column;

    return 1;
}

static int mapgrd_set(lua_State *ls)
{
    return luaL_error(ls, "%s", "Cannot assign to read-only table.");
}

static char* mapgrd_glyph(lua_State *ls, int &col, int &row)
{
    mapcolumn *mapc = (mapcolumn *)luaL_checkudata(ls, 1, MAPGRD_COL_METATABLE);
    row = luaL_checkint(ls, 2);
    col = mapc->col;

    map_lines &lines = mapc->map->map;
    if (row < 0 || col < 0 || col >= lines.width() || row >= lines.height())
        return nullptr;

    coord_def mc(col, row);
    return &lines(mc);
}

static int mapgrd_col_get(lua_State *ls)
{
    int col, row;
    char *gly = mapgrd_glyph(ls, col, row);
    if (!gly)
        return luaL_error(ls, "Invalid coords: %d, %d", col, row);

    char buf[2];
    buf[0] = *gly;
    buf[1] = '\0';

    lua_pushstring(ls, buf);

    return 1;
}

static int mapgrd_col_set(lua_State *ls)
{
    int col, row;
    char *gly = mapgrd_glyph(ls, col, row);
    if (!gly)
        return luaL_error(ls, "Invalid coords: %d, %d", col, row);

    const char *str = luaL_checkstring(ls, 3);
    if (!str[0] || str[1])
        return luaL_error(ls, "%s", "mapgrd must be set to a single char.");

    (*gly) = str[0];

    return 0;
}

void dluaopen_mapgrd(lua_State *ls)
{
    // mapgrd table
    luaL_newmetatable(ls, MAPGRD_METATABLE);
    lua_pushstring(ls, "__index");
    lua_pushcfunction(ls, mapgrd_get);
    lua_settable(ls, -3);

    lua_pushstring(ls, "__newindex");
    lua_pushcfunction(ls, mapgrd_set);
    lua_settable(ls, -3);

    lua_pop(ls, 1);

    // mapgrd col table
    luaL_newmetatable(ls, MAPGRD_COL_METATABLE);
    lua_pushstring(ls, "__index");
    lua_pushcfunction(ls, mapgrd_col_get);
    lua_settable(ls, -3);

    lua_pushstring(ls, "__newindex");
    lua_pushcfunction(ls, mapgrd_col_set);
    lua_settable(ls, -3);

    lua_pop(ls, 1);
}
