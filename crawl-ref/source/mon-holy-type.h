#pragma once

#include "enum.h"

enum mon_holy_type_flags
{
    MH_NONE              = 0,
    MH_HOLY              = 1<<0,
    MH_NATURAL           = 1<<1,
    MH_UNDEAD            = 1<<2,
    MH_DEMONIC           = 1<<3,
    MH_NONLIVING         = 1<<4, // golems and other constructs
    MH_PLANT             = 1<<5,
    MH_LAST = MH_PLANT,
};
DEF_BITFIELD(mon_holy_type, mon_holy_type_flags, 5);
COMPILE_CHECK(mon_holy_type::exponent(mon_holy_type::last_exponent)
                                            == mon_holy_type_flags::MH_LAST);
