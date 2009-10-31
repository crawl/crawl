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
        push_monster(ls, (monsters *) t->data);
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

void clua_push_coord(lua_State *ls, const coord_def &c)
{
    lua_pushnumber(ls, c.x);
    lua_pushnumber(ls, c.y);
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
    {
        luaL_openlib(ls, NULL, lr, 0);
    }
}


template <typename list, typename lpush>
static int dlua_gentable(lua_State *ls, const list &strings, lpush push)
{
    lua_newtable(ls);
    for (int i = 0, size = strings.size(); i < size; ++i)
    {
        push(ls, strings[i]);
        lua_rawseti(ls, -2, i + 1);
    }
    return (1);
}

inline static void dlua_pushcxxstring(lua_State *ls, const std::string &s)
{
    lua_pushstring(ls, s.c_str());
}

int dlua_stringtable(lua_State *ls, const std::vector<std::string> &s)
{
    return dlua_gentable(ls, s, dlua_pushcxxstring);
}
