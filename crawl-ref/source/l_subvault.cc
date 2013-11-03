/**
 * @file
 * @brief Subvault routines (library "dgn").
**/

#include "AppHdr.h"

#include "cluautil.h"
#include "libutil.h"
#include "l_libs.h"
#include "mapdef.h"

static int dgn_is_subvault(lua_State *ls)
{
    MAP(ls, 1, map);

    lua_pushboolean(ls, map->is_subvault());
    return 1;
}

static int dgn_default_subvault_glyphs(lua_State *ls)
{
    MAP(ls, 1, map);

    map->apply_subvault_mask();
    return 0;
}

static int dgn_subvault_cell_valid(lua_State *ls)
{
    MAP(ls, 1, map);
    coord_def c;
    c.x = luaL_checkint(ls, 2);
    c.y = luaL_checkint(ls, 3);

    lua_pushboolean(ls, map->subvault_cell_valid(c));
    return 1;
}

static int dgn_subvault_size(lua_State *ls)
{
    MAP(ls, 1, map);

    lua_pushnumber(ls, map->subvault_width());
    lua_pushnumber(ls, map->subvault_height());
    return 2;
}

const struct luaL_reg dgn_subvault_dlib[] =
{
{ "is_subvault", dgn_is_subvault },
{ "default_subvault_glyphs", dgn_default_subvault_glyphs },
{ "subvault_cell_valid", dgn_subvault_cell_valid },
{ "subvault_size", dgn_subvault_size },

{ NULL, NULL }
};
