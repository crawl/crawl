#include "AppHdr.h"

#include "cluautil.h"
#include "clua.h"
#include "l_libs.h"

#include "delay.h"

int push_activity_interrupt(lua_State *ls, activity_interrupt_data *t)
{
    if (!t->data)
    {
        lua_pushnil(ls);
        return 0;
    }

    switch (t->apt)
    {
    case AIP_HP_LOSS:
        {
            const ait_hp_loss *ahl = (const ait_hp_loss *) t->data;
            lua_pushnumber(ls, ahl->hp);
            lua_pushnumber(ls, ahl->hurt_type);
            return 1;
        }
    case AIP_INT:
        lua_pushnumber(ls, *(const int *) t->data);
        break;
    case AIP_STRING:
        lua_pushstring(ls, (const char *) t->data);
        break;
    case AIP_MONSTER:
        // FIXME: We're casting away the const...
        push_monster(ls, (monster*) t->data);
        break;
    default:
        lua_pushnil(ls);
        break;
    }
    return 0;
}

void clua_push_map(lua_State *ls, map_def *map)
{
    map_def **mapref = clua_new_userdata<map_def *>(ls, MAP_METATABLE);
    *mapref = map;
}

void clua_push_dgn_event(lua_State *ls, const dgn_event *devent)
{
    const dgn_event **de =
        clua_new_userdata<const dgn_event *>(ls, DEVENT_METATABLE);
    *de = devent;
}

void luaopen_setmeta(lua_State *ls,
                     const char *global,
                     const luaL_reg *lua_lib,
                     const char *meta)
{
    luaL_newmetatable(ls, meta);
    lua_setglobal(ls, global);

    luaL_openlib(ls, global, lua_lib, 0);

    // Do <global>.__index = <global>
    lua_pushstring(ls, "__index");
    lua_pushvalue(ls, -2);
    lua_settable(ls, -3);
}

void clua_register_metatable(lua_State *ls, const char *tn,
                             const luaL_reg *lr,
                             int (*gcfn)(lua_State *ls))
{
    lua_stack_cleaner clean(ls);
    luaL_newmetatable(ls, tn);
    lua_pushstring(ls, "__index");
    lua_pushvalue(ls, -2);
    lua_settable(ls, -3);

    if (gcfn)
    {
        lua_pushstring(ls, "__gc");
        lua_pushcfunction(ls, gcfn);
        lua_settable(ls, -3);
    }

    if (lr)
        luaL_openlib(ls, NULL, lr, 0);
}

int clua_pushcxxstring(lua_State *ls, const string &s)
{
    lua_pushstring(ls, s.c_str());
    return 1;
}

int clua_stringtable(lua_State *ls, const vector<string> &s)
{
    return clua_gentable(ls, s, clua_pushcxxstring);
}

// Pushes a coord_def as a dgn.point Lua object. Note that this is quite
// different from dlua_pushcoord.
int clua_pushpoint(lua_State *ls, const coord_def &pos)
{
    lua_pushnumber(ls, pos.x);
    lua_pushnumber(ls, pos.y);
    CLua &vm(CLua::get_vm(ls));
    if (!vm.callfn("dgn.point", 2, 1))
    {
        luaL_error(ls, "dgn.point(%d,%d) failed: %s",
                   pos.x, pos.y, vm.error.c_str());
    }
    return 1;
}
