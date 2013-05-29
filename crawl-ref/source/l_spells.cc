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
