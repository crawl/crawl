#include "AppHdr.h"

#include "l-libs.h"

#include "butcher.h"
#include "cluautil.h"
#include "food.h"
#include "itemprop.h"
#include "player.h"

/////////////////////////////////////////////////////////////////////
// Food information.

static int food_do_eat(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = eat_food(-1);
    lua_pushboolean(ls, eaten);
    return 1;
}

static int food_prompt_eat_chunks(lua_State *ls)
{
    int eaten = 0;
    if (!you.turn_is_over)
        eaten = prompt_eat_chunks();

    lua_pushboolean(ls, (eaten != 0));
    return 1;
}

static int food_prompt_inventory_menu(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = prompt_eat_item();
    lua_pushboolean(ls, eaten);
    return 1;
}

static int food_can_eat(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);
    bool hungercheck = true;

    if (lua_isboolean(ls, 2))
        hungercheck = lua_toboolean(ls, 2);

    const bool edible = item && can_eat(*item, true, hungercheck);
    lua_pushboolean(ls, edible);
    return 1;
}

static int food_eat(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);

    bool eaten = false;
    if (!you.turn_is_over && item && can_eat(*item, false))
        eaten = eat_item(*item);
    lua_pushboolean(ls, eaten);
    return 1;
}

static int food_dangerous(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);

    bool dangerous = false;
    if (item)
        dangerous = is_bad_food(*item);

    lua_pushboolean(ls, dangerous);
    return 1;
}

static int food_ischunk(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);
    lua_pushboolean(ls, item && item->is_type(OBJ_FOOD, FOOD_CHUNK));
    return 1;
}

static int food_isfruit(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);
    lua_pushboolean(ls, is_fruit(*item));
    return 1;
}

static int food_ismeaty(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);
    lua_pushboolean(ls, food_is_meaty(*item));
    return 1;
}

static int food_isveggie(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);
    lua_pushboolean(ls, food_is_veggie(*item));
    return 1;
}

static int food_bottleable(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);
    lua_pushboolean(ls, item && item->is_type(OBJ_CORPSES, CORPSE_BODY)
                             && can_bottle_blood_from_corpse(item->mon_type));
    return 1;
}

// differs from food_can_eat in several respects
static int food_edible(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);
    lua_pushboolean(ls, item && !is_inedible(*item));
    return 1;
}

static const struct luaL_reg food_lib[] =
{
    { "do_eat",            food_do_eat },
    { "prompt_eat_chunks", food_prompt_eat_chunks },
    { "prompt_inv_menu",   food_prompt_inventory_menu },
    { "can_eat",           food_can_eat },
    { "eat",               food_eat },
    { "dangerous",         food_dangerous },
    { "ischunk",           food_ischunk },
    { "isfruit",           food_isfruit },
    { "ismeaty",           food_ismeaty },
    { "isveggie",          food_isveggie },
    { "bottleable",        food_bottleable },
    { "edible",            food_edible },
    { nullptr, nullptr },
};

void cluaopen_food(lua_State *ls)
{
    luaL_openlib(ls, "food", food_lib, 0);
}
