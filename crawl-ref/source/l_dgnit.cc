/**
 * @file
 * @brief Item-related functions in lua library "dgn".
**/

#include "AppHdr.h"

#include "l_libs.h"

#include "cluautil.h"
#include "coord.h"
#include "dungeon.h"
#include "items.h"
#include "stash.h"
#include "stringutil.h"

#define ITEMLIST_METATABLE "crawldgn.item_list"

static item_list _lua_get_ilist(lua_State *ls, int ndx)
{
    if (lua_isstring(ls, ndx))
    {
        const char *spec = lua_tostring(ls, ndx);

        item_list ilist;
        const string err = ilist.add_item(spec);
        if (!err.empty())
            luaL_error(ls, err.c_str());

        return ilist;
    }
    else
    {
        item_list **ilist =
            clua_get_userdata<item_list*>(ls, ITEMLIST_METATABLE, ndx);
        if (ilist)
            return **ilist;

        luaL_argerror(ls, ndx, "Expected item list object or string");
        return item_list();
    }
}

void register_itemlist(lua_State *ls)
{
    clua_register_metatable(ls, ITEMLIST_METATABLE, nullptr,
                            lua_object_gc<item_list>);
}

static int dgn_item_from_index(lua_State *ls)
{
    const int index = luaL_checkint(ls, 1);

    item_def *item = &mitm[index];

    if (item->defined())
        clua_push_item(ls, item);
    else
        lua_pushnil(ls);

    return 1;
}

static int dgn_items_at(lua_State *ls)
{
    COORDS(c, 1, 2);
    lua_push_floor_items(ls, env.igrid(c));
    return 1;
}

static int _dgn_item_spec(lua_State *ls)
{
    const item_list ilist = _lua_get_ilist(ls, 1);
    dlua_push_object_type<item_list>(ls, ITEMLIST_METATABLE, ilist);
    return 1;
}

static int dgn_create_item(lua_State *ls)
{
    COORDS(c, 1, 2);

    item_list ilist = _lua_get_ilist(ls, 3);

    dgn_place_multiple_items(ilist, c);
    link_items();
    return 0;
}

static int dgn_item_property_remove(lua_State *ls)
{
    if (item_def *item = clua_get_item(ls, 1))
        item->props.erase(luaL_checkstring(ls, 2));
    return 0;
}

static int dgn_item_property_set(lua_State *ls)
{
    if (item_def *item = clua_get_item(ls, 1))
    {
        const string key = luaL_checkstring(ls, 2);
        const string type = luaL_checkstring(ls, 3);
        if (type.empty() || type.length() > 1)
        {
            luaL_error(ls, "Expected type: [BbSifsC], got: '%s'",
                       type.c_str());
        }

        switch (type[0])
        {
        case 'B':
            item->props[key].get_bool() = lua_toboolean(ls, 4);
            break;
        case 'b':
            item->props[key].get_byte() = luaL_checkint(ls, 4);
            break;
        case 'S':
            item->props[key].get_short() = luaL_checkint(ls, 4);
            break;
        case 'i':
            item->props[key].get_int() = luaL_checkint(ls, 4);
            break;
        case 'f':
            item->props[key].get_float() = luaL_checknumber(ls, 4);
            break;
        case 's':
            item->props[key].get_string() = luaL_checkstring(ls, 4);
            break;
        case 'C':
            item->props[key].get_coord() = coord_def(luaL_checkint(ls, 4),
                                                     luaL_checkint(ls, 5));
            break;
        default:
            luaL_error(ls, "Unknown type: '%s'", type.c_str());
            break;
        }
    }
    return 0;
}

static int dgn_item_property(lua_State *ls)
{
    if (item_def *item = clua_get_item(ls, 1))
    {
        const string key = luaL_checkstring(ls, 2);
        const string type = luaL_checkstring(ls, 3);
        if (type.empty() || type.length() > 1)
        {
            luaL_error(ls, "Expected type: [BbSifsC], got: '%s'",
                       type.c_str());
        }

        if (!item->props.exists(key))
        {
            lua_pushnil(ls);
            return 1;
        }

        switch (type[0])
        {
        case 'B':
            lua_pushboolean(ls, item->props[key].get_byte());
            break;
        case 'b':
            lua_pushnumber(ls, item->props[key].get_byte());
            break;
        case 'S':
            lua_pushnumber(ls, item->props[key].get_short());
            break;
        case 'i':
            lua_pushnumber(ls, item->props[key].get_int());
            break;
        case 'f':
            lua_pushnumber(ls, item->props[key].get_float());
            break;
        case 's':
            lua_pushstring(ls, item->props[key].get_string().c_str());
            break;
        case 'C':
        {
            const coord_def p(item->props[key].get_coord());
            lua_pushnumber(ls, p.x);
            lua_pushnumber(ls, p.y);
            return 2;
        }
        default:
            luaL_error(ls, "Unknown type: '%s'", type.c_str());
            break;
        }
    }
    return 1;
}

// Returns two arrays: one of floor items, one of shop items.
static int dgn_stash_items(lua_State *ls)
{
    unsigned min_value  = lua_isnumber(ls, 1)  ? luaL_checkint(ls, 1) : 0;
    bool skip_stackable = lua_isboolean(ls, 2) ? lua_toboolean(ls, 2)
                                                : false;
    vector<const item_def*> floor_items;
    vector<const item_def*> shop_items;

    for (ST_ItemIterator stii; stii; ++stii)
    {
        // if this function is added to clua or used for something else than
        // troves, we'll might need to parametrize this
        if (stii->flags & ISFLAG_UNOBTAINABLE)
            continue;

        if (skip_stackable && is_stackable_item(*stii))
            continue;
        if (min_value > 0)
        {
            if (stii.shop())
            {
                if (stii.price() < min_value)
                    continue;
            }
            else if (item_value(*stii, true) < min_value)
                continue;
        }
        if (stii.shop())
            shop_items.push_back(&(*stii));
        else
            floor_items.push_back(&(*stii));
    }

    lua_newtable(ls);
    int index = 0;

    for (const item_def *item : floor_items)
    {
        clua_push_item(ls, const_cast<item_def*>(item));
        lua_rawseti(ls, -2, ++index);
    }

    lua_newtable(ls);
    index = 0;

    for (const item_def *item : shop_items)
    {
        clua_push_item(ls, const_cast<item_def*>(item));
        lua_rawseti(ls, -2, ++index);
    }

    return 2;
}

const struct luaL_reg dgn_item_dlib[] =
{
    { "item_from_index", dgn_item_from_index },
    { "items_at", dgn_items_at },
    { "create_item", dgn_create_item },
    { "item_property_remove", dgn_item_property_remove },
    { "item_property_set", dgn_item_property_set },
    { "item_property", dgn_item_property },
    { "item_spec", _dgn_item_spec },
    { "stash_items", dgn_stash_items },

    { nullptr, nullptr }
};
