#include "AppHdr.h"

#include "dlua.h"
#include "l_libs.h"

#include "dgnevent.h"

static int dgnevent_type(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    PLUARET(number, dev->type);
}

static int dgnevent_place(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    lua_pushnumber(ls, dev->place.x);
    lua_pushnumber(ls, dev->place.y);
    return (2);
}

static int dgnevent_dest(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    lua_pushnumber(ls, dev->dest.x);
    lua_pushnumber(ls, dev->dest.y);
    return (2);
}

static int dgnevent_ticks(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    PLUARET(number, dev->elapsed_ticks);
}

static int dgnevent_arg1(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    PLUARET(number, dev->arg1);
}

static int dgnevent_arg2(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    PLUARET(number, dev->arg2);
}

const struct luaL_reg dgnevent_lib[] =
{
{ "type",  dgnevent_type },
{ "pos",   dgnevent_place },
{ "dest",  dgnevent_dest },
{ "ticks", dgnevent_ticks },
{ "arg1",  dgnevent_arg1 },
{ "arg2",  dgnevent_arg2 },
{ NULL, NULL }
};

void luaopen_dgnevent(lua_State *ls)
{
    luaopen_setmeta(ls, "dgnevent", dgnevent_lib, DEVENT_METATABLE);
}
