#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "env.h"
#include "mapmark.h"

static int mapmarker_pos(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    lua_pushnumber(ls, mark->pos.x);
    lua_pushnumber(ls, mark->pos.y);
    return 2;
}

static int mapmarker_move(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    const coord_def dest(luaL_safe_checkint(ls, 2), luaL_safe_checkint(ls, 3));
    env.markers.move_marker(mark, dest);
    return 0;
}

static int mapmarker_remove(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    env.markers.remove(mark);
    return 0;
}

static int mapmarker_property(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    lua_pushstring(ls, mark->property(luaL_checkstring(ls, 2)).c_str());
    return 1;
}

const struct luaL_Reg mapmarker_dlib[] =
{
{ "pos", mapmarker_pos },
{ "move", mapmarker_move },
{ "remove", mapmarker_remove },
{ "property", mapmarker_property },
{ nullptr, nullptr }
};

void luaopen_mapmarker(lua_State *ls)
{
    luaopen_setmeta(ls, "mapmarker", mapmarker_dlib, MAPMARK_METATABLE);
}
