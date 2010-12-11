/*
 * File:      l_dgnlvl.cc
 * Summary:   Level and branch bindings (library "dgn").
 */

#include "AppHdr.h"

#include "cluautil.h"
#include "l_libs.h"

#include "branch.h"
#include "player.h"

#define BRANCH(br, pos)                                                 \
const char *branch_name = luaL_checkstring(ls, pos);                \
branch_type req_branch_type = str_to_branch(branch_name);           \
if (req_branch_type == NUM_BRANCHES)                                \
luaL_error(ls, "Expected branch name");                         \
Branch &br = branches[req_branch_type]

#define BRANCHFN(name, type, expr)   \
LUAFN(dgn_br_##name) {           \
BRANCH(br, 1);                   \
PLUARET(type, expr);             \
}

BRANCHFN(floorcol, number, br.floor_colour)
BRANCHFN(rockcol, number, br.rock_colour)
BRANCHFN(has_shops, boolean, br.shop_chance > 0)
BRANCHFN(has_uniques, boolean, br.has_uniques)
BRANCHFN(parent_branch, string,
         br.parent_branch == NUM_BRANCHES
             ? ""
             : branches[br.parent_branch].abbrevname)

static void _push_level_id(lua_State *ls, const level_id &lid)
{
    // [ds] Should really set up a metatable to delete (FIXME).
    level_id *nlev = static_cast<level_id*>(
        lua_newuserdata(ls, sizeof(level_id)));
    new (nlev) level_id(lid);
}

level_id dlua_level_id(lua_State *ls, int ndx)
{
    if (lua_isstring(ls, ndx))
    {
        const char *s = lua_tostring(ls, 1);
        try
        {
            return level_id::parse_level_id(s);
        }
        catch (const std::string &err)
        {
            luaL_error(ls, err.c_str());
        }
    }
    else if (lua_isuserdata(ls, ndx))
    {
        const level_id *lid = static_cast<level_id*>(lua_touserdata(ls, ndx));
        return (*lid);
    }

    luaL_argerror(ls, ndx, "Expected level_id");
    // Never gets here.
    return level_id();
}

LUAFN(dgn_level_id)
{
    const int nargs = lua_gettop(ls);
    if (!nargs)
        _push_level_id(ls, level_id::current());
    else if (nargs == 1)
        _push_level_id(ls, dlua_level_id(ls, 1));
    return (1);
}

LUAFN(dgn_level_name)
{
    const level_id lid(dlua_level_id(ls, 1));
    lua_pushstring(ls, lid.describe().c_str());
    return (1);
}

LUAFN(dgn_set_level_type_name)
{
    if (!lua_isstring(ls, 1))
    {
        luaL_argerror(ls, 1, "Expected string for level type name");
        return(0);
    }

    you.level_type_name = luaL_checkstring(ls, 1);

    return(0);
}

LUAFN(dgn_set_level_type_name_abbrev)
{
    if (!lua_isstring(ls, 1))
    {
        luaL_argerror(ls, 1, "Expected string for level type name "
                      "abbreviation");
        return(0);
    }

    you.level_type_name_abbrev = luaL_checkstring(ls, 1);

    return(0);
}

LUAFN(dgn_set_level_type_origin)
{
    if (!lua_isstring(ls, 1))
    {
        luaL_argerror(ls, 1, "Expected string for level type origin");
        return(0);
    }

    you.level_type_origin = luaL_checkstring(ls, 1);

    return(0);
}

const struct luaL_reg dgn_level_dlib[] =
{
{ "br_floorcol", dgn_br_floorcol },
{ "br_rockcol", dgn_br_rockcol },
{ "br_has_shops", dgn_br_has_shops },
{ "br_has_uniques", dgn_br_has_uniques },
{ "br_parent_branch", dgn_br_parent_branch },

{ "level_id", dgn_level_id },
{ "level_name", dgn_level_name },
{ "set_level_type_name", dgn_set_level_type_name },
{ "set_level_type_name_abbrev", dgn_set_level_type_name_abbrev },
{ "set_level_type_origin", dgn_set_level_type_origin },

{ NULL, NULL }
};
