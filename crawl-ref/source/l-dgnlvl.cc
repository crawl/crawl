/***
 * @module dgn
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "branch.h"
#include "cluautil.h"
#include "mon-util.h"
#include "player.h"

#define BRANCH(br, pos)                                                 \
const char *branch_name = luaL_checkstring(ls, pos);                \
branch_type req_branch_type = branch_by_abbrevname(branch_name);    \
if (req_branch_type == NUM_BRANCHES)                                \
luaL_error(ls, "Expected branch name");                         \
const Branch &br = branches[req_branch_type]

#define BRANCHFN(name, type, expr)   \
LUAFN(dgn_br_##name) {           \
BRANCH(br, 1);                   \
PLUARET(type, expr);             \
}

BRANCHFN(floorcol, number, br.floor_colour)
BRANCHFN(rockcol, number, br.rock_colour)
BRANCHFN(parent_branch, string,
         br.parent_branch == NUM_BRANCHES
             ? ""
             : branches[parent_branch(br.id)].abbrevname)

LUAFN(dgn_br_depth)
{
    branch_type brn = you.where_are_you;
    if (lua_gettop(ls) == 1)
    {
        const char *branch_name = luaL_checkstring(ls, 1);
        brn = branch_by_abbrevname(branch_name);
        if (brn == NUM_BRANCHES)
            luaL_argerror(ls, 1, "No such branch");
    }
    PLUARET(number, brdepth[brn]);
}

LUAFN(dgn_br_exists)
{
    branch_type brn = you.where_are_you;
    if (lua_gettop(ls) == 1)
    {
        const char *branch_name = luaL_checkstring(ls, 1);
        brn = branch_by_abbrevname(branch_name);
        if (brn == NUM_BRANCHES)
            luaL_argerror(ls, 1, "No such branch");
    }

    PLUARET(boolean, brn == root_branch
                     || !is_connected_branch(brn)
                     || brentry[brn].is_valid());
}

static void _push_level_id(lua_State *ls, const level_id &lid)
{
    // [ds] Should really set up a metatable to delete (FIXME).
    level_id *nlev = static_cast<level_id*>(
        lua_newuserdata(ls, sizeof(level_id)));
    new (nlev) level_id(lid);
}

LUAFN(dgn_br_entrance)
{
    const int nargs = lua_gettop(ls);
    branch_type brn = you.where_are_you;
    if (nargs == 1)
    {
        const char *branch_name = luaL_checkstring(ls, 1);
        brn = branch_by_abbrevname(branch_name);
        if (brn == NUM_BRANCHES)
            luaL_argerror(ls, 1, "No such branch");
    }

    _push_level_id(ls, brentry[brn]);
    return 1;
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
        catch (const bad_level_id &err)
        {
            luaL_error(ls, err.what());
        }
    }
    else if (lua_isuserdata(ls, ndx))
    {
        const level_id *lid = static_cast<level_id*>(lua_touserdata(ls, ndx));
        return *lid;
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
    return 1;
}

LUAFN(dgn_level_name)
{
    const level_id lid(dlua_level_id(ls, 1));
    lua_pushstring(ls, lid.describe().c_str());
    return 1;
}

LUAFN(dgn_zot_orb_type)
{
    lua_pushstring(ls, mons_type_name(you.zot_orb_monster, DESC_DBNAME).c_str());
    return 1;
}

const struct luaL_reg dgn_level_dlib[] =
{
{ "br_floorcol", dgn_br_floorcol },
{ "br_rockcol", dgn_br_rockcol },
{ "br_parent_branch", dgn_br_parent_branch },
{ "br_depth", dgn_br_depth },
{ "br_exists", dgn_br_exists },
{ "br_entrance", dgn_br_entrance },

{ "level_id", dgn_level_id },
{ "level_name", dgn_level_name },

{ "zot_orb_type", dgn_zot_orb_type },

{ nullptr, nullptr }
};
