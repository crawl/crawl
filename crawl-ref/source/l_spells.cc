/*
 * File:     l_spells.cc
 * Summary:  Boolean feat-related functions lua library "feat".
 */

#include "AppHdr.h"

#include "clua.h"
#include "cluautil.h"
#include "l_libs.h"

#include "coord.h"
#include "dungeon.h"
#include "env.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "terrain.h"

LUAWRAP(_refrigeration, cast_refrigeration(luaL_checkint(ls, 1), true))
LUAWRAP(_toxic_radiance, cast_toxic_radiance(true))

const struct luaL_reg spells_dlib[] =
{
{ "refrigeration", _refrigeration },
{ "toxic_radiance", _toxic_radiance },
{ NULL, NULL }
};

