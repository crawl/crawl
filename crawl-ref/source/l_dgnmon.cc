/**
 * @file
 * @brief Monster-related functions in lua library "dgn".
**/

#include "AppHdr.h"

#include "branch.h"
#include "cluautil.h"
#include "coord.h"
#include "l_libs.h"
#include "dungeon.h"
#include "env.h"
#include "libutil.h"
#include "mapdef.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "mon-util.h"

#define MONSLIST_METATABLE "crawldgn.monster_list"

static mons_list _lua_get_mlist(lua_State *ls, int ndx)
{
    if (lua_isstring(ls, ndx))
    {
        const char *spec = lua_tostring(ls, ndx);
        mons_list mlist;
        const string err = mlist.add_mons(spec);
        if (!err.empty())
            luaL_error(ls, err.c_str());
        return mlist;
    }
    else
    {
        mons_list **mlist =
        clua_get_userdata<mons_list*>(ls, MONSLIST_METATABLE, ndx);
        if (mlist)
            return **mlist;

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
    const int nargs = lua_gettop(ls);

    map_def *map = NULL;
    if (nargs > 2)
    {
        luaL_error(ls, "Too many arguments.");
        return 0;
    }
    else if (nargs == 0)
    {
        luaL_error(ls, "Too few arguments.");
        return 0;
    }
    else if (nargs == 2)
    {
        map_def **_map =
        clua_get_userdata<map_def*>(ls, MAP_METATABLE, 1);
        map = *_map;
    }

    int       list_pos = (map != NULL) ? 2 : 1;
    mons_list mlist    = _lua_get_mlist(ls, list_pos);

    if (mlist.empty())
    {
        luaL_argerror(ls, list_pos, "Mon list is empty.");
        return 0;
    }

    if (mlist.size() > 1)
    {
        luaL_argerror(ls, list_pos, "Mon list must contain only one slot.");
        return 0;
    }

    const int num_mons = mlist.slot_size(0);

    if (num_mons == 0)
    {
        luaL_argerror(ls, list_pos, "Mon list is empty.");
        return 0;
    }

    vector<mons_spec> mons;
    int num_lords = 0;
    for (int i = 0; i < num_mons; i++)
    {
        mons_spec mon = mlist.get_monster(0, i);

        // Pandemonium lords are pseudo-unique, so don't randomly generate
        // them.
        if (mon.type == MONS_PANDEMONIUM_LORD)
        {
            num_lords++;
            continue;
        }

        string name;
        if (mon.place.is_valid())
        {
            if (!branch_has_monsters(mon.place.branch))
            {
                string err;
                err = make_stringf("mon #%d: Can't use a place with no natural "
                                   "monsters as a monster place.", i + 1);
                luaL_argerror(ls, list_pos, err.c_str());
                return 0;
            }
            name = mon.place.describe();
        }
        else
        {
            if (mon.type == RANDOM_MONSTER || mon.monbase == RANDOM_MONSTER)
            {
                string err;
                err = make_stringf("mon #%d: can't use random monster in "
                                   "list specifying random monsters", i + 1);
                luaL_argerror(ls, list_pos, err.c_str());
                return 0;
            }
#if TAG_MAJOR_VERSION == 34
            if ((int)mon.type == -1)
                mon.type = MONS_PROGRAM_BUG;
#endif
            if (mon.type == MONS_NO_MONSTER)
                name = "nothing";
            else
            {
                name = mons_type_name(static_cast<monster_type>(mon.type),
                                      DESC_PLAIN);
            }
        }

        mons.push_back(mon);

        if (mon.number != 0)
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : number for %s "
                 "being discarded.",
                 name.c_str());

        if (mon.colour != BLACK)
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : colour for "
                 "%s being ignored.",
                 name.c_str());

        if (!mon.items.empty())
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : items for "
                 "%s being ignored.",
                 name.c_str());
    } // for (int i = 0; i < num_mons; i++)

    if (mons.empty() && num_lords > 0)
    {
        luaL_argerror(ls, list_pos,
                      "Mon list contains only pandemonium lords.");
        return 0;
    }

    if (map)
        map->random_mons = mons;
    else
        set_vault_mon_list(mons);

    return 0;
}

static int dgn_mons_from_mid(lua_State *ls)
{
    const int mid = luaL_checkint(ls, 1);

    monster* mons = monster_by_mid(mid);

    if (mons->type != MONS_NO_MONSTER)
        push_monster(ls, mons);
    else
        lua_pushnil(ls);

    return 1;
}

static int dgn_mons_at(lua_State *ls)
{
    COORDS(c, 1, 2);

    monster* mon = monster_at(c);
    if (mon && mon->alive())
        push_monster(ls, mon);
    else
        lua_pushnil(ls);
    return 1;
}


static int dgn_create_monster(lua_State *ls)
{
    COORDS(c, 1, 2);

    mons_list mlist = _lua_get_mlist(ls, 3);
    for (int i = 0, size = mlist.size(); i < size; ++i)
    {
        mons_spec mspec = mlist.get_monster(i);
        if (monster *mon = dgn_place_monster(mspec, c, false, false, false))
        {
            push_monster(ls, mon);
            return 1;
        }
    }
    lua_pushnil(ls);
    return 1;
}

static int _dgn_monster_spec(lua_State *ls)
{
    const mons_list mlist = _lua_get_mlist(ls, 1);
    dlua_push_object_type<mons_list>(ls, MONSLIST_METATABLE, mlist);
    return 1;
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
{ "mons_from_mid", dgn_mons_from_mid },
{ "mons_at", dgn_mons_at },
{ "create_monster", dgn_create_monster },
{ "monster_spec", _dgn_monster_spec },
{ "max_monsters", _dgn_max_monsters },
{ "dismiss_monsters", dgn_dismiss_monsters },

{ NULL, NULL }
};
