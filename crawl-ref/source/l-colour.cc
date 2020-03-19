/*** Colour related functions (dlua only).
 * @module colour
 */

#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "colour.h"
#include "dlua.h"
#include "mpr.h"

typedef int (*lua_element_colour_calculator)(int, const coord_def&, lua_datum);

struct lua_element_colour_calc : public base_colour_calc
{
    lua_element_colour_calc(element_type _type, string _name,
                            lua_datum _function)
        : base_colour_calc(_type, _name), function(_function) {}

    virtual int get(const coord_def& loc = coord_def(),
                    bool non_random = false) override;

protected:
    lua_datum function;
};

int lua_element_colour_calc::get(const coord_def& loc, bool non_random)
{
    lua_State *ls = dlua.state();

    function.push();
    lua_pushinteger(ls, rand(non_random));
    lua_pushinteger(ls, loc.x);
    lua_pushinteger(ls, loc.y);
    if (!dlua.callfn(nullptr, 3, 1))
    {
        mprf(MSGCH_WARN, "%s", dlua.error.c_str());
        return BLACK;
    }

    string colour = luaL_checkstring(ls, -1);
    lua_pop(ls, 1);

    return str_to_colour(colour,-1,false,true);
}

/***
 * Define a new elemental colour.
 * @within dlua
 * @tparam string name colour name
 * @tparam colour_generation_function fun generation function
 * @function add_colour
 */
/***
 * This function is not a member of the module, but documents the expected
 * behaviour of the second argument to add_colour.
 * @within dlua
 * @tparam int rand random number between 0 and 119
 * @tparam int x
 * @tparam int y The coordinates of the cell to be coloured
 * @treturn string The name of the basic colour to be used
 * @function colour_generation_function
 */
LUAFN(l_add_colour)
{
    const string &name = luaL_checkstring(ls, 1);
    if (lua_gettop(ls) != 2 || !lua_isfunction(ls, 2))
        luaL_error(ls, "Expected colour generation function.");

    int col = str_to_colour(name, -1, false, true);
    if (col == -1)
        luaL_error(ls, ("Unknown colour: " + name).c_str());

    CLua& vm(CLua::get_vm(ls));
    lua_datum function(vm, 2);

    add_element_colour(
        new lua_element_colour_calc((element_type)col, name, function)
    );

    return 0;
}

static const struct luaL_reg colour_lib[] =
{
    { "add_colour", l_add_colour },

    { nullptr, nullptr }
};

void dluaopen_colour(lua_State *ls)
{
    luaL_openlib(ls, "colour", colour_lib, 0);
}
