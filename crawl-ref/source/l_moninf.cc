/*
 * File:      l_moninf.cc
 * Summary:   User-accessible monster info.
 */

#include "AppHdr.h"

#include "l_libs.h"

#include "cluautil.h"
#include "mon-info.h"

#define MONINF_METATABLE "monster.info"

void lua_push_moninf(lua_State *ls, monster_info *mi)
{
    monster_info **miref = 
        clua_new_userdata<monster_info *>(ls, MONINF_METATABLE);
    *miref = mi;
}

#define MONINF(ls, n, var) \
    monster_info *var = *(monster_info **) \
        luaL_checkudata(ls, n, MONINF_METATABLE)

#define MIRET1(type, field) \
    static int moninf_get_##field(lua_State *ls) \
    { \
        MONINF(ls, 1, mi); \
        lua_push##type(ls, mi->m_##field); \
        return (1); \
    }

#define MIREG(field) { #field, moninf_get_##field }

MIRET1(number, damage_level)

LUAFN(moninf_get_damage_desc)
{
    MONINF(ls, 1, mi);
    lua_pushstring(ls, mi->m_damage_desc.c_str());
    return (1);
}

static const struct luaL_reg moninf_lib[] =
{
    MIREG(damage_level),
    MIREG(damage_desc),

    { NULL, NULL }
};

void cluaopen_moninf(lua_State *ls)
{
    clua_register_metatable(ls, MONINF_METATABLE, moninf_lib,
                            lua_object_gc<monster_info>);
}
