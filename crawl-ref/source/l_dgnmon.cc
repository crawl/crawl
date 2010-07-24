/*
 * File:     l_dgnmon.cc
 * Summary:  Monster-related functions in lua library "dgn".
 */

#include "AppHdr.h"

#include "cluautil.h"
#include "l_libs.h"

#include "dungeon.h"
#include "env.h"
#include "libutil.h"
#include "mapdef.h"
#include "mon-util.h"
#include "mon-place.h"
#include "coord.h"
#include "mon-stuff.h"

#define MONSLIST_METATABLE "crawldgn.monster_list"

static mons_list _lua_get_mlist(lua_State *ls, int ndx)
{
    if (lua_isstring(ls, ndx))
    {
        const char *spec = lua_tostring(ls, ndx);
        mons_list mlist;
        const std::string err = mlist.add_mons(spec);
        if (!err.empty())
            luaL_error(ls, err.c_str());
        return (mlist);
    }
    else
    {
        mons_list **mlist =
        clua_get_userdata<mons_list*>(ls, MONSLIST_METATABLE, ndx);
        if (mlist)
            return (**mlist);

        luaL_argerror(ls, ndx, "Expected monster list object or string");
        return mons_list();
    }
}

void register_monslist(lua_State *ls)
{
    clua_register_metatable(ls, MONSLIST_METATABLE, NULL,
                            lua_object_gc<mons_list>);
}

static int dgn_set_random_mon_list(lua_State *ls)
{
    // Don't complain if we're being called when the map is being loaded
    // and validated.
    if (you.level_type != LEVEL_PORTAL_VAULT
        && (you.entering_level || Generating_Level))
    {
        luaL_error(ls, "Can only be used in portal vaults.");
        return (0);
    }

    const int nargs = lua_gettop(ls);

    map_def *map = NULL;
    if (nargs > 2)
    {
        luaL_error(ls, "Too many arguments.");
        return (0);
    }
    else if (nargs == 0)
    {
        luaL_error(ls, "Too few arguments.");
        return (0);
    }
    else if (nargs == 2)
    {
        map_def **_map =
        clua_get_userdata<map_def*>(ls, MAP_METATABLE, 1);
        map = *_map;
    }

    if (map)
    {
        if (map->orient != MAP_ENCOMPASS || map->place.is_valid()
            || !map->depths.empty())
        {
            luaL_error(ls, "Can only be used in portal vaults.");
            return (0);
        }
    }

    int       list_pos = (map != NULL) ? 2 : 1;
    mons_list mlist    = _lua_get_mlist(ls, list_pos);

    if (mlist.size() == 0)
    {
        luaL_argerror(ls, list_pos, "Mon list is empty.");
        return (0);
    }

    if (mlist.size() > 1)
    {
        luaL_argerror(ls, list_pos, "Mon list must contain only one slot.");
        return (0);
    }

    const int num_mons = mlist.slot_size(0);

    if (num_mons == 0)
    {
        luaL_argerror(ls, list_pos, "Mon list is empty.");
        return (0);
    }

    std::vector<mons_spec> mons;
    int num_lords = 0;
    for (int i = 0; i < num_mons; i++)
    {
        mons_spec mon = mlist.get_monster(0, i);

        // Pandemonium lords are pseudo-unique, so don't randomly generate
        // them.
        if (mon.mid == MONS_PANDEMONIUM_DEMON)
        {
            num_lords++;
            continue;
        }

        std::string name;
        if (mon.place.is_valid())
        {
            if (mon.place.level_type == LEVEL_LABYRINTH
                || mon.place.level_type == LEVEL_PORTAL_VAULT)
            {
                std::string err;
                err = make_stringf("mon #%d: Can't use Lab or Portal as a "
                                   "monster place.", i + 1);
                luaL_argerror(ls, list_pos, err.c_str());
                return(0);
            }
            name = mon.place.describe();
        }
        else
        {
            if (mon.mid == RANDOM_MONSTER || mon.monbase == RANDOM_MONSTER)
            {
                std::string err;
                err = make_stringf("mon #%d: can't use random monster in "
                                   "list specifying random monsters", i + 1);
                luaL_argerror(ls, list_pos, err.c_str());
                return(0);
            }
            if (mon.mid == -1)
                mon.mid = MONS_PROGRAM_BUG;
            name = mons_type_name(mon.mid, DESC_PLAIN);
        }

        mons.push_back(mon);

        if (mon.number != 0)
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : number for %s "
                 "being discarded.",
                 name.c_str());

        if (mon.band)
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : band request for "
                 "%s being ignored.",
                 name.c_str());

        if (mon.colour != BLACK)
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : colour for "
                 "%s being ignored.",
                 name.c_str());

        if (mon.items.size() > 0)
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : items for "
                 "%s being ignored.",
                 name.c_str());
    } // for (int i = 0; i < num_mons; i++)

    if (mons.size() == 0 && num_lords > 0)
    {
        luaL_argerror(ls, list_pos,
                      "Mon list contains only pandemonium lords.");
        return (0);
    }

    if (map)
        map->random_mons = mons;
    else
        set_vault_mon_list(mons);

    return (0);
}

static int dgn_mons_from_index(lua_State *ls)
{
    const int index = luaL_checkint(ls, 1);

    monsters *mons = &menv[index];

    if (mons->type != -1)
        push_monster(ls, mons);
    else
        lua_pushnil(ls);

    return (1);
}

static int dgn_mons_at(lua_State *ls)
{
    COORDS(c, 1, 2);

    monsters *mon = monster_at(c);
    if (mon && mon->alive())
        push_monster(ls, mon);
    else
        lua_pushnil(ls);
    return (1);
}


static int dgn_create_monster(lua_State *ls)
{
    COORDS(c, 1, 2);

    mons_list mlist = _lua_get_mlist(ls, 3);
    for (int i = 0, size = mlist.size(); i < size; ++i)
    {
        mons_spec mspec = mlist.get_monster(i);
        const int mid = dgn_place_monster(mspec, you.absdepth0, c,
                                          false, false, false);
        if (mid != -1)
        {
            push_monster(ls, &menv[mid]);
            return (1);
        }
    }
    lua_pushnil(ls);
    return (1);
}

static int _dgn_monster_spec(lua_State *ls)
{
    const mons_list mlist = _lua_get_mlist(ls, 1);
    dlua_push_object_type<mons_list>(ls, MONSLIST_METATABLE, mlist);
    return (1);
}

LUARET1(_dgn_max_monsters, number, MAX_MONSTERS)

LUAFN(dgn_dismiss_monsters)
{
    PLUARET(number,
            dismiss_monsters(lua_gettop(ls) == 0 ? "" :
                             luaL_checkstring(ls, 1)));
}

const struct luaL_reg dgn_mons_dlib[] =
{
{ "set_random_mon_list", dgn_set_random_mon_list },
{ "mons_from_index", dgn_mons_from_index },
{ "mons_at", dgn_mons_at },
{ "create_monster", dgn_create_monster },
{ "monster_spec", _dgn_monster_spec },
{ "max_monsters", _dgn_max_monsters },
{ "dismiss_monsters", dgn_dismiss_monsters },

{ NULL, NULL }
};
