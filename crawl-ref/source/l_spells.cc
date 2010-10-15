/*
 * File:     l_spells.cc
 * Summary:  Boolean feat-related functions lua library "feat".
 */

#include "AppHdr.h"

#include "clua.h"
#include "cluautil.h"
#include "l_libs.h"

#include "env.h"
#include "spl-damage.h"

LUAWRAP(_refrigeration, cast_refrigeration(luaL_checkint(ls, 1), true))
LUAWRAP(_toxic_radiance, cast_toxic_radiance(true))

const struct luaL_reg spells_dlib[] =
{
{ "refrigeration", _refrigeration },
{ "toxic_radiance", _toxic_radiance },
{ NULL, NULL }
};
