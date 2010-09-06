/*
 * File:      l_moninf.cc
 * Summary:   User-accessible monster info.
 */

#include "AppHdr.h"

#include "l_libs.h"

#include "cluautil.h"
#include "coord.h"
#include "env.h"
#include "libutil.h"
#include "mon-info.h"
#include "player.h"

#define MONINF_METATABLE "monster.info"

void lua_push_moninf(lua_State *ls, monster_info *mi)
{
    monster_info **miref =
        clua_new_userdata<monster_info *>(ls, MONINF_METATABLE);
    *miref = new monster_info(*mi);
}

#define MONINF(ls, n, var) \
    monster_info *var = *(monster_info **) \
        luaL_checkudata(ls, n, MONINF_METATABLE)

#define MIRET1(type, field, cfield) \
    static int moninf_get_##field(lua_State *ls) \
    { \
        MONINF(ls, 1, mi); \
        lua_push##type(ls, mi->cfield); \
        return (1); \
    }

#define MIREG(field) { #field, moninf_get_##field }

MIRET1(number, damage_level, dam)
MIRET1(boolean, is_safe, is(MB_SAFE))
MIRET1(string, mname, mname.c_str())
MIRET1(number, type, type)
MIRET1(number, base_type, base_type)
MIRET1(number, number, number)
MIRET1(number, colour, colour)

LUAFN(moninf_get_is)
{
    MONINF(ls, 1, mi);
    int num = luaL_checknumber(ls, 2);
    lua_pushboolean(ls, mi->is(num));
    return (1);
}

LUAFN(moninf_get_damage_desc)
{
    MONINF(ls, 1, mi);
    std::string s = mi->damage_desc();
    lua_pushstring(ls, s.c_str());
    return (1);
}

LUAFN(moninf_get_desc)
{
    MONINF(ls, 1, mi);
    std::string desc;
    int col;
    mi->to_string(1, desc, col);
    lua_pushstring(ls, desc.c_str());
    return (1);
}

static const struct luaL_reg moninf_lib[] =
{
    MIREG(type),
    MIREG(base_type),
    MIREG(number),
    MIREG(colour),
    MIREG(mname),
    MIREG(is),
    MIREG(is_safe),
    MIREG(damage_level),
    MIREG(damage_desc),
    MIREG(desc),

    { NULL, NULL }
};

// XXX: unify with directn.cc/h
// This uses relative coordinates with origin the player.
bool in_show_bounds(const coord_def &s)
{
    return (s.rdist() <= ENV_SHOW_OFFSET);
}

LUAFN(mi_get_monster_at)
{
    COORDSHOW(s, 1, 2)
    coord_def p = player2grid(s);
    if (!you.see_cell(p))
        return (0);
    if (env.mgrid(p) == NON_MONSTER)
        return (0);
    monster* m = &env.mons[env.mgrid(p)];
    if (!m->visible_to(&you))
        return (0);
    monster_info mi(m);
    lua_push_moninf(ls, &mi);
    return (1);
}

static const struct luaL_reg mon_lib[] =
{
    { "get_monster_at", mi_get_monster_at },

    { NULL, NULL }
};

void cluaopen_moninf(lua_State *ls)
{
    clua_register_metatable(ls, MONINF_METATABLE, moninf_lib,
                            lua_object_gc<monster_info>);
    luaL_openlib(ls, "monster", mon_lib, 0);
}
