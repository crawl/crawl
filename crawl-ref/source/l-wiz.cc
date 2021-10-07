/*
--- General game bindings

module "crawl"
*/

#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "mon-util.h"
#include "options.h"
#include "stringutil.h"
#include "wiz-dgn.h"
#include "wiz-fsim.h"
#include "wiz-item.h"

#ifdef WIZARD

//
// Non-user-accessible bindings (dlua).
//

LUAFN(wiz_quick_fsim)
{
    // quick and dirty, get av effective damage. Will crash if
    // monster can't be placed.
    string mon_name = luaL_checkstring(ls, 1);
    monster_type mtype = get_monster_by_name(mon_name, true);
    if (mtype == MONS_PROGRAM_BUG)
    {
        string err = make_stringf("No such monster: '%s'.", mon_name.c_str());
        return luaL_argerror(ls, 1, err.c_str());
    }
    const int fsim_rounds = luaL_safe_checkint(ls, 2);

    Options.fsim_mons = mon_name;
    Options.fsim_rounds = fsim_rounds;

    fight_data fdata = wizard_quick_fsim_raw(false);
    PLUARET(number, fdata.player.av_eff_dam);
}

LUAWRAP(wiz_identify_all_items, wizard_identify_all_items())

LUAWRAP(wiz_map_level, wizard_map_level())

static const struct luaL_reg wiz_dlib[] =
{
{ "quick_fsim", wiz_quick_fsim },
{ "identify_all_items", wiz_identify_all_items},
{ "map_level", wiz_map_level},
{ nullptr, nullptr }
};

void dluaopen_wiz(lua_State *ls)
{
    luaL_openlib(ls, "wiz", wiz_dlib, 0);
}

#endif
