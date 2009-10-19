#include "AppHdr.h"

#include "dlua.h"
#include "l_libs.h"

#include "los.h"
#include "spells3.h"

LUARET1(you_can_hear_pos, boolean,
        player_can_hear(coord_def(luaL_checkint(ls,1), luaL_checkint(ls, 2))))
LUARET1(you_x_pos, number, you.pos().x)
LUARET1(you_y_pos, number, you.pos().y)
LUARET2(you_pos, number, you.pos().x, you.pos().y)

// see_cell should not be exposed to user scripts. The game should
// never disclose grid coordinates to the player. Therefore we load it
// only into the core Lua interpreter (dlua), never into the user
// script interpreter (clua).
LUARET1(you_see_cell, boolean,
        see_cell(luaL_checkint(ls, 1), luaL_checkint(ls, 2)))
LUARET1(you_see_cell_no_trans, boolean,
        see_cell_no_trans(luaL_checkint(ls, 1), luaL_checkint(ls, 2)))

LUAFN(you_moveto)
{
    const coord_def place(luaL_checkint(ls, 1), luaL_checkint(ls, 2));
    ASSERT(map_bounds(place));
    you.moveto(place);
    return (0);
}

LUAFN(you_random_teleport)
{
    you_teleport_now(false, false);
    return (0);
}

LUAFN(you_losight)
{
    calc_show_los();
    return (0);
}

const struct luaL_reg you_lib[] =
{
{ "hear_pos", you_can_hear_pos },
{ "x_pos", you_x_pos },
{ "y_pos", you_y_pos },
{ "pos",   you_pos },
{ "moveto", you_moveto },
{ "see_cell",          you_see_cell },
{ "see_cell_no_trans", you_see_cell_no_trans },
{ "random_teleport", you_random_teleport },
{ "losight", you_losight },
{ NULL, NULL }
};
