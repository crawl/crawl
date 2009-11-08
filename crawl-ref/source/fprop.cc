/*
 * File:      envmap.cc
 * Summary:   Functions dealing with env.map_knowledge.
 */

#include "AppHdr.h"

#include "fprop.h"

#include "coord.h"
#include "env.h"
#include "stuff.h"

bool is_sanctuary(const coord_def& p)
{
    if (!map_bounds(p))
        return (false);
    return (testbits(env.pgrid(p), FPROP_SANCTUARY_1)
            || testbits(env.pgrid(p), FPROP_SANCTUARY_2));
}

bool is_bloodcovered(const coord_def& p)
{
    return (testbits(env.pgrid(p), FPROP_BLOODY));
}
