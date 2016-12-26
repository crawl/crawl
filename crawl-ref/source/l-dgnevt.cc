#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "dgnevent.h"

/*
 * Methods for DEVENT_METATABLE.
 */

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
    return 2;
}

static int dgnevent_dest(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    lua_pushnumber(ls, dev->dest.x);
    lua_pushnumber(ls, dev->dest.y);
    return 2;
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

static const struct luaL_reg dgnevent_dlib[] =
{
{ "type",  dgnevent_type },
{ "pos",   dgnevent_place },
{ "dest",  dgnevent_dest },
{ "ticks", dgnevent_ticks },
{ "arg1",  dgnevent_arg1 },
{ "arg2",  dgnevent_arg2 },
{ nullptr, nullptr }
};

void luaopen_dgnevent(lua_State *ls)
{
    luaopen_setmeta(ls, "dgnevent", dgnevent_dlib, DEVENT_METATABLE);
}

/*
 * Functions for library "dgn".
 */

static const char *dgn_event_type_names[] =
{
"none", "turn", "mons_move", "player_move", "leave_level",
"entering_level", "entered_level", "player_los", "player_climb",
"monster_dies", "item_pickup", "item_moved", "feat_change",
"unused", "door_opened", "door_closed", "hp_warning",
"pressure_plate",
};

static dgn_event_type dgn_event_type_by_name(const string &name)
{
    for (unsigned i = 0; i < ARRAYSZ(dgn_event_type_names); ++i)
        if (dgn_event_type_names[i] == name)
            return static_cast<dgn_event_type>(i? 1 << (i - 1) : 0);
    return DET_NONE;
}

static const char *dgn_event_type_name(unsigned evmask)
{
    if (evmask == 0)
        return dgn_event_type_names[0];

    for (unsigned i = 1; i < ARRAYSZ(dgn_event_type_names); ++i)
        if (evmask & (1 << (i - 1)))
            return dgn_event_type_names[i];

    return dgn_event_type_names[0];
}

static void dgn_push_event_type(lua_State *ls, int n)
{
    if (lua_isstring(ls, n))
        lua_pushnumber(ls, dgn_event_type_by_name(lua_tostring(ls, n)));
    else if (lua_isnumber(ls, n))
        lua_pushstring(ls, dgn_event_type_name(luaL_checkint(ls, n)));
    else
        lua_pushnil(ls);
}

static int dgn_dgn_event(lua_State *ls)
{
    const int start = lua_isuserdata(ls, 1)? 2 : 1;
    int retvals = 0;
    for (int i = start, nargs = lua_gettop(ls); i <= nargs; ++i)
    {
        dgn_push_event_type(ls, i);
        retvals++;
    }
    return retvals;
}

static dgn_event_type dgn_param_to_event_type(lua_State *ls, int n)
{
    if (lua_isnumber(ls, n))
        return static_cast<dgn_event_type>(luaL_checkint(ls, n));
    else if (lua_isstring(ls, n))
        return dgn_event_type_by_name(lua_tostring(ls, n));
    else
        return DET_NONE;
}

static int dgn_dgn_event_is_global(lua_State *ls)
{
    lua_pushboolean(ls, dgn_param_to_event_type(ls, 1) & DET_GLOBAL_MASK);
    return 1;
}

static int dgn_dgn_event_is_position(lua_State *ls)
{
    lua_pushboolean(ls, dgn_param_to_event_type(ls, 1) & DET_POSITION_MASK);
    return 1;
}

const struct luaL_reg dgn_event_dlib[] =
{
{ "dgn_event_type",        dgn_dgn_event },
{ "dgn_event_is_global",   dgn_dgn_event_is_global },
{ "dgn_event_is_position", dgn_dgn_event_is_position},

{ nullptr, nullptr }
};
