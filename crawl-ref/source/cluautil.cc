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
