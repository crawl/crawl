#include "AppHdr.h"

#include "clua.h"
#include "l_libs.h"

#include "food.h"
#include "invent.h"
#include "items.h"
#include "player.h"

/////////////////////////////////////////////////////////////////////
// Food information.

static int food_do_eat(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = eat_food(-1);
    lua_pushboolean(ls, eaten);
    return (1);
}

static int food_prompt_eat_chunks(lua_State *ls)
{
    int eaten = 0;
    if (!you.turn_is_over)
        eaten = prompt_eat_chunks();

    lua_pushboolean(ls, (eaten != 0));
    return (1);
}

static int food_prompt_floor(lua_State *ls)
{
    int eaten = 0;
    if (!you.turn_is_over)
    {
        eaten = eat_from_floor();
        if (eaten == 1)
            burden_change();
    }
    lua_pushboolean(ls, (eaten != 0));
    return (1);
}

static int food_prompt_inventory(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = eat_from_inventory();
    lua_pushboolean(ls, eaten);
    return (1);
}

static int food_prompt_inventory_menu(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = prompt_eat_inventory_item();
    lua_pushboolean(ls, eaten);
    return (1);
}

static int food_can_eat(lua_State *ls)
{
    LUA_ITEM(item, 1);
    bool hungercheck = true;

    if (lua_isboolean(ls, 2))
        hungercheck = lua_toboolean(ls, 2);

    bool edible = item && can_ingest(item->base_type,
                                     item->sub_type,
                                     true,
                                     true,
                                     hungercheck);
    lua_pushboolean(ls, edible);
    return (1);
}

static bool eat_item(const item_def &item)
{
    if (in_inventory(item))
    {
        eat_inventory_item(item.link);
        return (true);
    }
    else
    {
        int ilink = item_on_floor(item, you.pos());

        if (ilink != NON_ITEM)
        {
            eat_floor_item(ilink);
            return (true);
        }
        return (false);
    }
}

static int food_eat(lua_State *ls)
{
    LUA_ITEM(item, 1);

    bool eaten = false;
    if (!you.turn_is_over)
    {
        // When we get down to eating, we don't care if the eating is courtesy
        // an un-ided amulet of the gourmand.
        bool edible = item && can_ingest(item->base_type,
                                         item->sub_type,
                                         false,
                                         false);
        if (edible)
            eaten = eat_item(*item);
    }
    lua_pushboolean(ls, eaten);
    return (1);
}

static int food_rotting(lua_State *ls)
{
    LUA_ITEM(item, 1);

    bool rotting = false;
    if (item && item->base_type == OBJ_FOOD && item->sub_type == FOOD_CHUNK)
        rotting = food_is_rotten(*item);

    lua_pushboolean(ls, rotting);
    return (1);
}

static int food_dangerous(lua_State *ls)
{
    LUA_ITEM(item, 1);

    bool dangerous = false;
    if (item)
    {
        dangerous = (is_poisonous(*item) || is_mutagenic(*item)
                     || causes_rot(*item) || is_forbidden_food(*item));
    }
    lua_pushboolean(ls, dangerous);
    return (1);
}

static int food_ischunk(lua_State *ls)
{
    LUA_ITEM(item, 1);
    lua_pushboolean(ls,
            item && item->base_type == OBJ_FOOD
                 && item->sub_type == FOOD_CHUNK);
    return (1);
}

static const struct luaL_reg food_lib[] =
{
    { "do_eat",            food_do_eat },
    { "prompt_eat_chunks", food_prompt_eat_chunks },
    { "prompt_floor",      food_prompt_floor },
    { "prompt_inventory",  food_prompt_inventory },
    { "prompt_inv_menu",   food_prompt_inventory_menu },
    { "can_eat",           food_can_eat },
    { "eat",               food_eat },
    { "rotting",           food_rotting },
    { "dangerous",         food_dangerous },
    { "ischunk",           food_ischunk },
    { NULL, NULL },
};

void cluaopen_food(lua_State *ls)
{
    luaL_openlib(ls, "food", food_lib, 0);
}
