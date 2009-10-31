/*
 * File:      l_moninf.cc
 * Summary:   User-accessible monster info.
 */

#include "AppHdr.h"

#include "l_libs.h"

#include "cluautil.h"
#include "env.h"
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

// XXX: unify with directn.cc/h
// This uses relative coordinates with origin the player.
bool in_show_bounds(const coord_def &s)
{
    return (s.rdist() <= ENV_SHOW_OFFSET);
}

coord_def player2grid(const coord_def &s)
{
    return (you.pos() + s);
}

coord_def grid2player(const coord_def &g)
{
    return (g - you.pos());
}

LUAFN(mi_get_monster_at)
{
    COORDSHOW(s, 1, 2)
    coord_def p = player2grid(s);
    if (!you.see_cell(p))
        return (0);
    if (env.mgrid(p) == NON_MONSTER)
        return (0);
    monsters* m = &env.mons[env.mgrid(p)];
    if (!m->visible_to(&you))
        return (0);
    monster_info *mi = new monster_info(m);
    lua_push_moninf(ls, mi);
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
