/*** Interact with food and eating.
 * @module food
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "food.h"
#include "item-prop.h"
#include "player.h"

/*** Eat food.
 * Tries to eat.
 * @treturn boolean returns true if we did
 * @function do_eat
 */
static int food_do_eat(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = eat_food();
    lua_pushboolean(ls, eaten);
    return 1;
}

/*** Can we eat this (in our current hunger state)?
 * @tparam items.Item morsel
 * @tparam[opt=true] boolean hungercheck
 * @treturn boolean
 * @function can_eat
 */
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

/*** Is this food a chunk?
 * @tparam items.Item morsel
 * @treturn boolean
 * @function ischunk
 */
static int food_ischunk(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);
    lua_pushboolean(ls, item && item->is_type(OBJ_FOOD, FOOD_CHUNK));
    return 1;
}

/*** Is this edible?
 * Differs from can_eat in that it does not check hunger state
 * @tparam items.Item morsel
 * @treturn boolean
 * @function edible
 */
static int food_edible(lua_State *ls)
{
    LUA_ITEM(ls, item, 1);
    lua_pushboolean(ls, item && !is_inedible(*item));
    return 1;
}

static const struct luaL_reg food_lib[] =
{
    { "do_eat",            food_do_eat },
    { "can_eat",           food_can_eat },
    { "ischunk",           food_ischunk },
    { "edible",            food_edible },
    { nullptr, nullptr },
};

void cluaopen_food(lua_State *ls)
{
    luaL_openlib(ls, "food", food_lib, 0);
}
