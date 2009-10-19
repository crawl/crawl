/*
 * File:     l_dgnit.cc
 * Summary:  Item-related functions in lua library "dgn".
 */

#include "AppHdr.h"

#include "dlua.h"
#include "l_libs.h"

#include "dungeon.h"
#include "items.h"
#include "mapdef.h"

#define ITEMLIST_METATABLE "crawldgn.item_list"

static item_list _lua_get_ilist(lua_State *ls, int ndx)
{
    if (lua_isstring(ls, ndx))
    {
        const char *spec = lua_tostring(ls, ndx);

        item_list ilist;
        const std::string err = ilist.add_item(spec);
        if (!err.empty())
            luaL_error(ls, err.c_str());

        return (ilist);
    }
    else
    {
        item_list **ilist =
            clua_get_userdata<item_list*>(ls, ITEMLIST_METATABLE, ndx);
        if (ilist)
            return (**ilist);

        luaL_argerror(ls, ndx, "Expected item list object or string");
        return item_list();
    }
}

void register_itemlist(lua_State *ls)
{
    clua_register_metatable(ls, ITEMLIST_METATABLE, NULL,
                            lua_object_gc<item_list>);
}

static int dgn_item_from_index(lua_State *ls)
{
    const int index = luaL_checkint(ls, 1);

    item_def *item = &mitm[index];

    if (is_valid_item(*item))
        lua_pushlightuserdata(ls, item);
    else
        lua_pushnil(ls);

    return (1);
}

static int dgn_items_at(lua_State *ls)
{
    COORDS(c, 1, 2);
    lua_push_items(ls, env.igrid(c));
    return (1);
}

static int _dgn_item_spec(lua_State *ls)
{
    const item_list ilist = _lua_get_ilist(ls, 1);
    dlua_push_object_type<item_list>(ls, ITEMLIST_METATABLE, ilist);
    return (1);
}

static int dgn_create_item(lua_State *ls)
{
    COORDS(c, 1, 2);

    item_list ilist = _lua_get_ilist(ls, 3);
    const int level =
    lua_isnumber(ls, 4) ? lua_tointeger(ls, 4) : you.your_level;

    dgn_place_multiple_items(ilist, c, level);
    link_items();
    return (0);
}

const struct luaL_reg dgn_item_lib[] =
{
{ "item_from_index", dgn_item_from_index },
{ "items_at", dgn_items_at },
{ "create_item", dgn_create_item },
{ "item_spec", _dgn_item_spec },

{ NULL, NULL }
};
