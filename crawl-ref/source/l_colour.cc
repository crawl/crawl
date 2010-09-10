/*
 * File:     l_colour.cc
 * Summary:  Colour related functions
 */

#include "AppHdr.h"

#include "clua.h"
#include "cluautil.h"
#include "colour.h"
#include "message.h"

typedef int (*lua_element_colour_calculator)(int, const coord_def&,
                                             std::string);

static int _lua_element_colour(int rand, const coord_def& loc,
                               std::string function);

struct lua_element_colour_calc : public element_colour_calc
{
    lua_element_colour_calc(element_type _type, std::string _name,
                            std::string _function)
        : element_colour_calc(_type, _name, (element_colour_calculator)_lua_element_colour),
          function(_function)
        {};

    virtual int get(const coord_def& loc = coord_def(),
                    bool non_random = false);

protected:
    std::string function;
};

int lua_element_colour_calc::get(const coord_def& loc, bool non_random)
{
    // casting function pointers from other function pointers is guaranteed
    // to be safe, but calling them on pointers not of their type isn't, so
    // assert here to be safe - add to this assert if something different is
    // needed
    ASSERT((lua_element_colour_calculator)calc == _lua_element_colour);
    lua_element_colour_calculator real_calc =
        (lua_element_colour_calculator)calc;
    return (*real_calc)(rand(non_random), loc, function);
}

static int next_colour = ETC_FIRST_LUA;

static int _lua_element_colour(int rand, const coord_def& loc,
                               std::string function)
{
    lua_State *ls = clua.state();

    if (clua.loadbuffer(function.data(), function.length(),
                        "lua colour definition"))
    {
        mpr(clua.error.c_str(), MSGCH_WARN);
        return BLACK;
    }
    lua_pushinteger(ls, rand);
    lua_pushinteger(ls, loc.x);
    lua_pushinteger(ls, loc.y);
    if (!clua.callfn(NULL, 3, 1))
    {
        mpr(clua.error.c_str(), MSGCH_WARN);
        return BLACK;
    }

    std::string colour = luaL_checkstring(ls, -1);
    lua_pop(ls, 1);

    return str_to_colour(colour);
}

static int _string_dumper(lua_State *ls, const void *p, size_t sz, void *ud)
{
    std::string *function = (std::string*)ud;
    *function += std::string((const char *)p, sz);
    return 0;
}

LUAFN(l_add_colour)
{
    const std::string &name = luaL_checkstring(ls, 1);
    if (lua_gettop(ls) != 2 || !lua_isfunction(ls, 2))
        luaL_error(ls, "Expected colour generation function.");

    std::string function;
    lua_dump(ls, _string_dumper, &function);

    add_element_colour(
        new lua_element_colour_calc((element_type)(next_colour++),
                                    name, function)
    );

    return 0;
}

static const struct luaL_reg colour_lib[] =
{
    { "add_colour", l_add_colour },

    { NULL, NULL }
};

void dluaopen_colour(lua_State *ls)
{
    luaL_openlib(ls, "colour", colour_lib, 0);
}
