/**
 * @file
 * @brief Boolean feat-related functions lua library "feat".
**/

#include "AppHdr.h"

#include "clua.h"
#include "cluautil.h"
#include "l_libs.h"

#include "env.h"
#include "spl-damage.h"
#include "spl-cast.h"
#include "spl-util.h"

LUAFN(l_spells_mana_cost)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_mana(spell));
}

LUAFN(l_spells_range)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_range(spell, calc_spell_power(spell, true)));
}

LUAFN(l_spells_fail)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, failure_rate_to_int(spell_fail(spell)));
}

static const struct luaL_reg spells_clib[] =
{
    { "mana_cost"     , l_spells_mana_cost },
    { "range"         , l_spells_range },
    { "fail"          , l_spells_fail },
    { NULL, NULL }
};

void cluaopen_spells(lua_State *ls)
{
    luaL_openlib(ls, "spells", spells_clib, 0);
}

LUAWRAP(_refrigeration,
        cast_los_attack_spell(SPELL_OZOCUBUS_REFRIGERATION,
                              luaL_checkint(ls, 1), NULL, true))
LUAWRAP(_toxic_radiance,
        cast_los_attack_spell(SPELL_OLGREBS_TOXIC_RADIANCE,
                              luaL_checkint(ls, 1), NULL, true))

const struct luaL_reg spells_dlib[] =
{
{ "refrigeration", _refrigeration },
{ "toxic_radiance", _toxic_radiance },
{ NULL, NULL }
};
